/**
 * @file tts_engine.h
 * @brief TTS (Text-to-Speech) 引擎接口
 * 
 * 用于语音反馈，如状态查询回复
 */

#ifndef TTS_ENGINE_H
#define TTS_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "Lighting_Event.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief TTS 播放完成回调
 * @param user_data 用户数据
 */
typedef void (*tts_complete_callback_t)(void *user_data);

/**
 * @brief TTS 配置
 */
typedef struct {
    uint32_t sample_rate;               /* 采样率 */
    tts_complete_callback_t callback;   /* 播放完成回调 */
    void *user_data;                    /* 用户数据 */
} tts_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 TTS 引擎
 * @param config 配置参数 (NULL 使用默认配置)
 * @return ESP_OK 成功
 */
esp_err_t tts_engine_init(const tts_config_t *config);

/**
 * @brief 释放 TTS 引擎
 */
void tts_engine_deinit(void);

/**
 * @brief 启动 TTS 播放任务 (异步播放需要)
 * @return ESP_OK 成功
 */
esp_err_t tts_engine_start(void);

/**
 * @brief 播放文本 (异步)
 * @param text 要播放的文本
 * @return ESP_OK 成功
 */
esp_err_t tts_speak(const char *text);

/**
 * @brief 播放文本 (阻塞)
 * @param text 要播放的文本
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_blocking(const char *text);

/**
 * @brief 播放场景状态
 * @param name 场景名称
 * @param brightness 亮度值
 * @param color_temp 色温值
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_scene_status(const char *name, uint8_t brightness, uint16_t color_temp);

/**
 * @brief 播放亮度状态
 * @param brightness 亮度值 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_brightness(uint8_t brightness);

/**
 * @brief 播放模式状态
 * @param is_auto 是否自动模式
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_mode(bool is_auto);

/**
 * @brief 播放电源状态
 * @param power_on 是否开机
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_power(bool power_on);

/**
 * @brief 播放色温状态
 * @param color_temp 色温值 (K)
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_color_temp(uint16_t color_temp);

/**
 * @brief 播放问候语
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_greeting(void);

/**
 * @brief 播放确认语
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_confirm(void);

/**
 * @brief 播放错误提示
 * @return ESP_OK 成功
 */
esp_err_t tts_speak_error(void);

/**
 * @brief 停止播放
 * @return ESP_OK 成功
 */
esp_err_t tts_stop(void);

/**
 * @brief 是否正在播放
 * @return true 正在播放
 */
bool tts_is_playing(void);

/**
 * @brief 设置播放完成回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void tts_set_callback(tts_complete_callback_t callback, void *user_data);

/**
 * @brief 设置语音速度
 * @param speed 速度值 (0-5, 0 最慢, 5 最快)
 */
void tts_set_speed(unsigned int speed);

/**
 * @brief 获取语音速度
 * @return 当前速度值
 */
unsigned int tts_get_speed(void);

#endif /* TTS_ENGINE_H */