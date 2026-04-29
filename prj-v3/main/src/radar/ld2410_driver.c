/**
 * @file ld2410_driver.c
 * @brief LD2410 毫米波雷达驱动实现 (ESP-IDF 版本)
 * 
 * 从 prj-v2 Zephyr 版本迁移
 */

#include <esp_log.h>
#include <driver/uart.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "ld2410_driver.h"

static const char *TAG = "ld2410";

/* UART 配置 */
#define LD2410_UART_NUM     UART_NUM_1
#define LD2410_UART_BAUD    256000
#define LD2410_UART_TX_PIN  8   /* 改为 GPIO8，避免与 DAC (GPIO17/18) 冲突 */
#define LD2410_UART_RX_PIN  9   /* 改为 GPIO9，避免与 DAC (GPIO17/18) 冲突 */
#define UART_BUF_SIZE       256
#define UART_READ_TIMEOUT_MS  100  /* UART 读取超时 */

/* DMA 接收缓冲区 */
static uint8_t rx_buf[UART_BUF_SIZE];
static volatile bool data_ready = false;
static SemaphoreHandle_t data_mutex;
static bool g_initialized = false;  /**< 初始化状态 */

/* 当前帧数据 */
static ld2410_raw_data_t current_data;
static radar_features_t current_features;
static radar_history_t history_buffer;

/* 归一化参数 */
#define DISTANCE_MAX    8.0f    /* 最大距离 8m */
#define VELOCITY_MAX    5.0f    /* 最大速度 5m/s */
#define ENERGY_MAX      100.0f  /* 最大能量 */

/* 滑动平均滤波窗口 */
#define FILTER_WINDOW   5
static float distance_filter[FILTER_WINDOW];
static float velocity_filter[FILTER_WINDOW];
static float energy_filter[FILTER_WINDOW];
static int distance_filter_idx = 0;  /* 距离滤波器独立索引 */
static int velocity_filter_idx = 0;  /* 速度滤波器独立索引 */
static int energy_filter_idx = 0;    /* 能量滤波器独立索引 */
static bool filter_initialized = false;  /* 滤波器是否已用首值初始化 */

/* 呼吸检测阈值参数 */
#define BREATHING_PERIOD_MIN    0.1f    /* 呼吸周期最小值 (秒) */
#define BREATHING_PERIOD_MAX    0.5f    /* 呼吸周期最大值 (秒) */
#define BREATHING_ENERGY_MAX    0.3f    /* 呼吸时最大能量 */
#define BREATHING_VELOCITY_MAX  0.2f    /* 呼吸时最大速度 */

/* 风扇检测阈值参数 */
#define FAN_PERIOD_MIN          0.5f    /* 风扇周期最小值 (秒) */
#define FAN_ENERGY_MIN          0.3f    /* 风扇最小能量 */
#define FAN_DISTANCE_VAR_MAX    0.1f    /* 风扇时距离方差最大值 */

/* 默认运动周期 */
#define DEFAULT_MOTION_PERIOD   0.2f    /* 默认运动周期 (秒) */

/**
 * @brief 解析 LD2410 数据帧
 */
static int parse_frame(const uint8_t *buf, size_t len, ld2410_raw_data_t *data)
{
    /* 查找帧头 */
    int header_pos = -1;
    for (size_t i = 0; i < len - 4; i++) {
        if (buf[i] == LD2410_FRAME_HEADER_0 &&
            buf[i+1] == LD2410_FRAME_HEADER_1 &&
            buf[i+2] == LD2410_FRAME_HEADER_2 &&
            buf[i+3] == LD2410_FRAME_HEADER_3) {
            header_pos = i;
            break;
        }
    }
    
    if (header_pos < 0) {
        return -1;  /* 未找到帧头 */
    }
    
    /* 解析数据字段 */
    const uint8_t *frame = buf + header_pos;
    
    data->target_state = frame[4];
    data->target_count = frame[5];
    
    /* 距离和速度为浮点数 */
    memcpy(&data->distance, frame + 6, sizeof(float));
    memcpy(&data->velocity, frame + 10, sizeof(float));
    memcpy(&data->energy, frame + 14, sizeof(float));
    
    data->timestamp = esp_timer_get_time();
    
    return 0;
}

