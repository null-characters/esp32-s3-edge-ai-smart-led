/**
 * @file tts_engine.c
 * @brief TTS 引擎实现
 * 
 * 使用 ESP-SR 的 esp_tts 中文语音合成库
 */

#include "tts_engine.h"
#include "dac_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_tts.h"
#include "esp_tts_voice_xiaole.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "TTS_ENGINE";

/* ================================================================
 * 配置常量
 * ================================================================ */

#define TTS_DEFAULT_SPEED      2    /* 语音速度: 0-5, 2 为中等 */
#define TTS_TASK_STACK_SIZE    4096
#define TTS_TASK_PRIORITY      4
#define TTS_PLAY_QUEUE_SIZE    4

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    bool playing;
    
    /* ESP-SR TTS 实例 */
    esp_tts_handle_t tts_handle;
    esp_tts_voice_t *voice_set;
    
    /* 配置 */
    uint32_t sample_rate;
    unsigned int speed;
    
    /* 播放队列 */
    QueueHandle_t play_queue;
    TaskHandle_t play_task;
    volatile bool running;
    
    /* 回调 */
    tts_complete_callback_t callback;
    void *user_data;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} tts_state_t;

/* 播放请求结构 */
typedef struct {
    char text[128];
    bool blocking;
} tts_play_request_t;

static tts_state_t g_tts = {0};

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief TTS 播放任务
 */
static void tts_play_task(void *arg)
{
    tts_play_request_t request;
    
    ESP_LOGI(TAG, "TTS 播放任务启动");
    
    while (g_tts.running) {
        if (xQueueReceive(g_tts.play_queue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
            g_tts.playing = true;
            xSemaphoreGive(g_tts.mutex);
            
            ESP_LOGI(TAG, "播放: \"%s\"", request.text);
            
            /* 解析中文文本 */
            int parse_ret = esp_tts_parse_chinese(g_tts.tts_handle, request.text);
            if (parse_ret != 1) {
                ESP_LOGW(TAG, "文本解析失败");
                xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
                g_tts.playing = false;
                xSemaphoreGive(g_tts.mutex);
                continue;
            }
            
            /* 流式播放语音 */
            int len = 0;
            short *data = NULL;
            
            while ((data = esp_tts_stream_play(g_tts.tts_handle, &len, g_tts.speed)) != NULL) {
                if (len == 0) {
                    break;  /* 播放完成 */
                }
                
                /* 通过 DAC 输出 */
                esp_err_t ret = dac_play_blocking(data, len * sizeof(short));
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "DAC 播放失败: %s", esp_err_to_name(ret));
                    break;
                }
                
                /* 检查是否被停止 */
                xSemaphoreTake(g_tts.mutex, pdMS_TO_TICKS(10));
                bool still_playing = g_tts.playing;
                xSemaphoreGive(g_tts.mutex);
                
                if (!still_playing) {
                    esp_tts_stream_reset(g_tts.tts_handle);
                    break;
                }
            }
            
            /* 重置 TTS 流 */
            esp_tts_stream_reset(g_tts.tts_handle);
            
            xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
            g_tts.playing = false;
            tts_complete_callback_t cb = g_tts.callback;
            void *user_data = g_tts.user_data;
            xSemaphoreGive(g_tts.mutex);
            
            /* 播放完成回调 */
            if (cb) {
                cb(user_data);
            }
            
            ESP_LOGI(TAG, "播放完成");
        }
    }
    
    ESP_LOGI(TAG, "TTS 播放任务退出");
    vTaskDelete(NULL);
}

/**
 * @brief 同步播放 TTS (阻塞调用)
 */
