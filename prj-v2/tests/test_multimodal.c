/**
 * @file test_multimodal.c
 * @brief 多模态推理模块单元测试
 * 
 * 测试用例:
 * - TC-001: 验证三个模型使用独立 Arena
 * - TC-002: 验证历史缓冲不越界
 * - TC-003: 验证 VAD 检测功能
 * - TC-004: 验证推理输出范围
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <math.h>
#include "multimodal/multimodal_infer.h"
#include "radar/ld2410_driver.h"
#include "audio/inmp441_driver.h"

/* ==================== TC-001: Arena 独立性测试 ==================== */

/**
 * @brief 测试目的：验证三个模型使用独立的 Tensor Arena
 * 
 * 失败条件：三个模型的 Arena 地址有重叠
 */
ZTEST(multimodal_tests, test_arena_independence)
{
    int ret = multimodal_init();
    zassert_equal(ret, 0, "multimodal_init should succeed");
    
    /* 获取 Arena 信息（需要添加 API） */
    uintptr_t sound_arena_start, sound_arena_end;
    uintptr_t radar_arena_start, radar_arena_end;
    uintptr_t fusion_arena_start, fusion_arena_end;
    
    ret = multimodal_get_arena_info(&sound_arena_start, &sound_arena_end,
                                     &radar_arena_start, &radar_arena_end,
                                     &fusion_arena_start, &fusion_arena_end);
    zassert_equal(ret, 0, "get_arena_info should succeed");
    
    /* 验证 Arena 不重叠 */
    bool sound_radar_overlap = !(sound_arena_end <= radar_arena_start || 
                                  radar_arena_end <= sound_arena_start);
    bool sound_fusion_overlap = !(sound_arena_end <= fusion_arena_start || 
                                   fusion_arena_end <= sound_arena_start);
    bool radar_fusion_overlap = !(radar_arena_end <= fusion_arena_start || 
                                   fusion_arena_end <= radar_arena_start);
    
    zassert_false(sound_radar_overlap, "Sound and Radar Arena must not overlap");
    zassert_false(sound_fusion_overlap, "Sound and Fusion Arena must not overlap");
    zassert_false(radar_fusion_overlap, "Radar and Fusion Arena must not overlap");
    
    /* 验证 Arena 大小 */
    size_t sound_size = sound_arena_end - sound_arena_start;
    size_t radar_size = radar_arena_end - radar_arena_start;
    size_t fusion_size = fusion_arena_end - fusion_arena_start;
    
    zassert_true(sound_size >= 64 * 1024, "Sound Arena should be >= 64KB");
    zassert_true(radar_size >= 64 * 1024, "Radar Arena should be >= 64KB");
    zassert_true(fusion_size >= 64 * 1024, "Fusion Arena should be >= 64KB");
}

/* ==================== TC-002: 历史缓冲越界测试 ==================== */

/**
 * @brief 测试目的：验证历史缓冲区访问不越界
 * 
 * 失败条件：访问 history_buffer 时索引为负或超出范围
 */
ZTEST(multimodal_tests, test_history_buffer_bounds)
{
    /* 初始化历史缓冲 */
    radar_history_t history;
    memset(&history, 0, sizeof(history));
    
    /* 填充部分数据 */
    for (int i = 0; i < 5; i++) {
        radar_features_t feat = {
            .distance_norm = (float)i / 10.0f,
            .energy_norm = 0.5f,
        };
        ld2410_update_history(&feat);
    }
    
    /* 测试统计计算（n < 10 的情况） */
    float variance, period, trend;
    ld2410_get_history_stats(&variance, &period, &trend);
    
    zassert_true(variance >= 0, "Variance should be non-negative");
    zassert_true(!isnan(variance), "Variance should not be NaN");
    zassert_true(!isinf(variance), "Variance should not be Inf");
    
    /* 填充更多数据触发 trend 计算 */
    for (int i = 5; i < 15; i++) {
        radar_features_t feat = {
            .distance_norm = (float)i / 10.0f,
            .energy_norm = 0.5f + (float)i * 0.01f,
        };
        ld2410_update_history(&feat);
    }
    
    ld2410_get_history_stats(&variance, &period, &trend);
    zassert_true(!isnan(trend), "Trend should not be NaN");
}

/* ==================== TC-003: VAD 检测测试 ==================== */

/**
 * @brief 测试目的：验证 VAD 能量阈值检测
 */