/**
 * @brief 滑动平均滤波 (带独立索引)
 * @param buffer 滤波缓冲区
 * @param idx 滤波索引指针
 * @param new_value 新值
 * @return 滤波后的平均值
 */
static float filter_value(float *buffer, int *idx, float new_value)
{
    buffer[*idx] = new_value;
    *idx = (*idx + 1) % FILTER_WINDOW;
    
    float sum = 0;
    for (int i = 0; i < FILTER_WINDOW; i++) {
        sum += buffer[i];
    }
    return sum / FILTER_WINDOW;
}

/**
 * @brief 初始化滤波缓冲区 (用首值填充)
 * @param buffer 滤波缓冲区
 * @param first_value 第一次测量值
 */
static void init_filter_buffer(float *buffer, float first_value)
{
    for (int i = 0; i < FILTER_WINDOW; i++) {
        buffer[i] = first_value;
    }
}

/**
 * @brief 特征归一化
 */
static void normalize_features(const ld2410_raw_data_t *raw, 
                                radar_features_t *features)
{
    /* 基础特征归一化 */
    features->distance_norm = raw->distance / DISTANCE_MAX;
    features->velocity_norm = raw->velocity / VELOCITY_MAX;
    features->energy_norm = raw->energy / ENERGY_MAX;
    features->target_count = (float)raw->target_count;
    
    /* 首次滤波时用首值初始化缓冲区，避免初始偏差 */
    if (!filter_initialized) {
        init_filter_buffer(distance_filter, features->distance_norm);
        init_filter_buffer(velocity_filter, features->velocity_norm);
        init_filter_buffer(energy_filter, features->energy_norm);
        filter_initialized = true;
    }
    
    /* 滤波处理 (使用独立索引) */
    features->distance_norm = filter_value(distance_filter, 
                                           &distance_filter_idx,
                                           features->distance_norm);
    features->velocity_norm = filter_value(velocity_filter, 
                                           &velocity_filter_idx,
                                           features->velocity_norm);
    features->energy_norm = filter_value(energy_filter, 
                                          &energy_filter_idx,
                                          features->energy_norm);
    
    /* 扩展特征 */
    ld2410_get_history_stats(&features->distance_variance,
                              &features->motion_period,
                              &features->energy_trend);
    
    /* 运动幅度 = 能量 * 速度绝对值 */
    features->motion_amplitude = features->energy_norm * 
                                  fabsf(features->velocity_norm);
}

int ld2410_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "LD2410 already initialized");
        return 0;
    }
    
    /* 配置 UART */
    uart_config_t uart_cfg = {
        .baud_rate = LD2410_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t ret = uart_param_config(LD2410_UART_NUM, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(ret));
        return -1;
    }
    
    ret = uart_set_pin(LD2410_UART_NUM, LD2410_UART_TX_PIN, LD2410_UART_RX_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(ret));
        return -1;
    }
    
    ret = uart_driver_install(LD2410_UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
        return -1;
    }
    
    /* 创建互斥锁 */
    data_mutex = xSemaphoreCreateMutex();
    if (!data_mutex) {
        ESP_LOGE(TAG, "创建 mutex 失败");
        return -1;
    }
    
    /* 初始化历史缓冲 */
    history_buffer.head = 0;
    history_buffer.count = 0;
    history_buffer.last_update = 0;
    
    /* 滤波缓冲区将在首次数据时用首值初始化 */
    filter_initialized = false;
    
    g_initialized = true;
    
    ESP_LOGI(TAG, "LD2410 radar driver initialized");
    return 0;
}

int ld2410_read_raw(ld2410_raw_data_t *data)
{
    if (!g_initialized) {
        return -EINVAL;
    }
    
    /* 直接轮询 UART 接收数据 */
    int bytes_read = uart_read_bytes(LD2410_UART_NUM, rx_buf, UART_BUF_SIZE, 
                                      pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));
    if (bytes_read <= 0) {
        return -EAGAIN;  /* 无数据 */
    }
    
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    
    /* 解析帧数据 */
    int ret = parse_frame(rx_buf, bytes_read, data);
    
    xSemaphoreGive(data_mutex);
    
    return ret;
}

