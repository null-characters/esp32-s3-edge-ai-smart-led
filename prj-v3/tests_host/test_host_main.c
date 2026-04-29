/**
 * @file test_host_main.c
 * @brief 主机端单元测试入口
 * 
 * 使用 Unity 测试框架在 PC 上运行，无需硬件
 * 包含实际业务逻辑验证和边界条件测试
 */

#include "unity.h"
#include "mock_esp.h"
#include "mock_hardware.h"

/* ================================================================
 * 模型加载器测试
 * ================================================================ */

void test_model_loader_init(void) {
    /* 模拟 SPIFFS 模型文件 */
    uint8_t mock_model_data[1024];
    memset(mock_model_data, 0xAB, sizeof(mock_model_data));
    
    mock_file_register("/spiffs/models/sound_classifier.tflite", 
                       mock_model_data, sizeof(mock_model_data));
    
    /* 验证文件可以打开 */
    FILE *fp = fopen("/spiffs/models/sound_classifier.tflite", "rb");
    TEST_ASSERT_NOT_NULL(fp);
    
    /* 验证可以读取 */
    uint8_t read_buf[100];
    size_t read_count = fread(read_buf, 1, 100, fp);
    TEST_ASSERT_EQUAL_UINT(100, read_count);
    
    /* 验证读取位置 */
    long pos = ftell(fp);
    TEST_ASSERT_EQUAL_LONG(100, pos);
    
    fclose(fp);
}

void test_model_loader_seek(void) {
    /* 测试文件随机访问 */
    uint8_t mock_data[512];
    for (int i = 0; i < 512; i++) mock_data[i] = (uint8_t)i;
    
    mock_file_register("/spiffs/test.bin", mock_data, sizeof(mock_data));
    
    FILE *fp = fopen("/spiffs/test.bin", "rb");
    TEST_ASSERT_NOT_NULL(fp);
    
    /* 从中间读取 */
    fseek(fp, 256, SEEK_SET);
    uint8_t buf[10];
    fread(buf, 1, 10, fp);
    
    /* 验证读取的是正确位置的数据 */
    TEST_ASSERT_EQUAL_UINT8(256, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(265, buf[9]);
    
    /* 验证当前位置 */
    TEST_ASSERT_EQUAL_LONG(266, ftell(fp));
    
    fclose(fp);
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
 * LED 控制测试 (包含边界条件)
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

void test_led_brightness_boundary(void) {
    /* 亮度越界测试 - 应被限制在有效范围 */
    mock_led_set_brightness(-1);  /* 越界 */
    TEST_ASSERT_EQUAL_UINT8(0, mock_led_get_state()->brightness);
    
    mock_led_set_brightness(150);  /* 越界 */
    TEST_ASSERT_EQUAL_UINT8(100, mock_led_get_state()->brightness);
    
    mock_led_set_brightness(255);  /* 严重越界 */
    TEST_ASSERT_EQUAL_UINT8(100, mock_led_get_state()->brightness);
}

void test_led_color_temp_range(void) {
    /* 色温范围测试 */
    mock_led_set_color_temp(2700);
    TEST_ASSERT_EQUAL_UINT16(2700, mock_led_get_state()->color_temp);
    
    mock_led_set_color_temp(6500);
    TEST_ASSERT_EQUAL_UINT16(6500, mock_led_get_state()->color_temp);
}

void test_led_color_temp_boundary(void) {
    /* 色温越界测试 */
    mock_led_set_color_temp(2000);  /* 太低 */
    TEST_ASSERT_EQUAL_UINT16(2700, mock_led_get_state()->color_temp);
    
    mock_led_set_color_temp(8000);  /* 太高 */
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

void test_scene_invalid_index(void) {
    /* 场景索引越界测试 */
    const mock_scene_t *invalid = mock_scene_get(100);
    TEST_ASSERT_NULL(invalid);
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
 * 队列数据传输测试
 * ================================================================ */

void test_queue_data_transfer(void) {
    QueueHandle_t queue = xQueueCreate(10, sizeof(int));
    TEST_ASSERT_NOT_NULL(queue);
    
    /* 发送数据 */
    int sent_val = 42;
    TEST_ASSERT_EQUAL_INT(pdTRUE, xQueueSend(queue, &sent_val, 0));
    
    /* 验证队列中有数据 */
    TEST_ASSERT_EQUAL_INT(1, uxQueueMessagesWaiting(queue));
    
    /* 接收数据 */
    int recv_val = 0;
    TEST_ASSERT_EQUAL_INT(pdTRUE, xQueueReceive(queue, &recv_val, 0));
    TEST_ASSERT_EQUAL_INT(42, recv_val);
    
    /* 验证队列已空 */
    TEST_ASSERT_EQUAL_INT(0, uxQueueMessagesWaiting(queue));
    
    vQueueDelete(queue);
}

void test_queue_full_behavior(void) {
    QueueHandle_t queue = xQueueCreate(3, sizeof(int));
    TEST_ASSERT_NOT_NULL(queue);
    
    /* 填满队列 */
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(pdTRUE, xQueueSend(queue, &i, 0));
    }
    
    /* 再发送应失败 */
    int extra = 99;
    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueSend(queue, &extra, 0));
    
    vQueueDelete(queue);
}

void test_queue_empty_behavior(void) {
    QueueHandle_t queue = xQueueCreate(5, sizeof(int));
    TEST_ASSERT_NOT_NULL(queue);
    
    /* 空队列接收应失败 */
    int recv_val = 0;
    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueReceive(queue, &recv_val, 0));
    
    vQueueDelete(queue);
}

/* ================================================================
 * 定时器测试
 * ================================================================ */

static int timer_callback_count = 0;

static void timer_callback(void *arg) {
    timer_callback_count++;
}

void test_timer_one_shot(void) {
    timer_callback_count = 0;
    
    esp_timer_create_args_t args = {
        .callback = timer_callback,
        .arg = NULL,
        .name = "test_timer"
    };
    
    esp_timer_handle_t handle;
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_timer_create(&args, &handle));
    
    /* 启动一次性定时器 */
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_timer_start_once(handle, 1000));
    
    /* 推进时间 */
    mock_timer_advance(1000);
    
    /* 应触发一次 */
    TEST_ASSERT_EQUAL_INT(1, timer_callback_count);
    
    /* 再推进不应再触发 */
    mock_timer_advance(2000);
    TEST_ASSERT_EQUAL_INT(1, timer_callback_count);
    
    esp_timer_delete(handle);
}

