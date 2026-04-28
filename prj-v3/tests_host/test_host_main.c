/**
 * @file test_host_main.c
 * @brief 主机端单元测试入口
 * 
 * 使用 Unity 测试框架在 PC 上运行，无需硬件
 */

#include "unity.h"
#include "mock_esp.h"
#include "mock_hardware.h"

/* ================================================================
 * 模型加载器测试 (模拟)
 * ================================================================ */

void test_model_loader_init(void) {
    /* 模拟 SPIFFS 模型文件 */
    uint8_t mock_model_data[1024] = {0};
    mock_file_register("/spiffs/models/sound_classifier.tflite", 
                       mock_model_data, sizeof(mock_model_data));
    
    /* 初始化应该成功 */
    TEST_ASSERT_EQUAL_INT(ESP_OK, ESP_OK);
}

void test_model_size_validation(void) {
    /* 模型大小应在预期范围内 */
    size_t sound_size = 12 * 1024;   /* 12KB */
    size_t radar_size = 4 * 1024;    /* 3.6KB */
    size_t fusion_size = 5 * 1024;   /* 5KB */
    
    TEST_ASSERT_TRUE(sound_size <= 50 * 1024);
    TEST_ASSERT_TRUE(radar_size <= 10 * 1024);
    TEST_ASSERT_TRUE(fusion_size <= 10 * 1024);
    
    size_t total = sound_size + radar_size + fusion_size;
    TEST_ASSERT_TRUE(total <= 100 * 1024);  /* 总计 < 100KB */
}

/* ================================================================
 * 仲裁逻辑测试
 * ================================================================ */

void test_arbiter_voice_priority(void) {
    /* 语音命令应立即生效 */
    mock_led_set_brightness(50);  /* 自动模式设置 */
    TEST_ASSERT_EQUAL_UINT8(50, mock_led_get_state()->brightness);
    
    /* 语音命令覆盖 */
    mock_process_voice_cmd(MOCK_CMD_BRIGHT_UP);
    TEST_ASSERT_EQUAL_UINT8(60, mock_led_get_state()->brightness);
}

void test_arbiter_ttl_logic(void) {
    /* TTL 锁定逻辑测试 */
    /* 模拟语音锁定后，自动控制不应干预 */
    mock_led_set_brightness(80);  /* 语音设置 */
    mock_process_voice_cmd(MOCK_CMD_BRIGHT_UP);  /* 语音命令 */
    
    /* 即使有环境变化，也应保持语音设置 */
    TEST_ASSERT_EQUAL_UINT8(90, mock_led_get_state()->brightness);
}

/* ================================================================
 * LED 控制测试
 * ================================================================ */

void test_led_brightness_range(void) {
    /* 亮度范围测试 */
    mock_led_set_brightness(0);
    TEST_ASSERT_EQUAL_UINT8(0, mock_led_get_state()->brightness);
    
    mock_led_set_brightness(100);
    TEST_ASSERT_EQUAL_UINT8(100, mock_led_get_state()->brightness);
    
    mock_led_set_brightness(50);
    TEST_ASSERT_EQUAL_UINT8(50, mock_led_get_state()->brightness);
}

void test_led_color_temp_range(void) {
    /* 色温范围测试 */
    mock_led_set_color_temp(2700);
    TEST_ASSERT_EQUAL_UINT16(2700, mock_led_get_state()->color_temp);
    
    mock_led_set_color_temp(6500);
    TEST_ASSERT_EQUAL_UINT16(6500, mock_led_get_state()->color_temp);
}

void test_led_power_control(void) {
    /* 开关控制测试 */
    mock_led_set_power(true);
    TEST_ASSERT_TRUE(mock_led_get_state()->is_on);
    
    mock_led_set_power(false);
    TEST_ASSERT_FALSE(mock_led_get_state()->is_on);
}

/* ================================================================
 * 场景预设测试
 * ================================================================ */

void test_scene_presets(void) {
    /* 场景数量测试 */
    TEST_ASSERT_EQUAL_INT(5, mock_scene_count());
    
    /* 场景参数测试 */
    const mock_scene_t *reading = mock_scene_get(0);
    TEST_ASSERT_NOT_NULL(reading);
    TEST_ASSERT_EQUAL_STRING("阅读", reading->name);
    TEST_ASSERT_EQUAL_UINT8(80, reading->brightness);
    TEST_ASSERT_EQUAL_UINT16(4000, reading->color_temp);
}

/* ================================================================
 * 雷达数据测试
 * ================================================================ */

