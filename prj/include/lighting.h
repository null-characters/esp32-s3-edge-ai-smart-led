/*
 * Lighting Command Interface - 驱动板调光控制
 * Phase 1: HW-013
 */

#ifndef LIGHTING_H
#define LIGHTING_H

#include <stdint.h>

/* 命令码定义 (驱动板协议) */
#define CMD_SET_COLOR_TEMP   0x01  /* 设置色温 (小端16位) */
#define CMD_SET_BRIGHTNESS   0x02  /* 设置光强 (1字节百分比) */
#define CMD_SET_BOTH         0x03  /* 同时设置色温+光强 */
#define CMD_GET_STATUS       0x04  /* 查询驱动板状态 */

/* 响应状态码 */
#define RESP_ACK             0x00  /* 成功确认 */
#define RESP_NAK             0xFF  /* 失败/拒绝 */

/* 参数范围 */
#define COLOR_TEMP_MIN       2700
#define COLOR_TEMP_MAX       6500
#define BRIGHTNESS_MIN       0
#define BRIGHTNESS_MAX       100

/**
 * @brief 初始化照明控制模块
 * @return 0=成功, 负值=失败
 */
int lighting_init(void);

/**
 * @brief 设置灯具色温
 * @param color_temp 色温值 (2700-6500K)
 * @return 0=成功, 负值=失败
 */
int lighting_set_color_temp(uint16_t color_temp);

/**
 * @brief 设置灯具光强
 * @param brightness 光强百分比 (0-100)
 * @return 0=成功, 负值=失败
 */
int lighting_set_brightness(uint8_t brightness);

/**
 * @brief 同时设置色温和光强
 * @param color_temp 色温值 (2700-6500K)
 * @param brightness 光强百分比 (0-100)
 * @return 0=成功, 负值=失败
 */
int lighting_set_both(uint16_t color_temp, uint8_t brightness);

/**
 * @brief 查询驱动板当前状态
 * @return 0=成功, 负值=失败
 */
int lighting_get_status(void);

#endif /* LIGHTING_H */
