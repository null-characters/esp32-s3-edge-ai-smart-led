/**
 * @file vad_detector.c
 * @brief VAD 检测模块实现
 * 
 * 使用 ESP-SR 的 VAD (Voice Activity Detection) 进行语音活动检测
 */

#include "vad_detector.h"
#include "esp_log.h"
#include "esp_vad.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "VAD";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    
    /* ESP-SR VAD 实例 */
    vad_handle_t vad_handle;
    
    /* 当前状态 */
    vad_state_t current_state;
    int hangover_count;
    
    /* 配置 */
    vad_config_t config;
    
    /* 回调 */
    vad_state_callback_t callback;
    void *user_data;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} vad_detector_state_t;

static vad_detector_state_t g_vad = {0};

/* ================================================================
 * 默认配置
 * ================================================================ */

static const vad_config_t vad_default_config = {
    .vad_mode = VAD_MODE_3,            /* 较激进的 VAD 模式 */
    .min_speech_ms = 100,              /* 最小语音时长 100ms */
    .min_noise_ms = 300,               /* 最小静音时长 300ms */
    .hangover_frames = 5,              /* 拖尾帧数 */
    .energy_threshold = 500.0f,        /* 能量阈值 (备用) */
    .voice_prob_threshold = 0.5f,      /* 语音概率阈值 */
};

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 触发状态变化回调
 */
static void trigger_callback(vad_state_t state, const vad_result_t *result)
{
    if (g_vad.callback) {
        g_vad.callback(state, result, g_vad.user_data);
    }
}

/**
 * @brief 计算帧能量 (备用方法)
 */
