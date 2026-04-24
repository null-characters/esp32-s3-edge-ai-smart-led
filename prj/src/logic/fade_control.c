/*
 * Fade Control Implementation
 * Phase 1: HW-020
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "fade_control.h"

LOG_MODULE_REGISTER(fade, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 当前状态
 * ================================================================ */
static uint16_t current_temp = 4200;    /* 默认下午设置 */
static uint8_t  current_bright = 60;
static bool     fading = false;

/* ================================================================
 * 初始化
 * ================================================================ */
int fade_control_init(void)
{
	LOG_INF("Fade control initialized: %dK @ %d%%",
		current_temp, current_bright);
	return 0;
}

/* ================================================================
 * 渐变到目标值 (阻塞式简单实现)
 * ================================================================ */
int fade_to_target(uint16_t target_temp, uint8_t target_bright,
		   uint32_t duration_ms)
{
	if (target_temp < COLOR_TEMP_MIN || target_temp > COLOR_TEMP_MAX) {
		LOG_ERR("Invalid target temp: %d", target_temp);
		return -EINVAL;
	}
	if (target_bright > BRIGHTNESS_MAX) {
		LOG_ERR("Invalid target brightness: %d", target_bright);
		return -EINVAL;
	}

	/* 如果渐变时间为0,立即设置 */
	if (duration_ms == 0) {
		return fade_set_immediate(target_temp, target_bright);
	}

	fading = true;

	/* 计算步数 */
	uint32_t steps = duration_ms / FADE_STEP_INTERVAL_MS;
	if (steps == 0) {
		steps = 1;
	}

	/* 计算每步变化量 */
	int16_t temp_step = ((int16_t)target_temp - (int16_t)current_temp) / (int16_t)steps;
	int16_t bright_step = ((int16_t)target_bright - (int16_t)current_bright) / (int16_t)steps;

	LOG_INF("Fade start: %dK@%d%% → %dK@%d%% over %dms (%d steps)",
		current_temp, current_bright,
		target_temp, target_bright,
		duration_ms, steps);

	/* 执行渐变 */
	for (uint32_t i = 0; i < steps; i++) {
		current_temp += temp_step;
		current_bright += bright_step;

		/* 边界保护 */
		if (current_temp < COLOR_TEMP_MIN) current_temp = COLOR_TEMP_MIN;
		if (current_temp > COLOR_TEMP_MAX) current_temp = COLOR_TEMP_MAX;
		if (current_bright > BRIGHTNESS_MAX) current_bright = BRIGHTNESS_MAX;

		/* 发送到灯具 */
		int ret = lighting_set_both(current_temp, current_bright);
		if (ret < 0) {
			LOG_WRN("Fade step %d/%d failed: %d", i, steps, ret);
		}

		k_sleep(K_MSEC(FADE_STEP_INTERVAL_MS));
	}

	/* 确保最终值准确 */
	current_temp = target_temp;
	current_bright = target_bright;
	lighting_set_both(current_temp, current_bright);

	fading = false;
	LOG_INF("Fade complete: %dK @ %d%%", current_temp, current_bright);

	return 0;
}

/* ================================================================
 * 立即设置
 * ================================================================ */
int fade_set_immediate(uint16_t temp, uint8_t bright)
{
	if (temp < COLOR_TEMP_MIN || temp > COLOR_TEMP_MAX) {
		return -EINVAL;
	}
	if (bright > BRIGHTNESS_MAX) {
		return -EINVAL;
	}

	current_temp = temp;
	current_bright = bright;

	int ret = lighting_set_both(temp, bright);
	if (ret == 0) {
		LOG_INF("Set immediate: %dK @ %d%%", temp, bright);
	}
	return ret;
}

/* ================================================================
 * 检查渐变状态
 * ================================================================ */
bool fade_is_in_progress(void)
{
	return fading;
}

/* ================================================================
 * 获取当前值
 * ================================================================ */
void fade_get_current(uint16_t *temp, uint8_t *bright)
{
	if (temp) {
		*temp = current_temp;
	}
	if (bright) {
		*bright = current_bright;
	}
}
