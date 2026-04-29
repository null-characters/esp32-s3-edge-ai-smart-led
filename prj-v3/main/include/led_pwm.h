/**
 * @file led_pwm.h
 * @brief LED PWM 驱动接口
 * 
 * 支持多通道 PWM 调光，用于控制 LED 亮度和色温
 */

#ifndef LED_PWM_H
#define LED_PWM_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化 LED PWM 驱动
 * 
 * @return ESP_OK 成功
 */
esp_err_t led_pwm_init(void);

/**
 * @brief 设置亮度百分比
 * 
 * @param percent 亮度百分比 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t led_set_brightness(uint8_t percent);

/**
 * @brief 设置色温
 * 
 * @param kelvin 色温值 (2700-6500K)
 * @return ESP_OK 成功
 */
esp_err_t led_set_color_temp(uint16_t kelvin);

/**
 * @brief 设置 RGB 值
 * 
 * @param r 红色分量 (0-255)
 * @param g 绿色分量 (0-255)
 * @param b 蓝色分量 (0-255)
 * @return ESP_OK 成功
 */
esp_err_t led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 渐变到目标亮度
 * 
 * @param target_brightness 目标亮度 (0-100)
 * @param duration_ms 渐变时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t led_fade_to_brightness(uint8_t target_brightness, uint32_t duration_ms);

/**
 * @brief 渐变到目标色温
 * 
 * @param target_kelvin 目标色温 (Kelvin)
 * @param duration_ms 渐变时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t led_fade_to_color_temp(uint16_t target_kelvin, uint32_t duration_ms);

/**
 * @brief 获取当前亮度
 * 
 * @return 亮度百分比 (0-100)
 */
uint8_t led_get_brightness(void);

/**
 * @brief 获取当前色温
 * 
 * @return 色温值 (Kelvin)
 */
uint16_t led_get_color_temp(void);

/**
 * @brief 释放 LED PWM 驱动
 * 
 * @return ESP_OK 成功
 */
esp_err_t led_pwm_deinit(void);

#endif /* LED_PWM_H */
