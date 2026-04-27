/*
 * LD2410 毫米波雷达 UART 通信驱动
 * 基于 ESPHome 自定义组件
 * 
 * 来源: https://github.com/rainchi/ESPHome-LD2410
 * 协议: UART 256000 bps, 8N1
 */

#ifndef LD2410_UART_H
#define LD2410_UART_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * UART 配置
 * ================================================================ */
#define LD2410_BAUD_RATE        256000
#define LD2410_UART_TIMEOUT_MS 100

/* ================================================================
 * 帧格式定义
 * ================================================================ */
#define FRAME_HEADER_0         0xFD
#define FRAME_HEADER_1         0xFC
#define FRAME_HEADER_2         0xFB
#define FRAME_HEADER_3         0xFA
#define FRAME_FOOTER_0         0x04
#define FRAME_FOOTER_1         0x03
#define FRAME_FOOTER_2         0x02
#define FRAME_FOOTER_3         0x01

/* ================================================================
 * 命令类型
 * ================================================================ */
typedef enum {
    CMD_READ_PARAMS        = 0x61,   /* 读取参数 */
    CMD_WRITE_PARAMS       = 0x62,   /* 写入参数 */
    CMD_ENABLE_ENGINEERING = 0x62,   /* 启用工程模式 */
    CMD_DISABLE_ENGINEERING= 0x62,   /* 禁用工程模式 */
    CMD_FACTORY_RESET      = 0x63,   /* 恢复出厂设置 */
    CMD_RESTART            = 0x64,   /* 重启模块 */
    CMD_READ_VERSION       = 0x65,   /* 读取固件版本 */
    CMD_SET_DISTANCE_GATE  = 0x62,   /* 设置距离门 */
    CMD_SET_SENSITIVITY    = 0x62,   /* 设置灵敏度 */
    CMD_SET_TIMEOUT        = 0x62,   /* 设置超时时间 */
} ld2410_cmd_t;

/* ================================================================
 * 目标状态
 * ================================================================ */
typedef enum {
    TARGET_NONE    = 0,    /* 无目标 */
    TARGET_STILL   = 1,    /* 静止目标 */
    TARGET_MOVING  = 2,    /* 运动目标 */
} ld2410_target_state_t;

/* ================================================================
 * 数据结构
 * ================================================================ */

/* 雷达输出数据 */
typedef struct {
    uint8_t  target_state;      /* 目标状态 */
    uint8_t  moving_target;      /* 运动目标标志 */
    uint8_t  still_target;       /* 静止目标标志 */
    uint16_t moving_distance;    /* 运动目标距离 (cm) */
    uint16_t still_distance;     /* 静止目标距离 (cm) */
    uint8_t  moving_energy;     /* 运动目标能量 */
    uint8_t  still_energy;      /* 静止目标能量 */
    uint16_t detection_distance; /* 检测距离 (cm) */
    uint8_t  gate_energy[9];    /* 9个距离门能量值 G0-G8 */
} ld2410_data_t;

/* 模块参数 */
typedef struct {
    uint8_t  max_move_gate;     /* 最大运动距离门 */
    uint8_t  max_still_gate;    /* 最大静止距离门 */
    uint16_t timeout;           /* 无目标超时时间 (s) */
    uint8_t  move_threshold[9]; /* 运动阈值 G0-G8 */
    uint8_t  still_threshold[9]; /* 静止阈值 G0-G8 */
} ld2410_params_t;

/* 固件版本 */
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} ld2410_version_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 LD2410 模块
 * @return 0=成功, <0=错误码
 */
int ld2410_init(void);

/**
 * @brief 读取雷达数据（非阻塞）
 * @param data 输出数据结构
 * @return 0=成功, <0=错误码
 */
int ld2410_read(ld2410_data_t *data);

/**
 * @brief 启用工程模式
 * @return 0=成功
 */
int ld2410_enable_engineering_mode(void);

/**
 * @brief 禁用工程模式
 * @return 0=成功
 */
int ld2410_disable_engineering_mode(void);

/**
 * @brief 读取模块参数
 * @param params 输出参数结构
 * @return 0=成功
 */
int ld2410_read_params(ld2410_params_t *params);

/**
 * @brief 设置距离门
 * @param max_move_gate 最大运动距离门 (0-8)
 * @param max_still_gate 最大静止距离门 (0-8)
 * @return 0=成功
 */
int ld2410_set_distance_gate(uint8_t max_move_gate, uint8_t max_still_gate);

/**
 * @brief 设置灵敏度阈值
 * @param gate 距离门号 (0-8)
 * @param move_threshold 运动阈值 (0-100)
 * @param still_threshold 静止阈值 (0-100)
 * @return 0=成功
 */
int ld2410_set_threshold(uint8_t gate, uint8_t move_threshold, uint8_t still_threshold);

/**
 * @brief 设置超时时间
 * @param timeout 超时秒数
 * @return 0=成功
 */
int ld2410_set_timeout(uint16_t timeout);

/**
 * @brief 读取固件版本
 * @param version 输出版本结构
 * @return 0=成功
 */
int ld2410_read_version(ld2410_version_t *version);

/**
 * @brief 恢复出厂设置
 * @return 0=成功
 */
int ld2410_factory_reset(void);

/**
 * @brief 重启模块
 * @return 0=成功
 */
int ld2410_restart(void);

/**
 * @brief 获取特征向量（供 AI 使用）
 * @param data 雷达数据
 * @param features 输出特征向量 (8维)
 */
void ld2410_get_features(const ld2410_data_t *data, float *features);

#endif /* LD2410_UART_H */
