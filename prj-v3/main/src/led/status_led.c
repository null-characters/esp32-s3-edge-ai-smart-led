/**
 * @file status_led.c
 * @brief 状态指示灯管理模块实现
 */

#include "status_led.h"
#include "ws2812_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "STATUS_LED";

/* 默认配置 */
#define DEFAULT_BRIGHTNESS       50
#define DEFAULT_BLINK_INTERVAL   500   /* ms */
#define DEFAULT_BREATH_PERIOD    2000  /* ms */
#define RESULT_DISPLAY_DURATION  1000  /* ms */

/* 默认 GPIO (可通过 Kconfig 配置) */
#ifndef CONFIG_STATUS_LED_GPIO
#define STATUS_LED_DEFAULT_GPIO  48
#else
#define STATUS_LED_DEFAULT_GPIO  CONFIG_STATUS_LED_GPIO
#endif

/* 状态颜色映射表 */
typedef struct {
    ws2812_color_t color;       ///< 基础颜色
    bool blink;                 ///< 是否闪烁
    bool breath;                ///< 是否呼吸
    uint16_t period_ms;        ///< 周期 (ms)
} status_color_map_t;

/* 模块状态 */
typedef struct {
    bool initialized;
    bool running;
    status_led_state_enum_t current_state;
    status_led_state_enum_t previous_state;
    status_led_config_t config;
    
    /* 动画状态 */
    bool led_on;
    int64_t last_toggle_ms;
    int64_t breath_start_ms;
    uint32_t result_end_time;
    
    /* 后台任务 */
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
} status_led_state_t;

static status_led_state_t g_status = {0};

/* ================================================================
 * 状态颜色映射表 (紧凑数组)
 * ================================================================ */

/**
 * @brief 状态映射条目
 */
typedef struct {
    status_led_state_enum_t state;  ///< 状态枚举值
    status_color_map_t map;         ///< 颜色映射
} status_map_entry_t;

/* 紧凑映射表，避免稀疏数组的未初始化元素 */
static const status_map_entry_t status_map_table[] = {
    /* WiFi 状态 */
    {STATUS_LED_WIFI_DISCONNECTED,  {WS2812_COLOR_RED,     true,  false, 1000}},  /* 红色慢闪 */
    {STATUS_LED_WIFI_CONNECTING,    {WS2812_COLOR_YELLOW,  true,  false, 250}},   /* 黄色快闪 */
    {STATUS_LED_WIFI_CONNECTED,     {WS2812_COLOR_GREEN,   false, false, 0}},     /* 绿色常亮 */
    {STATUS_LED_WIFI_FAILED,        {WS2812_COLOR_RED,     false, false, 0}},     /* 红色常亮 */
    
    /* 系统模式 */
    {STATUS_LED_MODE_AUTO,          {WS2812_COLOR_BLUE,    false, false, 0}},     /* 蓝色常亮 */
    {STATUS_LED_MODE_MANUAL,        {WS2812_COLOR_CYAN,    false, false, 0}},     /* 青色常亮 */
    {STATUS_LED_MODE_VOICE,         {WS2812_COLOR_PURPLE,  false, false, 0}},     /* 紫色常亮 */
    {STATUS_LED_MODE_SLEEP,         {WS2812_COLOR_BLACK,   false, false, 0}},     /* 熄灭 */
    
    /* 语音交互 */
    {STATUS_LED_VOICE_IDLE,         {WS2812_COLOR_BLACK,   false, false, 0}},     /* 随模式 */
    {STATUS_LED_VOICE_LISTENING,    {WS2812_COLOR_WHITE,   false, true,  2000}},  /* 白色呼吸 */
    {STATUS_LED_VOICE_PROCESSING,   {WS2812_COLOR_WHITE,   true,  false, 200}},   /* 白色快闪 */
    {STATUS_LED_VOICE_SUCCESS,      {WS2812_COLOR_GREEN,   true,  false, 100}},   /* 绿色快闪 */
    {STATUS_LED_VOICE_ERROR,        {WS2812_COLOR_RED,     true,  false, 100}},   /* 红色快闪 */
    
    /* 特殊状态 */
    {STATUS_LED_BOOTING,            {WS2812_COLOR_WHITE,   false, true,  3000}},  /* 彩虹渐变 */
    {STATUS_LED_ERROR,              {WS2812_COLOR_RED,     true,  false, 200}},   /* 红色快闪 */
    {STATUS_LED_UPDATING,           {WS2812_COLOR_YELLOW,  false, true,  2000}},  /* 黄色呼吸 */
};

#define STATUS_MAP_TABLE_SIZE  (sizeof(status_map_table) / sizeof(status_map_table[0]))

/**
 * @brief 查找状态映射
 * @param state 状态枚举值
 * @return 映射条目指针，未找到返回 NULL
 */
static const status_color_map_t* find_status_map(status_led_state_enum_t state)
{
    for (size_t i = 0; i < STATUS_MAP_TABLE_SIZE; i++) {
        if (status_map_table[i].state == state) {
            return &status_map_table[i].map;
        }
    }
    return NULL;
}

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 获取当前时间戳 (毫秒)
 */
