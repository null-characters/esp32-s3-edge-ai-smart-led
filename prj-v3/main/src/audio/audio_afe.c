/**
 * @file audio_afe.c
 * @brief ESP-SR 音频前端 (AFE) 实现文件
 * 
 * 框架模式：提供 AFE 配置接口，实际 ESP-SR 集成由应用层处理
 */

#include "audio_afe.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "AUDIO_AFE";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    audio_afe_config_t config;
    audio_afe_callback_t callback;
    void *user_data;
} audio_afe_state_t;

static audio_afe_state_t g_state = {0};

/* ================================================================
 * 默认配置
 * ================================================================ */

const audio_afe_config_t audio_afe_default_config = {
    .sample_rate = 16000,
    .mic_channels = 1,
    .ref_channels = 0,
    .enable_aec = false,
    .enable_ns = true,
    .enable_agc = true,
    .enable_vad = true,
    .enable_bss = false,
    .agc_gain = 15.0f,
    .ns_level = 2.0f,
    .vad_threshold = 0,
    .enable_wakenet = true,
    .enable_multinet = true,
    .wakenet_model = "wn_xiaobaitong",
    .multinet_model = "mn2_cn",
};

/* ================================================================
 * 公共 API
 * ================================================================ */

int audio_afe_init(const audio_afe_config_t *config)
{
    if (!config) {
        config = &audio_afe_default_config;
    }
    
    ESP_LOGI(TAG, "初始化 AFE 模块 (框架模式)");
    
    /* 保存配置 */
    memcpy(&g_state.config, config, sizeof(audio_afe_config_t));
    
    g_state.initialized = true;
    
    ESP_LOGI(TAG, "AFE 配置: 采样率=%d, 麦克风=%d, WakeNet=%s",
             config->sample_rate, config->mic_channels,
             config->wakenet_model ? config->wakenet_model : "无");
    
    return 0;
}

void audio_afe_deinit(void)
{
    if (!g_state.initialized) {
        return;
    }
    
    g_state.initialized = false;
    ESP_LOGI(TAG, "AFE 已释放");
}

int audio_afe_get_feed_chunk_size(void)
{
    /* ESP-SR AFE 默认块大小: 16kHz * 10ms = 160 samples */
    return 160;
}

int audio_afe_get_fetch_chunk_size(void)
{
    /* ESP-SR AFE 默认输出块大小 */
    return 160;
}

int audio_afe_get_sample_rate(void)
{
    if (!g_state.initialized) {
        return 0;
    }
    return g_state.config.sample_rate;
}

int audio_afe_feed(const int16_t *samples)
{
    if (!g_state.initialized || !samples) {
        return 0;
    }
    /* 框架模式: 实际喂入由 ESP-SR AFE 处理 */
    return audio_afe_get_feed_chunk_size();
}

bool audio_afe_fetch(audio_afe_result_t *result)
{
    if (!g_state.initialized || !result) {
        return false;
    }
    
    /* 框架模式: 结果由 ESP-SR AFE 回调填充 */
    return false;
}

void audio_afe_set_callback(audio_afe_callback_t callback, void *user_data)
{
    g_state.callback = callback;
    g_state.user_data = user_data;
}

void audio_afe_wakenet_enable(bool enable)
{
    if (!g_state.initialized) {
        return;
    }
    ESP_LOGI(TAG, "WakeNet %s (框架模式)", enable ? "已启用" : "已禁用");
}

void audio_afe_reset(void)
{
    if (!g_state.initialized) {
        return;
    }
    ESP_LOGD(TAG, "AFE 状态已重置 (框架模式)");
}

/* ================================================================
 * 辅助 API
 * ================================================================ */

const audio_afe_config_t* audio_afe_get_config(void)
{
    if (!g_state.initialized) {
        return NULL;
    }
    return &g_state.config;
}