int ld2410_get_features(radar_features_t *features)
{
    if (!g_initialized || !features) {
        return -EINVAL;
    }
    
    /* 读取原始数据 */
    ld2410_raw_data_t raw;
    int ret = ld2410_read_raw(&raw);
    if (ret < 0) {
        /* 无新数据，返回上一帧特征 */
        *features = current_features;
        return ret;
    }
    
    /* 归一化特征 */
    normalize_features(&raw, features);
    
    /* 更新历史 */
    ld2410_update_history(features);
    
    /* 保存当前特征 */
    current_features = *features;
    current_data = raw;
    
    return 0;
}

void ld2410_update_history(const radar_features_t *features)
{
    if (!g_initialized || !features) {
        return;
    }
    
    history_buffer.history[history_buffer.head] = *features;
    history_buffer.head = (history_buffer.head + 1) % RADAR_HISTORY_SIZE;
    if (history_buffer.count < RADAR_HISTORY_SIZE) {
        history_buffer.count++;
    }
    history_buffer.last_update = esp_timer_get_time();
}

void ld2410_get_history_stats(float *variance, float *period, float *trend)
{
    if (!variance || !period || !trend) {
        return;
    }
    
    if (!g_initialized || history_buffer.count < 2) {
        *variance = 0;
        *period = DEFAULT_MOTION_PERIOD;
        *trend = 0;
        return;
    }
    
    /* 计算距离方差 */
    float mean = 0, var = 0;
    int n = history_buffer.count;
    for (int i = 0; i < n; i++) {
        mean += history_buffer.history[i].distance_norm;
    }
    mean /= n;
    for (int i = 0; i < n; i++) {
        float diff = history_buffer.history[i].distance_norm - mean;
        var += diff * diff;
    }
    *variance = var / n;
    
    /* 运动周期 (使用默认值，后续可扩展为 FFT 计算) */
    *period = DEFAULT_MOTION_PERIOD;
    
    /* 能量趋势 */
    if (n >= 10) {
        float recent = 0, older = 0;
        for (int i = 0; i < 5; i++) {
            int idx = (history_buffer.head - 1 - i + RADAR_HISTORY_SIZE) % RADAR_HISTORY_SIZE;
            recent += history_buffer.history[idx].energy_norm;
        }
        for (int i = 5; i < 10; i++) {
            int idx = (history_buffer.head - 1 - i + RADAR_HISTORY_SIZE) % RADAR_HISTORY_SIZE;
            older += history_buffer.history[idx].energy_norm;
        }
        *trend = (recent - older) / 5;
    } else {
        *trend = 0;
    }
}

bool ld2410_detect_breathing(void)
{
    if (!g_initialized) {
        return false;
    }
    
    if (current_features.motion_period >= BREATHING_PERIOD_MIN && 
        current_features.motion_period <= BREATHING_PERIOD_MAX &&
        current_features.energy_norm < BREATHING_ENERGY_MAX &&
        fabsf(current_features.velocity_norm) < BREATHING_VELOCITY_MAX) {
        return true;
    }
    return false;
}

bool ld2410_detect_fan(void)
{
    if (!g_initialized) {
        return false;
    }
    
    if (current_features.motion_period >= FAN_PERIOD_MIN &&
        current_features.energy_norm >= FAN_ENERGY_MIN &&
        current_features.distance_variance < FAN_DISTANCE_VAR_MAX) {
        return true;
    }
    return false;
}

void ld2410_deinit(void)
{
    if (!g_initialized) {
        return;
    }
    
    g_initialized = false;  /* 先标记，防止重入 */
    
    if (data_mutex) {
        xSemaphoreTake(data_mutex, portMAX_DELAY);
    }
    
    /* 删除 UART 驱动 */
    uart_driver_delete(LD2410_UART_NUM);
    
    /* 清理状态 */
    filter_initialized = false;
    history_buffer.head = 0;
    history_buffer.count = 0;
    
    if (data_mutex) {
        xSemaphoreGive(data_mutex);
        vSemaphoreDelete(data_mutex);
        data_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "LD2410 radar driver deinitialized");
}