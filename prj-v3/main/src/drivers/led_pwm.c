/**
 * @file led_pwm.c
 * @brief LED PWM 驱动实现
 */

#include "led_pwm.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>

static const char *TAG = "LED_PWM";

/* PWM 配置 */
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY          (5000)  // 5kHz

/* 通道定义 */
#define LEDC_CHANNEL_BRIGHTNESS LEDC_CHANNEL_0
#define LEDC_CHANNEL_COLOR_TEMP LEDC_CHANNEL_1
#define LEDC_CHANNEL_R          LEDC_CHANNEL_2
#define LEDC_CHANNEL_G          LEDC_CHANNEL_3
#define LEDC_CHANNEL_B          LEDC_CHANNEL_4

/* GPIO 定义 */
#define LED_GPIO_BRIGHTNESS     4
#define LED_GPIO_COLOR_TEMP     5
#define LED_GPIO_R              6
#define LED_GPIO_G              7
#define LED_GPIO_B              15

/* 状态变量 */
static uint8_t g_brightness = 100;
static uint16_t g_color_temp = 4000;
static bool g_initialized = false;
static SemaphoreHandle_t s_led_mutex = NULL;

esp_err_t led_pwm_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "LED PWM 已初始化");
        return ESP_OK;
    }
    
    /* 创建互斥锁 */
    s_led_mutex = xSemaphoreCreateMutex();
    if (!s_led_mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "定时器配置失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_led_mutex);
        return ret;
    }

    // 配置亮度通道
    ledc_channel_config_t brightness_conf = {
        .gpio_num = LED_GPIO_BRIGHTNESS,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_BRIGHTNESS,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 8191,  // 100%
        .hpoint = 0
    };
    ret = ledc_channel_config(&brightness_conf);
    if (ret != ESP_OK) goto cleanup;

    // 配置色温通道
    ledc_channel_config_t color_temp_conf = {
        .gpio_num = LED_GPIO_COLOR_TEMP,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_COLOR_TEMP,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 4095,  // 50%
        .hpoint = 0
    };
    ret = ledc_channel_config(&color_temp_conf);
    if (ret != ESP_OK) goto cleanup;

    // 配置 RGB 通道
    ledc_channel_config_t rgb_confs[3] = {
        {.gpio_num = LED_GPIO_R, .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL_R, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0},
        {.gpio_num = LED_GPIO_G, .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL_G, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0},
        {.gpio_num = LED_GPIO_B, .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL_B, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0}
    };
    
    for (int i = 0; i < 3; i++) {
        ret = ledc_channel_config(&rgb_confs[i]);
        if (ret != ESP_OK) goto cleanup;
    }

    // 启用渐变功能
    ret = ledc_fade_func_install(0);
    if (ret != ESP_OK) goto cleanup;

    g_initialized = true;
    ESP_LOGI(TAG, "LED PWM initialized: freq=%dHz, channels=5", LEDC_FREQUENCY);
    return ESP_OK;

cleanup:
    vSemaphoreDelete(s_led_mutex);
    s_led_mutex = NULL;
    ESP_LOGE(TAG, "LED PWM 初始化失败: %s", esp_err_to_name(ret));
    return ret;
}

esp_err_t led_set_brightness(uint8_t percent)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (percent > 100) percent = 100;
    
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    
    g_brightness = percent;
    uint32_t duty = (8191 * percent) / 100;
    
    /* 使用线程安全API (ESP-IDF v5.1+) */
    esp_err_t ret = ledc_set_duty_and_update(LEDC_MODE, LEDC_CHANNEL_BRIGHTNESS, duty, 0);
    
    xSemaphoreGive(s_led_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置亮度失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Brightness set to %d%% (duty=%lu)", percent, duty);
    return ESP_OK;
}

esp_err_t led_set_color_temp(uint16_t kelvin)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (kelvin < 2700) kelvin = 2700;
    if (kelvin > 6500) kelvin = 6500;
    
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    
    g_color_temp = kelvin;
    
    // 计算冷暖光比例 (2700K -> 6500K)
    uint32_t warm_ratio = (6500 - kelvin) * 100 / (6500 - 2700);
    uint32_t duty = (8191 * warm_ratio) / 100;
    
    /* 使用线程安全API (ESP-IDF v5.1+) */
    esp_err_t ret = ledc_set_duty_and_update(LEDC_MODE, LEDC_CHANNEL_COLOR_TEMP, duty, 0);
    
    xSemaphoreGive(s_led_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置色温失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Color temp set to %dK (warm_ratio=%lu%%)", kelvin, warm_ratio);
    return ESP_OK;
}

