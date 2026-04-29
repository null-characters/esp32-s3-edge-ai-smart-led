/**
 * @file audio_afe.c
 * @brief ESP-SR 音频前端 (AFE) 实现文件
 * 
 * 集成 INMP441 麦克风 + ESP-SR AFE (WakeNet/MultiNet)
 */

#include "audio_afe.h"
#include "i2s_driver.h"
#include "esp_log.h"
#include "esp_afe_config.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "AUDIO_AFE";

/* ================================================================
 * 内存状态打印辅助函数
 * ================================================================ */

static void print_memory_status(void)
{
    ESP_LOGI(TAG, "内存状态:");
    ESP_LOGI(TAG, "  内部RAM: %zu bytes 可用", 
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "  PSRAM: %zu bytes 可用", 
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "  DMA可用: %zu bytes", 
             heap_caps_get_free_size(MALLOC_CAP_DMA));
}

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    audio_afe_config_t config;
    
    /* ESP-SR AFE */
    const esp_afe_sr_iface_t *afe_iface;
    esp_afe_sr_data_t *afe_data;
    afe_config_t *afe_cfg;
    srmodel_list_t *models;
    
    /* 音频缓冲 */
    int16_t *audio_buffer;
    int feed_chunk_size;
    int fetch_chunk_size;
    
    /* 回调 */
    audio_afe_callback_t callback;
    void *user_data;
    SemaphoreHandle_t callback_mutex;  /**< 回调设置保护 */
    
    /* 任务控制 */
    TaskHandle_t task_handle;
    volatile bool running;
    TaskHandle_t caller_handle;        /**< 停止调用者任务句柄 */
    
    /* 运行时统计 (从任务函数静态变量移入) */
    uint32_t loop_count;               /**< 循环计数器 */
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
 * 内部函数
 * ================================================================ */

/**
 * @brief 音频处理任务
 */
static void audio_afe_task(void *arg)
{
    audio_afe_state_t *state = (audio_afe_state_t *)arg;
    size_t bytes_read;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "音频处理任务启动");
    
    /* 打印初始栈水位 */
    UBaseType_t initial_watermark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "初始栈水位: %u bytes", initial_watermark * sizeof(StackType_t));
    
    while (state->running) {
        /* 从 I2S 读取音频 */
        ret = i2s_read_timeout(state->audio_buffer, state->feed_chunk_size, 
                              &bytes_read, pdMS_TO_TICKS(100));
        
        if (ret != ESP_OK || bytes_read == 0) {
            continue;
        }
        
        /* 喂入 AFE */
        if (state->afe_iface && state->afe_data) {
            state->afe_iface->feed(state->afe_data, state->audio_buffer);
            
            /* 获取处理结果 */
            afe_fetch_result_t *result = state->afe_iface->fetch(state->afe_data);
            
            if (result) {
                audio_afe_result_t afe_result = {
                    .data = result->data,
                    .data_size = result->data_size,
                    .is_speech = (result->vad_state == VAD_SPEECH),
                    .volume = result->data_volume,
                    .wake_word_detected = (result->wakeup_state == WAKENET_DETECTED),
                    .wake_word_index = result->wake_word_index,
                };
                
                /* 安全回调通知 */
                if (xSemaphoreTake(state->callback_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    audio_afe_callback_t cb = state->callback;
                    void *user_data = state->user_data;
                    xSemaphoreGive(state->callback_mutex);
                    
                    if (cb) {
                        cb(&afe_result, user_data);
                    }
                }
            }
        }
        
        /* 每1000次循环打印栈水位 (监控栈使用) */
        state->loop_count++;
        if (state->loop_count % 1000 == 0) {
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGD(TAG, "栈水位: %u bytes (使用: %u bytes)", 
                     watermark * sizeof(StackType_t),
                     (16 * 1024) - watermark * sizeof(StackType_t));
        }
    }
    
    /* 打印最终栈水位 */
    UBaseType_t final_watermark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "最终栈水位: %u bytes (峰值使用: %u bytes)", 
             final_watermark * sizeof(StackType_t),
             (16 * 1024) - final_watermark * sizeof(StackType_t));
    
    ESP_LOGI(TAG, "音频处理任务退出");
    
    /* 通知停止调用者 */
    if (state->caller_handle) {
        xTaskNotifyGive(state->caller_handle);
    }
    
    vTaskDelete(NULL);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

