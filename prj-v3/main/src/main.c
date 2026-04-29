/**
 * @file main.c
 * @brief prj-v3 主入口 - 语音交互智能照明网关
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "led_pwm.h"
#include "uart_driver.h"
#include "i2s_driver.h"
#include "wifi_sta.h"
#include "status_led.h"
#include "ld2410_driver.h"
#include "audio_pipeline.h"
#include "voice_commands.h"
#include "command_handler.h"
#include "model_loader.h"
#include "tflm_allocator.h"
#include "env_watcher.h"
#include "priority_arbiter.h"
#include "Lighting_Controller.h"

static const char *TAG = "MAIN";

/* ================================================================
 * WiFi 配置 (从 Kconfig 读取)
 * ================================================================ */

/* 兼容性宏: 如果 Kconfig 未配置，使用默认值 */
#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID       "YourSSID"
#endif

#ifndef CONFIG_WIFI_PASSWORD
#define CONFIG_WIFI_PASSWORD   "YourPassword"
#endif

#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASSWORD  CONFIG_WIFI_PASSWORD

/* ================================================================
 * 系统启动时间记录
 * ================================================================ */

static int64_t g_boot_time_us = 0;

/**
 * @brief 记录启动完成时间
 */
static void record_startup_time(void)
{
    g_boot_time_us = esp_timer_get_time();
    ESP_LOGI(TAG, "=== System startup completed in %.2f seconds ===", 
             g_boot_time_us / 1000000.0);
}

/**
 * @brief 获取系统运行时间 (秒)
 */
static float get_uptime_seconds(void)
{
    return (esp_timer_get_time() - g_boot_time_us) / 1000000.0f;
}

/* ================================================================
 * 回调函数
 * ================================================================ */

static void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "Wi-Fi connected callback triggered");
}

static void on_wifi_disconnected(void)
{
    ESP_LOGW(TAG, "Wi-Fi disconnected callback triggered");
}

static void on_audio_event(const audio_event_t *event, void *user_data)
{
    if (event == NULL) return;
    
    switch (event->type) {
        case AUDIO_EVENT_WAKE_WORD:
            ESP_LOGI(TAG, "Wake word detected: index=%d", event->wake_word_index);
            break;
        case AUDIO_EVENT_COMMAND:
            ESP_LOGI(TAG, "Command recognized: id=%d, phrase=%s, confidence=%.2f",
                     event->command_id, event->command_phrase ? event->command_phrase : "N/A",
                     event->volume);
            /* 提交到仲裁器 */
            priority_arbiter_submit_voice(event->command_id, 0.9f);
            break;
        case AUDIO_EVENT_VAD_SPEECH:
            ESP_LOGD(TAG, "VAD: speech started");
            break;
        case AUDIO_EVENT_VAD_SILENCE:
            ESP_LOGD(TAG, "VAD: silence detected");
            break;
        default:
            break;
    }
}

/* ================================================================
 * 主程序入口
 * ================================================================ */