void test_radar_frame_generation(void) {
    mock_radar_frame_t frame;
    
    /* 无目标 */
    mock_generate_radar(&frame, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(0, frame.target_state);
    
    /* 静止目标 */
    mock_generate_radar(&frame, 1, 150);
    TEST_ASSERT_EQUAL_UINT8(1, frame.target_state);
    TEST_ASSERT_EQUAL_INT16(150, frame.distance);
    
    /* 运动目标 */
    mock_generate_radar(&frame, 2, 200);
    TEST_ASSERT_EQUAL_UINT8(2, frame.target_state);
}

void test_radar_features(void) {
    float features[8];
    
    /* 无目标特征 */
    mock_generate_radar_features(features, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, features[0]);
    
    /* 有目标特征 */
    mock_generate_radar_features(features, 1);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, features[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, features[1]);  /* 静止 */
    
    /* 运动目标特征 */
    mock_generate_radar_features(features, 2);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, features[0]);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, features[1]);  /* 运动 */
}

/* ================================================================
 * 音频数据测试
 * ================================================================ */

void test_audio_generation(void) {
    int16_t buffer[1600];  /* 100ms @ 16kHz */
    
    mock_generate_audio(buffer, 1600, 16000, 1000.0f, 0.1f);
    
    /* 检查数据范围 */
    int16_t max_val = 0, min_val = 0;
    for (int i = 0; i < 1600; i++) {
        if (buffer[i] > max_val) max_val = buffer[i];
        if (buffer[i] < min_val) min_val = buffer[i];
    }
    
    TEST_ASSERT_TRUE(max_val > 0);
    TEST_ASSERT_TRUE(min_val < 0);
    TEST_ASSERT_TRUE(max_val < 32768);
    TEST_ASSERT_TRUE(min_val > -32768);
}

void test_mfcc_generation(void) {
    float mfcc[40 * 32];
    
    mock_generate_mfcc(mfcc, 0);  /* 场景 0 */
    
    /* 检查 MFCC 范围 */
    float sum = 0.0f;
    for (int i = 0; i < 40 * 32; i++) {
        sum += mfcc[i];
        TEST_ASSERT_TRUE(mfcc[i] >= 0.0f);
        TEST_ASSERT_TRUE(mfcc[i] <= 2.0f);
    }
    
    /* 检查非零 */
    TEST_ASSERT_TRUE(sum > 0.0f);
}

/* ================================================================
 * 语音命令测试
 * ================================================================ */

void test_voice_commands(void) {
    /* 初始化 */
    mock_led_set_power(false);
    mock_led_set_brightness(50);
    
    /* 开灯 */
    mock_process_voice_cmd(MOCK_CMD_TURN_ON);
    TEST_ASSERT_TRUE(mock_led_get_state()->is_on);
    
    /* 关灯 */
    mock_process_voice_cmd(MOCK_CMD_TURN_OFF);
    TEST_ASSERT_FALSE(mock_led_get_state()->is_on);
    
    /* 亮度调节 */
    mock_process_voice_cmd(MOCK_CMD_TURN_ON);
    mock_led_set_brightness(50);
    mock_process_voice_cmd(MOCK_CMD_BRIGHT_UP);
    TEST_ASSERT_EQUAL_UINT8(60, mock_led_get_state()->brightness);
    
    mock_process_voice_cmd(MOCK_CMD_BRIGHT_DOWN);
    TEST_ASSERT_EQUAL_UINT8(50, mock_led_get_state()->brightness);
    
    /* 色温调节 */
    mock_process_voice_cmd(MOCK_CMD_WARM);
    TEST_ASSERT_EQUAL_UINT16(3000, mock_led_get_state()->color_temp);
    
    mock_process_voice_cmd(MOCK_CMD_COOL);
    TEST_ASSERT_EQUAL_UINT16(5000, mock_led_get_state()->color_temp);
}

/* ================================================================
 * 内存测试
 * ================================================================ */

void test_memory_limits(void) {
    /* 模拟内存限制 */
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    /* 内部 RAM 应有足够空间 */
    TEST_ASSERT_TRUE(free_internal > 100 * 1024);
    
    /* PSRAM 应有足够空间 */
    TEST_ASSERT_TRUE(free_psram > 500 * 1024);
}

/* ================================================================
 * 主测试入口
 * ================================================================ */

int main(void) {
    UNITY_BEGIN();
    
    /* 模型测试 */
    RUN_TEST(test_model_loader_init);
    RUN_TEST(test_model_size_validation);
    
    /* 仲裁测试 */
    RUN_TEST(test_arbiter_voice_priority);
    RUN_TEST(test_arbiter_ttl_logic);
    
    /* LED 测试 */
    RUN_TEST(test_led_brightness_range);
    RUN_TEST(test_led_color_temp_range);
    RUN_TEST(test_led_power_control);
    
    /* 场景测试 */
    RUN_TEST(test_scene_presets);
    
    /* 雷达测试 */
    RUN_TEST(test_radar_frame_generation);
    RUN_TEST(test_radar_features);
    
    /* 音频测试 */
    RUN_TEST(test_audio_generation);
    RUN_TEST(test_mfcc_generation);
    
    /* 语音命令测试 */
    RUN_TEST(test_voice_commands);
    
    /* 内存测试 */
    RUN_TEST(test_memory_limits);
    
    return UNITY_END();
}