static int64_t get_timestamp_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/**
 * @brief 计算呼吸亮度
 */
static uint8_t calculate_breath_brightness(int64_t elapsed_ms, uint16_t period_ms)
{
    /* 正弦波呼吸效果 */
    float phase = (float)(elapsed_ms % period_ms) / period_ms;
    float brightness = (sinf(phase * 2 * 3.14159f) + 1.0f) / 2.0f;
    return (uint8_t)(brightness * 255);
}

/**
 * @brief 计算彩虹颜色
 */
static ws2812_color_t calculate_rainbow(int64_t elapsed_ms, uint16_t period_ms)
{
    float phase = (float)(elapsed_ms % period_ms) / period_ms;
    float h = phase * 360.0f;
    
    /* HSV to RGB (S=1, V=1) */
    float c = 1.0f;
    float x = 1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f);
    
    ws2812_color_t color = {0};
    
    if (h < 60) {
        color.r = 255; color.g = (uint8_t)(x * 255);
    } else if (h < 120) {
        color.r = (uint8_t)(x * 255); color.g = 255;
    } else if (h < 180) {
        color.g = 255; color.b = (uint8_t)(x * 255);
    } else if (h < 240) {
        color.g = (uint8_t)(x * 255); color.b = 255;
    } else if (h < 300) {
        color.r = (uint8_t)(x * 255); color.b = 255;
    } else {
        color.r = 255; color.b = (uint8_t)(x * 255);
    }
    
    return color;
}

/**
 * @brief 应用状态颜色
 */
static void apply_state_color(status_led_state_enum_t state)
{
    const status_color_map_t *map = find_status_map(state);
    if (!map) {
        return;
    }
    
    int64_t now = get_timestamp_ms();
    
    ws2812_color_t color = map->color;
    
    if (map->blink) {
        /* 闪烁效果 */
        int64_t elapsed = now - g_status.last_toggle_ms;
        if (elapsed >= map->period_ms / 2) {
            g_status.led_on = !g_status.led_on;
            g_status.last_toggle_ms = now;
        }
        if (!g_status.led_on) {
            color = WS2812_COLOR_BLACK;
        }
    } else if (map->breath) {
        /* 呼吸效果 */
        if (state == STATUS_LED_BOOTING) {
            /* 启动时彩虹渐变 */
            color = calculate_rainbow(now - g_status.breath_start_ms, map->period_ms);
        } else {
            uint8_t brightness = calculate_breath_brightness(now - g_status.breath_start_ms, 
                                                              map->period_ms);
            color.r = (color.r * brightness) / 255;
            color.g = (color.g * brightness) / 255;
            color.b = (color.b * brightness) / 255;
        }
    }
    
    ws2812_set_all(color);
    ws2812_show();
}

/**
 * @brief 状态更新任务
 */
