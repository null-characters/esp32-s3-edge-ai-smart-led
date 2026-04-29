/**
 * @file test_utils.h
 * @brief 公共测试工具函数
 * 
 * 提供测试数据生成、验证等通用功能
 * 避免在多个测试文件中重复定义
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdint.h>
#include <math.h>

/* ================================================================
 * 信号生成函数
 * ================================================================ */

/**
 * @brief 生成正弦波音频数据
 * 
 * @param buffer 输出缓冲区
 * @param samples 样本数
 * @param sample_rate 采样率 (Hz)
 * @param frequency 信号频率 (Hz)
 * @param amplitude 振幅 (0.0 - 1.0)
 */
static inline void generate_sine_wave(int16_t *buffer, int samples, 
                                      int sample_rate, float frequency, 
                                      float amplitude)
{
    for (int i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        float val = amplitude * sinf(2.0f * 3.14159265f * frequency * t);
        buffer[i] = (int16_t)(val * 32767.0f);
    }
}

/**
 * @brief 生成加噪正弦波
 * 
 * @param buffer 输出缓冲区
 * @param samples 样本数
 * @param sample_rate 采样率
 * @param frequency 信号频率
 * @param amplitude 振幅
 * @param noise_level 噪声级别 (0.0 - 1.0)
 */
static inline void generate_noisy_sine(int16_t *buffer, int samples,
                                        int sample_rate, float frequency,
                                        float amplitude, float noise_level)
{
    for (int i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        float signal = amplitude * sinf(2.0f * 3.14159265f * frequency * t);
        float noise = noise_level * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
        float val = signal + noise;
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        buffer[i] = (int16_t)(val * 32767.0f);
    }
}

/**
 * @brief 生成多频率混合信号
 * 
 * @param buffer 输出缓冲区
 * @param samples 样本数
 * @param sample_rate 采样率
 * @param frequencies 频率数组
 * @param amplitudes 振幅数组
 * @param num_freqs 频率数量
 */
static inline void generate_mixed_signal(int16_t *buffer, int samples,
                                          int sample_rate,
                                          const float *frequencies,
                                          const float *amplitudes,
                                          int num_freqs)
{
    for (int i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        float val = 0.0f;
        
        for (int j = 0; j < num_freqs; j++) {
            val += amplitudes[j] * sinf(2.0f * 3.14159265f * frequencies[j] * t);
        }
        
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        
        buffer[i] = (int16_t)(val * 32767.0f);
    }
}

/* ================================================================
 * 特征生成函数
 * ================================================================ */

/**
 * @brief 生成模拟 MFCC 特征
 * 
 * @param mfcc 输出缓冲区 (frames * mel_bands)
 * @param frames 帧数
 * @param mel_bands Mel 滤波器组数
 * @param scene_type 场景类型 (影响特征分布)
 */
static inline void generate_mock_mfcc(float *mfcc, int frames, 
                                       int mel_bands, int scene_type)
{
    /* 基于场景类型生成不同特征分布 */
    float base_value = 0.5f + 0.1f * scene_type;
    
    for (int f = 0; f < frames; f++) {
        for (int m = 0; m < mel_bands; m++) {
            /* 模拟频谱包络 */
            float freq_factor = 1.0f - 0.5f * (float)m / mel_bands;
            float time_factor = 1.0f - 0.3f * fabsf((float)f / frames - 0.5f);
            
            mfcc[f * mel_bands + m] = base_value * freq_factor * time_factor;
        }
    }
}

/**
 * @brief 生成模拟雷达特征向量
 * 
 * @param features 输出缓冲区 (8 维)
 * @param target_state 目标状态 (0=无, 1=静止, 2=运动)
 * @param distance 目标距离 (cm)
 */
static inline void generate_mock_radar_features(float *features, 
                                                 int target_state, 
                                                 int distance)
{
    /* 初始化 */
    for (int i = 0; i < 8; i++) {
        features[i] = 0.0f;
    }
    
    if (target_state == 0) {
        /* 无目标 */
        return;
    }
    
    /* 有目标 */
    features[0] = 1.0f;  /* 存在标志 */
    features[1] = (target_state == 2) ? 1.0f : 0.0f;  /* 运动标志 */
    features[2] = distance / 600.0f;  /* 归一化距离 */
    features[3] = 0.5f;  /* 信号强度 */
}

/* ================================================================
 * 断言辅助宏
 * ================================================================ */

/**
 * @brief 验证数组元素在指定范围内
 */
#define TEST_ASSERT_ARRAY_IN_RANGE(arr, len, min_val, max_val) \
    do { \
        for (int _i = 0; _i < (len); _i++) { \
            TEST_ASSERT_TRUE((arr)[_i] >= (min_val)); \
            TEST_ASSERT_TRUE((arr)[_i] <= (max_val)); \
        } \
    } while(0)

/**
 * @brief 验证数组非零
 */
#define TEST_ASSERT_ARRAY_NON_ZERO(arr, len) \
    do { \
        float _sum = 0.0f; \
        for (int _i = 0; _i < (len); _i++) { \
            _sum += fabsf((arr)[_i]); \
        } \
        TEST_ASSERT_TRUE(_sum > 0.0f); \
    } while(0)

/**
 * @brief 验证两个浮点数近似相等
 */
#define TEST_ASSERT_FLOAT_WITHIN_TOLERANCE(expected, actual, tolerance) \
    TEST_ASSERT_TRUE(fabsf((expected) - (actual)) <= (tolerance))

#endif /* TEST_UTILS_H */
