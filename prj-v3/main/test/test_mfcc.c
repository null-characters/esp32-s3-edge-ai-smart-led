/**
 * @file test_mfcc.c
 * @brief MFCC 特征提取单元测试 (TDD RED Phase)
 * 
 * 测试内容：
 * - Mel 频率转换
 * - 预加重滤波
 * - 汉明窗计算
 * - FFT 配置
 */

#include <unity.h>
#include <math.h>
#include "mfcc.h"

#define FLOAT_TOLERANCE 0.01f
#define PI 3.14159265358979f

/* ================================================================
 * T1: Mel 频率转换测试
 * ================================================================ */
TEST_CASE("mfcc_hz_to_mel_zero", "[mfcc]")
{
    /* 0 Hz → 0 Mel */
    float mel = mfcc_hz_to_mel(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.0f, mel);
}

TEST_CASE("mfcc_hz_to_mel_1000hz", "[mfcc]")
{
    /* 1000 Hz → 约 1000 Mel (近似线性区) */
    float mel = mfcc_hz_to_mel(1000.0f);
    /* 公式: mel = 2595 * log10(1 + f/700) */
    float expected = 2595.0f * log10f(1.0f + 1000.0f/700.0f);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, expected, mel);
}

TEST_CASE("mfcc_hz_to_mel_8000hz", "[mfcc]")
{
    /* 8000 Hz → 高 Mel 值 */
    float mel = mfcc_hz_to_mel(8000.0f);
    float expected = 2595.0f * log10f(1.0f + 8000.0f/700.0f);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, expected, mel);
}

TEST_CASE("mfcc_mel_to_hz_roundtrip", "[mfcc]")
{
    /* Hz → Mel → Hz 应还原 */
    float hz_in = 1000.0f;
    float mel = mfcc_hz_to_mel(hz_in);
    float hz_out = mfcc_mel_to_hz(mel);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, hz_in, hz_out);
}

/* ================================================================
 * T2: MFCC 配置常量测试
 * ================================================================ */
TEST_CASE("mfcc_config_constants", "[mfcc]")
{
    /* MFCC 系数应为 40 */
    TEST_ASSERT_EQUAL_INT(40, MFCC_NUM_COEFFS);
    
    /* 帧数应为 32 */
    TEST_ASSERT_EQUAL_INT(32, MFCC_NUM_FRAMES);
    
    /* FFT 大小应为 512 */
    TEST_ASSERT_EQUAL_INT(512, MFCC_FFT_SIZE);
    
    /* 帧长应为 400 (25ms @ 16kHz) */
    TEST_ASSERT_EQUAL_INT(400, MFCC_FRAME_SIZE);
    
    /* 帧移应为 160 (10ms @ 16kHz) */
    TEST_ASSERT_EQUAL_INT(160, MFCC_FRAME_SHIFT);
    
    /* 采样率应为 16000 */
    TEST_ASSERT_EQUAL_INT(16000, MFCC_SAMPLE_RATE);
}

/* ================================================================
 * T3: 汉明窗公式测试
 * ================================================================ */
TEST_CASE("mfcc_hamming_window_center", "[mfcc]")
{
    /* 汉明窗中心应为 1.0 */
    /* w(n) = 0.54 - 0.46 * cos(2πn/(N-1)) */
    /* 中心 n = (N-1)/2 时, cos(π) = -1, w = 0.54 + 0.46 = 1.0 */
    int N = MFCC_FRAME_SIZE;
    int center = (N - 1) / 2;
    float expected = 0.54f - 0.46f * cosf(2.0f * PI * center / (N - 1));
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, expected);
}

TEST_CASE("mfcc_hamming_window_edges", "[mfcc]")
{
    /* 汉明窗边缘应约 0.08 */
    int N = MFCC_FRAME_SIZE;
    float edge_start = 0.54f - 0.46f * cosf(0.0f);  /* n=0 */
    float edge_end = 0.54f - 0.46f * cosf(2.0f * PI * (N-1) / (N-1));  /* n=N-1 */
    
    /* 边缘值应为 0.54 - 0.46 = 0.08 */
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.08f, edge_start);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.08f, edge_end);
}

/* ================================================================
 * T4: 预加重滤波测试
 * ================================================================ */
TEST_CASE("mfcc_pre_emphasis_first_sample", "[mfcc]")
{
    /* 第一个样本: y[0] = x[0] (无前一样本) */
    int16_t input[] = {1000, 2000, 3000};
    float output[3];
    
    mfcc_pre_emphasis(input, output, 3, 0.97f);
    
    /* 第一个样本应等于输入 */
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1000.0f, output[0]);
}

TEST_CASE("mfcc_pre_emphasis_subsequent_samples", "[mfcc]")
{
    /* y[n] = x[n] - α * x[n-1] */
    int16_t input[] = {1000, 2000, 3000};
    float output[3];
    float alpha = 0.97f;
    
    mfcc_pre_emphasis(input, output, 3, alpha);
    
    /* y[1] = 2000 - 0.97 * 1000 = 1030 */
    float expected_1 = 2000.0f - alpha * 1000.0f;
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, expected_1, output[1]);
    
    /* y[2] = 3000 - 0.97 * 2000 = 1060 */
    float expected_2 = 3000.0f - alpha * 2000.0f;
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, expected_2, output[2]);
}

/* ================================================================
 * T5: 结构体大小测试
 * ================================================================ */
TEST_CASE("mfcc_struct_sizes", "[mfcc]")
{
    /* mfcc_features_t 应为 40 * 32 * 4 + 8 = 5168 bytes */
    size_t expected_size = sizeof(float) * MFCC_NUM_COEFFS * MFCC_NUM_FRAMES + sizeof(int) * 2;
    TEST_ASSERT_EQUAL_INT(expected_size, sizeof(mfcc_features_t));
    
    /* mfcc_state_t 应大于 0 */
    TEST_ASSERT_TRUE(sizeof(mfcc_state_t) > 0);
}