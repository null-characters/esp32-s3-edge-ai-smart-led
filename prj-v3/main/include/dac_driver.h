/**
 * @file dac_driver.h
 * @brief I2S DAC 驱动接口 (MAX98357A)
 * 
 * 使用独立 I2S1 通道输出音频到 DAC，避免与麦克风 I2S0 冲突
 * 用于 TTS 语音反馈播放
 */

#ifndef DAC_DRIVER_H
#define DAC_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

/* ================================================================
 * 配置常量
 * ================================================================ */

#define DAC_SAMPLE_RATE         16000   /* 默认采样率 16kHz */
#define DAC_BITS_PER_SAMPLE     16      /* 16-bit */
#define DAC_CHANNELS            1       /* 单声道 */

/* MAX98357A 默认 GPIO */
#define DAC_BCLK_GPIO           16
#define DAC_WS_GPIO             17
#define DAC_DATA_OUT_GPIO       18

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief DAC 配置参数
 */
typedef struct {
    i2s_port_t i2s_port;         /* I2S 端口号 (建议 I2S_NUM_1) */
    int bclk_gpio;               /* BCLK GPIO */
    int ws_gpio;                 /* WS (LRCK) GPIO */
    int data_out_gpio;           /* DATA OUT GPIO */
    uint32_t sample_rate;        /* 采样率 */
    int bits_per_sample;         /* 位深 */
} dac_config_t;

/**
 * @brief DAC 播放状态回调
 * @param playing 是否正在播放
 * @param user_data 用户数据
 */
typedef void (*dac_playback_callback_t)(bool playing, void *user_data);

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 DAC 驱动
 * @param config 配置参数 (NULL 使用默认配置)
 * @return ESP_OK 成功
 */
esp_err_t dac_init(const dac_config_t *config);

/**
 * @brief 释放 DAC 驱动
 */
void dac_deinit(void);

/**
 * @brief 写入音频数据到 DAC
 * @param data 音频数据 (16-bit signed PCM)
 * @param len 数据长度 (字节数)
 * @param bytes_written 实际写入字节数
 * @return ESP_OK 成功
 */
esp_err_t dac_write(const int16_t *data, size_t len, size_t *bytes_written);

/**
 * @brief 播放音频缓冲区 (阻塞式)
 * @param data 音频数据
 * @param len 数据长度 (字节数)
 * @return ESP_OK 成功
 */
esp_err_t dac_play_blocking(const int16_t *data, size_t len);

/**
 * @brief 设置音量
 * @param volume 音量 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t dac_set_volume(uint8_t volume);

/**
 * @brief 获取当前音量
 * @return 音量 (0-100)
 */
uint8_t dac_get_volume(void);

/**
 * @brief DAC 是否正在播放
 * @return true 正在播放
 */
bool dac_is_playing(void);

/**
 * @brief 停止播放
 * @return ESP_OK 成功
 */
esp_err_t dac_stop(void);

/**
 * @brief 设置播放状态回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void dac_set_playback_callback(dac_playback_callback_t callback, void *user_data);

/**
 * @brief 获取 DAC 的 I2S 端口号 (用于 AEC Reference)
 * @return I2S 端口号
 */
i2s_port_t dac_get_i2s_port(void);

#endif /* DAC_DRIVER_H */