static esp_err_t tts_play_blocking_internal(const char *text)
{
    if (!g_tts.initialized || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "同步播放: \"%s\"", text);
    
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    g_tts.playing = true;
    xSemaphoreGive(g_tts.mutex);
    
    /* 解析文本 */
    int parse_ret = esp_tts_parse_chinese(g_tts.tts_handle, text);
    if (parse_ret != 1) {
        ESP_LOGW(TAG, "文本解析失败");
        xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
        g_tts.playing = false;
        xSemaphoreGive(g_tts.mutex);
        return ESP_FAIL;
    }
    
    /* 流式播放 */
    int len = 0;
    short *data = NULL;
    
    while ((data = esp_tts_stream_play(g_tts.tts_handle, &len, g_tts.speed)) != NULL) {
        if (len == 0) {
            break;
        }
        
        esp_err_t ret = dac_play_blocking(data, len * sizeof(short));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "DAC 播放失败");
            break;
        }
        
        /* 检查是否被停止 */
        xSemaphoreTake(g_tts.mutex, pdMS_TO_TICKS(10));
        bool still_playing = g_tts.playing;
        xSemaphoreGive(g_tts.mutex);
        
        if (!still_playing) {
            esp_tts_stream_reset(g_tts.tts_handle);
            break;
        }
    }
    
    esp_tts_stream_reset(g_tts.tts_handle);
    
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    g_tts.playing = false;
    tts_complete_callback_t cb = g_tts.callback;
    void *user_data = g_tts.user_data;
    xSemaphoreGive(g_tts.mutex);
    
    if (cb) {
        cb(user_data);
    }
    
    return ESP_OK;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t tts_engine_init(const tts_config_t *config)
{
    if (g_tts.initialized) {
        ESP_LOGW(TAG, "TTS 引擎已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化 TTS 引擎 (使用 ESP-SR 中文 TTS)");
    
    g_tts.mutex = xSemaphoreCreateMutex();
    if (!g_tts.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 初始化语音集 */
    /* 使用 xiaole 语音 (轻量级，适合嵌入式) */
    g_tts.voice_set = esp_tts_voice_set_init(&esp_tts_voice_xiaole, NULL);
    if (!g_tts.voice_set) {
        ESP_LOGE(TAG, "初始化语音集失败");
        vSemaphoreDelete(g_tts.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    /* 创建 TTS 实例 */
    g_tts.tts_handle = esp_tts_create(g_tts.voice_set);
    if (!g_tts.tts_handle) {
        ESP_LOGE(TAG, "创建 TTS 实例失败");
        esp_tts_voice_set_free(g_tts.voice_set);
        vSemaphoreDelete(g_tts.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    /* 应用配置 */
    if (config) {
        g_tts.sample_rate = config->sample_rate;
        g_tts.callback = config->callback;
        g_tts.user_data = config->user_data;
    } else {
        g_tts.sample_rate = 16000;
    }
    
    g_tts.speed = TTS_DEFAULT_SPEED;
    g_tts.playing = false;
    g_tts.running = false;
    
    /* 创建播放队列 */
    g_tts.play_queue = xQueueCreate(TTS_PLAY_QUEUE_SIZE, sizeof(tts_play_request_t));
    if (!g_tts.play_queue) {
        ESP_LOGE(TAG, "创建播放队列失败");
        esp_tts_destroy(g_tts.tts_handle);
        esp_tts_voice_set_free(g_tts.voice_set);
        vSemaphoreDelete(g_tts.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    g_tts.initialized = true;
    
    ESP_LOGI(TAG, "TTS 引擎初始化完成 (语音: xiaole, 速度: %d)", g_tts.speed);
    
    return ESP_OK;
}

void tts_engine_deinit(void)
{
    if (!g_tts.initialized) {
        return;
    }
    
    /* 停止播放任务 */
    tts_stop();
    
    if (g_tts.running) {
        g_tts.running = false;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    if (g_tts.play_queue) {
        vQueueDelete(g_tts.play_queue);
    }
    
    if (g_tts.tts_handle) {
        esp_tts_destroy(g_tts.tts_handle);
    }
    
    if (g_tts.voice_set) {
        esp_tts_voice_set_free(g_tts.voice_set);
    }
    
    if (g_tts.mutex) {
        vSemaphoreDelete(g_tts.mutex);
    }
    
    memset(&g_tts, 0, sizeof(tts_state_t));
    ESP_LOGI(TAG, "TTS 引擎已释放");
}

esp_err_t tts_engine_start(void)
{
    if (!g_tts.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_tts.running) {
        return ESP_OK;
    }
    
    g_tts.running = true;
    
    BaseType_t ret = xTaskCreate(
        tts_play_task,
        "tts_play",
        TTS_TASK_STACK_SIZE,
        NULL,
        TTS_TASK_PRIORITY,
        &g_tts.play_task
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建播放任务失败");
        g_tts.running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "TTS 播放任务已启动");
    return ESP_OK;
}

esp_err_t tts_speak(const char *text)
{
    if (!g_tts.initialized || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 如果播放任务未启动，使用同步播放 */
    if (!g_tts.running) {
        return tts_play_blocking_internal(text);
    }
    
    /* 异步播放 */
    tts_play_request_t request;
    size_t text_len = strlen(text);
    if (text_len >= sizeof(request.text)) {
        ESP_LOGW(TAG, "文本过长 (%zu 字符)，将被截断为 %zu 字符", 
                 text_len, sizeof(request.text) - 1);
    }
    strncpy(request.text, text, sizeof(request.text) - 1);
    request.text[sizeof(request.text) - 1] = '\0';
    request.blocking = false;
    
    if (xQueueSend(g_tts.play_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "播放队列满");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t tts_speak_blocking(const char *text)
{
    return tts_play_blocking_internal(text);
}

esp_err_t tts_speak_scene_status(const char *name, uint8_t brightness, uint16_t color_temp)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char text[64];
    snprintf(text, sizeof(text), "当前%s，亮度%d%%", 
             name, brightness);
    
    return tts_speak(text);
}

esp_err_t tts_speak_brightness(uint8_t brightness)
{
    char text[32];
    snprintf(text, sizeof(text), "亮度%d%%", brightness);
    return tts_speak(text);
}

esp_err_t tts_speak_mode(bool is_auto)
{
    return tts_speak(is_auto ? "自动模式" : "手动模式");
}

esp_err_t tts_speak_power(bool power_on)
{
    return tts_speak(power_on ? "已开灯" : "已关灯");
}

esp_err_t tts_speak_color_temp(uint16_t color_temp)
{
    char text[32];
    if (color_temp <= 3000) {
        snprintf(text, sizeof(text), "暖光%dK", color_temp);
    } else if (color_temp >= 5500) {
        snprintf(text, sizeof(text), "冷光%dK", color_temp);
    } else {
        snprintf(text, sizeof(text), "色温%dK", color_temp);
    }
    return tts_speak(text);
}

esp_err_t tts_speak_greeting(void)
{
    return tts_speak("你好，我是智能台灯");
}

esp_err_t tts_speak_confirm(void)
{
    return tts_speak("好的");
}

esp_err_t tts_speak_error(void)
{
    return tts_speak("抱歉，操作失败");
}

esp_err_t tts_stop(void)
{
    if (!g_tts.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    g_tts.playing = false;
    xSemaphoreGive(g_tts.mutex);
    
    /* 清空播放队列 */
    xQueueReset(g_tts.play_queue);
    
    /* 停止 DAC */
    return dac_stop();
}

bool tts_is_playing(void)
{
    if (!g_tts.initialized) {
        return false;
    }
    
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    bool playing = g_tts.playing;
    xSemaphoreGive(g_tts.mutex);
    
    return playing;
}

void tts_set_speed(unsigned int speed)
{
    if (!g_tts.initialized) {
        return;
    }
    
    if (speed > 5) {
        speed = 5;
    }
    
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    g_tts.speed = speed;
    xSemaphoreGive(g_tts.mutex);
    
    ESP_LOGI(TAG, "语音速度设置为 %d", speed);
}

unsigned int tts_get_speed(void)
{
    if (!g_tts.initialized) {
        return TTS_DEFAULT_SPEED;
    }
    
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    unsigned int speed = g_tts.speed;
    xSemaphoreGive(g_tts.mutex);
    
    return speed;
}

void tts_set_callback(tts_complete_callback_t callback, void *user_data)
{
    xSemaphoreTake(g_tts.mutex, portMAX_DELAY);
    g_tts.callback = callback;
    g_tts.user_data = user_data;
    xSemaphoreGive(g_tts.mutex);
}