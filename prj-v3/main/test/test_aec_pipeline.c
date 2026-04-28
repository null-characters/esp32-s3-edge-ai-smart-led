/**
 * @file test_aec_pipeline.c
 * @brief AEC 回声消除流水线单元测试 (TDD)
 * 
 * 测试内容：
 * - 初始化/释放
 * - 配置参数验证
 * - 帧大小常量
 * - 启用/禁用状态
 * - 延迟补偿设置
 */

#include <unity.h>
#include "aec_pipeline.h"
#include <string.h>

/* ================================================================
 * T1: 常量验证测试
 * ================================================================ */

TEST_CASE("aec_sample_rate", "[aec_pipeline]")
{
    /* 采样率应为 16kHz (语音识别标准) */
    TEST_ASSERT_EQUAL_INT(16000, AEC_SAMPLE_RATE);
}

TEST_CASE("aec_frame_size", "[aec_pipeline]")
{
    /* 帧大小应为 256 (ESP-SR AEC 默认) */
    TEST_ASSERT_EQUAL_INT(256, AEC_FRAME_SIZE);
}

/* ================================================================
 * T2: 配置结构测试
 * ================================================================ */

TEST_CASE("aec_config_default", "[aec_pipeline]")
{
    /* 默认配置结构应可初始化 */
    aec_config_t config = {
        .mic_channel = 1,
        .ref_channel = 1,
        .aec_enable = true,
        .ns_enable = true,
        .agc_enable = false
    };
    
    TEST_ASSERT_EQUAL_INT(1, config.mic_channel);
    TEST_ASSERT_EQUAL_INT(1, config.ref_channel);
    TEST_ASSERT_TRUE(config.aec_enable);
    TEST_ASSERT_TRUE(config.ns_enable);
    TEST_ASSERT_FALSE(config.agc_enable);
}

TEST_CASE("aec_config_multichannel", "[aec_pipeline]")
{
    /* 多通道配置 */
    aec_config_t config = {
        .mic_channel = 2,
        .ref_channel = 1,
        .aec_enable = true,
        .ns_enable = false,
        .agc_enable = true
    };
    
    TEST_ASSERT_EQUAL_INT(2, config.mic_channel);
    TEST_ASSERT_TRUE(config.agc_enable);
}

/* ================================================================
 * T3: 初始化测试
 * ================================================================ */

TEST_CASE("aec_init_with_null_config", "[aec_pipeline]")
{
    /* NULL 配置应使用默认值 */
    esp_err_t ret = aec_pipeline_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 清理 */
    aec_pipeline_deinit();
}

TEST_CASE("aec_init_with_custom_config", "[aec_pipeline]")
{
    aec_config_t config = {
        .mic_channel = 1,
        .ref_channel = 1,
        .aec_enable = true,
        .ns_enable = true,
        .agc_enable = false
    };
    
    esp_err_t ret = aec_pipeline_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 清理 */
    aec_pipeline_deinit();
}

TEST_CASE("aec_deinit", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* 释放不应崩溃 */
    aec_pipeline_deinit();
    
    /* 重复释放应安全 */
    aec_pipeline_deinit();
    
    TEST_ASSERT_TRUE(true);
}

/* ================================================================
 * T4: 启用/禁用测试
 * ================================================================ */

TEST_CASE("aec_enable_disable", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* 初始应启用 */
    TEST_ASSERT_TRUE(aec_pipeline_is_enabled());
    
    /* 禁用 */
    aec_pipeline_set_enable(false);
    TEST_ASSERT_FALSE(aec_pipeline_is_enabled());
    
    /* 重新启用 */
    aec_pipeline_set_enable(true);
    TEST_ASSERT_TRUE(aec_pipeline_is_enabled());
    
    aec_pipeline_deinit();
}

/* ================================================================
 * T5: 帧大小 API 测试
 * ================================================================ */

TEST_CASE("aec_get_frame_size", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    int frame_size = aec_pipeline_get_frame_size();
    TEST_ASSERT_EQUAL_INT(AEC_FRAME_SIZE, frame_size);
    
    aec_pipeline_deinit();
}

