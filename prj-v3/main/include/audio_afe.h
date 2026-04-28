/**
 * @file audio_afe.h
 * @brief ESP-SR 音频前端 (AFE) 配置模块
 * 
 * 封装 ESP-SR AFE (Audio Front-End) 功能:
 * - AEC: 声学回声消除
 * - NS: 噪声抑制
 * - AGC: 自动增益控制
 * - VAD: 语音活动检测
 * - BSS: 盲源分离 (多麦克风)
 */

#ifndef AUDIO_AFE_H
#define AUDIO_AFE_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief AFE 配置参数
 */
typedef struct {
    int sample_rate;            /* 采样率 (默认 16000 Hz) */
    int mic_channels;           /* 麦克风通道数 (1 或 2) */
    int ref_channels;           /* 参考信号通道数 (回声消除) */
    
    /* 音频处理开关 */
    bool enable_aec;            /* 声学回声消除 */
    bool enable_ns;             /* 噪声抑制 */
    bool enable_agc;            /* 自动增益控制 */
    bool enable_vad;            /* 语音活动检测 */
    bool enable_bss;            /* 盲源分离 (多麦克风) */
    
    /* 处理参数 */
    float agc_gain;             /* AGC 目标增益 (dB) */
    float ns_level;             /* NS 抑制级别 (0-4) */
    int vad_threshold;          /* VAD 阈值 */
    
    /* WakeNet/MultiNet 集成 */
    bool enable_wakenet;        /* 启用唤醒词检测 */
    bool enable_multinet;       /* 启用命令词识别 */
    const char *wakenet_model;  /* WakeNet 模型名称 */
    const char *multinet_model; /* MultiNet 模型名称 */
} audio_afe_config_t;

/**
 * @brief AFE 处理结果
 */
typedef struct {
    int16_t *data;              /* 处理后的音频数据 */
    int data_size;              /* 数据大小 (字节) */
    bool is_speech;             /* 是否为语音 (VAD) */
    float volume;               /* 音量 (dB) */
    bool wake_word_detected;    /* 是否检测到唤醒词 */
    int wake_word_index;        /* 唤醒词索引 */
} audio_afe_result_t;

/**
 * @brief 音频处理回调
 */
typedef void (*audio_afe_callback_t)(const audio_afe_result_t *result, void *user_data);

/* ================================================================
 * 全局默认配置
 * ================================================================ */

/**
 * @brief 默认 AFE 配置 (INMP441 单麦克风)
 */
extern const audio_afe_config_t audio_afe_default_config;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 AFE 模块
 * @param config 配置参数
 * @return 0 成功, <0 失败
 */
int audio_afe_init(const audio_afe_config_t *config);

/**
 * @brief 释放 AFE 资源
 */
void audio_afe_deinit(void);

/**
 * @brief 获取需要的输入音频块大小 (采样数)
 * @return 采样数
 */
int audio_afe_get_feed_chunk_size(void);

/**
 * @brief 获取输出音频块大小 (采样数)
 * @return 采样数
 */
int audio_afe_get_fetch_chunk_size(void);

/**
 * @brief 获取采样率
 * @return 采样率 (Hz)
 */
int audio_afe_get_sample_rate(void);

/**
 * @brief 喂入音频数据 (麦克风 + 参考信号)
 * @param samples 音频采样数据 (交错格式)
 * @return 喂入的采样数
 */
int audio_afe_feed(const int16_t *samples);

/**
 * @brief 获取处理后的音频数据
 * @param result 输出结果
 * @return true 有数据, false 无数据
 */
bool audio_afe_fetch(audio_afe_result_t *result);

/**
 * @brief 设置音频处理回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void audio_afe_set_callback(audio_afe_callback_t callback, void *user_data);

/**
 * @brief 启用/禁用 WakeNet
 * @param enable true 启用, false 禁用
 */
void audio_afe_wakenet_enable(bool enable);

/**
 * @brief 启用/禁用 MultiNet
 * @param enable true 启用, false 禁用
 */
void audio_afe_multinet_enable(bool enable);

/**
 * @brief 重置 AFE 状态 (唤醒后调用)
 */
void audio_afe_reset(void);

/**
 * @brief 启动音频处理任务
 * @return 0 成功, <0 失败
 */
int audio_afe_start(void);

/**
 * @brief 停止音频处理任务
 */
void audio_afe_stop(void);

/**
 * @brief 设置唤醒词检测阈值
 * @param threshold 阈值 (0.4 - 0.9999)
 * @return 0 成功, <0 失败
 */
int audio_afe_set_wakenet_threshold(float threshold);

/**
 * @brief 检查是否正在运行
 * @return true 正在运行
 */
bool audio_afe_is_running(void);

/**
 * @brief 获取当前配置
 * @return 配置指针 (只读)
 */
const audio_afe_config_t* audio_afe_get_config(void);

#endif /* AUDIO_AFE_H */
