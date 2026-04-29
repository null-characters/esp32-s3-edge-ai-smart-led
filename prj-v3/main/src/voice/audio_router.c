/**
 * @file audio_router.c
 * @brief 音频路由器实现
 */

#include "audio_router.h"
#include "vad_detector.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "AUDIO_ROUTER";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    
    /* 当前路由 */
    audio_route_target_t current_target;
    vad_state_t vad_state;
    
    /* 回调 */
    audio_data_callback_t esp_sr_callback;
    audio_data_callback_t tflm_callback;
    void *esp_sr_user_data;
    void *tflm_user_data;
    
    /* 统计 */
    uint32_t esp_sr_count;
    uint32_t tflm_count;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} router_state_t;

static router_state_t g_router = {0};

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t audio_router_init(const audio_router_config_t *config)
{
    if (g_router.initialized) {
        ESP_LOGW(TAG, "音频路由器已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化音频路由器");
    
    g_router.mutex = xSemaphoreCreateMutex();
    if (!g_router.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    if (config) {
        g_router.esp_sr_callback = config->esp_sr_callback;
        g_router.tflm_callback = config->tflm_callback;
        g_router.esp_sr_user_data = config->esp_sr_user_data;
        g_router.tflm_user_data = config->tflm_user_data;
    }
    
    g_router.current_target = AUDIO_ROUTE_TFLM;  /* 默认路由到 TFLM */
    g_router.vad_state = VAD_SILENCE;
    g_router.esp_sr_count = 0;
    g_router.tflm_count = 0;
    
    g_router.initialized = true;
    
    ESP_LOGI(TAG, "音频路由器初始化完成");
    
    return ESP_OK;
}

void audio_router_deinit(void)
{
    if (!g_router.initialized) {
        return;
    }
    
    if (g_router.mutex) {
        vSemaphoreDelete(g_router.mutex);
    }
    
    memset(&g_router, 0, sizeof(router_state_t));
    ESP_LOGI(TAG, "音频路由器已释放");
}

bool audio_router_is_initialized(void)
{
    return g_router.initialized;
}

audio_route_target_t audio_router_route(const int16_t *samples, int len)
{
    if (!g_router.initialized || !samples) {
        return AUDIO_ROUTE_NONE;
    }
    
    xSemaphoreTake(g_router.mutex, portMAX_DELAY);
    
    /* 根据 VAD 状态决定路由 */
    audio_route_target_t target;
    
    if (g_router.vad_state == VAD_SPEECH) {
        target = AUDIO_ROUTE_ESP_SR;
    } else {
        target = AUDIO_ROUTE_TFLM;
    }
    
    /* 路由数据到对应模块 */
    if (target == AUDIO_ROUTE_ESP_SR && g_router.esp_sr_callback) {
        g_router.esp_sr_callback(target, samples, len, g_router.esp_sr_user_data);
        g_router.esp_sr_count++;
    } else if (target == AUDIO_ROUTE_TFLM && g_router.tflm_callback) {
        g_router.tflm_callback(target, samples, len, g_router.tflm_user_data);
        g_router.tflm_count++;
    }
    
    /* 状态变化日志 */
    if (target != g_router.current_target) {
        ESP_LOGI(TAG, "路由切换: %s → %s",
                 g_router.current_target == AUDIO_ROUTE_ESP_SR ? "ESP-SR" : "TFLM",
                 target == AUDIO_ROUTE_ESP_SR ? "ESP-SR" : "TFLM");
        g_router.current_target = target;
    }
    
    xSemaphoreGive(g_router.mutex);
    
    return target;
}

audio_route_target_t audio_router_get_target(void)
{
    if (!g_router.initialized) {
        return AUDIO_ROUTE_NONE;
    }
    
    xSemaphoreTake(g_router.mutex, portMAX_DELAY);
    audio_route_target_t target = g_router.current_target;
    xSemaphoreGive(g_router.mutex);
    
    return target;
}

esp_err_t audio_router_set_target(audio_route_target_t target)
{
    if (!g_router.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_router.mutex, portMAX_DELAY);
    g_router.current_target = target;
    xSemaphoreGive(g_router.mutex);
    
    ESP_LOGI(TAG, "强制路由到: %s", target == AUDIO_ROUTE_ESP_SR ? "ESP-SR" : "TFLM");
    
    return ESP_OK;
}

void audio_router_set_vad_state(vad_state_t state)
{
    if (!g_router.initialized) {
        return;
    }
    
    xSemaphoreTake(g_router.mutex, portMAX_DELAY);
    g_router.vad_state = state;
    xSemaphoreGive(g_router.mutex);
}

void audio_router_get_stats(uint32_t *esp_sr_count, uint32_t *tflm_count)
{
    if (!g_router.initialized) {
        if (esp_sr_count) *esp_sr_count = 0;
        if (tflm_count) *tflm_count = 0;
        return;
    }
    
    xSemaphoreTake(g_router.mutex, portMAX_DELAY);
    if (esp_sr_count) {
        *esp_sr_count = g_router.esp_sr_count;
    }
    if (tflm_count) {
        *tflm_count = g_router.tflm_count;
    }
    xSemaphoreGive(g_router.mutex);
}