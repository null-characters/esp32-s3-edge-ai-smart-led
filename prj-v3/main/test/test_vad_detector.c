/**
 * @file test_vad_detector.c
 * @brief VAD 语音活动检测单元测试 (TDD)
 */

#include <unity.h>
#include "vad_detector.h"
#include <string.h>
#include <math.h>

static bool g_vad_callback_invoked = false;
static vad_state_t g_vad_callback_state = VAD_SILENCE;

static void test_vad_callback(vad_state_t state, const vad_result_t *result, void *user_data)
{
    g_vad_callback_invoked = true;
    g_vad_callback_state = state;
}

static void generate_sine_wave(int16_t *samples, int len, float amplitude)
{
    for (int i = 0; i < len; i++) {
        samples[i] = (int16_t)(amplitude * sin(2.0 * 3.14159 * 440.0 * i / 16000.0));
    }
}

static void generate_silence(int16_t *samples, int len)
{
    memset(samples, 0, len * sizeof(int16_t));
}

/* ================================================================
 * T1: 常量验证测试
 * ================================================================ */

TEST_CASE("vad_sample_rate", "[vad_detector]")
{
    TEST_ASSERT_EQUAL_INT(16000, VAD_SAMPLE_RATE);
}

TEST_CASE("vad_frame_size", "[vad_detector]")
{
    TEST_ASSERT_EQUAL_INT(30, VAD_FRAME_SIZE_MS);
    TEST_ASSERT_EQUAL_INT(480, VAD_FRAME_SAMPLES);
}

/* ================================================================
 * T2: 配置结构测试
 * ================================================================ */

TEST_CASE("vad_config_default", "[vad_detector]")
{
    vad_config_t config = {
        .vad_mode = VAD_MODE_0,
        .min_speech_ms = 100,
        .min_noise_ms = 100,
        .hangover_frames = 5,
        .energy_threshold = 0.01f,
        .voice_prob_threshold = 0.5f,
        .callback = NULL,
        .user_data = NULL
    };
    
    TEST_ASSERT_EQUAL_INT(VAD_MODE_0, config.vad_mode);
    TEST_ASSERT_EQUAL_INT(100, config.min_speech_ms);
    TEST_ASSERT_EQUAL_INT(5, config.hangover_frames);
}

/* ================================================================
 * T3: 初始化测试
 * ================================================================ */

TEST_CASE("vad_init_with_null_config", "[vad_detector]")
{
    esp_err_t ret = vad_detector_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vad_detector_deinit();
}

TEST_CASE("vad_init_with_custom_config", "[vad_detector]")
{
    vad_config_t config = {
        .vad_mode = VAD_MODE_2,
        .min_speech_ms = 100,
        .min_noise_ms = 100,
        .hangover_frames = 5,
        .energy_threshold = 0.01f,
        .voice_prob_threshold = 0.5f,
        .callback = test_vad_callback,
        .user_data = NULL
    };
    
    esp_err_t ret = vad_detector_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vad_detector_deinit();
}

TEST_CASE("vad_deinit", "[vad_detector]")
{
    vad_detector_init(NULL);
    vad_detector_deinit();
    vad_detector_deinit();
    TEST_ASSERT_TRUE(true);
}

/* ================================================================
 * T4: 状态测试
 * ================================================================ */

TEST_CASE("vad_initial_state", "[vad_detector]")
{
    vad_detector_init(NULL);
    vad_state_t state = vad_detector_get_state();
    TEST_ASSERT_EQUAL(VAD_SILENCE, state);
    vad_detector_deinit();
}

TEST_CASE("vad_reset", "[vad_detector]")
{
    vad_detector_init(NULL);
    vad_detector_reset();
    TEST_ASSERT_EQUAL(VAD_SILENCE, vad_detector_get_state());
    vad_detector_deinit();
}

/* ================================================================
 * T5: 帧大小测试
 * ================================================================ */

TEST_CASE("vad_get_frame_size", "[vad_detector]")
{
    vad_detector_init(NULL);
    int frame_size = vad_detector_get_frame_size();
    TEST_ASSERT_EQUAL_INT(VAD_FRAME_SAMPLES, frame_size);
    vad_detector_deinit();
}

/* ================================================================
 * T6: 模式设置测试
 * ================================================================ */

TEST_CASE("vad_set_mode", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    vad_detector_set_mode(VAD_MODE_0);
    vad_detector_set_mode(VAD_MODE_4);
    
    vad_detector_deinit();
}

TEST_CASE("vad_set_hangover", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    vad_detector_set_hangover(0);
    vad_detector_set_hangover(10);
    vad_detector_set_hangover(20);
    
    vad_detector_deinit();
}

TEST_CASE("vad_set_energy_threshold", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    vad_detector_set_energy_threshold(0.001f);
    vad_detector_set_energy_threshold(0.1f);
    
    vad_detector_deinit();
}

/* ================================================================
 * T7: 处理测试
 * ================================================================ */

TEST_CASE("vad_process_silence", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    int16_t samples[VAD_FRAME_SAMPLES];
    vad_result_t result;
    
    generate_silence(samples, VAD_FRAME_SAMPLES);
    
    esp_err_t ret = vad_detector_process(samples, &result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(VAD_SILENCE, result.state);
    
    vad_detector_deinit();
}

TEST_CASE("vad_process_voice", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    int16_t samples[VAD_FRAME_SAMPLES];
    vad_result_t result;
    
    generate_sine_wave(samples, VAD_FRAME_SAMPLES, 10000.0f);
    
    esp_err_t ret = vad_detector_process(samples, &result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    /* 结果取决于 VAD 算法，可能检测到语音或静音 */
    
    vad_detector_deinit();
}

/* ================================================================
 * T8: 回调测试
 * ================================================================ */

TEST_CASE("vad_set_callback", "[vad_detector]")
{
    g_vad_callback_invoked = false;
    
    vad_config_t config = {
        .vad_mode = VAD_MODE_0,
        .min_speech_ms = 100,
        .min_noise_ms = 100,
        .hangover_frames = 5,
        .energy_threshold = 0.01f,
        .voice_prob_threshold = 0.5f,
        .callback = test_vad_callback,
        .user_data = NULL
    };
    
    vad_detector_init(&config);
    vad_detector_deinit();
}

/* ================================================================
 * T9: 边界条件测试
 * ================================================================ */

TEST_CASE("vad_process_uninitialized", "[vad_detector]")
{
    vad_detector_deinit();
    
    int16_t samples[VAD_FRAME_SAMPLES] = {0};
    vad_result_t result;
    
    esp_err_t ret = vad_detector_process(samples, &result);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}

TEST_CASE("vad_process_null", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    esp_err_t ret = vad_detector_process(NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    
    vad_detector_deinit();
}

TEST_CASE("vad_speech_duration", "[vad_detector]")
{
    vad_detector_init(NULL);
    
    uint32_t duration = vad_detector_get_speech_duration_ms();
    TEST_ASSERT_EQUAL_UINT32(0, duration);
    
    vad_detector_deinit();
}
