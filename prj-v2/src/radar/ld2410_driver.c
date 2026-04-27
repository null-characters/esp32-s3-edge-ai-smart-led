/**
 * @file ld2410_driver.c
 * @brief LD2410 毫米波雷达驱动实现
 * 
 * Phase 1: 雷达接入模块
 * 任务 RD-004 ~ RD-012
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>
#include "ld2410_driver.h"

LOG_MODULE_REGISTER(ld2410, LOG_LEVEL_INF);

/* UART 设备 */
static const struct device *uart_dev;

/* DMA 接收缓冲区 */
#define UART_BUF_SIZE  256
static uint8_t rx_buf[2][UART_BUF_SIZE];
static int active_buf = 0;
static volatile bool data_ready = false;

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
static int filter_idx = 0;

/**
 * @brief UART DMA 接收回调
 */
static void uart_rx_callback(const struct device *dev, 
                              uint8_t *buf, size_t len, 
                              void *user_data)
{
    /* 切换缓冲区 */
    active_buf = (active_buf + 1) % 2;
    data_ready = true;
    
    /* 启动下一次接收 */
    uart_rx_enable(dev, rx_buf[active_buf], UART_BUF_SIZE, 100);
}

/**
 * @brief 帧校验和计算
 */
static uint8_t calculate_checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum & 0xFF;
}

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
    
    /* 解析数据字段 (简化版，实际协议需参考 LD2410 文档) */
    const uint8_t *frame = buf + header_pos;
    
    /* 跳过帧头，解析数据 */
    data->target_state = frame[4];
    data->target_count = frame[5];
    
    /* 距离和速度为浮点数，需按协议解析 */
    /* TODO: 根据 LD2410 实际协议调整 */
    memcpy(&data->distance, frame + 6, sizeof(float));
    memcpy(&data->velocity, frame + 10, sizeof(float));
    memcpy(&data->energy, frame + 14, sizeof(float));
    
    data->timestamp = k_uptime_get();
    
    return 0;
}

/**
 * @brief 滑动平均滤波
 */
static float filter_value(float *buffer, float new_value)
{
    buffer[filter_idx] = new_value;
    filter_idx = (filter_idx + 1) % FILTER_WINDOW;
    
    float sum = 0;
    for (int i = 0; i < FILTER_WINDOW; i++) {
        sum += buffer[i];
    }
    return sum / FILTER_WINDOW;
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
    
    /* 滤波处理 */
    features->distance_norm = filter_value(distance_filter, 
                                            features->distance_norm);
    features->velocity_norm = filter_value(velocity_filter, 
                                            features->velocity_norm);
    features->energy_norm = filter_value(energy_filter, 
                                          features->energy_norm);
    
    /* 扩展特征 (需历史数据) */
    ld2410_get_history_stats(&features->distance_variance,
                              &features->motion_period,
                              &features->energy_trend);
    
    /* 运动幅度 = 能量 * 速度绝对值 */
    features->motion_amplitude = features->energy_norm * 
                                  fabsf(features->velocity_norm);
}

int ld2410_init(void)
{
    /* 获取 UART 设备 */
    uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART1 device not ready");
        return -ENODEV;
    }
    
    /* 配置 UART: 256000 波特率, 8N1 */
    struct uart_config cfg = {
        .baudrate = 256000,
        .data_bits = UART_CFG_DATA_BITS_8,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    uart_configure(uart_dev, &cfg);
    
    /* 启动 DMA 接收 */
    uart_rx_enable(uart_dev, rx_buf[0], UART_BUF_SIZE, 100);
    
    /* 初始化历史缓冲 */
    history_buffer.head = 0;
    history_buffer.count = 0;
    history_buffer.last_update = 0;
    
    /* 初始化滤波缓冲 */
    memset(distance_filter, 0, sizeof(distance_filter));
    memset(velocity_filter, 0, sizeof(velocity_filter));
    memset(energy_filter, 0, sizeof(energy_filter));
    
    LOG_INF("LD2410 radar driver initialized");
    return 0;
}

int ld2410_read_raw(ld2410_raw_data_t *data)
{
    if (!data_ready) {
        return -EAGAIN;
    }
    
    /* 解析帧数据 */
    int ret = parse_frame(rx_buf[(active_buf + 1) % 2], UART_BUF_SIZE, data);
    if (ret < 0) {
        return ret;
    }
    
    data_ready = false;
    return 0;
}

int ld2410_get_features(radar_features_t *features)
{
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
    history_buffer.history[history_buffer.head] = *features;
    history_buffer.head = (history_buffer.head + 1) % RADAR_HISTORY_SIZE;
    if (history_buffer.count < RADAR_HISTORY_SIZE) {
        history_buffer.count++;
    }
    history_buffer.last_update = k_uptime_get();
}

void ld2410_get_history_stats(float *variance, float *period, float *trend)
{
    if (history_buffer.count < 2) {
        *variance = 0;
        *period = 0;
        *trend = 0;
        return;
    }
    
    /* 计算距离方差 (多人检测) */
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
    
    /* 计算运动周期 (呼吸检测) */
    /* TODO: FFT 分析速度序列 */
    *period = 0.2f;  /* 默认呼吸频率 0.2Hz */
    
    /* 计算能量趋势 (进入/离开) */
    if (n >= 10) {
        float recent = 0, older = 0;
        for (int i = 0; i < 5; i++) {
            recent += history_buffer.history[history_buffer.head - 1 - i].energy_norm;
        }
        for (int i = 5; i < 10; i++) {
            older += history_buffer.history[history_buffer.head - 1 - i].energy_norm;
        }
        *trend = (recent - older) / 5;
    } else {
        *trend = 0;
    }
}

bool ld2410_detect_breathing(void)
{
    /* 呼吸特征: 周期 0.1-0.5Hz, 能量低, 速度小 */
    if (current_features.motion_period >= 0.1f && 
        current_features.motion_period <= 0.5f &&
        current_features.energy_norm < 0.3f &&
        fabsf(current_features.velocity_norm) < 0.2f) {
        return true;
    }
    return false;
}

bool ld2410_detect_fan(void)
{
    /* 风扇特征: 周期规律, 能量中等, 位置固定 */
    if (current_features.motion_period >= 0.5f &&
        current_features.energy_norm >= 0.3f &&
        current_features.distance_variance < 0.1f) {
        return true;
    }
    return false;
}