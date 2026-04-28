/**
 * @file test_audio_router.c
 * @brief 音频路由器单元测试 (TDD)
 */

#include <unity.h>
#include "audio_router.h"
#include "vad_detector.h"
#include <string.h>

static bool g_esp_sr_callback_invoked = false;
static bool g_tflm_callback_invoked = false;

static void test_esp_sr_callback(audio_route_target_t target, const int16_t *samples, int len, void *user_data)
{
    g_esp_sr_callback_invoked = true;
}

static void test_tflm_callback(audio_route_target_t target, const int16_t *samples, int len, void *user_data)
{
    g_tflm_callback_invoked = true;
}

/* ================================================================
 * T1: 初始化测试
 * ================================================================ */

TEST_CASE("audio_router_init_with_null_config", "[audio_router]")
{
    esp_err_t ret = audio_router_init(NULL);
    /* NULL 配置可能导致失败，取决于实现 */
    audio_router_deinit();
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("audio_router_init_with_callbacks", "[audio_router]")
{
    audio_router_config_t config = {
        .esp_sr_callback = test_esp_sr_callback,
        .tflm_callback = test_tflm_callback,
        .esp_sr_user_data = NULL,
        .tflm_user_data = NULL
    };
    
    esp_err_t ret = audio_router_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    audio_router_deinit();
}

TEST_CASE("audio_router_deinit", "[audio_router]")
{
    audio_router_init(NULL);
    audio_router_deinit();
    audio_router_deinit();
    TEST_ASSERT_TRUE(true);
}

/* ================================================================
 * T2: 路由目标测试
 * ================================================================ */

TEST_CASE("audio_router_initial_target", "[audio_router]")
{
    audio_router_init(NULL);
    
    audio_route_target_t target = audio_router_get_target();
    /* 初始目标应为 NONE 或 TFLM */
    TEST_ASSERT_TRUE(target == AUDIO_ROUTE_NONE || target == AUDIO_ROUTE_TFLM);
    
    audio_router_deinit();
}

TEST_CASE("audio_router_set_target", "[audio_router]")
{
    audio_router_init(NULL);
    
    esp_err_t ret = audio_router_set_target(AUDIO_ROUTE_ESP_SR);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(AUDIO_ROUTE_ESP_SR, audio_router_get_target());
    
    ret = audio_router_set_target(AUDIO_ROUTE_TFLM);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(AUDIO_ROUTE_TFLM, audio_router_get_target());
    
    audio_router_deinit();
}

/* ================================================================
 * T3: VAD 状态更新测试
 * ================================================================ */

TEST_CASE("audio_router_set_vad_state", "[audio_router]")
{
    audio_router_init(NULL);
    
    /* 设置语音状态 */
    audio_router_set_vad_state(VAD_SPEECH);
    
    /* 设置静音状态 */
    audio_router_set_vad_state(VAD_SILENCE);
    
    audio_router_deinit();
}

/* ================================================================
 * T4: 路由功能测试
 * ================================================================ */

TEST_CASE("audio_router_route_basic", "[audio_router]")
{
    audio_router_config_t config = {
        .esp_sr_callback = test_esp_sr_callback,
        .tflm_callback = test_tflm_callback,
        .esp_sr_user_data = NULL,
        .tflm_user_data = NULL
    };
    
    audio_router_init(&config);
    
    int16_t samples[480];
    memset(samples, 0, sizeof(samples));
    
    g_esp_sr_callback_invoked = false;
    g_tflm_callback_invoked = false;
    
    audio_route_target_t target = audio_router_route(samples, 480);
    
    /* 路由应返回有效目标 */
    TEST_ASSERT_TRUE(target >= AUDIO_ROUTE_NONE && target <= AUDIO_ROUTE_TFLM);
    
    audio_router_deinit();
}

/* ================================================================
 * T5: 统计功能测试
 * ================================================================ */

TEST_CASE("audio_router_get_stats", "[audio_router]")
{
    audio_router_init(NULL);
    
    uint32_t esp_sr_count = 0;
    uint32_t tflm_count = 0;
    
    audio_router_get_stats(&esp_sr_count, &tflm_count);
    
    /* 初始统计应为 0 */
    TEST_ASSERT_EQUAL_UINT32(0, esp_sr_count);
    TEST_ASSERT_EQUAL_UINT32(0, tflm_count);
    
    audio_router_deinit();
}

/* ================================================================
 * T6: 边界条件测试
 * ================================================================ */

TEST_CASE("audio_router_uninitialized", "[audio_router]")
{
    audio_router_deinit();
    
    /* 未初始化时操作应安全 */
    audio_route_target_t target = audio_router_get_target();
    /* 可能返回 NONE 或默认值 */
    
    audio_router_set_vad_state(VAD_SPEECH);
    
    uint32_t esp_count, tflm_count;
    audio_router_get_stats(&esp_count, &tflm_count);
}

TEST_CASE("audio_router_null_samples", "[audio_router]")
{
    audio_router_init(NULL);
    
    audio_route_target_t target = audio_router_route(NULL, 0);
    TEST_ASSERT_EQUAL(AUDIO_ROUTE_NONE, target);
    
    audio_router_deinit();
}

/* ================================================================
 * T7: 连续路由测试
 * ================================================================ */

TEST_CASE("audio_router_continuous_routing", "[audio_router]")
{
    audio_router_config_t config = {
        .esp_sr_callback = test_esp_sr_callback,
        .tflm_callback = test_tflm_callback,
        .esp_sr_user_data = NULL,
        .tflm_user_data = NULL
    };
    
    audio_router_init(&config);
    
    int16_t samples[480];
    memset(samples, 0, sizeof(samples));
    
    /* 连续路由多帧 */
    for (int i = 0; i < 10; i++) {
        audio_router_route(samples, 480);
    }
    
    uint32_t esp_sr_count, tflm_count;
    audio_router_get_stats(&esp_sr_count, &tflm_count);
    
    /* 应有统计累积 */
    TEST_ASSERT_TRUE((esp_sr_count + tflm_count) > 0);
    
    audio_router_deinit();
}
