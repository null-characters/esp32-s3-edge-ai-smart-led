/**
 * @file aec_pipeline.c
 * @brief AEC 流水线实现
 * 
 * 注意：完整实现需要 ESP-ADF 库支持，此处提供框架代码
 */

#include "aec_pipeline.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "AEC_PIPELINE";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    bool enabled;
    
    /* 配置 */
    aec_config_t config;
    
    /* 参考音频缓冲 */
    int16_t *ref_buffer;
    int ref_buffer_size;
    int ref_write_pos;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} aec_state_t;

static aec_state_t g_aec = {0};

/* ================================================================
 * 默认配置
 * ================================================================ */

static const aec_config_t aec_default_config = {
    .mic_channel = 1,
    .ref_channel = 1,
    .aec_enable = true,
    .ns_enable = true,
    .agc_enable = false,
};

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t aec_pipeline_init(const aec_config_t *config)
{
    if (g_aec.initialized) {
        ESP_LOGW(TAG, "AEC 流水线已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化 AEC 流水线");
    
    g_aec.mutex = xSemaphoreCreateMutex();
    if (!g_aec.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    if (config) {
        memcpy(&g_aec.config, config, sizeof(aec_config_t));
    } else {
        memcpy(&g_aec.config, &aec_default_config, sizeof(aec_config_t));
    }
    
    /* 创建参考音频缓冲区 */
    g_aec.ref_buffer_size = AEC_FRAME_SIZE * 4;
    g_aec.ref_buffer = (int16_t *)malloc(g_aec.ref_buffer_size * sizeof(int16_t));
    if (!g_aec.ref_buffer) {
        ESP_LOGE(TAG, "分配参考缓冲区失败");
        vSemaphoreDelete(g_aec.mutex);
        return ESP_ERR_NO_MEM;
    }
    memset(g_aec.ref_buffer, 0, g_aec.ref_buffer_size * sizeof(int16_t));
    g_aec.ref_write_pos = 0;
    
    g_aec.enabled = g_aec.config.aec_enable;
    g_aec.initialized = true;
    
    ESP_LOGI(TAG, "AEC 流水线初始化完成: aec=%d, ns=%d, agc=%d",
             g_aec.config.aec_enable, g_aec.config.ns_enable, g_aec.config.agc_enable);
    
    /* 注意：完整实现需要集成 ESP-ADF 的 esp_afe_sr_models */
    ESP_LOGW(TAG, "当前为框架实现，完整 AEC 需集成 ESP-ADF");
    
    return ESP_OK;
}

void aec_pipeline_deinit(void)
{
    if (!g_aec.initialized) {
        return;
    }
    
    if (g_aec.ref_buffer) {
        free(g_aec.ref_buffer);
    }
    
    if (g_aec.mutex) {
        vSemaphoreDelete(g_aec.mutex);
    }
    
    memset(&g_aec, 0, sizeof(aec_state_t));
    ESP_LOGI(TAG, "AEC 流水线已释放");
}

esp_err_t aec_pipeline_process(const int16_t *mic_input, const int16_t *ref_input, int16_t *output, int len)
{
    if (!g_aec.initialized || !mic_input || !output) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_aec.mutex, portMAX_DELAY);
    
    if (!g_aec.enabled) {
        /* AEC 未启用，直接复制输入到输出 */
        memcpy(output, mic_input, len * sizeof(int16_t));
        xSemaphoreGive(g_aec.mutex);
        return ESP_OK;
    }
    
    /* 框架实现：简单延迟补偿 + 能量抑制 */
    /* 完整实现需要使用 ESP-ADF 的 afe_handle */
    
    /* 模拟 AEC：当有参考信号时，降低输入信号的能量 */
    if (ref_input) {
        /* 检测参考信号能量 */
        int64_t ref_energy = 0;
        for (int i = 0; i < len; i++) {
            ref_energy += (int64_t)ref_input[i] * ref_input[i];
        }
        float ref_energy_avg = (float)ref_energy / len;
        
        /* 如果参考信号能量较高，降低麦克风信号 */
        if (ref_energy_avg > 1000.0f) {
            float attenuation = 0.3f;  /* 70% 抑制 */
            for (int i = 0; i < len; i++) {
                output[i] = (int16_t)(mic_input[i] * attenuation);
            }
            ESP_LOGD(TAG, "AEC 抑制回声: ref_energy=%.1f", ref_energy_avg);
        } else {
            memcpy(output, mic_input, len * sizeof(int16_t));
        }
    } else {
        memcpy(output, mic_input, len * sizeof(int16_t));
    }
    
    xSemaphoreGive(g_aec.mutex);
    
    return ESP_OK;
}

esp_err_t aec_pipeline_set_reference(const int16_t *ref_data, int len)
{
    if (!g_aec.initialized || !ref_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_aec.mutex, portMAX_DELAY);
    
    /* 写入参考缓冲区 */
    int write_len = len;
    if (write_len > g_aec.ref_buffer_size) {
        write_len = g_aec.ref_buffer_size;
    }
    
    memcpy(g_aec.ref_buffer, ref_data, write_len * sizeof(int16_t));
    g_aec.ref_write_pos = write_len;
    
    xSemaphoreGive(g_aec.mutex);
    
    return ESP_OK;
}

bool aec_pipeline_is_enabled(void)
{
    return g_aec.enabled;
}

void aec_pipeline_set_enable(bool enable)
{
    g_aec.enabled = enable;
    ESP_LOGI(TAG, "AEC %s", enable ? "启用" : "禁用");
}

int aec_pipeline_get_frame_size(void)
{
    return AEC_FRAME_SIZE;
}