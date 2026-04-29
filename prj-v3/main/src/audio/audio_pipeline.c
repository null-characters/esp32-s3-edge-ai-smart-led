/**
 * @file audio_pipeline.c
 * @brief 音频处理流水线实现
 * 
 * 协调 I2S → AFE → WakeNet → MultiNet → 命令处理的完整流水线
 */

#include "audio_pipeline.h"
#include "audio_afe.h"
#include "voice_commands.h"
#include "command_handler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "AUDIO_PIPE";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef enum {
    STATE_IDLE,         /* 空闲状态，等待唤醒 */
    STATE_WAKE_WAIT,    /* 已唤醒，等待命令 */
    STATE_COMMAND,      /* 命令识别中 */
} pipeline_state_enum_t;

static struct {
    bool initialized;
    bool running;
    pipeline_state_enum_t state;
    
    /* 配置 */
    int wake_sensitivity;
    float wake_threshold;
    uint32_t command_timeout_ms;    /**< 命令等待超时时间 (ms) */
    
    /* 回调 */
    audio_event_callback_t event_callback;
    void *user_data;
    
    /* 统计 */
    int wake_count;
    int command_count;
    int64_t last_wake_time_ms;
} g_pipe = {
    .initialized = false,
    .running = false,
    .state = STATE_IDLE,
    .wake_sensitivity = 3,
    .wake_threshold = 0.65f,
    .command_timeout_ms = 5000,  /* 默认 5 秒 */
};

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 获取当前时间戳 (毫秒)
 */
static int64_t get_timestamp_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/**
 * @brief 发送事件到回调
 */
static void send_event(audio_event_type_t type, int wake_index, int cmd_id, 
                       const char *phrase, float volume)
{
    if (!g_pipe.event_callback) {
        return;
    }
    
    audio_event_t event = {
        .type = type,
        .wake_word_index = wake_index,
        .command_id = cmd_id,
        .command_phrase = phrase,
        .volume = volume,
        .timestamp_ms = get_timestamp_ms(),
    };
    
    g_pipe.event_callback(&event, g_pipe.user_data);
}

/**
 * @brief AFE 结果回调
 */
