/*
 * Rule-Based Lighting Engine
 * Phase 1: HW-019
 */

#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#include "time_period.h"
#include "presence_state.h"

/* 照明参数结构体 */
typedef struct {
	uint16_t color_temp;   /* 色温 (K) */
	uint8_t  brightness;   /* 光强 (0-100) */
} lighting_settings_t;

/* 时段默认参数 */
#define MORNING_COLOR_TEMP    4800
#define MORNING_BRIGHTNESS    65

#define AFTERNOON_COLOR_TEMP  4200
#define AFTERNOON_BRIGHTNESS  60

#define EVENING_COLOR_TEMP    3500
#define EVENING_BRIGHTNESS    55

/* 关灯设置 (无人超时后) */
#define LIGHTS_OFF_COLOR_TEMP 2700
#define LIGHTS_OFF_BRIGHTNESS 0

/**
 * @brief 初始化规则引擎
 * @return 0=成功
 */
int rule_engine_init(void);

/**
 * @brief 根据时段获取默认照明参数
 * @param period 时段枚举值
 * @param settings 输出参数结构体
 */
void rule_get_period_settings(time_period_t period, lighting_settings_t *settings);

/**
 * @brief 计算当前应有的照明参数 (综合状态+时段)
 * @param presence 人员状态
 * @param period   当前时段
 * @param settings 输出参数结构体
 */
void rule_calculate_settings(presence_state_t presence,
			     time_period_t period,
			     lighting_settings_t *settings);

/**
 * @brief 自动判断并获取当前设置 (主循环调用)
 * @param settings 输出参数结构体
 * @return 0=灯应该开启, 1=灯应该关闭 (无人超时)
 */
int rule_get_current_settings(lighting_settings_t *settings);

#endif /* RULE_ENGINE_H */