/* ================================================================
 * T6: 延迟补偿测试
 * ================================================================ */

TEST_CASE("aec_delay_set_get", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* 设置延迟 */
    aec_pipeline_set_delay(3);
    TEST_ASSERT_EQUAL_INT(3, aec_pipeline_get_delay());
    
    /* 边界值: 0 */
    aec_pipeline_set_delay(0);
    TEST_ASSERT_EQUAL_INT(0, aec_pipeline_get_delay());
    
    /* 边界值: 6 */
    aec_pipeline_set_delay(6);
    TEST_ASSERT_EQUAL_INT(6, aec_pipeline_get_delay());
    
    aec_pipeline_deinit();
}

/* ================================================================
 * T7: 状态重置测试
 * ================================================================ */

TEST_CASE("aec_reset", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* 重置不应崩溃 */
    aec_pipeline_reset();
    
    /* 重置后仍应可用 */
    TEST_ASSERT_TRUE(aec_pipeline_is_enabled());
    
    aec_pipeline_deinit();
}

/* ================================================================
 * T8: 处理测试 (模拟数据)
 * ================================================================ */

TEST_CASE("aec_process_silence", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* 静音数据 */
    int16_t mic[AEC_FRAME_SIZE] = {0};
    int16_t ref[AEC_FRAME_SIZE] = {0};
    int16_t output[AEC_FRAME_SIZE] = {0};
    
    esp_err_t ret = aec_pipeline_process(mic, ref, output, AEC_FRAME_SIZE);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 静音输入应产生静音输出 */
    for (int i = 0; i < AEC_FRAME_SIZE; i++) {
        TEST_ASSERT_EQUAL_INT(0, output[i]);
    }
    
    aec_pipeline_deinit();
}

TEST_CASE("aec_process_with_reference", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* 麦克风输入 (模拟) */
    int16_t mic[AEC_FRAME_SIZE];
    int16_t ref[AEC_FRAME_SIZE];
    int16_t output[AEC_FRAME_SIZE];
    
    /* 填充测试数据 */
    for (int i = 0; i < AEC_FRAME_SIZE; i++) {
        mic[i] = (int16_t)(1000 * sin(2 * 3.14159 * i / 32));  /* 信号 */
        ref[i] = (int16_t)(500 * sin(2 * 3.14159 * i / 32));   /* 参考回声 */
    }
    
    esp_err_t ret = aec_pipeline_process(mic, ref, output, AEC_FRAME_SIZE);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 输出应有数据 (回声被消除) */
    bool has_data = false;
    for (int i = 0; i < AEC_FRAME_SIZE; i++) {
        if (output[i] != 0) {
            has_data = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(has_data);
    
    aec_pipeline_deinit();
}

TEST_CASE("aec_set_reference", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    int16_t ref[AEC_FRAME_SIZE];
    for (int i = 0; i < AEC_FRAME_SIZE; i++) {
        ref[i] = (int16_t)i;
    }
    
    esp_err_t ret = aec_pipeline_set_reference(ref, AEC_FRAME_SIZE);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    aec_pipeline_deinit();
}

/* ================================================================
 * T9: 边界条件测试
 * ================================================================ */

TEST_CASE("aec_process_uninitialized", "[aec_pipeline]")
{
    /* 未初始化时处理应返回错误 */
    int16_t mic[AEC_FRAME_SIZE] = {0};
    int16_t ref[AEC_FRAME_SIZE] = {0};
    int16_t output[AEC_FRAME_SIZE] = {0};
    
    /* 确保未初始化 */
    aec_pipeline_deinit();
    
    esp_err_t ret = aec_pipeline_process(mic, ref, output, AEC_FRAME_SIZE);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}

TEST_CASE("aec_process_null_pointers", "[aec_pipeline]")
{
    aec_pipeline_init(NULL);
    
    /* NULL 指针应安全处理 */
    esp_err_t ret = aec_pipeline_process(NULL, NULL, NULL, 0);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    
    aec_pipeline_deinit();
}