void app_main(void)
{
    ESP_LOGI(TAG, "=== prj-v3: Voice Interaction Smart LED Gateway ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    g_boot_time_us = esp_timer_get_time();

    /* ============================================================
     * 0. 配置看门狗
     * ============================================================ */
    ESP_LOGI(TAG, "Configuring watchdog...");
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 30000,           /* 30 秒超时 */
        .idle_core_mask = 0,           /* 不监控空闲任务 (ESP-IDF 6.1+) */
        .trigger_panic = true,         /* 超时触发 panic */
    };
    esp_err_t ret = esp_task_wdt_init(&twdt_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Watchdog init failed: %s", esp_err_to_name(ret));
    } else {
        /* 注册主任务到看门狗 */
        esp_task_wdt_add(NULL);
        ESP_LOGI(TAG, "Watchdog configured (30s timeout)");
    }

    /* ============================================================
     * 1. 初始化状态指示灯 (最先初始化，显示启动状态)
     * ============================================================ */
    ESP_LOGI(TAG, "[1/12] Initializing status LED...");
    ret = status_led_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Status LED init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Status LED initialized (booting...)");
        status_led_set_state(STATUS_LED_BOOTING);
        status_led_start();
    }

    /* ============================================================
     * 2. 初始化 LED PWM 驱动
     * ============================================================ */
    ESP_LOGI(TAG, "[2/12] Initializing LED PWM driver...");
    ret = led_pwm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED PWM init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LED PWM initialized successfully");
        led_fade_to_brightness(50, 1000);
        led_set_color_temp(4000);
    }

    /* ============================================================
     * 3. 初始化 UART 驱动
     * ============================================================ */
    ESP_LOGI(TAG, "[3/12] Initializing UART driver...");
    ret = uart_init(115200);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "UART initialized successfully");
    }

    /* ============================================================
     * 4. 初始化 I2S 麦克风驱动
     * ============================================================ */
    ESP_LOGI(TAG, "[4/12] Initializing I2S driver...");
    ret = i2s_init(16000, 16);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "I2S initialized successfully (16kHz, 16-bit)");
    }

    /* ============================================================
     * 5. 初始化 Wi-Fi STA
     * ============================================================ */
    ESP_LOGI(TAG, "[5/12] Initializing Wi-Fi STA...");
    status_led_set_state(STATUS_LED_WIFI_CONNECTING);
    wifi_sta_register_callback(on_wifi_connected, on_wifi_disconnected);
    ret = wifi_sta_init(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Wi-Fi initialized, connecting...");
        ret = wifi_sta_connect(10000);
        if (ret == ESP_OK) {
            char ip[16];
            wifi_sta_get_ip(ip, sizeof(ip));
            ESP_LOGI(TAG, "Wi-Fi connected, IP: %s", ip);
        } else {
            ESP_LOGW(TAG, "Wi-Fi connection failed or timeout, continuing...");
        }
    }

    /* ============================================================
     * 6. 初始化 LD2410 雷达驱动
     * ============================================================ */
    ESP_LOGI(TAG, "[6/12] Initializing LD2410 radar driver...");
    ret = ld2410_init();
    if (ret != 0) {
        ESP_LOGW(TAG, "LD2410 radar init failed: %d", ret);
    } else {
        ESP_LOGI(TAG, "LD2410 radar initialized successfully");
    }

    /* ============================================================
     * 7. 初始化 AI 模型加载器
     * ============================================================ */
    ESP_LOGI(TAG, "[7/12] Initializing model loader...");
    ret = model_loader_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Model loader init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Model loader initialized");
        /* 加载融合决策模型 */
        ret = model_loader_load(MODEL_TYPE_FUSION_MODEL, NULL, NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Fusion model load failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Fusion model loaded");
        }
    }

    /* ============================================================
     * 8. 初始化 TFLM 内存分配器
     * ============================================================ */
    ESP_LOGI(TAG, "[8/12] Initializing TFLM allocator...");
    ret = tflm_allocator_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TFLM allocator init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "TFLM allocator initialized");
    }

    /* ============================================================
     * 9. 初始化音频流水线
     * ============================================================ */
    ESP_LOGI(TAG, "[9/12] Initializing audio pipeline...");
    ret = audio_pipeline_init();
    if (ret != 0) {
        ESP_LOGW(TAG, "Audio pipeline init failed: %d", ret);
    } else {
        ESP_LOGI(TAG, "Audio pipeline initialized");
        audio_pipeline_set_event_callback(on_audio_event, NULL);
    }

    /* ============================================================
     * 10. 初始化仲裁器
     * ============================================================ */
    ESP_LOGI(TAG, "[10/12] Initializing priority arbiter...");
    arbiter_config_t arbiter_cfg = {
        .manual_timeout_ms = 30000,
        .persist_state = true,
        .default_mode = MODE_AUTO,
    };
    ret = priority_arbiter_init(&arbiter_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Priority arbiter init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Priority arbiter initialized");
    }

    /* ============================================================
     * 11. 初始化环境监听模块
     * ============================================================ */
    ESP_LOGI(TAG, "[11/12] Initializing environment watcher...");
    ret = env_watcher_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Environment watcher init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Environment watcher initialized");
    }

    /* ============================================================
     * 12. 初始化照明控制器
     * ============================================================ */
    ESP_LOGI(TAG, "[12/12] Initializing lighting controller...");
    ret = lighting_controller_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lighting controller init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Lighting controller initialized");
        ret = lighting_controller_start();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Lighting controller started");
        }
    }

    /* ============================================================
     * 启动音频流水线
     * ============================================================ */
    ESP_LOGI(TAG, "Starting audio pipeline...");
    ret = audio_pipeline_start();
    if (ret != 0) {
        ESP_LOGW(TAG, "Audio pipeline start failed: %d", ret);
    } else {
        ESP_LOGI(TAG, "Audio pipeline started");
    }

    /* ============================================================
     * 启动环境监听
     * ============================================================ */
    ESP_LOGI(TAG, "Starting environment watcher...");
    ret = env_watcher_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Environment watcher start failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Environment watcher started");
    }

    /* ============================================================
     * 初始化完成
     * ============================================================ */
    record_startup_time();
    status_led_set_state(STATUS_LED_MODE_AUTO);
    
    ESP_LOGI(TAG, "=== System initialized, entering main loop ===");

    /* ============================================================
     * 主循环: 系统健康监控
     * ============================================================ */
    uint32_t loop_count = 0;
    while (1) {
        /* 喂看门狗 */
        esp_task_wdt_reset();
        
        vTaskDelay(pdMS_TO_TICKS(5000));
        loop_count++;
        
        /* 每 30 秒打印一次系统状态 */
        if (loop_count % 6 == 0) {
            ESP_LOGI(TAG, "System running for %.1f seconds", get_uptime_seconds());
            ESP_LOGI(TAG, "  LED: brightness=%d%%, color_temp=%dK",
                     led_get_brightness(), led_get_color_temp());
            ESP_LOGI(TAG, "  Wi-Fi: state=%d", wifi_sta_get_state());
            ESP_LOGI(TAG, "  Mode: %d", priority_arbiter_get_mode());
            
            /* 打印内存使用情况 */
            ESP_LOGI(TAG, "  Heap: free=%lu, min_free=%lu",
                     (unsigned long)esp_get_free_heap_size(),
                     (unsigned long)esp_get_minimum_free_heap_size());
        }
    }
}