int audio_afe_init(const audio_afe_config_t *config)
{
    if (g_state.initialized) {
        ESP_LOGW(TAG, "AFE 已初始化");
        return 0;
    }
    
    if (!config) {
        config = &audio_afe_default_config;
    }
    
    ESP_LOGI(TAG, "初始化 AFE 模块 (INMP441 + ESP-SR)");
    
    /* 打印初始内存状态 */
    print_memory_status();
    
    /* 保存配置 */
    memcpy(&g_state.config, config, sizeof(audio_afe_config_t));
    
    /* 初始化 I2S 驱动 */
    esp_err_t ret = i2s_init(config->sample_rate, 32); /* INMP441 使用 32-bit */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 初始化失败: %d", ret);
        return -1;
    }
    
    /* 加载模型列表 */
    g_state.models = esp_srmodel_init("model");
    if (!g_state.models) {
        ESP_LOGW(TAG, "模型列表加载失败，使用默认配置");
        /* 创建空模型列表，ESP-SR 会使用内置模型 */
        g_state.models = (srmodel_list_t *)malloc(sizeof(srmodel_list_t));
        if (!g_state.models) {
            ESP_LOGE(TAG, "分配模型列表失败");
            goto error;
        }
        memset(g_state.models, 0, sizeof(srmodel_list_t));
    }
    
    /* 创建回调保护 mutex */
    g_state.callback_mutex = xSemaphoreCreateMutex();
    if (!g_state.callback_mutex) {
        ESP_LOGE(TAG, "创建 mutex 失败");
        goto error;
    }
    
    /* 配置 AFE */
    /* INMP441 单麦克风: input_format = "M" */
    const char *input_format = config->mic_channels == 1 ? "M" : 
                               config->ref_channels > 0 ? "MR" : "MM";
    
    g_state.afe_cfg = afe_config_init(input_format, g_state.models, 
                                       AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!g_state.afe_cfg) {
        ESP_LOGE(TAG, "AFE 配置创建失败");
        goto error;
    }
    
    /* 应用自定义配置 */
    g_state.afe_cfg->vad_init = config->enable_vad;
    g_state.afe_cfg->ns_init = config->enable_ns;
    g_state.afe_cfg->agc_init = config->enable_agc;
    g_state.afe_cfg->wakenet_init = config->enable_wakenet;
    
    if (config->wakenet_model) {
        g_state.afe_cfg->wakenet_model_name = strdup(config->wakenet_model);
    }
    
    /* 检查配置 */
    afe_config_check(g_state.afe_cfg);
    
    /* 获取 AFE 接口 */
    g_state.afe_iface = esp_afe_handle_from_config(g_state.afe_cfg);
    if (!g_state.afe_iface) {
        ESP_LOGE(TAG, "获取 AFE 接口失败");
        goto error;
    }
    
    /* 创建 AFE 实例 */
    g_state.afe_data = g_state.afe_iface->create_from_config(g_state.afe_cfg);
    if (!g_state.afe_data) {
        ESP_LOGE(TAG, "创建 AFE 实例失败");
        goto error;
    }
    
    /* 获取块大小 */
    g_state.feed_chunk_size = g_state.afe_iface->get_feed_chunksize(g_state.afe_data);
    g_state.fetch_chunk_size = g_state.afe_iface->get_fetch_chunksize(g_state.afe_data);
    
    /* 分配音频缓冲区到PSRAM (ESP32-S3有8MB外部内存) */
    g_state.audio_buffer = (int16_t *)heap_caps_malloc(
        g_state.feed_chunk_size * sizeof(int16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (!g_state.audio_buffer) {
        /* 回退到内部RAM */
        g_state.audio_buffer = (int16_t *)malloc(g_state.feed_chunk_size * sizeof(int16_t));
        if (!g_state.audio_buffer) {
            ESP_LOGE(TAG, "分配音频缓冲区失败");
            goto error;
        }
        ESP_LOGW(TAG, "音频缓冲区分配到内部RAM (PSRAM不可用)");
    } else {
        ESP_LOGI(TAG, "音频缓冲区分配到PSRAM (%d bytes)", 
                 g_state.feed_chunk_size * sizeof(int16_t));
    }
    
    g_state.initialized = true;
    
    /* 打印初始化后内存状态 */
    ESP_LOGI(TAG, "AFE 初始化后内存状态:");
    print_memory_status();
    
    ESP_LOGI(TAG, "AFE 初始化成功: feed=%d, fetch=%d, sr=%dHz",
             g_state.feed_chunk_size, g_state.fetch_chunk_size, config->sample_rate);
    afe_config_print(g_state.afe_cfg);
    
    return 0;

error:
    if (g_state.callback_mutex) {
        vSemaphoreDelete(g_state.callback_mutex);
    }
    if (g_state.afe_data && g_state.afe_iface) {
        g_state.afe_iface->destroy(g_state.afe_data);
    }
    if (g_state.afe_cfg) {
        if (g_state.afe_cfg->wakenet_model_name) {
            free(g_state.afe_cfg->wakenet_model_name);
        }
        afe_config_free(g_state.afe_cfg);
    }
    if (g_state.models) {
        esp_srmodel_deinit(g_state.models);
    }
    if (g_state.audio_buffer) {
        free(g_state.audio_buffer);
    }
    i2s_deinit();
    
    memset(&g_state, 0, sizeof(g_state));
    return -2;
}

void audio_afe_deinit(void)
{
    if (!g_state.initialized) {
        return;
    }
    
    /* 停止处理任务 */
    audio_afe_stop();
    
    /* 释放 AFE 资源 */
    if (g_state.afe_data && g_state.afe_iface) {
        g_state.afe_iface->destroy(g_state.afe_data);
    }
    
    if (g_state.afe_cfg) {
        /* 释放 strdup 分配的内存 */
        if (g_state.afe_cfg->wakenet_model_name) {
            free(g_state.afe_cfg->wakenet_model_name);
            g_state.afe_cfg->wakenet_model_name = NULL;
        }
        afe_config_free(g_state.afe_cfg);
    }
    
    if (g_state.models) {
        esp_srmodel_deinit(g_state.models);
    }
    
    if (g_state.audio_buffer) {
        free(g_state.audio_buffer);
    }
    
    /* 释放 mutex */
    if (g_state.callback_mutex) {
        vSemaphoreDelete(g_state.callback_mutex);
        g_state.callback_mutex = NULL;
    }
    
    /* 释放 I2S */
    i2s_deinit();
    
    memset(&g_state, 0, sizeof(g_state));
    ESP_LOGI(TAG, "AFE 已释放");
}

int audio_afe_start(void)
{
    if (!g_state.initialized) {
        return -1;
    }
    
    if (g_state.running) {
        ESP_LOGW(TAG, "AFE 已在运行");
        return 0;
    }
    
    g_state.running = true;
    
    /* 创建音频处理任务 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_afe_task,
        "audio_afe",
        16 * 1024,  /* 16KB 栈 (ESP-SR AFE需要较大栈空间) */
        &g_state,
        5,  /* 优先级 */
        &g_state.task_handle,
        1   /* 绑定到核心 1 */
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建音频处理任务失败");
        g_state.running = false;
        return -2;
    }
    
    ESP_LOGI(TAG, "音频处理已启动");
    return 0;
}

