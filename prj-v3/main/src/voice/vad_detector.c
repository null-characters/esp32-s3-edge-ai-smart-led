/**
 * @file vad_detector.c
 * @brief VAD 检测模块实现
 * 
 * 使用能量检测和零交叉率进行简单 VAD
 */

#include "vad_detector.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "VAD";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    
    /* 当前状态 */
    vad_state_t current_state;
    int hangover_count;
    
    /* 配置 */
    float energy_threshold;
    float voice_prob_threshold;
    int hangover_frames;
    
    /* 回调 */
    vad_state_callback_t callback;
    void *user_data;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} vad_state_t;

static vad_state_t g_vad = {0};

/* ================================================================
 * 默认配置
 * ================================================================ */

static const vad_config_t vad_default_config = {
    .energy_threshold = 500.0f,
    .voice_prob_threshold = 0.5f,
    .hangover_frames = 5,
};

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 计算帧能量
 */
static float calculate_energy(const int16_t *samples, int count)
{
    int64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += (int64_t)samples[i] * samples[i];
    }
    return (float)sum / count;
}

/**
 * @brief 计算零交叉率
 */
static float calculate_zcr(const int16_t *samples, int count)
{
    int crossings = 0;
    for (int i = 1; i < count; i++) {
        if ((samples[i] >= 0 && samples[i-1] < 0) ||
            (samples[i] < 0 && samples[i-1] >= 0)) {
            crossings++;
        }
    }
    return (float)crossings / count;
}

/**
 * @brief 触发状态变化回调
 */
static void trigger_callback(vad_state_t state, const vad_result_t *result)
{
    if (g_vad.callback) {
        g_vad.callback(state, result, g_vad.user_data);
    }
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
    
    ESP_LOGI(TAG, "初始化 VAD 检测模块");
    
    g_vad.mutex = xSemaphoreCreateMutex();
    if (!g_vad.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    if (config) {
        g_vad.energy_threshold = config->energy_threshold;
        g_vad.voice_prob_threshold = config->voice_prob_threshold;
        g_vad.hangover_frames = config->hangover_frames;
        g_vad.callback = config->callback;
        g_vad.user_data = config->user_data;
    } else {
        g_vad.energy_threshold = vad_default_config.energy_threshold;
        g_vad.voice_prob_threshold = vad_default_config.voice_prob_threshold;
        g_vad.hangover_frames = vad_default_config.hangover_frames;
    }
    
    g_vad.current_state = VAD_STATE_SILENCE;
    g_vad.hangover_count = 0;
    
    g_vad.initialized = true;
    
    ESP_LOGI(TAG, "VAD 初始化完成: energy_thresh=%.1f, hangover=%d",
             g_vad.energy_threshold, g_vad.hangover_frames);
    
    return ESP_OK;
}

void vad_detector_deinit(void)
{
    if (!g_vad.initialized) {
        return;
    }
    
    if (g_vad.mutex) {
        vSemaphoreDelete(g_vad.mutex);
    }
    
    memset(&g_vad, 0, sizeof(vad_state_t));
    ESP_LOGI(TAG, "VAD 已释放");
}

esp_err_t vad_detector_process(const int16_t *samples, vad_result_t *result)
{
    if (!g_vad.initialized || !samples || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    
    /* 计算特征 */
    float energy = calculate_energy(samples, VAD_FRAME_SAMPLES);
    float zcr = calculate_zcr(samples, VAD_FRAME_SAMPLES);
    
    /* 简单 VAD 判断 */
    /* 人声通常有中等能量和较低的零交叉率 */
    float voice_prob = 0.0f;
    
    if (energy > g_vad.energy_threshold) {
        voice_prob = 0.5f;
        
        /* 零交叉率在 0.1-0.3 之间更可能是人声 */
        if (zcr >= 0.05f && zcr <= 0.4f) {
            voice_prob += 0.3f;
        }
        
        /* 能量越高概率越高 */
        if (energy > g_vad.energy_threshold * 2) {
            voice_prob += 0.2f;
        }
    }
    
    /* 限制概率范围 */
    if (voice_prob > 1.0f) voice_prob = 1.0f;
    
    /* 状态判断 */
    vad_state_t new_state;
    if (voice_prob >= g_vad.voice_prob_threshold) {
        new_state = VAD_STATE_VOICE;
        g_vad.hangover_count = g_vad.hangover_frames;
    } else {
        /* 拖尾处理 */
        if (g_vad.hangover_count > 0) {
            g_vad.hangover_count--;
            new_state = VAD_STATE_VOICE;
        } else {
            new_state = VAD_STATE_SILENCE;
        }
    }
    
    /* 填充结果 */
    result->state = new_state;
    result->energy = energy;
    result->probability = voice_prob;
    result->transition = (new_state != g_vad.current_state);
    
    /* 状态变化回调 */
    if (result->transition) {
        ESP_LOGD(TAG, "VAD 状态变化: %s → %s",
                 g_vad.current_state == VAD_STATE_VOICE ? "人声" : "静音",
                 new_state == VAD_STATE_VOICE ? "人声" : "静音");
        trigger_callback(new_state, result);
    }
    
    g_vad.current_state = new_state;
    
    xSemaphoreGive(g_vad.mutex);
    
    return ESP_OK;
}

vad_state_t vad_detector_get_state(void)
{
    return g_vad.current_state;
}

void vad_detector_reset(void)
{
    if (!g_vad.initialized) {
        return;
    }
    
    xSemaphoreTake(g_vad.mutex, portMAX_DELAY);
    g_vad.current_state = VAD_STATE_SILENCE;
    g_vad.hangover_count = 0;
    xSemaphoreGive(g_vad.mutex);
}

void vad_detector_set_energy_threshold(float threshold)
{
    g_vad.energy_threshold = threshold;
    ESP_LOGI(TAG, "能量阈值设置为 %.1f", threshold);
}

void vad_detector_set_callback(vad_state_callback_t callback, void *user_data)
{
    g_vad.callback = callback;
    g_vad.user_data = user_data;
}

int vad_detector_get_frame_size(void)
{
    return VAD_FRAME_SAMPLES;
}