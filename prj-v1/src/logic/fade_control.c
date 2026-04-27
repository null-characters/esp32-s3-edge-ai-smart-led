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
static struct k_mutex fade_lock;

/* 渐变目标/步进（非阻塞推进） */
static uint16_t g_target_temp;
static uint8_t  g_target_bright;
static int16_t  temp_step;
static int16_t  bright_step;
static uint32_t steps_left;
static bool     fade_active;

static void fade_tick(struct k_timer *timer);
K_TIMER_DEFINE(fade_timer, fade_tick, NULL);

/* ================================================================
 * 初始化
 * ================================================================ */
int fade_control_init(void)
{
	k_mutex_init(&fade_lock);
	LOG_INF("Fade control initialized: %dK @ %d%%",
		current_temp, current_bright);
	return 0;
}

/* ================================================================
 * 渐变到目标值（非阻塞）
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

	k_mutex_lock(&fade_lock, K_FOREVER);

	/* 计算步数 */
	uint32_t steps = duration_ms / FADE_STEP_INTERVAL_MS;
	if (steps == 0) {
		steps = 1;
	}

	/* 记录目标与步进 */
	g_target_temp = target_temp;
	g_target_bright = target_bright;

	temp_step = ((int32_t)target_temp - (int32_t)current_temp) / (int32_t)steps;
	bright_step = ((int32_t)target_bright - (int32_t)current_bright) / (int32_t)steps;
	if (temp_step == 0 && current_temp != target_temp) {
		temp_step = (current_temp < target_temp) ? 1 : -1;
	}
	if (bright_step == 0 && current_bright != target_bright) {
		bright_step = (current_bright < target_bright) ? 1 : -1;
	}
	steps_left = steps;
	fade_active = true;
	fading = true;

	LOG_INF("Fade start: %dK@%d%% → %dK@%d%% over %dms (%u steps)",
		current_temp, current_bright, target_temp, target_bright,
		duration_ms, steps);

	k_timer_start(&fade_timer, K_NO_WAIT, K_MSEC(FADE_STEP_INTERVAL_MS));
	k_mutex_unlock(&fade_lock);

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

	k_mutex_lock(&fade_lock, K_FOREVER);
	k_timer_stop(&fade_timer);
	fade_active = false;
	fading = false;
	current_temp = temp;
	current_bright = bright;
	k_mutex_unlock(&fade_lock);

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
	bool val;
	k_mutex_lock(&fade_lock, K_FOREVER);
	val = fading;
	k_mutex_unlock(&fade_lock);
	return val;
}

/* ================================================================
 * 获取当前值
 * ================================================================ */
void fade_get_current(uint16_t *temp, uint8_t *bright)
{
	k_mutex_lock(&fade_lock, K_FOREVER);
	if (temp) {
		*temp = current_temp;
	}
	if (bright) {
		*bright = current_bright;
	}
	k_mutex_unlock(&fade_lock);
}

static void fade_tick(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	uint16_t next_temp;
	uint8_t next_bright;
	bool done = false;

	k_mutex_lock(&fade_lock, K_FOREVER);

	if (!fade_active || steps_left == 0) {
		k_mutex_unlock(&fade_lock);
		return;
	}

	/* 计算下一步 */
	int32_t t = (int32_t)current_temp + temp_step;
	int32_t b = (int32_t)current_bright + bright_step;

	/* 边界保护 */
	if (t < COLOR_TEMP_MIN) t = COLOR_TEMP_MIN;
	if (t > COLOR_TEMP_MAX) t = COLOR_TEMP_MAX;
	if (b < BRIGHTNESS_MIN) b = BRIGHTNESS_MIN;
	if (b > BRIGHTNESS_MAX) b = BRIGHTNESS_MAX;

	next_temp = (uint16_t)t;
	next_bright = (uint8_t)b;

	current_temp = next_temp;
	current_bright = next_bright;

	if (steps_left > 0) {
		steps_left--;
	}

	/* 若接近结束，则强制落到目标 */
	if (steps_left == 0 ||
	    (current_temp == g_target_temp && current_bright == g_target_bright)) {
		current_temp = g_target_temp;
		current_bright = g_target_bright;
		fade_active = false;
		fading = false;
		done = true;
		k_timer_stop(&fade_timer);
	}

	k_mutex_unlock(&fade_lock);

	/* 发送到灯具（不在锁内，避免长时间占用） */
	int ret = lighting_set_both(current_temp, current_bright);
	if (ret < 0) {
		LOG_WRN("Fade step failed: %d", ret);
	}

	if (done) {
		LOG_INF("Fade complete: %dK @ %d%%", current_temp, current_bright);
	}
}
