/**
 * @file audio_pipeline.h
 * @brief 音频处理流水线接口
 * 
 * 协调 I2S → AFE → WakeNet → MultiNet → 命令处理的完整流水线
 */

#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include "audio_afe.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 语音事件类型
 */
typedef enum {
    AUDIO_EVENT_NONE = 0,
    AUDIO_EVENT_WAKE_WORD,      /* 唤醒词检测 */
    AUDIO_EVENT_COMMAND,        /* 命令词识别 */
    AUDIO_EVENT_VAD_SPEECH,     /* 语音活动开始 */
    AUDIO_EVENT_VAD_SILENCE,    /* 语音活动结束 */
} audio_event_type_t;

/**
 * @brief 语音事件数据
 */
typedef struct {
    audio_event_type_t type;
    int wake_word_index;        /* 唤醒词索引 (1-based) */
    int command_id;             /* 命令词 ID */
    const char *command_phrase; /* 命令词短语 */
    float volume;               /* 音量 (dB) */
    int64_t timestamp_ms;       /* 时间戳 */
} audio_event_t;

/**
 * @brief 语音事件回调
 */
typedef void (*audio_event_callback_t)(const audio_event_t *event, void *user_data);

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化音频流水线
 * @return 0 成功, <0 失败
 */
int audio_pipeline_init(void);

/**
 * @brief 释放音频流水线
 */
void audio_pipeline_deinit(void);

/**
 * @brief 启动音频流水线
 * @return 0 成功, <0 失败
 */
int audio_pipeline_start(void);

/**
 * @brief 停止音频流水线
 */
void audio_pipeline_stop(void);

/**
 * @brief 设置语音事件回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void audio_pipeline_set_event_callback(audio_event_callback_t callback, void *user_data);

/**
 * @brief 设置唤醒灵敏度
 * @param sensitivity 灵敏度级别 (1-5, 5 最灵敏)
 * @return 0 成功
 */
int audio_pipeline_set_wake_sensitivity(int sensitivity);

/**
 * @brief 获取当前状态
 * @return true 正在运行
 */
bool audio_pipeline_is_running(void);

/**
 * @brief 设置命令等待超时时间
 * @param timeout_ms 超时时间 (毫秒, 范围 1000-30000)
 */
void audio_pipeline_set_command_timeout(uint32_t timeout_ms);

/**
 * @brief 获取命令等待超时时间
 * @return 超时时间 (毫秒)
 */
uint32_t audio_pipeline_get_command_timeout(void);

#endif /* AUDIO_PIPELINE_H */