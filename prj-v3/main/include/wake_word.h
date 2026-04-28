/**
 * @file wake_word.h
 * @brief 唤醒词检测模块头文件 (ESP-SR 封装)
 * 
 * 功能：
 * - 封装 ESP-SR WakeNet API
 * - 管理唤醒状态
 * - 提供事件通知机制
 */

#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <stdint.h>
#include <stdbool.h>

/* 唤醒词事件类型 */
typedef enum {
    WAKE_WORD_NONE,         /* 无事件 */
    WAKE_WORD_DETECTED,     /* 唤醒词检测到 */
    WAKE_WORD_TIMEOUT,      /* 命令等待超时 */
} wake_word_event_t;

/* 唤醒词回调函数类型 */
typedef void (*wake_word_callback_t)(wake_word_event_t event, void *user_data);

/* 唤醒词配置 */
typedef struct {
    const char *model_name;         /* 唤醒词模型名称 */
    float threshold;                /* 唤醒阈值 (0.0 - 1.0) */
    wake_word_callback_t callback;  /* 事件回调 */
    void *user_data;                /* 用户数据 */
} wake_word_config_t;

/**
 * @brief 初始化唤醒词检测模块
 * @param config 配置参数
 * @return 0 成功, <0 失败
 */
int wake_word_init(const wake_word_config_t *config);

/**
 * @brief 启动唤醒词检测
 * @return 0 成功
 */
int wake_word_start(void);

/**
 * @brief 停止唤醒词检测
 * @return 0 成功
 */
int wake_word_stop(void);

/**
 * @brief 释放唤醒词模块资源
 */
void wake_word_deinit(void);

/**
 * @brief 获取当前唤醒状态
 * @return true 已唤醒, false 未唤醒
 */
bool wake_word_is_awake(void);

/**
 * @brief 重置唤醒状态 (退出命令模式)
 */
void wake_word_reset(void);

/**
 * @brief 设置唤醒阈值
 * @param threshold 阈值 (0.0 - 1.0)
 */
void wake_word_set_threshold(float threshold);

/**
 * @brief 获取唤醒阈值
 * @return 当前阈值
 */
float wake_word_get_threshold(void);

#endif /* WAKE_WORD_H */