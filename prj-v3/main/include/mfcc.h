/**
 * @file mfcc.h
 * @brief MFCC 特征提取头文件 (ESP-IDF 版本)
 * 
 * 功能：
 * - 预加重滤波
 * - 分帧加窗
 * - FFT 计算 (使用 ESP-DSP)
 * - Mel 滤波器组
 * - DCT 变换
 */

#ifndef MFCC_H
#define MFCC_H

#include <stdint.h>
#include <stdbool.h>

/* MFCC 配置 */
#define MFCC_NUM_COEFFS     40      /* MFCC 系数数量 */
#define MFCC_NUM_FRAMES     32      /* 帧数量 */
#define MFCC_FFT_SIZE       512     /* FFT 大小 */
#define MFCC_FRAME_SIZE     400     /* 帧长 (25ms @ 16kHz) */
#define MFCC_FRAME_SHIFT    160     /* 帧移 (10ms @ 16kHz) */
#define MFCC_MEL_FILTERS    40      /* Mel 滤波器数量 */

/* 音频参数 */
#define MFCC_SAMPLE_RATE    16000

/* MFCC 特征矩阵 */
typedef struct {
    float data[MFCC_NUM_COEFFS][MFCC_NUM_FRAMES];  /* 40 x 32 */
    int num_coeffs;
    int num_frames;
} mfcc_features_t;

/* MFCC 计算状态 */
typedef struct {
    float pre_emph_last;                    /* 预加重状态 */
    float window[MFCC_FRAME_SIZE];          /* 汉明窗 */
    float mel_filters[MFCC_MEL_FILTERS][MFCC_FFT_SIZE/2+1];  /* Mel 滤波器 */
    float fft_buffer[MFCC_FFT_SIZE];        /* FFT 缓冲区 */
    bool initialized;
} mfcc_state_t;

/**
 * @brief 初始化 MFCC 提取器
 * @return 0 成功
 */
int mfcc_init(void);

/**
 * @brief 提取 MFCC 特征
 * @param samples 音频采样数据
 * @param num_samples 采样点数
 * @param features 输出 MFCC 特征
 * @return 0 成功
 */
int mfcc_extract(const int16_t *samples, int num_samples, mfcc_features_t *features);

/**
 * @brief 预加重滤波
 * @param input 输入信号
 * @param output 输出信号
 * @param len 信号长度
 * @param alpha 预加重系数 (默认 0.97)
 */
void mfcc_pre_emphasis(const int16_t *input, float *output, int len, float alpha);

/**
 * @brief 分帧
 * @param signal 输入信号
 * @param signal_len 信号长度
 * @param frames 输出帧数组
 * @param num_frames 帧数量
 */
void mfcc_frame_signal(const float *signal, int signal_len, float frames[][MFCC_FRAME_SIZE], int *num_frames);

/**
 * @brief 应用汉明窗
 * @param frame 帧数据
 * @param len 帧长度
 */
void mfcc_apply_window(float *frame, int len);

/**
 * @brief 计算 FFT
 * @param input 输入信号
 * @param output 输出频谱
 * @param len FFT 长度
 */
void mfcc_fft(const float *input, float *output, int len);

/**
 * @brief 计算 Mel 频率
 * @param freq 线性频率 (Hz)
 * @return Mel 频率
 */
float mfcc_hz_to_mel(float freq);

/**
 * @brief Mel 频率转线性频率
 * @param mel Mel 频率
 * @return 线性频率 (Hz)
 */
float mfcc_mel_to_hz(float mel);

/**
 * @brief 应用 Mel 滤波器组
 * @param power_spectrum 功率谱
 * @param mel_spectrum 输出 Mel 谱
 * @param len 频谱长度
 */
void mfcc_apply_mel_filters(const float *power_spectrum, float *mel_spectrum, int len);

/**
 * @brief DCT 变换
 * @param input 输入
 * @param output 输出
 * @param len 长度
 */
void mfcc_dct(const float *input, float *output, int len);

#endif /* MFCC_H */