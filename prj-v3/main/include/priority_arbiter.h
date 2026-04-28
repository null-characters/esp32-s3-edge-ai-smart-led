/**
 * @file priority_arbiter.h
 * @brief 双轨优先级仲裁器接口
 * 
 * 仲裁策略: 语音 > 自动 > 默认
 * - 语音命令: 最高优先级，立即执行
 * - 自动控制: 雷达/TFLM 融合决策
 * - 默认行为: 无输入时的安全状态
 */

#ifndef PRIORITY_ARBITER_H
#define PRIORITY_ARBITER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 控制决策来源
 */
typedef enum {
    DECISION_SOURCE_NONE = 0,      /* 无决策 */
    DECISION_SOURCE_VOICE = 1,     /* 语音命令 */
    DECISION_SOURCE_AUTO = 2,      /* 自动控制 (雷达+TFLM) */
    DECISION_SOURCE_DEFAULT = 3,   /* 默认行为 */
} decision_source_t;

/**
 * @brief 系统运行模式
 */
typedef enum {
    MODE_AUTO = 0,      /* 自动模式: 雷达+TFLM 控制 */
    MODE_MANUAL = 1,    /* 手动模式: 用户手动控制 */
    MODE_VOICE = 2,     /* 语音模式: 语音命令控制 */
    MODE_SLEEP = 3,     /* 睡眠模式: 低功耗待机 */
} system_mode_t;

/**
 * @brief LED 控制决策
 */
typedef struct {
    bool power;             /* 开关状态 */
    uint8_t brightness;     /* 亮度 (0-100) */
    uint16_t color_temp;    /* 色温 (2700-6500K) */
    uint8_t scene_id;       /* 场景 ID */
    decision_source_t source; /* 决策来源 */
    uint64_t timestamp_ms;  /* 时间戳 */
} led_decision_t;

/**
 * @brief 语音命令输入
 */
typedef struct {
    bool valid;             /* 命令是否有效 */
    int command_id;         /* 命令 ID */
    const char *phrase;     /* 命令短语 */
    float confidence;       /* 置信度 */
    led_decision_t decision; /* 对应的控制决策 */
} voice_input_t;

/**
 * @brief 自动控制输入
 */
typedef struct {
    bool valid;             /* 决策是否有效 */
    float radar_distance;   /* 雷达距离 */
    float radar_energy;     /* 雷达能量 */
    float tflm_confidence;  /* TFLM 置信度 */
    uint8_t scene_id;       /* 识别的场景 */
    led_decision_t decision; /* 对应的控制决策 */
} auto_input_t;

/**
 * @brief 仲裁器配置
 */
typedef struct {
    uint32_t manual_timeout_ms;   /* 手动模式超时 (默认 30000ms) */
    bool persist_state;           /* 是否持久化状态到 NVS */
    system_mode_t default_mode;   /* 默认启动模式 */
} arbiter_config_t;

/**
 * @brief 仲裁器状态回调
 */
typedef void (*arbiter_state_callback_t)(system_mode_t mode, const led_decision_t *decision, void *user_data);

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化仲裁器
 * @param config 配置参数
 * @return ESP_OK 成功
 */
esp_err_t priority_arbiter_init(const arbiter_config_t *config);

/**
 * @brief 释放仲裁器
 */
void priority_arbiter_deinit(void);

/**
 * @brief 执行仲裁决策
 * @param voice 语音命令输入 (可为 NULL)
 * @param auto_input 自动控制输入 (可为 NULL)
 * @param output 输出决策
 * @return 决策来源
 */
decision_source_t priority_arbiter_decide(const voice_input_t *voice, 
                                          const auto_input_t *auto_input,
                                          led_decision_t *output);

/**
 * @brief 提交语音命令
 * @param command_id 命令 ID
 * @param confidence 置信度
 * @return ESP_OK 成功
 */
esp_err_t priority_arbiter_submit_voice(int command_id, float confidence);

/**
 * @brief 提交自动控制决策
 * @param scene_id 场景 ID
 * @param confidence 置信度
 * @return ESP_OK 成功
 */
esp_err_t priority_arbiter_submit_auto(uint8_t scene_id, float confidence);

/**
 * @brief 获取当前运行模式
 * @return 当前模式
 */
system_mode_t priority_arbiter_get_mode(void);

/**
 * @brief 获取当前决策
 * @param output 输出决策
 * @return true 有有效决策
 */
bool priority_arbiter_get_decision(led_decision_t *output);

/**
 * @brief 设置状态回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void priority_arbiter_set_callback(arbiter_state_callback_t callback, void *user_data);

/**
 * @brief 强制切换模式
 * @param mode 目标模式
 * @return ESP_OK 成功
 */
esp_err_t priority_arbiter_set_mode(system_mode_t mode);

/**
 * @brief 重置超时计时器
 */
void priority_arbiter_reset_timeout(void);

/**
 * @brief 获取剩余超时时间
 * @return 剩余毫秒数，-1 表示无超时
 */
int32_t priority_arbiter_get_timeout_remaining(void);

#endif /* PRIORITY_ARBITER_H */