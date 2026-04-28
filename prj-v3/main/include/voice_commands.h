/**
 * @file voice_commands.h
 * @brief 语音命令识别模块头文件 (ESP-SR MultiNet 封装)
 */

#ifndef VOICE_COMMANDS_H
#define VOICE_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include "command_words.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 命令识别结果
 */
typedef struct {
    int command_id;             /* 识别到的命令 ID */
    float confidence;           /* 置信度 (0.0 - 1.0) */
    const char *phrase;         /* 识别到的短语 */
} voice_command_result_t;

/**
 * @brief 命令识别回调
 * @param result 识别结果
 * @param user_data 用户数据
 */
typedef void (*voice_command_callback_t)(const voice_command_result_t *result, void *user_data);

/**
 * @brief 语音命令配置
 */
typedef struct {
    const char *model_name;             /* MultiNet 模型名称 */
    float threshold;                    /* 识别阈值 */
    int timeout_ms;                     /* 命令等待超时 (毫秒) */
    voice_command_callback_t callback;  /* 识别回调 */
    void *user_data;                    /* 用户数据 */
} voice_commands_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化语音命令识别模块
 * @param config 配置参数
 * @return 0 成功, <0 失败
 */
int voice_commands_init(const voice_commands_config_t *config);

/**
 * @brief 启动命令识别
 * @return 0 成功
 */
int voice_commands_start(void);

/**
 * @brief 停止命令识别
 * @return 0 成功
 */
int voice_commands_stop(void);

/**
 * @brief 添加自定义命令词
 * @param command_id 命令 ID
 * @param phrase 命令词拼音
 * @return 0 成功
 */
int voice_commands_add_phrase(int command_id, const char *phrase);

/**
 * @brief 清除所有自定义命令词
 * @return 0 成功
 */
int voice_commands_clear_phrases(void);

/**
 * @brief 注册所有预定义命令词 (50+ 条)
 * @return 成功注册的命令词数量, <0 失败
 */
int voice_commands_register_all(void);

/**
 * @brief 设置识别阈值
 * @param threshold 阈值 (0.0 - 1.0)
 */
void voice_commands_set_threshold(float threshold);

/**
 * @brief 获取识别阈值
 * @return 当前阈值
 */
float voice_commands_get_threshold(void);

/**
 * @brief 获取需要的音频采样数
 * @return 采样数
 */
int voice_commands_get_chunk_size(void);

/**
 * @brief 获取采样率
 * @return 采样率 (Hz)
 */
int voice_commands_get_sample_rate(void);

/**
 * @brief 处理音频数据 (识别命令)
 * @param samples 音频采样数据 (16-bit signed)
 * @return true 检测到命令
 */
bool voice_commands_process(int16_t *samples);

/**
 * @brief 获取最近识别结果
 * @return 识别结果指针
 */
const voice_command_result_t* voice_commands_get_result(void);

#endif /* VOICE_COMMANDS_H */