static void status_led_task(void *arg)
{
    ESP_LOGI(TAG, "状态指示灯任务启动");
    
    while (g_status.running) {
        xSemaphoreTake(g_status.mutex, portMAX_DELAY);
        
        status_led_state_enum_t state = g_status.current_state;
        
        /* 检查临时结果状态是否超时 */
        if ((state == STATUS_LED_VOICE_SUCCESS || state == STATUS_LED_VOICE_ERROR) &&
            get_timestamp_ms() >= g_status.result_end_time) {
            /* 恢复之前的状态 */
            state = g_status.previous_state;
            g_status.current_state = state;
        }
        
        /* 应用状态颜色 */
        apply_state_color(state);
        
        xSemaphoreGive(g_status.mutex);
        
        /* 更新间隔 */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    ESP_LOGI(TAG, "状态指示灯任务停止");
    vTaskDelete(NULL);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t status_led_init(const status_led_config_t *config)
{
    if (g_status.initialized) {
        ESP_LOGW(TAG, "状态指示灯已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化状态指示灯模块");
    
    /* 创建互斥锁 */
    g_status.mutex = xSemaphoreCreateMutex();
    if (!g_status.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 保存配置 */
    if (config) {
        memcpy(&g_status.config, config, sizeof(status_led_config_t));
    } else {
        g_status.config.brightness = DEFAULT_BRIGHTNESS;
        g_status.config.blink_interval_ms = DEFAULT_BLINK_INTERVAL;
        g_status.config.breath_period_ms = DEFAULT_BREATH_PERIOD;
        g_status.config.enable_auto_update = true;
    }
    
    /* 初始化 WS2812 驱动 */
    ws2812_config_t ws_config = {
        .gpio_num = STATUS_LED_DEFAULT_GPIO,
        .led_count = 1,
        .brightness = g_status.config.brightness,
    };
    
    esp_err_t ret = ws2812_init(&ws_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 WS2812 失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_status.mutex);
        return ret;
    }
    
    /* 初始化状态 */
    g_status.current_state = STATUS_LED_BOOTING;
    g_status.previous_state = STATUS_LED_BOOTING;
    g_status.breath_start_ms = get_timestamp_ms();
    g_status.last_toggle_ms = get_timestamp_ms();
    g_status.led_on = true;
    
    g_status.initialized = true;
    
    ESP_LOGI(TAG, "状态指示灯初始化完成");
    return ESP_OK;
}

void status_led_deinit(void)
{
    if (!g_status.initialized) {
        return;
    }
    
    /* 停止后台任务 */
    status_led_stop();
    
    xSemaphoreTake(g_status.mutex, portMAX_DELAY);
    
    ws2812_deinit();
    
    g_status.initialized = false;
    
    xSemaphoreGive(g_status.mutex);
    vSemaphoreDelete(g_status.mutex);
    
    ESP_LOGI(TAG, "状态指示灯已释放");
}

esp_err_t status_led_set_state(status_led_state_enum_t state)
{
    if (!g_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_status.mutex, portMAX_DELAY);
    
    if (state != g_status.current_state) {
        g_status.previous_state = g_status.current_state;
        g_status.current_state = state;
        g_status.breath_start_ms = get_timestamp_ms();
        g_status.last_toggle_ms = get_timestamp_ms();
        g_status.led_on = true;
        
        /* 关键状态变化使用 INFO 级别日志 */
        ESP_LOGI(TAG, "状态切换: %d -> %d", g_status.previous_state, state);
    }
    
    xSemaphoreGive(g_status.mutex);
    
    return ESP_OK;
}

status_led_state_enum_t status_led_get_state(void)
{
    return g_status.current_state;
}

void status_led_update_wifi(wifi_sta_state_t wifi_state)
{
    status_led_state_enum_t state;
    
    switch (wifi_state) {
        case WIFI_STA_DISCONNECTED:
            state = STATUS_LED_WIFI_DISCONNECTED;
            break;
        case WIFI_STA_CONNECTING:
            state = STATUS_LED_WIFI_CONNECTING;
            break;
        case WIFI_STA_CONNECTED:
            state = STATUS_LED_WIFI_CONNECTED;
            break;
        case WIFI_STA_FAILED:
            state = STATUS_LED_WIFI_FAILED;
            break;
        default:
            state = STATUS_LED_WIFI_DISCONNECTED;
            break;
    }
    
    status_led_set_state(state);
}

void status_led_update_mode(system_mode_t mode)
{
    status_led_state_enum_t state;
    
    switch (mode) {
        case MODE_AUTO:
            state = STATUS_LED_MODE_AUTO;
            break;
        case MODE_MANUAL:
            state = STATUS_LED_MODE_MANUAL;
            break;
        case MODE_VOICE:
            state = STATUS_LED_MODE_VOICE;
            break;
        case MODE_SLEEP:
            state = STATUS_LED_MODE_SLEEP;
            break;
        default:
            state = STATUS_LED_MODE_AUTO;
            break;
    }
    
    status_led_set_state(state);
}

void status_led_set_voice(bool listening, bool processing)
{
    status_led_state_enum_t state;
    
    if (processing) {
        state = STATUS_LED_VOICE_PROCESSING;
    } else if (listening) {
        state = STATUS_LED_VOICE_LISTENING;
    } else {
        state = STATUS_LED_VOICE_IDLE;
    }
    
    status_led_set_state(state);
}

void status_led_show_result(bool success, uint32_t duration_ms)
{
    if (!g_status.initialized) {
        return;
    }
    
    xSemaphoreTake(g_status.mutex, portMAX_DELAY);
    
    g_status.previous_state = g_status.current_state;
    g_status.current_state = success ? STATUS_LED_VOICE_SUCCESS : STATUS_LED_VOICE_ERROR;
    g_status.result_end_time = (uint32_t)get_timestamp_ms() + duration_ms;
    g_status.last_toggle_ms = get_timestamp_ms();
    g_status.led_on = true;
    
    xSemaphoreGive(g_status.mutex);
}

void status_led_set_brightness(uint8_t brightness)
{
    ws2812_set_brightness(brightness);
    g_status.config.brightness = brightness;
}

void status_led_start(void)
{
    if (!g_status.initialized || g_status.running) {
        return;
    }
    
    g_status.running = true;
    
    BaseType_t ret = xTaskCreate(
        status_led_task,
        "status_led",
        2048,
        NULL,
        5,  /* 低优先级 */
        &g_status.task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建状态指示灯任务失败");
        g_status.running = false;
    }
}

void status_led_stop(void)
{
    if (!g_status.running) {
        return;
    }
    
    g_status.running = false;
    
    if (g_status.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(50));
        g_status.task_handle = NULL;
    }
}

void status_led_refresh(void)
{
    if (!g_status.initialized) {
        return;
    }
    
    xSemaphoreTake(g_status.mutex, portMAX_DELAY);
    apply_state_color(g_status.current_state);
    xSemaphoreGive(g_status.mutex);
}
