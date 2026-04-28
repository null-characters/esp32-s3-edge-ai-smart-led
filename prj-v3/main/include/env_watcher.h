/**
 * @file env_watcher.h
 * @brief 环境变化监听模块
 * 
 * 监听雷达数据，检测环境变化（如人离开），
 * 触发 TTL 租约的环境变化释放
 */

#ifndef ENV_WATCHER_H
#define ENV_WATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 配置常量
 * ================================================================ */

#define ENV_ABSENT_THRESHOLD_MS     (10 * 60 * 1000)  /* 人离开阈值: 10 分钟 */
#define ENV_CHECK_INTERVAL_MS       5000              /* 检查间隔: 5 秒 */

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 环境状态
 */
typedef enum {
    ENV_STATE_UNKNOWN = 0,      /* 未知 */
    ENV_STATE_EMPTY = 1,        /* 无人 */
    ENV_STATE_OCCUPIED = 2,     /* 有人 */
} env_state_t;

/**
 * @brief 环境变化回调
 * @param state 当前环境状态
 * @param absent_ms 离开时长 (毫秒), 0 表示有人
 * @param user_data 用户数据
 */
typedef void (*env_change_callback_t)(env_state_t state, uint32_t absent_ms, void *user_data);

/**
 * @brief 环境监听配置
 */
typedef struct {
    uint32_t absent_threshold_ms;   /* 人离开阈值 (毫秒) */
    uint32_t check_interval_ms;     /* 检查间隔 (毫秒) */
    env_change_callback_t callback; /* 变化回调 */
    void *user_data;                /* 用户数据 */
} env_watcher_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化环境监听模块
 * @param config 配置参数 (NULL 使用默认配置)
 * @return ESP_OK 成功
 */
esp_err_t env_watcher_init(const env_watcher_config_t *config);

/**
 * @brief 释放环境监听模块
 */
void env_watcher_deinit(void);

/**
 * @brief 启动环境监听
 * @return ESP_OK 成功
 */
esp_err_t env_watcher_start(void);

/**
 * @brief 停止环境监听
 * @return ESP_OK 成功
 */
esp_err_t env_watcher_stop(void);

/**
 * @brief 更新雷达数据 (人存在状态)
 * @param presence 是否有人
 * @param distance 距离 (厘米)
 * @param energy 能量值
 * @return ESP_OK 成功
 */
esp_err_t env_watcher_update_radar(bool presence, float distance, float energy);

/**
 * @brief 获取当前环境状态
 * @return 环境状态
 */
env_state_t env_watcher_get_state(void);

/**
 * @brief 获取离开时长
 * @return 离开时长 (毫秒), 0 表示有人
 */
uint32_t env_watcher_get_absent_time_ms(void);

/**
 * @brief 设置环境变化回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void env_watcher_set_callback(env_change_callback_t callback, void *user_data);

#endif /* ENV_WATCHER_H */