void audio_afe_stop(void)
{
    if (!g_state.running) {
        return;
    }
    
    g_state.running = false;
    
    /* 使用任务通知等待任务退出 */
    if (g_state.task_handle) {
        g_state.caller_handle = xTaskGetCurrentTaskHandle();
        
        /* 等待任务通知，超时 500ms */
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        if (notified == 0) {
            ESP_LOGW(TAG, "等待任务退出超时");
        }
        
        g_state.task_handle = NULL;
        g_state.caller_handle = NULL;
    }
    
    ESP_LOGI(TAG, "音频处理已停止");
}

int audio_afe_get_feed_chunk_size(void)
{
    if (!g_state.initialized) {
        return 160;  /* 默认 10ms @ 16kHz */
    }
    return g_state.feed_chunk_size;
}

int audio_afe_get_fetch_chunk_size(void)
{
    if (!g_state.initialized) {
        return 160;
    }
    return g_state.fetch_chunk_size;
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
    
    if (g_state.afe_iface && g_state.afe_data) {
        return g_state.afe_iface->feed(g_state.afe_data, samples);
    }
    
    return g_state.feed_chunk_size;
}

bool audio_afe_fetch(audio_afe_result_t *result)
{
    if (!g_state.initialized || !result) {
        return false;
    }
    
    if (g_state.afe_iface && g_state.afe_data) {
        afe_fetch_result_t *afe_res = g_state.afe_iface->fetch(g_state.afe_data);
        if (afe_res) {
            result->data = afe_res->data;
            result->data_size = afe_res->data_size;
            result->is_speech = (afe_res->vad_state == VAD_SPEECH);
            result->volume = afe_res->data_volume;
            result->wake_word_detected = (afe_res->wakeup_state == WAKENET_DETECTED);
            result->wake_word_index = afe_res->wake_word_index;
            return true;
        }
    }
    
    return false;
}