ZTEST(multimodal_tests, test_vad_energy_detection)
{
    /* 静音帧 */
    audio_frame_t silence_frame = {0};
    memset(silence_frame.samples, 0, sizeof(silence_frame.samples));
    silence_frame.num_samples = AUDIO_FRAME_SAMPLES;
    silence_frame.rms_energy = inmp441_calc_rms(silence_frame.samples, silence_frame.num_samples);
    
    bool vad_result = inmp441_vad_detect(&silence_frame);
    zassert_false(vad_result, "Silence should not trigger VAD");
    
    /* 有声音帧 */
    audio_frame_t voice_frame = {0};
    for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
        voice_frame.samples[i] = (int16_t)(10000 * sinf(2 * M_PI * 1000 * i / AUDIO_SAMPLE_RATE));
    }
    voice_frame.num_samples = AUDIO_FRAME_SAMPLES;
    voice_frame.rms_energy = inmp441_calc_rms(voice_frame.samples, voice_frame.num_samples);
    
    vad_result = inmp441_vad_detect(&voice_frame);
    zassert_true(vad_result, "Voice should trigger VAD");
}

/**
 * @brief 测试目的：验证 VAD 过零率检测
 */
ZTEST(multimodal_tests, test_vad_zcr_detection)
{
    /* TODO: 添加过零率检测测试 */
    ztest_test_pass();
}

/* ==================== TC-004: 推理输出范围测试 ==================== */

/**
 * @brief 测试目的：验证推理输出在有效范围内
 */
ZTEST(multimodal_tests, test_inference_output_range)
{
    multimodal_init();
    
    /* 构造测试输入 */
    multimodal_input_t input = {0};
    for (int i = 0; i < SOUND_MODEL_INPUT_SIZE; i++) {
        input.mfcc[i] = (float)i / SOUND_MODEL_INPUT_SIZE;
    }
    input.radar.distance_norm = 0.5f;
    input.radar.velocity_norm = 0.1f;
    input.radar.energy_norm = 0.3f;
    input.hour_sin = 0.0f;
    input.hour_cos = 1.0f;
    input.sunset_proximity = 0.5f;
    input.presence = 1.0f;
    
    multimodal_output_t output;
    int ret = multimodal_infer(&input, &output);
    
    zassert_equal(ret, 0, "Inference should succeed");
    
    /* 验证色温范围 2700K - 6500K */
    zassert_true(output.color_temp >= 2700 && output.color_temp <= 6500,
                 "Color temp should be in range [2700, 6500], got %u", output.color_temp);
    
    /* 验证亮度范围 0 - 100 */
    zassert_true(output.brightness <= 100,
                 "Brightness should be in range [0, 100], got %u", output.brightness);
    
    /* 验证概率和为 1 */
    float prob_sum = 0;
    for (int i = 0; i < SOUND_MODEL_OUTPUT_SIZE; i++) {
        zassert_true(output.sound_probs[i] >= 0 && output.sound_probs[i] <= 1,
                     "Probability should be in [0, 1]");
        prob_sum += output.sound_probs[i];
    }
    zassert_within(prob_sum, 1.0f, 0.01f, "Probabilities should sum to 1");
}

/* ==================== TC-005: 并发安全测试 ==================== */

/**
 * @brief 测试目的：验证多线程访问时的安全性
 */
#define THREAD_ITERATIONS 100
static int shared_counter = 0;

static void counter_thread(void *arg1, void *arg2, void *arg3)
{
    for (int i = 0; i < THREAD_ITERATIONS; i++) {
        k_mutex_lock(&features_mutex, K_FOREVER);
        shared_counter++;
        k_mutex_unlock(&features_mutex);
        k_msleep(1);
    }
}

ZTEST(multimodal_tests, test_thread_safety)
{
    shared_counter = 0;
    
    /* 创建多个线程同时访问共享数据 */
    K_THREAD_DEFINE(thread1, 1024, counter_thread, NULL, NULL, NULL, 5, 0, 0);
    K_THREAD_DEFINE(thread2, 1024, counter_thread, NULL, NULL, NULL, 5, 0, 0);
    
    k_msleep(THREAD_ITERATIONS * 2 + 100);
    
    zassert_equal(shared_counter, THREAD_ITERATIONS * 2,
                  "Counter should be %d, got %d (race condition detected)",
                  THREAD_ITERATIONS * 2, shared_counter);
}

/* ==================== 测试套件注册 ==================== */

ZTEST_SUITE(multimodal_tests, NULL, NULL, NULL, NULL, NULL);
