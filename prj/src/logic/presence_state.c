/*
 * Presence Detection State Machine Implementation
 * Phase 1: HW-017
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "presence_state.h"
#include "pir.h"

LOG_MODULE_REGISTER(presence, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static presence_state_t current_state = PRESENCE_NO;
static int64_t last_presence_time = 0;
static int64_t state_enter_time = 0;
static struct k_mutex presence_lock;

/* ================================================================
 * 初始化
 * ================================================================ */
int presence_state_init(void)
{
	int64_t now = k_uptime_get();

	k_mutex_init(&presence_lock);

	k_mutex_lock(&presence_lock, K_FOREVER);
	current_state = PRESENCE_NO;
	last_presence_time = now;
	state_enter_time = now;
	k_mutex_unlock(&presence_lock);

	LOG_INF("Presence FSM initialized: state=NO, timeout=%dms",
		PRESENCE_TIMEOUT_MS);
	return 0;
}

/* ================================================================
 * 更新状态机
 * ================================================================ */
presence_state_t presence_state_update(void)
{
	bool presence = pir_get_status();
	int64_t now = k_uptime_get();
	presence_state_t old_state;

	k_mutex_lock(&presence_lock, K_FOREVER);
	old_state = current_state;

	if (presence) {
		/* 检测到人员 */
		if (current_state != PRESENCE_YES) {
			current_state = PRESENCE_YES;
			last_presence_time = now;
			state_enter_time = now;
			LOG_INF("Presence detected: state→YES");
		}
	} else {
		/* 未检测到人员 */
		if (current_state == PRESENCE_YES) {
			/* 首次进入无人状态 */
			current_state = PRESENCE_NO;
			last_presence_time = now;
			state_enter_time = now;
			LOG_INF("Presence lost: state→NO, timer started");
		} else if (current_state == PRESENCE_NO) {
			/* 检查是否超时 */
			int64_t elapsed = now - state_enter_time;
			if (elapsed >= PRESENCE_TIMEOUT_MS) {
				current_state = PRESENCE_TIMEOUT;
				state_enter_time = now;
				LOG_INF("Presence timeout: state→TIMEOUT (elapsed=%ds)",
					(int32_t)(elapsed / 1000));
			}
		}
		/* PRESENCE_TIMEOUT 状态下保持不变 */
	}

	presence_state_t new_state = current_state;
	k_mutex_unlock(&presence_lock);

	if (old_state != new_state) {
		LOG_DBG("State transition: %d→%d", old_state, new_state);
	}

	return new_state;
}

/* ================================================================
 * 获取当前状态
 * ================================================================ */
presence_state_t presence_get_state(void)
{
	presence_state_t s;
	k_mutex_lock(&presence_lock, K_FOREVER);
	s = current_state;
	k_mutex_unlock(&presence_lock);
	return s;
}

/* ================================================================
 * 获取无人时间
 * ================================================================ */
uint32_t presence_get_absence_seconds(void)
{
	int64_t now = k_uptime_get();
	uint32_t sec;
	k_mutex_lock(&presence_lock, K_FOREVER);
	if (current_state == PRESENCE_YES) {
		sec = 0;
	} else {
		sec = (uint32_t)((now - state_enter_time) / 1000);
	}
	k_mutex_unlock(&presence_lock);
	return sec;
}

/* ================================================================
 * 重置计时器
 * ================================================================ */
void presence_reset_timer(void)
{
	int64_t now = k_uptime_get();
	k_mutex_lock(&presence_lock, K_FOREVER);
	last_presence_time = now;
	state_enter_time = now;
	k_mutex_unlock(&presence_lock);
	LOG_DBG("Presence timer reset");
}
