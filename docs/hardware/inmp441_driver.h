/*
 * INMP441 MEMS 麦克风 I2S 驱动
 * 
 * 基于 ESP-IDF I2S 驱动和 arduino-audio-tools 库
 * 来源: https://github.com/pschatzmann/arduino-audio-tools
 * 
 * INMP441 规格:
 * - I2S 数字输出
 * - 信噪比: 61 dBA
 * - 灵敏度: -26 dB FS
 * - 频率响应: 60 Hz ~ 15 kHz
 * - 采样率: 8kHz ~ 51.2kHz
 */

#ifndef INMP441_DRIVER_H
#define INMP441_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * I2S 配置
 * ================================================================ */
#define INMP441_SAMPLE_RATE     16000   /* 默认 16kHz */
#define INMP441_BITS_PER_SAMPLE 32     /* INMP441 为 24 位，I2S 用 32 位传输 */
#define INMP441_CHANNELS        1      /* 单声道 */

/* ================================================================
 * 引脚定义（可根据实际硬件调整）
 * ================================================================ */
#define INMP441_PIN_SCK         26     /* Bit Clock (BCLK) */
#define INMP441_PIN_WS          25     /* Word Select (LRCK) */
#define INMP441_PIN_SD          22     /* Serial Data */

/* ================================================================
 * 音频缓冲区配置
 * ================================================================ */
#define INMP441_DMA_BUF_COUNT   8      /* DMA 缓冲区数量 */
#define INMP441_DMA_BUF_LEN     1024   /* 每个缓冲区大小（采样点） */

/* ================================================================
 * MFCC 配置
 * ================================================================ */
#define MFCC_NUM_COEFFS         40     /* MFCC 系数数量 */
#define MFCC_FRAME_SIZE         400    /* 帧长 25ms @ 16kHz */
#define MFCC_FRAME_SHIFT        160    /* 帧移 10ms @ 16kHz */
#define MFCC_NUM_FRAMES         32     /* 用于 AI 推理的帧数 */

/* ================================================================
 * 数据结构
 * ================================================================ */

/* 音频缓冲区 */
typedef struct {
    int32_t *buffer;            /* 音频数据缓冲区 */
    uint32_t size;              /* 缓冲区大小（采样点） */
    uint32_t sample_rate;       /* 采样率 */
    uint8_t channels;           /* 声道数 */
} audio_buffer_t;

/* MFCC 特征 */
typedef struct {
    float data[MFCC_NUM_COEFFS * MFCC_NUM_FRAMES];  /* MFCC 系数 */
    uint32_t num_frames;                           /* 有效帧数 */
} mfcc_features_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 INMP441 麦克风
 * @param sample_rate 采样率（默认 16000）
 * @return 0=成功, <0=错误码
 */
int inmp441_init(uint32_t sample_rate);

/**
 * @brief 读取音频数据（阻塞）
 * @param buffer 输出缓冲区
 * @param samples 要读取的采样点数
 * @return 实际读取的采样点数, <0=错误码
 */
int inmp441_read(int32_t *buffer, uint32_t samples);

/**
 * @brief 读取音频数据（非阻塞）
 * @param buffer 输出缓冲区
 * @param samples 要读取的采样点数
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际读取的采样点数, <0=错误码
 */
int inmp441_read_nonblock(int32_t *buffer, uint32_t samples, uint32_t timeout_ms);

/**
 * @brief 停止麦克风
 */
void inmp441_stop(void);

/**
 * @brief 启动麦克风
 */
void inmp441_start(void);

/**
 * @brief 计算 RMS 能量（用于 VAD）
 * @param buffer 音频缓冲区
 * @param samples 采样点数
 * @return RMS 能量值
 */
float inmp441_calc_rms(const int32_t *buffer, uint32_t samples);

/**
 * @brief 语音活动检测 (VAD)
 * @param buffer 音频缓冲区
 * @param samples 采样点数
 * @param threshold 能量阈值
 * @return true=有语音, false=静音
 */
bool inmp441_vad(const int32_t *buffer, uint32_t samples, float threshold);

/**
 * @brief 提取 MFCC 特征（供 AI 使用）
 * @param audio 音频缓冲区
 * @param num_samples 音频采样点数
 * @param mfcc 输出 MFCC 特征
 * @return 0=成功
 */
int inmp441_extract_mfcc(const int32_t *audio, uint32_t num_samples, 
                         mfcc_features_t *mfcc);

/**
 * @brief 获取 MFCC 特征向量（供 AI 推理）
 * @param mfcc MFCC 特征
 * @param features 输出特征向量（用于 CNN 输入）
 * @param size 输出向量大小
 */
void inmp441_get_mfcc_features(const mfcc_features_t *mfcc, 
                               float *features, uint32_t size);

/* ================================================================
 * 辅助函数
 * ================================================================ */

/**
 * @brief 预加重滤波
 * @param input 输入音频
 * @param output 输出音频
 * @param samples 采样点数
 * @param coeff 预加重系数（通常 0.97）
 */
void inmp441_preemphasis(const int32_t *input, float *output, 
                         uint32_t samples, float coeff);

/**
 * @brief 应用汉明窗
 * @param frame 音频帧
 * @param size 帧大小
 */
void inmp441_apply_hamming_window(float *frame, uint32_t size);

/**
 * @brief 计算 FFT
 * @param input 输入信号（实数）
 * @param output 输出频谱（复数，交错存储）
 * @param size FFT 大小（必须为 2 的幂）
 */
int inmp441_fft(const float *input, float *output, uint32_t size);

/**
 * @brief 计算 Mel 滤波器组能量
 * @param spectrum 功率谱
 * @param mel_energy 输出 Mel 能量
 * @param num_filters 滤波器数量
 * @param fft_size FFT 大小
 * @param sample_rate 采样率
 */
void inmp441_mel_filter_bank(const float *spectrum, float *mel_energy,
                              uint32_t num_filters, uint32_t fft_size,
                              uint32_t sample_rate);

/**
 * @brief DCT 变换（获取 MFCC）
 * @param input 输入（Mel 能量对数）
 * @param output 输出 MFCC 系数
 * @param size 数据大小
 */
void inmp441_dct(const float *input, float *output, uint32_t size);

#endif /* INMP441_DRIVER_H */
