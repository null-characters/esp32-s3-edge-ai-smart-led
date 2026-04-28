/**
 * @file dac_driver.c
 * @brief I2S DAC 驱动实现 (MAX98357A)
 */

#include "dac_driver.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "DAC_DRIVER";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    i2s_port_t i2s_port;
    i2s_chan_handle_t tx_handle;
    uint8_t volume;
    bool playing;
    dac_playback_callback_t callback;
    void *user_data;
    SemaphoreHandle_t mutex;
} dac_state_t;

static dac_state_t g_dac = {0};

/* ================================================================
 * 默认配置
 * ================================================================ */

static const dac_config_t dac_default_config = {
    .i2s_port = I2S_NUM_1,           /* 使用独立 I2S1 */
    .bclk_gpio = DAC_BCLK_GPIO,
    .ws_gpio = DAC_WS_GPIO,
    .data_out_gpio = DAC_DATA_OUT_GPIO,
    .sample_rate = DAC_SAMPLE_RATE,
    .bits_per_sample = DAC_BITS_PER_SAMPLE,
};

/* ================================================================
 * 内部函数
 * ================================================================ */

static void notify_playback_state(bool playing)
{
    if (g_dac.callback) {
        g_dac.callback(playing, g_dac.user_data);
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t dac_init(const dac_config_t *config)
{
    if (g_dac.initialized) {
        ESP_LOGW(TAG, "DAC 已初始化");
        return ESP_OK;
    }
    
    if (!config) {
        config = &dac_default_config;
    }
    
    ESP_LOGI(TAG, "初始化 DAC 驱动: I2S%d, rate=%lu", config->i2s_port, config->sample_rate);
    
    /* 创建互斥锁 */
    g_dac.mutex = xSemaphoreCreateMutex();
    if (!g_dac.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* I2S 通道配置 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(config->i2s_port, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &g_dac.tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2S 通道失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_dac.mutex);
        return ret;
    }
    
    /* 标准模式配置 */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(config->bits_per_sample, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->bclk_gpio,
            .ws = config->ws_gpio,
            .dout = config->data_out_gpio,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_std_mode(g_dac.tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S 标准模式失败: %s", esp_err_to_name(ret));
        i2s_del_channel(g_dac.tx_handle);
        vSemaphoreDelete(g_dac.mutex);
        return ret;
    }
    
    /* 启动通道 */
    ret = i2s_channel_enable(g_dac.tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动 I2S 通道失败: %s", esp_err_to_name(ret));
        i2s_del_channel(g_dac.tx_handle);
        vSemaphoreDelete(g_dac.mutex);
        return ret;
    }
    
    g_dac.i2s_port = config->i2s_port;
    g_dac.volume = 80;  /* 默认音量 80% */
    g_dac.playing = false;
    g_dac.initialized = true;
    
    ESP_LOGI(TAG, "DAC 初始化完成");
    return ESP_OK;
}

void dac_deinit(void)
{
    if (!g_dac.initialized) {
        return;
    }
    
    xSemaphoreTake(g_dac.mutex, portMAX_DELAY);
    
    if (g_dac.tx_handle) {
        i2s_channel_disable(g_dac.tx_handle);
        i2s_del_channel(g_dac.tx_handle);
    }
    
    if (g_dac.mutex) {
        vSemaphoreDelete(g_dac.mutex);
    }
    
    memset(&g_dac, 0, sizeof(dac_state_t));
    ESP_LOGI(TAG, "DAC 已释放");
}

esp_err_t dac_write(const int16_t *data, size_t len, size_t *bytes_written)
{
    if (!g_dac.initialized || !data || !bytes_written) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_dac.mutex, portMAX_DELAY);
    
    /* 应用音量增益 */
    int16_t *adjusted_data = (int16_t *)malloc(len);
    if (!adjusted_data) {
        xSemaphoreGive(g_dac.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    float gain = g_dac.volume / 100.0f;
    size_t samples = len / sizeof(int16_t);
    for (size_t i = 0; i < samples; i++) {
        adjusted_data[i] = (int16_t)(data[i] * gain);
    }
    
    esp_err_t ret = i2s_channel_write(g_dac.tx_handle, adjusted_data, len, bytes_written, 1000);
    
    free(adjusted_data);
    
    if (!g_dac.playing) {
        g_dac.playing = true;
        notify_playback_state(true);
    }
    
    xSemaphoreGive(g_dac.mutex);
    
    return ret;
}

esp_err_t dac_play_blocking(const int16_t *data, size_t len)
{
    size_t bytes_written;
    esp_err_t ret = dac_write(data, len, &bytes_written);
    
    if (ret == ESP_OK) {
        /* 等待播放完成 */
        vTaskDelay(pdMS_TO_TICKS(len * 1000 / (DAC_SAMPLE_RATE * sizeof(int16_t)) + 50));
        
        xSemaphoreTake(g_dac.mutex, portMAX_DELAY);
        g_dac.playing = false;
        notify_playback_state(false);
        xSemaphoreGive(g_dac.mutex);
    }
    
    return ret;
}

esp_err_t dac_set_volume(uint8_t volume)
{
    if (!g_dac.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (volume > 100) {
        volume = 100;
    }
    
    xSemaphoreTake(g_dac.mutex, portMAX_DELAY);
    g_dac.volume = volume;
    xSemaphoreGive(g_dac.mutex);
    
    ESP_LOGI(TAG, "音量设置为 %d%%", volume);
    return ESP_OK;
}

uint8_t dac_get_volume(void)
{
    return g_dac.volume;
}

bool dac_is_playing(void)
{
    return g_dac.playing;
}

esp_err_t dac_stop(void)
{
    if (!g_dac.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_dac.mutex, portMAX_DELAY);
    
    /* 发送静音数据清空缓冲 */
    int16_t silence[256] = {0};
    size_t bytes_written;
    i2s_channel_write(g_dac.tx_handle, silence, sizeof(silence), &bytes_written, 100);
    
    g_dac.playing = false;
    notify_playback_state(false);
    
    xSemaphoreGive(g_dac.mutex);
    
    ESP_LOGI(TAG, "DAC 播放停止");
    return ESP_OK;
}

void dac_set_playback_callback(dac_playback_callback_t callback, void *user_data)
{
    g_dac.callback = callback;
    g_dac.user_data = user_data;
}

i2s_port_t dac_get_i2s_port(void)
{
    return g_dac.i2s_port;
}