static void afe_result_handler(const audio_afe_result_t *result, void *user_data)
{
    if (!result) {
        return;
    }
    
    /* 状态机处理 */
    switch (g_pipe.state) {
        case STATE_IDLE:
            /* 检测唤醒词 */
            if (result->wake_word_detected) {
                ESP_LOGI(TAG, "唤醒词检测! 索引=%d, 音量=%.1fdB",
                         result->wake_word_index, result->volume);
                
                g_pipe.state = STATE_WAKE_WAIT;
                g_pipe.wake_count++;
                g_pipe.last_wake_time_ms = get_timestamp_ms();
                
                /* 发送唤醒事件 */
                send_event(AUDIO_EVENT_WAKE_WORD, result->wake_word_index, 
                          0, NULL, result->volume);
                
                /* 重置 AFE 状态 */
                audio_afe_reset();
            }
            break;
            
        case STATE_WAKE_WAIT:
            /* 等待命令 (使用配置的超时时间) */
            if ((get_timestamp_ms() - g_pipe.last_wake_time_ms) > g_pipe.command_timeout_ms) {
                ESP_LOGI(TAG, "命令等待超时 (%lums)，返回空闲", g_pipe.command_timeout_ms);
                g_pipe.state = STATE_IDLE;
                break;
            }
            
            /* 检测语音活动 */
            if (result->is_speech) {
                g_pipe.state = STATE_COMMAND;
                send_event(AUDIO_EVENT_VAD_SPEECH, 0, 0, NULL, result->volume);
            }
            break;
            
        case STATE_COMMAND:
            /* 命令识别中 */
            if (!result->is_speech) {
                /* 语音结束，处理命令 */
                ESP_LOGD(TAG, "语音结束，处理命令");
                
                /* 获取命令识别结果 */
                const voice_command_result_t *cmd_result = voice_commands_get_result();
                if (cmd_result && cmd_result->command_id > 0) {
                    ESP_LOGI(TAG, "执行命令: ID=%d, 置信度=%.2f", 
                             cmd_result->command_id, cmd_result->confidence);
                    
                    /* 执行命令 */
                    command_handler_process(cmd_result->command_id);
                    
                    /* 发送命令事件 */
                    send_event(AUDIO_EVENT_COMMAND, 0, cmd_result->command_id,
                              cmd_result->phrase, result->volume);
                    
                    g_pipe.command_count++;
                }
                
                g_pipe.state = STATE_IDLE;
                send_event(AUDIO_EVENT_VAD_SILENCE, 0, 0, NULL, result->volume);
            }
            break;
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

int audio_pipeline_init(void)
{
    if (g_pipe.initialized) {
        ESP_LOGW(TAG, "流水线已初始化");
        return 0;
    }
    
    ESP_LOGI(TAG, "初始化音频流水线");
    
    /* 初始化 AFE */
    audio_afe_config_t afe_cfg = audio_afe_default_config;
    afe_cfg.enable_wakenet = true;
    afe_cfg.enable_multinet = true;
    afe_cfg.wakenet_model = "wn_xiaobaitong";
    
    int ret = audio_afe_init(&afe_cfg);
    if (ret != 0) {
        ESP_LOGE(TAG, "AFE 初始化失败: %d", ret);
        return -1;
    }
    
    /* 初始化命令词 */
    voice_commands_config_t cmd_cfg = {
        .model_name = "mn2_cn",
        .threshold = 0.5f,
        .timeout_ms = 5000,
        .callback = NULL,
        .user_data = NULL,
    };
    ret = voice_commands_init(&cmd_cfg);
    if (ret != 0) {
        ESP_LOGE(TAG, "命令词初始化失败: %d", ret);
        audio_afe_deinit();
        return -2;
    }
    
    /* 初始化命令处理器 */
    ret = command_handler_init();
    if (ret != 0) {
        ESP_LOGE(TAG, "命令处理器初始化失败: %d", ret);
        voice_commands_deinit();
        audio_afe_deinit();
        return -3;
    }
    
    /* 设置 AFE 回调 */
    audio_afe_set_callback(afe_result_handler, NULL);
    
    /* 设置唤醒阈值 */
    audio_afe_set_wakenet_threshold(g_pipe.wake_threshold);
    
    g_pipe.initialized = true;
    g_pipe.state = STATE_IDLE;
    
    ESP_LOGI(TAG, "音频流水线初始化完成");
    return 0;
}

void audio_pipeline_deinit(void)
{
    if (!g_pipe.initialized) {
        return;
    }
    
    audio_pipeline_stop();
    voice_commands_deinit();
    audio_afe_deinit();
    
    /* 重置所有状态 */
    memset(&g_pipe, 0, sizeof(g_pipe));
    g_pipe.state = STATE_IDLE;
    
    ESP_LOGI(TAG, "音频流水线已释放");
}

int audio_pipeline_start(void)
{
    if (!g_pipe.initialized) {
        ESP_LOGE(TAG, "流水线未初始化");
        return -1;
    }
    
    if (g_pipe.running) {
        ESP_LOGW(TAG, "流水线已运行");
        return 0;
    }
    
    ESP_LOGI(TAG, "启动音频流水线");
    
    /* 启动 AFE */
    int ret = audio_afe_start();
    if (ret != 0) {
        ESP_LOGE(TAG, "AFE 启动失败: %d", ret);
        return -2;
    }
    
    g_pipe.running = true;
    g_pipe.state = STATE_IDLE;
    
    ESP_LOGI(TAG, "音频流水线已启动，等待唤醒词...");
    return 0;
}

void audio_pipeline_stop(void)
{
    if (!g_pipe.running) {
        return;
    }
    
    ESP_LOGI(TAG, "停止音频流水线");
    
    g_pipe.running = false;
    audio_afe_stop();
    
    g_pipe.state = STATE_IDLE;
    ESP_LOGI(TAG, "音频流水线已停止");
}

void audio_pipeline_set_event_callback(audio_event_callback_t callback, void *user_data)
{
    if (!callback) {
        g_pipe.event_callback = NULL;
        g_pipe.user_data = NULL;
        return;
    }
    g_pipe.event_callback = callback;
    g_pipe.user_data = user_data;
}

int audio_pipeline_set_wake_sensitivity(int sensitivity)
{
    /* 灵敏度 1-5 映射到阈值 0.85-0.45 */
    if (sensitivity < 1) sensitivity = 1;
    if (sensitivity > 5) sensitivity = 5;
    
    g_pipe.wake_sensitivity = sensitivity;
    g_pipe.wake_threshold = 0.95f - (sensitivity - 1) * 0.1f;
    
    if (g_pipe.initialized) {
        audio_afe_set_wakenet_threshold(g_pipe.wake_threshold);
    }
    
    ESP_LOGI(TAG, "唤醒灵敏度: %d (阈值=%.2f)", sensitivity, g_pipe.wake_threshold);
    return 0;
}

bool audio_pipeline_is_running(void)
{
    if (!g_pipe.initialized) {
        return false;
    }
    return g_pipe.running;
}

void audio_pipeline_set_command_timeout(uint32_t timeout_ms)
{
    if (timeout_ms < 1000) timeout_ms = 1000;   /* 最小 1 秒 */
    if (timeout_ms > 30000) timeout_ms = 30000; /* 最大 30 秒 */
    
    g_pipe.command_timeout_ms = timeout_ms;
    ESP_LOGI(TAG, "命令等待超时设置为 %lums", timeout_ms);
}

uint32_t audio_pipeline_get_command_timeout(void)
{
    return g_pipe.command_timeout_ms;
}
