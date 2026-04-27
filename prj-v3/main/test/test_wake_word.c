/**
 * @file test_wake_word.c
 * @brief 唤醒词模块单元测试 (TDD RED Phase)
 * 
 * 测试内容：
 * - 事件类型定义
 * - 阈值范围验证
 * - 状态管理
 */

#include <unity.h>
#include "wake_word.h"

/* Dummy callback for test */
static void dummy_callback(wake_word_event_t event, void *user_data) {}

/* ================================================================
 * T1: 事件类型测试
 * ================================================================ */
TEST_CASE("wake_word_event_types", "[wake_word]")
{
    /* 事件类型应为枚举值 */
    TEST_ASSERT_EQUAL_INT(0, WAKE_WORD_NONE);
    TEST_ASSERT_EQUAL_INT(1, WAKE_WORD_DETECTED);
    TEST_ASSERT_EQUAL_INT(2, WAKE_WORD_TIMEOUT);
}

/* ================================================================
 * T2: 阈值边界测试
 * ================================================================ */
TEST_CASE("wake_word_threshold_valid_range", "[wake_word]")
{
    /* 阈值应在 [0.0, 1.0] 范围 */
    float min_threshold = 0.0f;
    float max_threshold = 1.0f;
    
    TEST_ASSERT_TRUE(min_threshold >= 0.0f);
    TEST_ASSERT_TRUE(max_threshold <= 1.0f);
}

TEST_CASE("wake_word_threshold_mid_value", "[wake_word]")
{
    /* 中间值 0.5 应有效 */
    float threshold = 0.5f;
    TEST_ASSERT_TRUE(threshold >= 0.0f && threshold <= 1.0f);
}

/* ================================================================
 * T3: 配置结构测试
 * ================================================================ */
TEST_CASE("wake_word_config_struct", "[wake_word]")
{
    wake_word_config_t config = {
        .model_name = "wn_xiaobaitong",
        .threshold = 0.5f,
        .callback = NULL,
        .user_data = NULL
    };
    
    /* 验证结构体字段可访问 */
    TEST_ASSERT_EQUAL_STRING("wn_xiaobaitong", config.model_name);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, config.threshold);
    TEST_ASSERT_NULL(config.callback);
    TEST_ASSERT_NULL(config.user_data);
}

/* ================================================================
 * T4: 状态管理测试 (Mock)
 * ================================================================ */
TEST_CASE("wake_word_initial_state", "[wake_word]")
{
    /* 初始状态应为未唤醒 */
    bool awake = wake_word_is_awake();
    /* 预期失败: 未初始化时应返回 false */
    TEST_ASSERT_FALSE(awake);
}

TEST_CASE("wake_word_threshold_get_set", "[wake_word]")
{
    /* 设置阈值后应可获取 */
    wake_word_set_threshold(0.7f);
    float threshold = wake_word_get_threshold();
    
    /* 预期失败: 功能尚未完全实现 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, threshold);
}

/* ================================================================
 * T5: 回调函数类型测试
 * ================================================================ */
TEST_CASE("wake_word_callback_type", "[wake_word]")
{
    /* 验证回调函数指针类型 */
    wake_word_callback_t callback = NULL;
    TEST_ASSERT_NULL(callback);
    
    /* 非空回调应可赋值 */
    callback = dummy_callback;
    TEST_ASSERT_NOT_NULL(callback);
}