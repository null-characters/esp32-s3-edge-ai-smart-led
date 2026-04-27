/**
 * @file inmp441_driver.h
 * @brief INMP441 MEMS 麦克风驱动头文件
 * 
 * 功能：
 * - I2S 音频采集 (16kHz, 16-bit)
 * - DMA 缓冲管理
 * - VAD 语音活动检测
 * 
 * 硬件接口：I2S0 (BCK=GPIO4, WS=GPIO5, DO=GPIO6)
 */

#ifndef INMP441_DRIVER_H
#define INMP441_DRIVER_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* 音频配置 */
#define AUDIO_SAMPLE_RATE      16000   /* 16kHz */
#define AUDIO_BITS_PER_SAMPLE   16     /* 16-bit */
#define AUDIO_CHANNELS          1      /* 单声道 */

/* 缓冲配置 */
#define AUDIO_FRAME_MS          25     /* 帧长 25ms */
#define AUDIO_FRAME_SAMPLES     (AUDIO_SAMPLE_RATE * AUDIO_FRAME_MS / 1000)  /* 400 采样点 */
#define AUDIO_FRAME_SHIFT_MS    10     /* 帧移 10ms */
#define AUDIO_FRAME_SHIFT       (AUDIO_SAMPLE_RATE * AUDIO_FRAME_SHIFT_MS / 1000)  /* 160 采样点 */

/* 音频缓冲区大小 (2秒音频) */
#define AUDIO_BUFFER_SECONDS    2
#define AUDIO_BUFFER_SAMPLES    (AUDIO_SAMPLE_RATE * AUDIO_BUFFER_SECONDS)
#define AUDIO_BUFFER_BYTES      (AUDIO_BUFFER_SAMPLES * sizeof(int16_t))

/* VAD 配置 */
#define VAD_ENERGY_THRESHOLD    0.01f   /* RMS 能量阈值 */
#define VAD_HANGOVER_MS         500     /* 拖尾时间 */

/* 音频帧数据 */
typedef struct {
    int16_t samples[AUDIO_FRAME_SAMPLES];  /* 帧采样数据 */
    int num_samples;                       /* 实际采样点数 */
    uint32_t timestamp;                    /* 时间戳 */
    float rms_energy;                      /* RMS 能量 */
    bool vad_active;                       /* VAD 状态 */
} audio_frame_t;

/* 音频缓冲区 */
typedef struct {
    int16_t buffer[AUDIO_BUFFER_SAMPLES];  /* 环形缓冲区 */
    int head;                              /* 写指针 */
    int tail;                              /* 读指针 */
    int count;                             /* 数据计数 */
    struct k_mutex lock;                   /* 互斥锁 */
} audio_buffer_t;

/* VAD 状态 */
typedef struct {
    bool active;                           /* 当前状态 */
    float energy_threshold;                /* 自适应阈值 */
    uint32_t last_active_time;             /* 最后激活时间 */
    float noise_floor;                     /* 噪声底 */
} vad_state_t;

/**
 * @brief 初始化 INMP441 麦克风驱动
 * @return 0 成功, <0 失败
 */
int inmp441_init(void);

/**
 * @brief 读取音频帧 (非阻塞)
 * @param frame 输出音频帧
 * @return 0 成功, -EAGAIN 无数据
 */
int inmp441_read_frame(audio_frame_t *frame);

/**
 * @brief 获取音频缓冲区
 * @param samples 输出缓冲区
 * @param max_samples 最大采样点数
 * @return 实际采样点数
 */
int inmp441_get_samples(int16_t *samples, int max_samples);

/**
 * @brief 计算 RMS 能量
 * @param samples 采样数据
 * @param num_samples 采样点数
 * @return RMS 能量值
 */
float inmp441_calc_rms(const int16_t *samples, int num_samples);

/**
 * @brief VAD 检测
 * @param frame 音频帧
 * @return true 语音活动
 */
bool inmp441_vad_detect(const audio_frame_t *frame);

/**
 * @brief 获取 VAD 状态
 * @return VAD 状态
 */
bool inmp441_get_vad_state(void);

/**
 * @brief 自适应噪声估计
 * @param energy 当前能量
 */
void inmp441_update_noise_floor(float energy);

/**
 * @brief 启动音频采集
 */
void inmp441_start(void);

/**
 * @brief 停止音频采集
 */
void inmp441_stop(void);

#endif /* INMP441_DRIVER_H */