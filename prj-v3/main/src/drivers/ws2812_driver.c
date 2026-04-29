/**
 * @file ws2812_driver.c
 * @brief WS2812 RGB LED 驱动实现
 * 
 * 使用 ESP-IDF RMT 外设驱动 WS2812 单线协议
 */

#include "ws2812_driver.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "WS2812";

/* WS2812 时序参数 (单位: RMT ticks, 1 tick = 12.5ns @ 80MHz) */
#define WS2812_T0H_TICKS    14      /* 0 码高电平: 0.4us */
#define WS2812_T0L_TICKS    52      /* 0 码低电平: 0.85us */
#define WS2812_T1H_TICKS    52      /* 1 码高电平: 0.85us */
#define WS2812_T1L_TICKS    14      /* 1 码低电平: 0.4us */
#define WS2812_RESET_TICKS  50000   /* 复位时间: >50us */

/* 默认配置 */
#define WS2812_DEFAULT_GPIO     48
#define WS2812_DEFAULT_COUNT    1
#define WS2812_DEFAULT_BRIGHT   50

/* 模块状态 */
typedef struct {
    bool initialized;
    rmt_channel_handle_t tx_channel;
    rmt_encoder_handle_t bytes_encoder;
    uint8_t *pixel_buf;
    uint8_t led_count;
    uint8_t brightness;
    SemaphoreHandle_t mutex;
} ws2812_state_t;

static ws2812_state_t g_ws2812 = {0};

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t ws2812_init(const ws2812_config_t *config)
{
    if (g_ws2812.initialized) {
        ESP_LOGW(TAG, "WS2812 已初始化");
        return ESP_OK;
    }
    
    int gpio = config ? config->gpio_num : WS2812_DEFAULT_GPIO;
    uint8_t count = config ? config->led_count : WS2812_DEFAULT_COUNT;
    uint8_t brightness = config ? config->brightness : WS2812_DEFAULT_BRIGHT;
    
    ESP_LOGI(TAG, "初始化 WS2812: GPIO=%d, count=%d, brightness=%d", gpio, count, brightness);
    
    /* 创建互斥锁 */
    g_ws2812.mutex = xSemaphoreCreateMutex();
    if (!g_ws2812.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 分配像素缓冲区 (GRB 格式, 每个 LED 3 字节) */
    g_ws2812.pixel_buf = heap_caps_malloc(count * 3, MALLOC_CAP_8BIT);
    if (!g_ws2812.pixel_buf) {
        ESP_LOGE(TAG, "分配像素缓冲区失败");
        vSemaphoreDelete(g_ws2812.mutex);
        return ESP_ERR_NO_MEM;
    }
    memset(g_ws2812.pixel_buf, 0, count * 3);
    
    /* 配置 RMT TX 通道 */
    rmt_tx_channel_config_t tx_conf = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = 64,
        .resolution_hz = 80 * 1000 * 1000,  /* 80MHz */
        .trans_queue_depth = 4,
        .intr_priority = 0,
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_conf, &g_ws2812.tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 RMT TX 通道失败: %s", esp_err_to_name(ret));
        free(g_ws2812.pixel_buf);
        vSemaphoreDelete(g_ws2812.mutex);
        return ret;
    }
    
    /* 创建字节编码器 */
    rmt_bytes_encoder_config_t encoder_conf = {
        .bit0 = {
            .level0 = 1,
            .duration0 = WS2812_T0H_TICKS,
            .level1 = 0,
            .duration1 = WS2812_T0L_TICKS,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = WS2812_T1H_TICKS,
            .level1 = 0,
            .duration1 = WS2812_T1L_TICKS,
        },
        .flags.msb_first = true,
    };
    
    ret = rmt_new_bytes_encoder(&encoder_conf, &g_ws2812.bytes_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建字节编码器失败: %s", esp_err_to_name(ret));
        rmt_del_channel(g_ws2812.tx_channel);
        free(g_ws2812.pixel_buf);
        vSemaphoreDelete(g_ws2812.mutex);
        return ret;
    }
    
    /* 启用 RMT 通道 */
    ret = rmt_enable(g_ws2812.tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用 RMT 通道失败: %s", esp_err_to_name(ret));
        rmt_del_encoder(g_ws2812.bytes_encoder);
        rmt_del_channel(g_ws2812.tx_channel);
        free(g_ws2812.pixel_buf);
        vSemaphoreDelete(g_ws2812.mutex);
        return ret;
    }
    
    g_ws2812.led_count = count;
    g_ws2812.brightness = brightness;
    g_ws2812.initialized = true;
    
    /* 初始状态: 熄灭 */
    ws2812_clear();
    ws2812_show();
    
    ESP_LOGI(TAG, "WS2812 初始化完成");
    return ESP_OK;
}