static float calculate_energy(const int16_t *samples, int count)
{
    int64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += (int64_t)samples[i] * samples[i];
    }
    return (float)sum / count;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t vad_detector_init(const vad_config_t *config)
{
    if (g_vad.initialized) {
        ESP_LOGW(TAG, "VAD 已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化 VAD 检测模块 (使用 ESP-SR VAD)");
    
    g_vad.mutex = xSemaphoreCreateMutex();
    if (!g_vad.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 应用配置 */
    if (config) {
        memcpy(&g_vad.config, config, sizeof(vad_config_t));
    } else {
        memcpy(&g_vad.config, &vad_default_config, sizeof(vad_config_t));
    }
    
    /* 创建 ESP-SR VAD 实例 */
    /* 参数说明:
     * - vad_mode: VAD 模式 (0-4), 越大越激进
     * - sample_rate: 采样率 (16000)
     * - one_frame_ms: 每帧时长 (10/20/30ms)
     * - min_speech_ms: 最小语音时长
     * - min_noise_ms: 最小静音时长
     */
    g_vad.vad_handle = vad_create_with_param(
        g_vad.config.vad_mode,
        SAMPLE_RATE_HZ,
        VAD_FRAME_LENGTH_MS,
        g_vad.config.min_speech_ms,
        g_vad.config.min_noise_ms
    );
    
    if (!g_vad.vad_handle) {
        ESP_LOGE(TAG, "创建 VAD 实例失败");
        vSemaphoreDelete(g_vad.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    g_vad.current_state = VAD_SILENCE;
    g_vad.hangover_count = 0;
    g_vad.callback = config ? config->callback : NULL;
    g_vad.user_data = config ? config->user_data : NULL;
    
    g_vad.initialized = true;
    
    ESP_LOGI(TAG, "VAD 初始化完成: mode=%d, min_speech=%dms, min_noise=%dms",
             g_vad.config.vad_mode, g_vad.config.min_speech_ms, g_vad.config.min_noise_ms);
    
    return ESP_OK;
}

void vad_detector_deinit(void)
{
    if (!g_vad.initialized) {
        return;
    }
    
    if (g_vad.vad_handle) {
        vad_destroy(g_vad.vad_handle);
        g_vad.vad_handle = NULL;
    }
    
    if (g_vad.mutex) {
        vSemaphoreDelete(g_vad.mutex);
    }
    
    memset(&g_vad, 0, sizeof(vad_detector_state_t));
    ESP_LOGI(TAG, "VAD 已释放");
}

esp_err_t vad_detector_process(const int16_t *samples, vad_result_t *result)
{
    if (!g_vad.initialized || !samples || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    
    /* 使用 ESP-SR VAD 处理 */
    /* VAD_FRAME_SAMPLES = SAMPLE_RATE_HZ * VAD_FRAME_LENGTH_MS / 1000 = 480 */
    /* ESP-SR VAD 需要 int16_t*，去除 const 以匹配 API */
    int16_t *samples_ptr = (int16_t *)samples;
    vad_state_t esp_vad_state = vad_process_with_trigger(g_vad.vad_handle, samples_ptr);
    
    /* 计算能量 (用于调试和备用判断) */
    float energy = calculate_energy(samples, VAD_FRAME_SAMPLES);
    
    /* 状态转换逻辑 */
    vad_state_t new_state = esp_vad_state;
    
    /* 拖尾处理: 语音结束后保持几帧语音状态，避免过早切换 */
    if (new_state == VAD_SILENCE && g_vad.current_state == VAD_SPEECH) {
        if (g_vad.hangover_count < g_vad.config.hangover_frames) {
            g_vad.hangover_count++;
            new_state = VAD_SPEECH;
        } else {
            g_vad.hangover_count = 0;
        }
    } else if (new_state == VAD_SPEECH) {
        g_vad.hangover_count = 0;
    }
    
    /* 计算语音概率 (基于 ESP-SR VAD 结果) */
    float voice_prob = (esp_vad_state == VAD_SPEECH) ? 0.8f : 0.1f;
    
    /* 填充结果 */
    result->state = new_state;
    result->energy = energy;
    result->probability = voice_prob;
    result->transition = (new_state != g_vad.current_state);
    
    /* 状态变化回调 */
    if (result->transition) {
        ESP_LOGD(TAG, "VAD 状态变化: %s → %s (energy=%.1f)",
                 g_vad.current_state == VAD_SPEECH ? "人声" : "静音",
                 new_state == VAD_SPEECH ? "人声" : "静音",
                 energy);
        trigger_callback(new_state, result);
    }
    
    g_vad.current_state = new_state;
    
    xSemaphoreGive(g_vad.mutex);
    
    return ESP_OK;
}

vad_state_t vad_detector_get_state(void)
{
    if (!g_vad.initialized) {
        return VAD_SILENCE;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    vad_state_t state = g_vad.current_state;
    xSemaphoreGive(g_vad.mutex);
    
    return state;
}

void vad_detector_reset(void)
{
    if (!g_vad.initialized) {
        return;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    
    /* 重置 ESP-SR VAD 触发状态 */
    if (g_vad.vad_handle) {
        vad_reset_trigger(g_vad.vad_handle);
    }
    
    g_vad.current_state = VAD_SILENCE;
    g_vad.hangover_count = 0;
    
    xSemaphoreGive(g_vad.mutex);
    
    ESP_LOGD(TAG, "VAD 状态已重置");
}

void vad_detector_set_mode(vad_mode_t mode)
{
    if (!g_vad.initialized) {
        return;
    }
    
    if (mode > VAD_MODE_4) {
        mode = VAD_MODE_4;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    
    /* 需要重新创建 VAD 实例以更改模式 */
    if (g_vad.vad_handle) {
        vad_destroy(g_vad.vad_handle);
    }
    
    g_vad.config.vad_mode = mode;
    
    g_vad.vad_handle = vad_create_with_param(
        mode,
        SAMPLE_RATE_HZ,
        VAD_FRAME_LENGTH_MS,
        g_vad.config.min_speech_ms,
        g_vad.config.min_noise_ms
    );
    
    xSemaphoreGive(g_vad.mutex);
    
    ESP_LOGI(TAG, "VAD 模式设置为 %d (%s)", mode,
             mode == VAD_MODE_0 ? "Normal" :
             mode == VAD_MODE_1 ? "Aggressive" :
             mode == VAD_MODE_2 ? "Very Aggressive" :
             mode == VAD_MODE_3 ? "Very Very Aggressive" : "Very Very Very Aggressive");
}

void vad_detector_set_energy_threshold(float threshold)
{
    if (!g_vad.initialized) {
        return;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    g_vad.config.energy_threshold = threshold;
    xSemaphoreGive(g_vad.mutex);
    
    ESP_LOGI(TAG, "能量阈值设置为 %.1f", threshold);
}

void vad_detector_set_hangover(int frames)
{
    if (!g_vad.initialized) {
        return;
    }
    
    if (frames < 0) frames = 0;
    if (frames > 20) frames = 20;
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    g_vad.config.hangover_frames = frames;
    xSemaphoreGive(g_vad.mutex);
    
    ESP_LOGI(TAG, "拖尾帧数设置为 %d", frames);
}

void vad_detector_set_callback(vad_state_callback_t callback, void *user_data)
{
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    g_vad.callback = callback;
    g_vad.user_data = user_data;
    xSemaphoreGive(g_vad.mutex);
}

int vad_detector_get_frame_size(void)
{
    /* ESP-SR VAD 使用 30ms 帧 @ 16kHz = 480 样本 */
    return VAD_FRAME_SAMPLES;
}

uint32_t vad_detector_get_speech_duration_ms(void)
{
    if (!g_vad.initialized || !g_vad.vad_handle) {
        return 0;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    
    /* ESP-SR VAD 内部追踪语音时长 */
    uint32_t duration_ms = 0;
    if (g_vad.vad_handle && g_vad.vad_handle->trigger) {
        duration_ms = g_vad.vad_handle->trigger->speech_len * VAD_FRAME_LENGTH_MS;
    }
    
    xSemaphoreGive(g_vad.mutex);
    
    return duration_ms;
}

void vad_detector_print_stats(void)
{
    if (!g_vad.initialized) {
        return;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    
    ESP_LOGI(TAG, "VAD 状态: %s", g_vad.current_state == VAD_SPEECH ? "人声" : "静音");
    ESP_LOGI(TAG, "VAD 模式: %d, 拖尾帧: %d", g_vad.config.vad_mode, g_vad.config.hangover_frames);
    
    if (g_vad.vad_handle && g_vad.vad_handle->trigger) {
        ESP_LOGI(TAG, "语音帧数: %d, 静音帧数: %d",
                 g_vad.vad_handle->trigger->speech_len,
                 g_vad.vad_handle->trigger->noise_len);
    }
    
    xSemaphoreGive(g_vad.mutex);
}