/**
 * @file i2s_driver.h
 * @brief I2S 驱动接口
 * 
 * 用于 MEMS 麦克风 (INMP441) 音频采集
 */

#ifndef I2S_DRIVER_H
#define I2S_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/**
 * @brief I2S 配置参数
 */
typedef struct {
    uint32_t sample_rate;     ///< 采样率 (默认 16000)
    uint8_t bits_per_sample;  ///< 位深 (默认 16)
    uint8_t channels;         ///< 通道数 (默认 1)
    int ws_gpio;              ///< Word Select GPIO
    int sd_gpio;              ///< Serial Data GPIO
    int clk_gpio;             ///< Clock GPIO
} i2s_config_t;

/**
 * @brief 初始化 I2S 驱动
 * 
 * @param sample_rate 采样率 (Hz)
 * @param bits_per_sample 位深 (16/24/32)
 * @return ESP_OK 成功
 */
esp_err_t i2s_init(uint32_t sample_rate, uint8_t bits_per_sample);

/**
 * @brief 读取音频数据
 * 
 * @param data 音频数据缓冲区
 * @param samples 要读取的采样点数
 * @param bytes_read 实际读取的字节数 (输出)
 * @return ESP_OK 成功
 */
esp_err_t i2s_read(int16_t *data, size_t samples, size_t *bytes_read);

/**
 * @brief 读取音频数据 (带超时)
 * 
 * @param data 音频数据缓冲区
 * @param samples 要读取的采样点数
 * @param bytes_read 实际读取的字节数 (输出)
 * @param timeout_ms 超时时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t i2s_read_timeout(int16_t *data, size_t samples, size_t *bytes_read, 
                           uint32_t timeout_ms);

/**
 * @brief 释放 I2S 驱动
 * 
 * @return ESP_OK 成功
 */
esp_err_t i2s_deinit(void);

/**
 * @brief 获取当前采样率
 * 
 * @return 采样率 (Hz)
 */
uint32_t i2s_get_sample_rate(void);

/**
 * @brief 清空 DMA 缓冲区
 * 
 * @return ESP_OK 成功
 */
esp_err_t i2s_clear_dma_buffer(void);

#endif /* I2S_DRIVER_H */