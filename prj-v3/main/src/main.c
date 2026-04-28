/**
 * @file main.c
 * @brief prj-v3 主入口 - 语音交互智能照明网关
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_pwm.h"
#include "uart_driver.h"
#include "i2s_driver.h"
#include "wifi_sta.h"
#include "status_led.h"

static const char *TAG = "MAIN";

/* Wi-Fi 配置 (从环境变量或配置文件读取) */
#define WIFI_SSID      "YourSSID"
#define WIFI_PASSWORD  "YourPassword"

/* 测试回调函数 */
static void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "Wi-Fi connected callback triggered");
}

static void on_wifi_disconnected(void)
{
    ESP_LOGW(TAG, "Wi-Fi disconnected callback triggered");
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== prj-v3: Voice Interaction Smart LED Gateway ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());

    /* 0. 初始化状态指示灯 (最先初始化，显示启动状态) */
    ESP_LOGI(TAG, "Initializing status LED...");
    esp_err_t ret = status_led_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Status LED init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Status LED initialized (booting...)");
        status_led_set_state(STATUS_LED_BOOTING);
        status_led_start();
    }

    /* 1. 初始化 LED PWM 驱动 */
    ESP_LOGI(TAG, "Initializing LED PWM driver...");
    ret = led_pwm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED PWM init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LED PWM initialized successfully");
        // 测试 LED 渐变
        led_fade_to_brightness(50, 1000);
        led_set_color_temp(4000);
    }

    /* 2. 初始化 UART 驱动 */
    ESP_LOGI(TAG, "Initializing UART driver...");
    ret = my_uart_init(115200);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "UART initialized successfully");
    }

    /* 3. 初始化 I2S 麦克风驱动 */
    ESP_LOGI(TAG, "Initializing I2S driver...");
    ret = i2s_init(16000, 16);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "I2S initialized successfully (16kHz, 16-bit)");
    }

    /* 4. 初始化 Wi-Fi STA */
    ESP_LOGI(TAG, "Initializing Wi-Fi STA...");
    status_led_set_state(STATUS_LED_WIFI_CONNECTING);
    my_wifi_sta_register_cb(on_wifi_connected, on_wifi_disconnected);
    ret = my_wifi_sta_init(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Wi-Fi initialized, connecting...");
        ret = my_wifi_sta_connect(10000);
        if (ret == ESP_OK) {
            char ip[16];
            my_wifi_sta_get_ip(ip, sizeof(ip));
            ESP_LOGI(TAG, "Wi-Fi connected, IP: %s", ip);
        } else {
            ESP_LOGW(TAG, "Wi-Fi connection failed or timeout");
        }
    }

    /* 5. 初始化完成，切换到自动模式 */
    ESP_LOGI(TAG, "=== System initialized, entering main loop ===");
    status_led_set_state(STATUS_LED_MODE_AUTO);
    
    uint32_t loop_count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        loop_count++;
        ESP_LOGI(TAG, "Main loop #%lu - System running", loop_count);
        
        // 打印系统状态
        ESP_LOGI(TAG, "  LED: brightness=%d%%, color_temp=%dK", 
                 led_get_brightness(), led_get_color_temp());
        ESP_LOGI(TAG, "  Wi-Fi: state=%d", my_wifi_sta_get_state());
        
        // 每 30 秒切换一次亮度 (测试)
        if (loop_count % 6 == 0) {
            uint8_t new_brightness = (led_get_brightness() > 50) ? 30 : 70;
            ESP_LOGI(TAG, "  Testing LED fade to %d%%", new_brightness);
            led_fade_to_brightness(new_brightness, 2000);
        }
    }
}