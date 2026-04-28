/**
 * @file test_priority_arbiter.c
 * @brief 优先级仲裁器单元测试
 */

#include "unity.h"
#include "priority_arbiter.h"
#include "esp_log.h"

static const char *TAG = "TEST_ARBITER";

/* 测试状态回调计数 */
static int s_callback_count = 0;
static system_mode_t s_last_mode = MODE_AUTO;

static void test_callback(system_mode_t mode, const led_decision_t *decision, void *user_data)
{
    s_callback_count++;
    s_last_mode = mode;
}

/**
 * @brief 测试仲裁器初始化
 */
TEST_CASE("arbiter_init", "[priority_arbiter]")
{
    arbiter_config_t config = {
        .manual_timeout_ms = 30000,
        .persist_state = false,
        .default_mode = MODE_AUTO,
    };
    
    esp_err_t ret = priority_arbiter_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    TEST_ASSERT_EQUAL(MODE_AUTO, priority_arbiter_get_mode());
    
    priority_arbiter_deinit();
    ESP_LOGI(TAG, "✓ 初始化测试通过");
}

/**
 * @brief 测试优先级: 语音 > 自动 > 默认
 */
TEST_CASE("arbiter_priority", "[priority_arbiter]")
{
    arbiter_config_t config = {
        .manual_timeout_ms = 30000,
        .persist_state = false,
        .default_mode = MODE_AUTO,
    };
    
    priority_arbiter_init(&config);
    
    led_decision_t output;
    decision_source_t source;
    
    /* 1. 无输入 -> 默认决策 */
    source = priority_arbiter_decide(NULL, NULL, &output);
    TEST_ASSERT_EQUAL(DECISION_SOURCE_DEFAULT, source);
    ESP_LOGI(TAG, "✓ 默认决策测试通过");
    
    /* 2. 仅自动输入 */
    auto_input_t auto_in = {
        .valid = true,
        .scene_id = 5,
        .tflm_confidence = 0.8f,
        .decision = {
            .power = true,
            .brightness = 70,
            .scene_id = 5,
            .source = DECISION_SOURCE_AUTO,
        },
    };
    source = priority_arbiter_decide(NULL, &auto_in, &output);
    TEST_ASSERT_EQUAL(DECISION_SOURCE_AUTO, source);
    TEST_ASSERT_EQUAL(5, output.scene_id);
    ESP_LOGI(TAG, "✓ 自动决策测试通过");
    
    /* 3. 语音 + 自动 -> 语音优先 */
    voice_input_t voice_in = {
        .valid = true,
        .command_id = 100,
        .confidence = 0.9f,
        .decision = {
            .power = true,
            .brightness = 100,
            .scene_id = 10,
            .source = DECISION_SOURCE_VOICE,
        },
    };
    source = priority_arbiter_decide(&voice_in, &auto_in, &output);
    TEST_ASSERT_EQUAL(DECISION_SOURCE_VOICE, source);
    TEST_ASSERT_EQUAL(10, output.scene_id);
    ESP_LOGI(TAG, "✓ 语音优先测试通过");
    
    priority_arbiter_deinit();
}

/**
 * @brief 测试模式切换
 */
TEST_CASE("arbiter_mode_switch", "[priority_arbiter]")
{
    arbiter_config_t config = {
        .manual_timeout_ms = 30000,
        .persist_state = false,
        .default_mode = MODE_AUTO,
    };
    
    priority_arbiter_init(&config);
    TEST_ASSERT_EQUAL(MODE_AUTO, priority_arbiter_get_mode());
    
    /* 切换到手动模式 */
    esp_err_t ret = priority_arbiter_set_mode(MODE_MANUAL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(MODE_MANUAL, priority_arbiter_get_mode());
    ESP_LOGI(TAG, "✓ 手动模式切换通过");
    
    /* 切换到睡眠模式 */
    ret = priority_arbiter_set_mode(MODE_SLEEP);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(MODE_SLEEP, priority_arbiter_get_mode());
    ESP_LOGI(TAG, "✓ 睡眠模式切换通过");
    
    priority_arbiter_deinit();
}

/**
 * @brief 测试语音命令提交
 */
TEST_CASE("arbiter_voice_submit", "[priority_arbiter]")
{
    arbiter_config_t config = {
        .manual_timeout_ms = 30000,
        .persist_state = false,
        .default_mode = MODE_AUTO,
    };
    
    priority_arbiter_init(&config);
    
    /* 提交语音命令应切换到手动模式 */
    esp_err_t ret = priority_arbiter_submit_voice(50, 0.95f);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(MODE_MANUAL, priority_arbiter_get_mode());
    ESP_LOGI(TAG, "✓ 语音命令提交测试通过");
    
    priority_arbiter_deinit();
}

/**
 * @brief 测试自动控制提交
 */
TEST_CASE("arbiter_auto_submit", "[priority_arbiter]")
{
    arbiter_config_t config = {
        .manual_timeout_ms = 30000,
        .persist_state = false,
        .default_mode = MODE_AUTO,
    };
    
    priority_arbiter_init(&config);
    
    /* 在自动模式下提交自动控制 */
    esp_err_t ret = priority_arbiter_submit_auto(3, 0.85f);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(MODE_AUTO, priority_arbiter_get_mode());
    ESP_LOGI(TAG, "✓ 自动控制提交测试通过");
    
    /* 切换到手动模式后，自动控制应被忽略 */
    priority_arbiter_set_mode(MODE_MANUAL);
    ret = priority_arbiter_submit_auto(5, 0.9f);
    TEST_ASSERT_EQUAL(ESP_OK, ret);  /* API 成功，但决策被忽略 */
    TEST_ASSERT_EQUAL(MODE_MANUAL, priority_arbiter_get_mode());
    ESP_LOGI(TAG, "✓ 手动模式下自动控制忽略测试通过");
    
    priority_arbiter_deinit();
}

/**
 * @brief 测试超时剩余时间
 */
TEST_CASE("arbiter_timeout", "[priority_arbiter]")
{
    arbiter_config_t config = {
        .manual_timeout_ms = 30000,
        .persist_state = false,
        .default_mode = MODE_AUTO,
    };
    
    priority_arbiter_init(&config);
    
    /* 自动模式下无超时 */
    int32_t remaining = priority_arbiter_get_timeout_remaining();
    TEST_ASSERT_EQUAL(-1, remaining);
    ESP_LOGI(TAG, "✓ 自动模式无超时测试通过");
    
    /* 手动模式下有超时 */
    priority_arbiter_set_mode(MODE_MANUAL);
    remaining = priority_arbiter_get_timeout_remaining();
    TEST_ASSERT_TRUE(remaining > 0 && remaining <= 30000);
    ESP_LOGI(TAG, "✓ 手动模式超时测试通过 (剩余 %ldms)", remaining);
    
    /* 重置超时 */
    priority_arbiter_reset_timeout();
    int32_t remaining2 = priority_arbiter_get_timeout_remaining();
    TEST_ASSERT_TRUE(remaining2 >= remaining);
    ESP_LOGI(TAG, "✓ 超时重置测试通过");
    
    priority_arbiter_deinit();
}
