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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "AUDIO_AFE";

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
    
    /* 任务控制 */
    TaskHandle_t task_handle;
    volatile bool running;
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
                
                /* 回调通知 */
                if (state->callback) {
                    state->callback(&afe_result, state->user_data);
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "音频处理任务退出");
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
        if (g_state.models) {
            memset(g_state.models, 0, sizeof(srmodel_list_t));
        }
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
    
    /* 分配音频缓冲区 */
    g_state.audio_buffer = (int16_t *)malloc(g_state.feed_chunk_size * sizeof(int16_t));
    if (!g_state.audio_buffer) {
        ESP_LOGE(TAG, "分配音频缓冲区失败");
        goto error;
    }
    
    g_state.initialized = true;
    
    ESP_LOGI(TAG, "AFE 初始化成功: feed=%d, fetch=%d, sr=%dHz",
             g_state.feed_chunk_size, g_state.fetch_chunk_size, config->sample_rate);
    afe_config_print(g_state.afe_cfg);
    
    return 0;

error:
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
        8 * 1024,  /* 8KB 栈 */
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
    
    /* 等待任务结束 */
    if (g_state.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_state.task_handle = NULL;
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
    g_state.callback = callback;
    g_state.user_data = user_data;
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