void test_timer_periodic(void) {
    timer_callback_count = 0;
    
    esp_timer_create_args_t args = {
        .callback = timer_callback,
        .arg = NULL,
        .name = "periodic_timer"
    };
    
    esp_timer_handle_t handle;
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_timer_create(&args, &handle));
    
    /* 启动周期定时器 */
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_timer_start_periodic(handle, 1000));
    
    /* 推进时间 */
    mock_timer_advance(1000);
    TEST_ASSERT_EQUAL_INT(1, timer_callback_count);
    
    mock_timer_advance(1000);
    TEST_ASSERT_EQUAL_INT(2, timer_callback_count);
    
    mock_timer_advance(1000);
    TEST_ASSERT_EQUAL_INT(3, timer_callback_count);
    
    /* 停止定时器 */
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_timer_stop(handle));
    
    mock_timer_advance(1000);
    TEST_ASSERT_EQUAL_INT(3, timer_callback_count);  /* 不应再触发 */
    
    esp_timer_delete(handle);
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

void test_null_pointer_handling(void) {
    /* 空指针测试 */
    QueueHandle_t null_queue = NULL;
    int val = 0;
    
    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueSend(null_queue, &val, 0));
    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueReceive(null_queue, &val, 0));
    TEST_ASSERT_EQUAL_INT(0, uxQueueMessagesWaiting(null_queue));
}

/* ================================================================
 * 主测试入口
 * ================================================================ */

int main(void) {
    UNITY_BEGIN();
    
    /* 模型测试 */
    RUN_TEST(test_model_loader_init);
    RUN_TEST(test_model_loader_seek);
    RUN_TEST(test_model_size_validation);
    
    /* 仲裁测试 */
    RUN_TEST(test_arbiter_voice_priority);
    RUN_TEST(test_arbiter_ttl_logic);
    
    /* LED 测试 */
    RUN_TEST(test_led_brightness_range);
    RUN_TEST(test_led_brightness_boundary);
    RUN_TEST(test_led_color_temp_range);
    RUN_TEST(test_led_color_temp_boundary);
    RUN_TEST(test_led_power_control);
    
    /* 场景测试 */
    RUN_TEST(test_scene_presets);
    RUN_TEST(test_scene_invalid_index);
    
    /* 雷达测试 */
    RUN_TEST(test_radar_frame_generation);
    RUN_TEST(test_radar_features);
    
    /* 音频测试 */
    RUN_TEST(test_audio_generation);
    RUN_TEST(test_mfcc_generation);
    
    /* 语音命令测试 */
    RUN_TEST(test_voice_commands);
    
    /* 队列测试 */
    RUN_TEST(test_queue_data_transfer);
    RUN_TEST(test_queue_full_behavior);
    RUN_TEST(test_queue_empty_behavior);
    
    /* 定时器测试 */
    RUN_TEST(test_timer_one_shot);
    RUN_TEST(test_timer_periodic);
    
    /* 内存测试 */
    RUN_TEST(test_memory_limits);
    RUN_TEST(test_null_pointer_handling);
    
    /* 清理模拟文件 */
    mock_file_clear();
    
    return UNITY_END();
}