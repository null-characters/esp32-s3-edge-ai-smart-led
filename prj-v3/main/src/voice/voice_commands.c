/**
 * @file voice_commands.c
 * @brief 语音命令识别模块实现 (ESP-SR MultiNet 封装)
 */

#include "voice_commands.h"
#include "esp_log.h"
#include <string.h>

/* ESP-SR MultiNet 头文件 (ESP32-S3) */
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "command_words.h"

static const char *TAG = "VOICE_CMD";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    bool running;
    float threshold;
    int timeout_ms;
    voice_command_callback_t callback;
    void *user_data;
    
    /* MultiNet 句柄 */
    const esp_mn_iface_t *multinet;
    model_iface_data_t *model_data;
    
    /* 识别结果 */
    voice_command_result_t last_result;
} voice_commands_state_t;

static voice_commands_state_t g_state = {0};

/* ================================================================
 * 公共 API
 * ================================================================ */

int voice_commands_init(const voice_commands_config_t *config)
{
    if (!config) {
        return -1;
    }
    
    ESP_LOGI(TAG, "初始化语音命令识别模块");
    
    /* 获取 MultiNet 接口 */
    char *model_name = config->model_name ? (char *)config->model_name : "mn2_cn";
    g_state.multinet = esp_mn_handle_from_name(model_name);
    if (!g_state.multinet) {
        ESP_LOGE(TAG, "获取 MultiNet 接口失败: %s", model_name);
        return -2;
    }
    
    /* 创建模型实例 */
    g_state.model_data = g_state.multinet->create(config->model_name ? config->model_name : "mn1",
                                                   config->timeout_ms);
    if (!g_state.model_data) {
        ESP_LOGE(TAG, "创建 MultiNet 模型失败");
        return -3;
    }
    
    /* 设置阈值 */
    g_state.threshold = config->threshold > 0 ? config->threshold : 0.5f;
    g_state.multinet->set_det_threshold(g_state.model_data, g_state.threshold);
    
    /* 保存配置 */
    g_state.timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 5000;
    g_state.callback = config->callback;
    g_state.user_data = config->user_data;
    
    g_state.initialized = true;
    g_state.running = false;
    
    ESP_LOGI(TAG, "MultiNet 初始化成功, 采样率=%d, 块大小=%d",
             g_state.multinet->get_samp_rate(g_state.model_data),
             g_state.multinet->get_samp_chunksize(g_state.model_data));
    
    return 0;
}

int voice_commands_start(void)
{
    if (!g_state.initialized) {
        return -1;
    }
    
    g_state.running = true;
    ESP_LOGI(TAG, "命令识别已启动");
    return 0;
}

int voice_commands_stop(void)
{
    if (!g_state.initialized) {
        return -1;
    }
    
    g_state.running = false;
    ESP_LOGI(TAG, "命令识别已停止");
    return 0;
}

void voice_commands_deinit(void)
{
    if (!g_state.initialized) {
        return;
    }
    
    g_state.running = false;
    
    /* 释放 MultiNet 资源 */
    if (g_state.model_data && g_state.multinet) {
        g_state.multinet->destroy(g_state.model_data);
        g_state.model_data = NULL;
    }
    
    /* 清理命令词链表 */
    esp_mn_commands_free();
    
    g_state.initialized = false;
    ESP_LOGI(TAG, "语音命令模块已释放");
}

int voice_commands_add_phrase(int command_id, const char *phrase)
{
    if (!g_state.initialized || !phrase) {
        return -1;
    }
    
    /* 使用 ESP-SR 命令词 API */
    esp_err_t ret = esp_mn_commands_add(command_id, phrase);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加命令词失败: ID=%d, 短语=%s, 错误=%d", command_id, phrase, ret);
        return -2;
    }
    
    ESP_LOGD(TAG, "添加命令词成功: ID=%d, 短语=%s", command_id, phrase);
    return 0;
}

int voice_commands_register_all(void)
{
    if (!g_state.initialized) {
        return -1;
    }
    
    ESP_LOGI(TAG, "注册所有命令词 (%d 条)", COMMAND_WORDS_COUNT);
    
    /* 分配命令词链表 */
    esp_err_t ret = esp_mn_commands_alloc(g_state.multinet, g_state.model_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "分配命令词链表失败: %d", ret);
        return -2;
    }
    
    /* 注册所有命令词 */
    int success_count = 0;
    for (size_t i = 0; i < COMMAND_WORDS_COUNT; i++) {
        ret = esp_mn_commands_add(g_command_words[i].command_id, 
                                   g_command_words[i].phrase);
        if (ret == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGW(TAG, "注册命令词失败: %s", g_command_words[i].desc);
        }
    }
    
    /* 更新语言模型 */
    esp_mn_error_t *error = esp_mn_commands_update();
    if (error) {
        ESP_LOGW(TAG, "部分命令词无法解析");
        esp_mn_active_commands_print();
    }
    
    ESP_LOGI(TAG, "命令词注册完成: 成功 %d/%d", success_count, COMMAND_WORDS_COUNT);
    return success_count;
}

int voice_commands_clear_phrases(void)
{
    if (!g_state.initialized) {
        return -1;
    }
    
    g_state.multinet->clean(g_state.model_data);
    return 0;
}

void voice_commands_set_threshold(float threshold)
{
    if (!g_state.initialized) {
        return;
    }
    
    g_state.threshold = threshold;
    g_state.multinet->set_det_threshold(g_state.model_data, threshold);
}

float voice_commands_get_threshold(void)
{
    return g_state.threshold;
}

int voice_commands_get_chunk_size(void)
{
    if (!g_state.initialized) {
        return 0;
    }
    return g_state.multinet->get_samp_chunksize(g_state.model_data);
}

int voice_commands_get_sample_rate(void)
{
    if (!g_state.initialized) {
        return 0;
    }
    return g_state.multinet->get_samp_rate(g_state.model_data);
}

bool voice_commands_process(int16_t *samples)
{
    if (!g_state.initialized || !g_state.running || !samples) {
        return false;
    }
    
    /* 执行识别 */
    esp_mn_state_t state = g_state.multinet->detect(g_state.model_data, samples);
    
    if (state == ESP_MN_STATE_DETECTED) {
        /* 获取识别结果 */
        esp_mn_results_t *results = g_state.multinet->get_results(g_state.model_data);
        
        if (results && results->num > 0) {
            g_state.last_result.command_id = results->command_id[0];
            g_state.last_result.confidence = results->prob[0];
            g_state.last_result.phrase = results->string;
            
            ESP_LOGI(TAG, "识别到命令: ID=%d, 置信度=%.2f, 短语=%s",
                     g_state.last_result.command_id,
                     g_state.last_result.confidence,
                     g_state.last_result.phrase ? g_state.last_result.phrase : "N/A");
            
            /* 调用回调 */
            if (g_state.callback) {
                g_state.callback(&g_state.last_result, g_state.user_data);
            }
            
            return true;
        }
    } else if (state == ESP_MN_STATE_TIMEOUT) {
        ESP_LOGD(TAG, "命令等待超时");
    }
    
    return false;
}

const voice_command_result_t* voice_commands_get_result(void)
{
    return &g_state.last_result;
}