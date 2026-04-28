/**
 * @file command_handler.h
 * @brief 命令处理器头文件
 * 
 * 处理识别到的语音命令，执行相应的控制动作
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "command_words.h"
#include "priority_arbiter.h"  /* 引入 system_mode_t 定义 */

/* ================================================================
 * 控制参数结构
 * ================================================================ */

/**
 * @brief 灯光控制参数
 */
typedef struct {
    int brightness;         /* 亮度 (0-100%) */
    int color_temp;         /* 色温 (2700-6500K) */
    bool main_light_on;     /* 主灯开关 */
    bool aux_light_on;      /* 辅助灯开关 */
} light_control_t;

/**
 * @brief 场景预设参数
 */
typedef struct {
    const char *name;       /* 场景名称 */
    light_control_t params; /* 控制参数 */
} scene_preset_t;

/* ================================================================
 * 系统状态结构
 * ================================================================ */

/**
 * @brief 系统状态
 */
typedef struct {
    system_mode_t mode;         /* 当前模式 */
    int current_scene;          /* 当前场景 ID */
    light_control_t light;      /* 灯光状态 */
    bool voice_active;          /* 语音命令激活状态 */
    uint32_t last_command_time; /* 最后命令时间戳 */
} system_state_t;

/* ================================================================
 * 命令执行结果
 * ================================================================ */

typedef enum {
    CMD_RESULT_OK,              /* 执行成功 */
    CMD_RESULT_FAILED,          /* 执行失败 */
    CMD_RESULT_INVALID,         /* 无效命令 */
    CMD_RESULT_BUSY,            /* 系统忙 */
    CMD_RESULT_TIMEOUT,         /* 超时 */
} command_result_t;

/* ================================================================
 * 命令回调类型
 * ================================================================ */

/**
 * @brief 命令执行回调
 * @param command_id 命令 ID
 * @param result 执行结果
 * @param user_data 用户数据
 */
typedef void (*command_callback_t)(int command_id, command_result_t result, void *user_data);

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化命令处理器
 * @return 0 成功, <0 失败
 */
int command_handler_init(void);

/**
 * @brief 处理语音命令
 * @param command_id 命令 ID
 * @return 执行结果
 */
command_result_t command_handler_process(int command_id);

/**
 * @brief 设置命令回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void command_handler_set_callback(command_callback_t callback, void *user_data);

/**
 * @brief 获取系统状态
 * @return 系统状态指针
 */
const system_state_t* command_handler_get_state(void);

/**
 * @brief 设置系统模式
 * @param mode 目标模式
 * @return 0 成功
 */
int command_handler_set_mode(system_mode_t mode);

/**
 * @brief 恢复自动控制
 * @return 0 成功
 */
int command_handler_restore_auto(void);

/**
 * @brief 检查语音命令超时
 * @param timeout_ms 超时时间 (毫秒)
 * @return true 已超时
 */
bool command_handler_check_timeout(uint32_t timeout_ms);

/**
 * @brief 应用场景预设
 * @param scene_id 场景 ID
 * @return 0 成功
 */
int command_handler_apply_scene(int scene_id);

/**
 * @brief 获取场景预设
 * @param scene_id 场景 ID
 * @return 场景预设指针, NULL 表示无效
 */
const scene_preset_t* command_handler_get_scene_preset(int scene_id);

#endif /* COMMAND_HANDLER_H */
