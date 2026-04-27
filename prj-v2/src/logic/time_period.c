/*
 * Time Period Judgment Implementation
 * Phase 1: HW-018
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "time_period.h"

LOG_MODULE_REGISTER(time_period, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static uint8_t current_hour = 12;  /* 默认中午12点 */
static bool hour_set = false;

/* ================================================================
 * 根据小时数获取时段
 * ================================================================ */
time_period_t time_period_get(uint8_t hour)
{
	if (hour >= PERIOD_MORNING_START && hour < PERIOD_AFTERNOON_START) {
		return PERIOD_MORNING;
	} else if (hour >= PERIOD_AFTERNOON_START && hour < PERIOD_EVENING_START) {
		return PERIOD_AFTERNOON;
	} else {
		return PERIOD_EVENING;
	}
}

/* ================================================================
 * 获取当前时段
 * ================================================================ */
time_period_t time_period_get_current(void)
{
	if (!hour_set) {
		LOG_WRN_ONCE("Time not synchronized, using default (noon)");
	}
	return time_period_get(current_hour);
}

/* ================================================================
 * 设置当前小时 (由外部调用, 如SNTP同步后)
 * ================================================================ */
void time_period_set_hour(uint8_t hour)
{
	if (hour > 23) {
		LOG_ERR("Invalid hour: %d", hour);
		return;
	}
	current_hour = hour;
	hour_set = true;
	LOG_INF("Time period set to %s (hour=%d)",
		time_period_name(time_period_get(hour)), hour);
}

/* ================================================================
 * 获取时段名称
 * ================================================================ */
const char *time_period_name(time_period_t period)
{
	switch (period) {
	case PERIOD_MORNING:
		return "Morning";
	case PERIOD_AFTERNOON:
		return "Afternoon";
	case PERIOD_EVENING:
		return "Evening";
	default:
		return "Unknown";
	}
}
