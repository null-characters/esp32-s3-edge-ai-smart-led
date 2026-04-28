/**
 * @file ld2410_driver.h
 * @brief LD2410 毫米波雷达驱动头文件 (ESP-IDF 版本)
 * 
 * 功能：
 * - UART 通信 (256000 波特率)
 * - 数据帧解析
 * - 特征提取
 * 
 * 硬件接口：UART1 (TX=GPIO8, RX=GPIO9)
 */

#ifndef LD2410_DRIVER_H
#define LD2410_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* LD2410 帧头帧尾定义 */
#define LD2410_FRAME_HEADER_0   0xF4
#define LD2410_FRAME_HEADER_1   0xF3
#define LD2410_FRAME_HEADER_2   0xF2
#define LD2410_FRAME_HEADER_3   0xF1
#define LD2410_FRAME_TAIL_0     0xF8
#define LD2410_FRAME_TAIL_1     0xF9
#define LD2410_FRAME_TAIL_2     0xFA
#define LD2410_FRAME_TAIL_3     0xFB

/* 目标状态定义 */
#define LD2410_TARGET_NONE      0   /* 无目标 */
#define LD2410_TARGET_STATIC    1   /* 静止目标 */
#define LD2410_TARGET_MOVING    2   /* 运动目标 */

/* 雷达原始数据结构 */
typedef struct {
    uint8_t target_state;       /* 目标状态: 0=无, 1=静止, 2=运动 */
    uint8_t target_count;       /* 目标数量 */
    float distance;             /* 目标距离 (m), 0-8m */
    float velocity;             /* 目标速度 (m/s), -5~+5 */
    float energy;               /* 目标能量/SNR */
    int64_t timestamp;          /* 数据时间戳 (us) */
} ld2410_raw_data_t;

/* 雷达特征向量 (8维) */
typedef struct {
    float distance_norm;        /* 归一化距离 [0, 1] */
    float velocity_norm;        /* 归一化速度 [-1, 1] */
    float energy_norm;          /* 归一化能量 [0, 1] */
    float target_count;         /* 目标数量 */
    float motion_amplitude;     /* 运动幅度 */
    float motion_period;        /* 运动周期 (Hz) */
    float distance_variance;    /* 距离方差 (多人检测) */
    float energy_trend;         /* 能量趋势 (进入/离开) */
} radar_features_t;

/* 雷达历史数据缓冲 */
#define RADAR_HISTORY_SIZE  60  /* 60秒历史数据 */

typedef struct {
    radar_features_t history[RADAR_HISTORY_SIZE];
    int head;
    int count;
    int64_t last_update;
} radar_history_t;

/**
 * @brief 初始化 LD2410 雷达驱动
 * @return 0 成功, <0 失败
 */
int ld2410_init(void);

/**
 * @brief 读取雷达原始数据
 * @param data 输出数据指针
 * @return 0 成功, <0 无数据或失败
 */
int ld2410_read_raw(ld2410_raw_data_t *data);

/**
 * @brief 获取雷达特征向量 (供 AI 使用)
 * @param features 输出特征向量
 * @return 0 成功
 */
int ld2410_get_features(radar_features_t *features);

/**
 * @brief 更新雷达历史数据
 * @param features 当前特征
 */
void ld2410_update_history(const radar_features_t *features);

/**
 * @brief 获取雷达历史统计特征
 * @param variance 输出距离方差
 * @param period 输出运动周期
 * @param trend 输出能量趋势
 */
void ld2410_get_history_stats(float *variance, float *period, float *trend);

/**
 * @brief 检测呼吸模式 (静坐检测)
 * @return true 检测到呼吸模式
 */
bool ld2410_detect_breathing(void);

/**
 * @brief 检测风扇模式 (风扇摇头)
 * @return true 检测到风扇模式
 */
bool ld2410_detect_fan(void);

#endif /* LD2410_DRIVER_H */
