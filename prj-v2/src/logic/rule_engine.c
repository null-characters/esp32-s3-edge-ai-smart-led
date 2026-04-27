/*
 * Rule-Based Lighting Engine Implementation
 * Phase 1: HW-019
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "rule_engine.h"
#include "presence_state.h"
#include "time_period.h"

LOG_MODULE_REGISTER(rule_engine, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 时段参数表
 * ================================================================ */
static const lighting_settings_t period_defaults[] = {
	[PERIOD_MORNING]   = { .color_temp = MORNING_COLOR_TEMP,
			       .brightness = MORNING_BRIGHTNESS },
	[PERIOD_AFTERNOON] = { .color_temp = AFTERNOON_COLOR_TEMP,
			       .brightness = AFTERNOON_BRIGHTNESS },
	[PERIOD_EVENING]   = { .color_temp = EVENING_COLOR_TEMP,
			       .brightness = EVENING_BRIGHTNESS },
};

/* ================================================================
 * 初始化
 * ================================================================ */
int rule_engine_init(void)
{
	LOG_INF("Rule engine initialized");
	LOG_INF("Morning:   %dK @ %d%%", MORNING_COLOR_TEMP, MORNING_BRIGHTNESS);
	LOG_INF("Afternoon: %dK @ %d%%", AFTERNOON_COLOR_TEMP, AFTERNOON_BRIGHTNESS);
	LOG_INF("Evening:   %dK @ %d%%", EVENING_COLOR_TEMP, EVENING_BRIGHTNESS);
	return 0;
}

/* ================================================================
 * 获取时段默认参数
 * ================================================================ */
void rule_get_period_settings(time_period_t period, lighting_settings_t *settings)
{
	if (!settings) {
		return;
	}

	if (period > PERIOD_EVENING) {
		period = PERIOD_EVENING;
	}

	settings->color_temp = period_defaults[period].color_temp;
	settings->brightness = period_defaults[period].brightness;
}

/* ================================================================
 * 计算照明参数 (综合判断)
 * ================================================================ */
void rule_calculate_settings(presence_state_t presence,
			     time_period_t period,
			     lighting_settings_t *settings)
{
	if (!settings) {
		return;
	}

	if (presence == PRESENCE_TIMEOUT) {
		/* 无人超时: 关灯 */
		settings->color_temp = LIGHTS_OFF_COLOR_TEMP;
		settings->brightness = LIGHTS_OFF_BRIGHTNESS;
		LOG_DBG("Rule: lights OFF (timeout)");
	} else {
		/* 有人或刚离开: 按时段设置 */
		rule_get_period_settings(period, settings);
		LOG_DBG("Rule: %s mode, %dK @ %d%%",
			time_period_name(period),
			settings->color_temp,
			settings->brightness);
	}
}

/* ================================================================
 * 获取当前应有设置 (自动判断)
 * ================================================================ */
int rule_get_current_settings(lighting_settings_t *settings)
{
	if (!settings) {
		return -EINVAL;
	}

	presence_state_t presence = presence_get_state();
	time_period_t period = time_period_get_current();

	rule_calculate_settings(presence, period, settings);

	/* 返回是否应该关灯 */
	return (presence == PRESENCE_TIMEOUT) ? 1 : 0;
}