esp_err_t led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    
    uint32_t duty_r = (8191 * r) / 255;
    uint32_t duty_g = (8191 * g) / 255;
    uint32_t duty_b = (8191 * b) / 255;
    
    /* 使用线程安全API (ESP-IDF v5.1+) */
    esp_err_t ret = ledc_set_duty_and_update(LEDC_MODE, LEDC_CHANNEL_R, duty_r, 0);
    if (ret == ESP_OK) {
        ret = ledc_set_duty_and_update(LEDC_MODE, LEDC_CHANNEL_G, duty_g, 0);
    }
    if (ret == ESP_OK) {
        ret = ledc_set_duty_and_update(LEDC_MODE, LEDC_CHANNEL_B, duty_b, 0);
    }
    
    xSemaphoreGive(s_led_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置RGB失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "RGB set to (%d, %d, %d)", r, g, b);
    return ESP_OK;
}

esp_err_t led_fade_to_brightness(uint8_t target_brightness, uint32_t duration_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (target_brightness > 100) target_brightness = 100;
    
    uint32_t target_duty = (8191 * target_brightness) / 100;
    
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    
    /* 使用线程安全API (ESP-IDF v5.1+) */
    esp_err_t ret = ledc_set_fade_time_and_start(LEDC_MODE, LEDC_CHANNEL_BRIGHTNESS, 
                                                   target_duty, duration_ms, 
                                                   LEDC_FADE_WAIT_DONE);
    
    if (ret == ESP_OK) {
        g_brightness = target_brightness;
    }
    
    xSemaphoreGive(s_led_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "渐变亮度失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Faded to brightness %d%% in %lums", target_brightness, duration_ms);
    return ESP_OK;
}

esp_err_t led_fade_to_color_temp(uint16_t target_kelvin, uint32_t duration_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (target_kelvin < 2700) target_kelvin = 2700;
    if (target_kelvin > 6500) target_kelvin = 6500;
    
    uint32_t warm_ratio = (6500 - target_kelvin) * 100 / (6500 - 2700);
    uint32_t target_duty = (8191 * warm_ratio) / 100;
    
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    
    /* 使用线程安全API (ESP-IDF v5.1+) */
    esp_err_t ret = ledc_set_fade_time_and_start(LEDC_MODE, LEDC_CHANNEL_COLOR_TEMP, 
                                                   target_duty, duration_ms, 
                                                   LEDC_FADE_WAIT_DONE);
    
    if (ret == ESP_OK) {
        g_color_temp = target_kelvin;
    }
    
    xSemaphoreGive(s_led_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "渐变色温失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Faded to color temp %dK in %lums", target_kelvin, duration_ms);
    return ESP_OK;
}

uint8_t led_get_brightness(void)
{
    uint8_t value;
    if (s_led_mutex) {
        xSemaphoreTake(s_led_mutex, portMAX_DELAY);
        value = g_brightness;
        xSemaphoreGive(s_led_mutex);
    } else {
        value = g_brightness;
    }
    return value;
}

uint16_t led_get_color_temp(void)
{
    uint16_t value;
    if (s_led_mutex) {
        xSemaphoreTake(s_led_mutex, portMAX_DELAY);
        value = g_color_temp;
        xSemaphoreGive(s_led_mutex);
    } else {
        value = g_color_temp;
    }
    return value;
}

esp_err_t led_pwm_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }
    
    g_initialized = false;
    
    SemaphoreHandle_t mutex_to_delete = s_led_mutex;
    s_led_mutex = NULL;
    
    if (mutex_to_delete) {
        xSemaphoreTake(mutex_to_delete, portMAX_DELAY);
        ledc_fade_func_uninstall();
        /* ESP-IDF 5.4+ ledc_stop 需要 idle_level 参数 */
        ledc_stop(LEDC_MODE, LEDC_CHANNEL_BRIGHTNESS, 0);
        ledc_stop(LEDC_MODE, LEDC_CHANNEL_COLOR_TEMP, 0);
        ledc_stop(LEDC_MODE, LEDC_CHANNEL_R, 0);
        ledc_stop(LEDC_MODE, LEDC_CHANNEL_G, 0);
        ledc_stop(LEDC_MODE, LEDC_CHANNEL_B, 0);
        xSemaphoreGive(mutex_to_delete);
        vSemaphoreDelete(mutex_to_delete);
    }
    
    g_brightness = 100;
    g_color_temp = 4000;
    
    ESP_LOGI(TAG, "LED PWM 已释放");
    return ESP_OK;
}
