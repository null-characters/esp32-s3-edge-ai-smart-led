/**
 * @file aec_pipeline.c
 * @brief AEC 流水线实现
 * 
 * 使用 ESP-SR 的 AEC (Acoustic Echo Cancellation) 消除回声
 */

#include "aec_pipeline.h"
#include "esp_log.h"
#include "esp_aec.h"
#include "esp_heap_caps.h"
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
    
    /* ESP-SR AEC 实例 */
    aec_handle_t *aec_handle;
    int frame_size;
    
    /* 参考音频缓冲 (用于延迟补偿) */
    int16_t *ref_ring_buffer;
    int ref_buffer_size;
    int ref_write_pos;
    int ref_read_pos;
    int ref_delay_frames;
    
    /* 处理缓冲区 */
    int16_t *aligned_input;
    int16_t *aligned_ref;
    int16_t *aligned_output;
    
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
 * 内部函数
 * ================================================================ */

/**
 * @brief 环形缓冲区写入
 */
static void ring_buffer_write(int16_t *buffer, int size, int *pos, const int16_t *data, int len)
{
    for (int i = 0; i < len; i++) {
        buffer[*pos] = data[i];
        *pos = (*pos + 1) % size;
    }
}

/**
 * @brief 环形缓冲区读取 (带延迟)
 */
