/**
 * @file i2s_driver.c
 * @brief I2S 驱动实现
 */

#include "i2s_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

/* ESP-IDF v6.1 I2S 头文件 */
#include "driver/i2s_std.h"

static const char *TAG = "I2S_DRV";

/* I2S 配置 */
#define I2S_NUM             I2S_NUM_0
#define I2S_WS_GPIO         12
#define I2S_SD_GPIO         13
#define I2S_CLK_GPIO        14

/* 默认配置 */
#define DEFAULT_SAMPLE_RATE 16000
#define DEFAULT_BITS        16
#define DMA_BUF_COUNT       8
#define DMA_BUF_SIZE        512

static i2s_chan_handle_t g_rx_handle = NULL;
static uint32_t g_sample_rate = DEFAULT_SAMPLE_RATE;

esp_err_t i2s_init(uint32_t sample_rate, uint8_t bits_per_sample)
{
    if (g_rx_handle != NULL) {
        ESP_LOGW(TAG, "I2S already initialized");
        return ESP_OK;
    }

    g_sample_rate = sample_rate;

    // 创建 I2S 通道
    i2s_chan_config_t chan_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_conf.dma_desc_num = DMA_BUF_COUNT;
    chan_conf.dma_frame_num = DMA_BUF_SIZE;
    
    esp_err_t ret = i2s_new_channel(&chan_conf, NULL, &g_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2S 通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置标准模式
    i2s_std_config_t std_conf = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = (bits_per_sample == 16) ? I2S_DATA_BIT_WIDTH_16BIT : 
                              (bits_per_sample == 24) ? I2S_DATA_BIT_WIDTH_24BIT : 
                              I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_CLK_GPIO,
            .ws = I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(g_rx_handle, &std_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S 标准模式失败: %s", esp_err_to_name(ret));
        i2s_del_channel(g_rx_handle);
        g_rx_handle = NULL;
        return ret;
    }
    
    ret = i2s_channel_enable(g_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用 I2S 通道失败: %s", esp_err_to_name(ret));
        i2s_del_channel(g_rx_handle);
        g_rx_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S initialized: rate=%luHz, bits=%d, ws=%d, sd=%d, clk=%d", 
             sample_rate, bits_per_sample, I2S_WS_GPIO, I2S_SD_GPIO, I2S_CLK_GPIO);
    return ESP_OK;
}

esp_err_t i2s_read(int16_t *data, size_t samples, size_t *bytes_read)
{
    if (g_rx_handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_to_read = samples * sizeof(int16_t);
    return i2s_channel_read(g_rx_handle, (char *)data, bytes_to_read, 
                            bytes_read, portMAX_DELAY);
}

esp_err_t i2s_read_timeout(int16_t *data, size_t samples, size_t *bytes_read, 
                           uint32_t timeout_ms)
{
    if (g_rx_handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_to_read = samples * sizeof(int16_t);
    return i2s_channel_read(g_rx_handle, (char *)data, bytes_to_read, 
                            bytes_read, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t i2s_deinit(void)
{
    if (g_rx_handle == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = i2s_channel_disable(g_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "禁用 I2S 通道失败: %s", esp_err_to_name(ret));
    }
    
    ret = i2s_del_channel(g_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "删除 I2S 通道失败: %s", esp_err_to_name(ret));
    }
    g_rx_handle = NULL;

    ESP_LOGI(TAG, "I2S deinitialized");
    return ESP_OK;
}

uint32_t i2s_get_sample_rate(void)
{
    return g_sample_rate;
}

esp_err_t i2s_clear_dma_buffer(void)
{
    if (g_rx_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // 读取并丢弃缓冲区数据
    int16_t dummy[DMA_BUF_SIZE];
    size_t bytes_read;
    for (int i = 0; i < DMA_BUF_COUNT; i++) {
        i2s_channel_read(g_rx_handle, (char *)dummy, sizeof(dummy), 
                         &bytes_read, pdMS_TO_TICKS(100));
    }

    return ESP_OK;
}