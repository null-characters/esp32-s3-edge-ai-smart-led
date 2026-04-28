/**
 * @file vad_detector.h
 * @brief VAD (Voice Activity Detection) 检测模块
 * 
 * 检测人声活动，用于动态路由麦克风流：
 * - 有人声 → ESP-SR 语音识别
 * - 无人声 → TFLM 多模态推理
 */

#ifndef VAD_DETECTOR_H
#define VAD_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 配置常量
 * ================================================================ */

#define VAD_SAMPLE_RATE         16000   /* 采样率 */
#define VAD_FRAME_SIZE_MS       20      /* 帧大小 20ms */
#define VAD_FRAME_SAMPLES       (VAD_SAMPLE_RATE * VAD_FRAME_SIZE_MS / 1000)  /* 320 样本 */

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief VAD 状态
 */
typedef enum {
    VAD_STATE_SILENCE = 0,      /* 静音/无人声 */
    VAD_STATE_VOICE = 1,        /* 有人声 */
} vad_state_t;

/**
 * @brief VAD 检测结果
 */
typedef struct {
    vad_state_t state;          /* 当前状态 */
    float energy;               /* 能量值 */
    float probability;          /* 人声概率 (0.0 - 1.0) */
    bool transition;            /* 是否发生状态转换 */
} vad_result_t;

/**
 * @brief VAD 状态变化回调
 * @param state 新状态
 * @param result 检测结果
 * @param user_data 用户数据
 */
typedef void (*vad_state_callback_t)(vad_state_t state, const vad_result_t *result, void *user_data);

/**
 * @brief VAD 配置
 */
typedef struct {
    float energy_threshold;         /* 能量阈值 */
    float voice_prob_threshold;     /* 人声概率阈值 */
    int hangover_frames;            /* 拖尾帧数 (防止频繁切换) */
    vad_state_callback_t callback;  /* 状态回调 */
    void *user_data;                /* 用户数据 */
} vad_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 VAD 检测模块
 * @param config 配置参数 (NULL 使用默认配置)
 * @return ESP_OK 成功
 */
esp_err_t vad_detector_init(const vad_config_t *config);

/**
 * @brief 释放 VAD 检测模块
 */
void vad_detector_deinit(void);

/**
 * @brief 处理音频帧，检测人声
 * @param samples 音频采样 (16-bit signed, VAD_FRAME_SAMPLES 个)
 * @param result 检测结果输出
 * @return ESP_OK 成功
 */
esp_err_t vad_detector_process(const int16_t *samples, vad_result_t *result);

/**
 * @brief 获取当前 VAD 状态
 * @return 当前状态
 */
vad_state_t vad_detector_get_state(void);

/**
 * @brief 重置 VAD 状态
 */
void vad_detector_reset(void);

/**
 * @brief 设置能量阈值
 * @param threshold 阈值
 */
void vad_detector_set_energy_threshold(float threshold);

/**
 * @brief 设置状态变化回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void vad_detector_set_callback(vad_state_callback_t callback, void *user_data);

/**
 * @brief 获取帧大小 (样本数)
 * @return 帧大小
 */
int vad_detector_get_frame_size(void);

#endif /* VAD_DETECTOR_H */