static void ring_buffer_read_delayed(int16_t *buffer, int size, int write_pos, 
                                      int delay_frames, int16_t *out, int len)
{
    int read_pos = (write_pos - delay_frames - len + size) % size;
    for (int i = 0; i < len; i++) {
        out[i] = buffer[read_pos];
        read_pos = (read_pos + 1) % size;
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t aec_pipeline_init(const aec_config_t *config)
{
    if (g_aec.initialized) {
        ESP_LOGW(TAG, "AEC 流水线已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化 AEC 流水线 (使用 ESP-SR AEC)");
    
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
    
    /* 创建 ESP-SR AEC 实例 */
    /* 参数说明:
     * - filter_length: 滤波器长度，推荐 4，越大消耗资源越多
     * - channel_num: 麦克风通道数
     * - mode: AEC 模式，语音识别推荐 AEC_MODE_SR_LOW_COST
     */
    g_aec.aec_handle = aec_create(
        AEC_SAMPLE_RATE,
        4,  /* filter_length */
        g_aec.config.mic_channel,
        AEC_MODE_SR_LOW_COST
    );
    
    if (!g_aec.aec_handle) {
        ESP_LOGE(TAG, "创建 AEC 实例失败");
        vSemaphoreDelete(g_aec.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    /* 获取帧大小 */
    g_aec.frame_size = aec_get_chunksize(g_aec.aec_handle);
    ESP_LOGI(TAG, "AEC 帧大小: %d 样本 (%d ms)", 
             g_aec.frame_size, AEC_FRAME_LENGTH_MS);
    
    /* 创建参考音频环形缓冲区 (用于延迟补偿) */
    /* DAC 输出到扬声器 -> 声音传播到麦克风 有物理延迟 */
    /* 缓冲区大小: 约 200ms 的数据 */
    g_aec.ref_buffer_size = g_aec.frame_size * 8;
    g_aec.ref_ring_buffer = (int16_t *)heap_caps_malloc(
        g_aec.ref_buffer_size * sizeof(int16_t),
        MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL
    );
    if (!g_aec.ref_ring_buffer) {
        ESP_LOGE(TAG, "分配参考缓冲区失败");
        aec_destroy(g_aec.aec_handle);
        vSemaphoreDelete(g_aec.mutex);
        return ESP_ERR_NO_MEM;
    }
    memset(g_aec.ref_ring_buffer, 0, g_aec.ref_buffer_size * sizeof(int16_t));
    g_aec.ref_write_pos = 0;
    g_aec.ref_delay_frames = 2;  /* 默认 2 帧 (~64ms) 延迟补偿 */
    
    /* 分配对齐的处理缓冲区 (ESP-SR AEC 要求对齐) */
    g_aec.aligned_input = (int16_t *)heap_caps_aligned_alloc(
        16, g_aec.frame_size * sizeof(int16_t), 
        MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL
    );
    g_aec.aligned_ref = (int16_t *)heap_caps_aligned_alloc(
        16, g_aec.frame_size * sizeof(int16_t),
        MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL
    );
    g_aec.aligned_output = (int16_t *)heap_caps_aligned_alloc(
        16, g_aec.frame_size * sizeof(int16_t),
        MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL
    );
    
    if (!g_aec.aligned_input || !g_aec.aligned_ref || !g_aec.aligned_output) {
        ESP_LOGE(TAG, "分配对齐缓冲区失败");
        if (g_aec.aligned_input) heap_caps_free(g_aec.aligned_input);
        if (g_aec.aligned_ref) heap_caps_free(g_aec.aligned_ref);
        if (g_aec.aligned_output) heap_caps_free(g_aec.aligned_output);
        heap_caps_free(g_aec.ref_ring_buffer);
        aec_destroy(g_aec.aec_handle);
        vSemaphoreDelete(g_aec.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    g_aec.enabled = g_aec.config.aec_enable;
    g_aec.initialized = true;
    
    ESP_LOGI(TAG, "AEC 流水线初始化完成: aec=%d, frame_size=%d, mode=%s",
             g_aec.config.aec_enable, g_aec.frame_size,
             aec_get_mode_string(AEC_MODE_SR_LOW_COST));
    
    return ESP_OK;
}

void aec_pipeline_deinit(void)
{
    if (!g_aec.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "释放 AEC 流水线");
    
    if (g_aec.aec_handle) {
        aec_destroy(g_aec.aec_handle);
        g_aec.aec_handle = NULL;
    }
    
    if (g_aec.ref_ring_buffer) {
        heap_caps_free(g_aec.ref_ring_buffer);
        g_aec.ref_ring_buffer = NULL;
    }
    
    if (g_aec.aligned_input) {
        heap_caps_free(g_aec.aligned_input);
        g_aec.aligned_input = NULL;
    }
    if (g_aec.aligned_ref) {
        heap_caps_free(g_aec.aligned_ref);
        g_aec.aligned_ref = NULL;
    }
    if (g_aec.aligned_output) {
        heap_caps_free(g_aec.aligned_output);
        g_aec.aligned_output = NULL;
    }
    
    if (g_aec.mutex) {
        vSemaphoreDelete(g_aec.mutex);
    }
    
    memset(&g_aec, 0, sizeof(aec_state_t));
    ESP_LOGI(TAG, "AEC 流水线已释放");
}

esp_err_t aec_pipeline_process(const int16_t *mic_input, const int16_t *ref_input, 
                                int16_t *output, int len)
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
    
    /* 处理必须是 AEC 帧大小的整数倍 */
    int processed = 0;
    
    while (processed + g_aec.frame_size <= len) {
        /* 复制输入数据到对齐缓冲区 */
        memcpy(g_aec.aligned_input, mic_input + processed, 
               g_aec.frame_size * sizeof(int16_t));
        
        /* 获取延迟后的参考信号 */
        if (ref_input) {
            /* 有新的参考输入，写入环形缓冲区 */
            ring_buffer_write(g_aec.ref_ring_buffer, g_aec.ref_buffer_size,
                              &g_aec.ref_write_pos, ref_input + processed,
                              g_aec.frame_size);
            
            /* 读取延迟后的参考信号 */
            ring_buffer_read_delayed(g_aec.ref_ring_buffer, g_aec.ref_buffer_size,
                                      g_aec.ref_write_pos, g_aec.ref_delay_frames,
                                      g_aec.aligned_ref, g_aec.frame_size);
        } else {
            /* 无参考输入，使用环形缓冲区中的历史数据 */
            ring_buffer_read_delayed(g_aec.ref_ring_buffer, g_aec.ref_buffer_size,
                                      g_aec.ref_write_pos, g_aec.ref_delay_frames,
                                      g_aec.aligned_ref, g_aec.frame_size);
        }
        
        /* 执行 AEC 处理 */
        aec_process(g_aec.aec_handle, g_aec.aligned_input, 
                    g_aec.aligned_ref, g_aec.aligned_output);
        
        /* 复制输出 */
        memcpy(output + processed, g_aec.aligned_output,
               g_aec.frame_size * sizeof(int16_t));
        
        processed += g_aec.frame_size;
    }
    
    /* 处理剩余数据 (不足一帧) */
    if (processed < len) {
        memcpy(output + processed, mic_input + processed,
               (len - processed) * sizeof(int16_t));
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
    
    ring_buffer_write(g_aec.ref_ring_buffer, g_aec.ref_buffer_size,
                      &g_aec.ref_write_pos, ref_data, write_len);
    
    xSemaphoreGive(g_aec.mutex);
    
    return ESP_OK;
}

bool aec_pipeline_is_enabled(void)
{
    return g_aec.enabled;
}

void aec_pipeline_set_enable(bool enable)
{
    xSemaphoreTake(g_aec.mutex, portMAX_DELAY);
    g_aec.enabled = enable;
    xSemaphoreGive(g_aec.mutex);
    ESP_LOGI(TAG, "AEC %s", enable ? "启用" : "禁用");
}

void aec_pipeline_set_delay(int delay_frames)
{
    xSemaphoreTake(g_aec.mutex, portMAX_DELAY);
    /* 限制延迟范围: 0-6 帧 */
    if (delay_frames < 0) delay_frames = 0;
    if (delay_frames > 6) delay_frames = 6;
    g_aec.ref_delay_frames = delay_frames;
    xSemaphoreGive(g_aec.mutex);
    ESP_LOGI(TAG, "延迟补偿设置为 %d 帧 (~%d ms)", 
             delay_frames, delay_frames * AEC_FRAME_LENGTH_MS);
}

int aec_pipeline_get_delay(void)
{
    return g_aec.ref_delay_frames;
}

int aec_pipeline_get_frame_size(void)
{
    if (!g_aec.initialized) {
        return AEC_FRAME_SIZE;
    }
    return g_aec.frame_size;
}

void aec_pipeline_reset(void)
{
    if (!g_aec.initialized) {
        return;
    }
    
    xSemaphoreTake(g_aec.mutex, portMAX_DELAY);
    
    /* 清空参考缓冲区 */
    memset(g_aec.ref_ring_buffer, 0, g_aec.ref_buffer_size * sizeof(int16_t));
    g_aec.ref_write_pos = 0;
    
    xSemaphoreGive(g_aec.mutex);
    
    ESP_LOGI(TAG, "AEC 状态已重置");
}