/**
 * @file test_tts_engine.c
 * @brief TTS 语音合成引擎单元测试 (TDD)
 * 
 * 测试内容：
 * - 初始化/释放
 * - 文本播放 API
 * - 播放状态管理
 * - 语音速度控制
 * - 回调机制
 */

#include <unity.h>
#include "tts_engine.h"
#include <string.h>

/* ================================================================
 * 测试辅助数据
 * ================================================================ */

static bool g_callback_invoked = false;
static void *g_callback_user_data = NULL;

static void test_tts_callback(void *user_data)
{
    g_callback_invoked = true;
    g_callback_user_data = user_data;
}

/* ================================================================
 * T1: 初始化测试
 * ================================================================ */

TEST_CASE("tts_init_with_null_config", "[tts_engine]")
{
    /* NULL 配置应使用默认值 */
    esp_err_t ret = tts_engine_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_engine_deinit();
}

TEST_CASE("tts_init_with_custom_config", "[tts_engine]")
{
    tts_config_t config = {
        .sample_rate = 16000,
        .callback = test_tts_callback,
        .user_data = (void *)0x1234
    };
    
    esp_err_t ret = tts_engine_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_engine_deinit();
}

TEST_CASE("tts_deinit", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_deinit();
    
    /* 重复释放应安全 */
    tts_engine_deinit();
    TEST_ASSERT_TRUE(true);
}

/* ================================================================
 * T2: 启动测试
 * ================================================================ */

TEST_CASE("tts_start", "[tts_engine]")
{
    tts_engine_init(NULL);
    
    esp_err_t ret = tts_engine_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_engine_deinit();
}

/* ================================================================
 * T3: 播放状态测试
 * ================================================================ */

TEST_CASE("tts_is_playing_initial", "[tts_engine]")
{
    tts_engine_init(NULL);
    
    /* 初始应不在播放 */
    TEST_ASSERT_FALSE(tts_is_playing());
    
    tts_engine_deinit();
}

TEST_CASE("tts_stop_when_idle", "[tts_engine]")
{
    tts_engine_init(NULL);
    
    /* 空闲时停止应成功 */
    esp_err_t ret = tts_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_engine_deinit();
}

/* ================================================================
 * T4: 文本播放测试
 * ================================================================ */

TEST_CASE("tts_speak_simple", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 简单文本播放 */
    esp_err_t ret = tts_speak("你好");
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 停止播放 */
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_empty", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 空文本应安全处理 */
    esp_err_t ret = tts_speak("");
    /* 空文本可能返回错误或成功，但不应崩溃 */
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_INVALID_ARG);
    
    tts_engine_deinit();
}

TEST_CASE("tts_speak_long_text", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 长文本播放 */
    const char *long_text = "当前亮度百分之八十，色温五千开尔文，处于自动模式";
    esp_err_t ret = tts_speak(long_text);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_blocking", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 阻塞播放 (短文本避免测试超时) */
    esp_err_t ret = tts_speak_blocking("好");
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 阻塞播放完成后应不在播放 */
    TEST_ASSERT_FALSE(tts_is_playing());
    
    tts_engine_deinit();
}

/* ================================================================
 * T5: 便捷 API 测试
 * ================================================================ */

TEST_CASE("tts_speak_brightness", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    esp_err_t ret = tts_speak_brightness(50);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_brightness_boundary", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 边界值: 0% */
    esp_err_t ret = tts_speak_brightness(0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 边界值: 100% */
    ret = tts_speak_brightness(100);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_color_temp", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    esp_err_t ret = tts_speak_color_temp(5000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_color_temp_boundary", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 暖光: 2700K */
    esp_err_t ret = tts_speak_color_temp(2700);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 冷光: 6500K */
    ret = tts_speak_color_temp(6500);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_mode", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 自动模式 */
    esp_err_t ret = tts_speak_mode(true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 手动模式 */
    ret = tts_speak_mode(false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_power", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* 开机 */
    esp_err_t ret = tts_speak_power(true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 关机 */
    ret = tts_speak_power(false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_scene_status", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    esp_err_t ret = tts_speak_scene_status("专注模式", 60, 5000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_greeting", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    esp_err_t ret = tts_speak_greeting();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_confirm", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    esp_err_t ret = tts_speak_confirm();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

TEST_CASE("tts_speak_error", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    esp_err_t ret = tts_speak_error();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    tts_stop();
    tts_engine_deinit();
}

/* ================================================================
 * T6: 速度控制测试
 * ================================================================ */

TEST_CASE("tts_speed_set_get", "[tts_engine]")
{
    tts_engine_init(NULL);
    
    /* 设置速度 */
    tts_set_speed(3);
    TEST_ASSERT_EQUAL_UINT(3, tts_get_speed());
    
    /* 边界: 最慢 */
    tts_set_speed(0);
    TEST_ASSERT_EQUAL_UINT(0, tts_get_speed());
    
    /* 边界: 最快 */
    tts_set_speed(5);
    TEST_ASSERT_EQUAL_UINT(5, tts_get_speed());
    
    tts_engine_deinit();
}

/* ================================================================
 * T7: 回调测试
 * ================================================================ */

TEST_CASE("tts_set_callback", "[tts_engine]")
{
    tts_engine_init(NULL);
    
    /* 设置回调 */
    g_callback_invoked = false;
    g_callback_user_data = NULL;
    
    tts_set_callback(test_tts_callback, (void *)0x5678);
    
    /* 回调已设置，播放完成后会被调用 */
    /* 注意: 异步播放可能无法立即触发回调 */
    
    tts_engine_deinit();
}

/* ================================================================
 * T8: 边界条件测试
 * ================================================================ */

TEST_CASE("tts_uninitialized_operations", "[tts_engine]")
{
    /* 确保未初始化 */
    tts_engine_deinit();
    
    /* 未初始化时操作应安全 */
    TEST_ASSERT_FALSE(tts_is_playing());
    
    /* 播放应返回错误 */
    esp_err_t ret = tts_speak("测试");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}

TEST_CASE("tts_null_text", "[tts_engine]")
{
    tts_engine_init(NULL);
    tts_engine_start();
    
    /* NULL 文本应安全处理 */
    esp_err_t ret = tts_speak(NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    
    tts_engine_deinit();
}