void ws2812_deinit(void)
{
    if (!g_ws2812.initialized) {
        return;
    }
    
    /* 先标记为未初始化 */
    g_ws2812.initialized = false;
    
    SemaphoreHandle_t mutex_to_delete = g_ws2812.mutex;
    
    xSemaphoreTake(mutex_to_delete, portMAX_DELAY);
    
    ws2812_clear();
    ws2812_show();
    
    rmt_disable(g_ws2812.tx_channel);
    rmt_del_encoder(g_ws2812.bytes_encoder);
    rmt_del_channel(g_ws2812.tx_channel);
    free(g_ws2812.pixel_buf);
    
    /* 清零整个状态结构 */
    memset(&g_ws2812, 0, sizeof(ws2812_state_t));
    
    xSemaphoreGive(mutex_to_delete);
    vSemaphoreDelete(mutex_to_delete);
    
    ESP_LOGI(TAG, "WS2812 已释放");
}

esp_err_t ws2812_set_pixel(uint8_t index, ws2812_color_t color)
{
    if (!g_ws2812.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (index >= g_ws2812.led_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_ws2812.mutex, portMAX_DELAY);
    
    /* WS2812 使用 GRB 顺序 */
    uint8_t *pixel = &g_ws2812.pixel_buf[index * 3];
    pixel[0] = (color.g * g_ws2812.brightness) / 255;  /* G */
    pixel[1] = (color.r * g_ws2812.brightness) / 255;  /* R */
    pixel[2] = (color.b * g_ws2812.brightness) / 255;  /* B */
    
    xSemaphoreGive(g_ws2812.mutex);
    
    return ESP_OK;
}

esp_err_t ws2812_set_all(ws2812_color_t color)
{
    if (!g_ws2812.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ws2812.mutex, portMAX_DELAY);
    
    uint8_t g = (color.g * g_ws2812.brightness) / 255;
    uint8_t r = (color.r * g_ws2812.brightness) / 255;
    uint8_t b = (color.b * g_ws2812.brightness) / 255;
    
    for (int i = 0; i < g_ws2812.led_count; i++) {
        uint8_t *pixel = &g_ws2812.pixel_buf[i * 3];
        pixel[0] = g;
        pixel[1] = r;
        pixel[2] = b;
    }
    
    xSemaphoreGive(g_ws2812.mutex);
    
    return ESP_OK;
}

esp_err_t ws2812_clear(void)
{
    return ws2812_set_all(WS2812_COLOR_BLACK);
}

esp_err_t ws2812_show(void)
{
    if (!g_ws2812.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ws2812.mutex, portMAX_DELAY);
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    esp_err_t ret = rmt_transmit(g_ws2812.tx_channel, 
                                  g_ws2812.bytes_encoder,
                                  g_ws2812.pixel_buf,
                                  g_ws2812.led_count * 3,
                                  &tx_config);
    
    if (ret == ESP_OK) {
        /* 等待传输完成，根据 LED 数量动态计算超时 */
        /* 每个 LED 约 30us，加上安全裕量 */
        uint32_t timeout_ms = (g_ws2812.led_count * 30 / 1000) + 10;
        if (timeout_ms < 10) timeout_ms = 10;
        if (timeout_ms > 1000) timeout_ms = 1000;
        rmt_tx_wait_all_done(g_ws2812.tx_channel, timeout_ms);
    }
    
    xSemaphoreGive(g_ws2812.mutex);
    
    return ret;
}

void ws2812_set_brightness(uint8_t brightness)
{
    if (!g_ws2812.initialized) {
        return;
    }
    
    xSemaphoreTake(g_ws2812.mutex, portMAX_DELAY);
    g_ws2812.brightness = brightness;
    xSemaphoreGive(g_ws2812.mutex);
}

uint8_t ws2812_get_brightness(void)
{
    return g_ws2812.brightness;
}
