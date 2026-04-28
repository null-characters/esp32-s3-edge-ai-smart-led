/**
 * @file audio_router.h
 * @brief 音频路由器
 * 
 * 根据 VAD 检测结果动态路由麦克风流：
 * - 有人声 → ESP-SR 语音识别
 * - 无人声 → TFLM 多模态推理
 */

#ifndef AUDIO_ROUTER_H
#define AUDIO_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "vad_detector.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 音频路由目标
 */
typedef enum {
    AUDIO_ROUTE_NONE = 0,       /* 无路由 */
    AUDIO_ROUTE_ESP_SR = 1,     /* ESP-SR 语音识别 */
    AUDIO_ROUTE_TFLM = 2,       /* TFLM 多模态推理 */
} audio_route_target_t;

/**
 * @brief 音频数据回调
 * @param target 路由目标
 * @param samples 音频数据 (16-bit signed)
 * @param len 数据长度 (样本数)
 * @param user_data 用户数据
 */
typedef void (*audio_data_callback_t)(audio_route_target_t target, const int16_t *samples, int len, void *user_data);

/**
 * @brief 路由配置
 */
typedef struct {
    audio_data_callback_t esp_sr_callback;  /* ESP-SR 数据回调 */
    audio_data_callback_t tflm_callback;    /* TFLM 数据回调 */
    void *esp_sr_user_data;                 /* ESP-SR 用户数据 */
    void *tflm_user_data;                   /* TFLM 用户数据 */
} audio_router_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化音频路由器
 * @param config 配置参数
 * @return ESP_OK 成功
 */
esp_err_t audio_router_init(const audio_router_config_t *config);

/**
 * @brief 释放音频路由器
 */
void audio_router_deinit(void);

/**
 * @brief 路由音频数据
 * @param samples 音频数据
 * @param len 数据长度 (样本数)
 * @return 路由目标
 */
audio_route_target_t audio_router_route(const int16_t *samples, int len);

/**
 * @brief 获取当前路由目标
 * @return 当前路由目标
 */
audio_route_target_t audio_router_get_target(void);

/**
 * @brief 强制切换路由目标
 * @param target 目标路由
 * @return ESP_OK 成功
 */
esp_err_t audio_router_set_target(audio_route_target_t target);

/**
 * @brief 设置 VAD 状态 (由外部 VAD 模块更新)
 * @param state VAD 状态
 */
void audio_router_set_vad_state(vad_state_t state);

/**
 * @brief 获取路由统计
 * @param esp_sr_count ESP-SR 路由次数
 * @param tflm_count TFLM 路由次数
 */
void audio_router_get_stats(uint32_t *esp_sr_count, uint32_t *tflm_count);

#endif /* AUDIO_ROUTER_H */