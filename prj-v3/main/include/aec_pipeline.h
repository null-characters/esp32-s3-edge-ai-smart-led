/**
 * @file aec_pipeline.h
 * @brief AEC (Acoustic Echo Cancellation) 流水线
 * 
 * 使用 ESP-ADF 的 AEC 消除设备自身播放声音的回声，
 * 防止 TTS 语音被麦克风误识别
 */

#ifndef AEC_PIPELINE_H
#define AEC_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 配置常量
 * ================================================================ */

#define AEC_SAMPLE_RATE         16000   /* 采样率 */
#define AEC_FRAME_SIZE          256     /* 帧大小 */

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief AEC 配置
 */
typedef struct {
    int mic_channel;            /* 麦克风通道数 */
    int ref_channel;            /* 参考通道数 */
    bool aec_enable;            /* 是否启用 AEC */
    bool ns_enable;             /* 是否启用噪声抑制 */
    bool agc_enable;            /* 是否启用自动增益控制 */
} aec_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 AEC 流水线
 * @param config 配置参数 (NULL 使用默认配置)
 * @return ESP_OK 成功
 */
esp_err_t aec_pipeline_init(const aec_config_t *config);

/**
 * @brief 释放 AEC 流水线
 */
void aec_pipeline_deinit(void);

/**
 * @brief 处理音频数据 (带回声消除)
 * @param mic_input 麦克风输入数据
 * @param ref_input 参考音频数据 (DAC 输出的回放信号)
 * @param output 处理后的输出数据
 * @param len 数据长度 (样本数)
 * @return ESP_OK 成功
 */
esp_err_t aec_pipeline_process(const int16_t *mic_input, const int16_t *ref_input, int16_t *output, int len);

/**
 * @brief 设置参考音频 (DAC 输出)
 * @param ref_data 参考音频数据
 * @param len 数据长度 (样本数)
 * @return ESP_OK 成功
 */
esp_err_t aec_pipeline_set_reference(const int16_t *ref_data, int len);

/**
 * @brief 获取 AEC 是否启用
 * @return true 启用
 */
bool aec_pipeline_is_enabled(void);

/**
 * @brief 启用/禁用 AEC
 * @param enable 是否启用
 */
void aec_pipeline_set_enable(bool enable);

/**
 * @brief 获取帧大小
 * @return 帧大小 (样本数)
 */
int aec_pipeline_get_frame_size(void);

#endif /* AEC_PIPELINE_H */