void audio_afe_set_callback(audio_afe_callback_t callback, void *user_data)
{
    if (g_state.callback_mutex) {
        xSemaphoreTake(g_state.callback_mutex, portMAX_DELAY);
    }
    g_state.callback = callback;
    g_state.user_data = user_data;
    if (g_state.callback_mutex) {
        xSemaphoreGive(g_state.callback_mutex);
    }
}

void audio_afe_wakenet_enable(bool enable)
{
    if (!g_state.initialized) {
        return;
    }
    
    if (g_state.afe_iface && g_state.afe_data) {
        if (enable) {
            g_state.afe_iface->enable_wakenet(g_state.afe_data);
        } else {
            g_state.afe_iface->disable_wakenet(g_state.afe_data);
        }
    }
    
    ESP_LOGI(TAG, "WakeNet %s", enable ? "已启用" : "已禁用");
}

void audio_afe_multinet_enable(bool enable)
{
    if (!g_state.initialized) {
        return;
    }
    
    /* MultiNet 通过 WakeNet 状态控制 */
    ESP_LOGI(TAG, "MultiNet %s (框架模式)", enable ? "已启用" : "已禁用");
}

void audio_afe_reset(void)
{
    if (!g_state.initialized) {
        return;
    }
    
    if (g_state.afe_iface && g_state.afe_data) {
        g_state.afe_iface->reset_buffer(g_state.afe_data);
    }
    
    ESP_LOGD(TAG, "AFE 状态已重置");
}

int audio_afe_set_wakenet_threshold(float threshold)
{
    if (!g_state.initialized) {
        return -1;
    }
    
    if (g_state.afe_iface && g_state.afe_data) {
        /* threshold 范围: 0.4 - 0.9999 */
        if (threshold < 0.4f) threshold = 0.4f;
        if (threshold > 0.9999f) threshold = 0.9999f;
        
        g_state.afe_iface->set_wakenet_threshold(g_state.afe_data, 1, threshold);
        ESP_LOGI(TAG, "唤醒阈值设置为 %.4f", threshold);
        return 0;
    }
    
    return -2;
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

bool audio_afe_is_running(void)
{
    return g_state.running;
}
