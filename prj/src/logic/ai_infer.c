/*
 * AI Inference Engine Implementation - TFLM推理 (Stub Version)
 * Phase 3: AI-008 ~ AI-012, AI-014 ~ AI-020
 *
 * 功能: TFLM初始化、特征计算、推理执行、后处理、推理线程
 * 注意: 此为桩实现，完整版需要集成 TensorFlow Lite Micro
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <string.h>

#include "ai_infer.h"
#include "model_data.h"

/* Phase 2模块 */
#include "sntp_time_sync.h"
#include "weather_api.h"
#include "sunrise_sunset.h"
#include "presence_state.h"
#include "time_period.h"
#include "fade_control.h"
#include "lighting.h"

LOG_MODULE_REGISTER(ai_infer, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static bool ai_initialized = false;
static uint32_t last_inference_us = 0;

static inline float sanitize_float(float v)
{
	if (isnan(v) || isinf(v)) {
		return 0.0f;
	}
	return v;
}

/* ================================================================
 * AI-008 ~ AI-010: 初始化 (Stub)
 * ================================================================ */
ai_status_t ai_infer_init(void)
{
	LOG_INF("Initializing AI inference engine (stub mode)...");

	if (g_lighting_model_data_len == 0) {
		LOG_WRN("Model data is empty - using rule-based fallback");
	}

	ai_initialized = true;
	LOG_INF("AI inference engine ready (stub mode)");
	return AI_STATUS_OK;
}

bool ai_infer_is_ready(void)
{
	return ai_initialized;
}

/* ================================================================
 * AI-012, AI-017: 推理执行 (Stub - 使用规则引擎)
 * ================================================================ */
ai_status_t ai_infer_run(const ai_features_t *features, ai_output_t *output)
{
	if (!ai_initialized) {
		return AI_STATUS_ERROR_INIT;
	}

	if (!features || !output) {
		return AI_STATUS_ERROR_INPUT;
	}

	/* Stub: 使用简单规则代替AI推理 */
	/* 无人时关灯 */
	if (features->presence < 0.5f) {
		output->color_temp = COLOR_TEMP_MIN;
		output->brightness = 0;
		return AI_STATUS_OK;
	}

	/* 根据时间设置 */
	float hour = atan2f(features->hour_sin, features->hour_cos) * 24.0f / (2.0f * 3.14159265f);
	if (hour < 0) hour += 24.0f;

	/* 白天: 冷白光, 高亮度 */
	if (hour >= 6.0f && hour < 18.0f) {
		output->color_temp = 5000;
		output->brightness = 100;
	}
	/* 傍晚: 暖白光, 中等亮度 */
	else if (hour >= 18.0f && hour < 22.0f) {
		output->color_temp = 3000;
		output->brightness = 70;
	}
	/* 深夜: 暖黄光, 低亮度 */
	else {
		output->color_temp = 2700;
		output->brightness = 30;
	}

	/* 日落临近时降低色温 */
	if (features->sunset_proximity > 0.5f) {
		output->color_temp = 2700;
		output->brightness = (uint8_t)(output->brightness * 0.8f);
	}

	/* 阴天时增加亮度 */
	if (features->weather > 50.0f) {
		output->brightness = (uint8_t)((float)output->brightness * 1.2f);
		if (output->brightness > 100) output->brightness = 100;
	}

	/* 范围限制 */
	if (output->color_temp < COLOR_TEMP_MIN) output->color_temp = COLOR_TEMP_MIN;
	if (output->color_temp > COLOR_TEMP_MAX) output->color_temp = COLOR_TEMP_MAX;
	if (output->brightness > 100) output->brightness = 100;

	last_inference_us = 100; /* Stub: 100us */

	return AI_STATUS_OK;
}

uint32_t ai_infer_get_latency_us(void)
{
	return last_inference_us;
}

/* ================================================================
 * AI-014: 时间特征计算
 * ================================================================ */
void ai_calc_time_features(float hour, ai_features_t *features)
{
	if (!features) return;

	const float PI = 3.14159265358979f;
	features->hour_sin = sinf(hour * 2.0f * PI / 24.0f);
	features->hour_cos = cosf(hour * 2.0f * PI / 24.0f);
}

/* ================================================================
 * AI-015: 日落临近度计算
 * ================================================================ */
float ai_calc_sunset_proximity(float hour, float sunset_hour)
{
	float hours_to_sunset = sunset_hour - hour;

	if (hours_to_sunset > 2.0f) {
		return 0.0f;
	} else if (hours_to_sunset >= 0.0f) {
		return 1.0f - (hours_to_sunset / 2.0f);
	} else if (hours_to_sunset > -2.0f) {
		return 1.0f;
	} else {
		float val = 1.0f + (hours_to_sunset + 2.0f) / 2.0f;
		return val > 0.0f ? val : 0.0f;
	}
}

/* ================================================================
 * AI-016: 特征向量组装
 * ================================================================ */
void ai_assemble_features(float hour, float weather, float presence,
			  float sunrise_hour, float sunset_hour,
			  ai_features_t *features)
{
	if (!features) return;

	/* 时间特征 */
	ai_calc_time_features(hour, features);

	/* 日落临近度 */
	features->sunset_proximity = ai_calc_sunset_proximity(hour, sunset_hour);

	/* 日出日落归一化 */
	features->sunrise_hour_norm = sunrise_hour / 24.0f;
	features->sunset_hour_norm = sunset_hour / 24.0f;

	/* 天气 */
	features->weather = weather;

	/* 人员存在 */
	features->presence = presence;
}

/* ================================================================
 * AI-018: 后处理
 * ================================================================ */
void ai_post_process(const ai_output_t *raw_output, float presence,
		     float hour, ai_output_t *final_output)
{
	if (!raw_output || !final_output) return;

	*final_output = *raw_output;

	/* 无人强制关灯 */
	if (presence < 0.5f) {
		final_output->brightness = 0;
		final_output->color_temp = COLOR_TEMP_MIN;
		return;
	}

	/* 深夜限幅: 22:00后最大亮度50% */
	if (hour >= 22.0f) {
		if (final_output->brightness > 50) {
			final_output->brightness = 50;
		}
	}

	/* 范围保护 */
	if (final_output->color_temp < COLOR_TEMP_MIN) {
		final_output->color_temp = COLOR_TEMP_MIN;
	}
	if (final_output->color_temp > COLOR_TEMP_MAX) {
		final_output->color_temp = COLOR_TEMP_MAX;
	}
	if (final_output->brightness > 100) {
		final_output->brightness = 100;
	}
}

/* ================================================================
 * AI-020: 推理线程
 * ================================================================ */
static bool ai_thread_running = false;

void ai_infer_thread_fn(void)
{
	LOG_INF("AI inference thread started (stub mode)");

	/* 等待AI初始化完成 */
	while (!ai_initialized) {
		k_sleep(K_SECONDS(1));
	}

	ai_thread_running = true;

	while (ai_thread_running) {
		/* 获取当前本地小时（带小数），避免整点跳变 */
		float hour = sntp_time_get_local_hour_f();
		if (hour < 0.0f) {
			/* 时间未同步,跳过 */
			k_sleep(K_SECONDS(5));
			continue;
		}

		/* 获取天气数据 */
		float weather = weather_get_code();
		if (weather < 0.0f) {
			weather = 0.0f; /* 默认晴天 */
		}

		/* 获取人员状态 */
		presence_state_t pstate = presence_get_state();
		float presence = (pstate == PRESENCE_YES) ? 1.0f : 0.0f;

		/* 获取日出日落时间 */
		float sunrise_hour = sun_get_sunrise_hour();
		float sunset_hour = sun_get_sunset_hour();
		if (sunrise_hour < 0) sunrise_hour = 6.0f; /* 默认 */
		if (sunset_hour < 0) sunset_hour = 18.5f;  /* 默认 */

		/* 组装特征 */
		ai_features_t features;
		ai_assemble_features(hour, weather, presence,
				     sunrise_hour, sunset_hour, &features);

		/* 执行推理 */
		ai_output_t raw_output;
		ai_status_t ret = ai_infer_run(&features, &raw_output);

		if (ret == AI_STATUS_OK) {
			/* 后处理 */
			ai_output_t final_output;
			ai_post_process(&raw_output, presence, hour, &final_output);

			/* 渐变到目标 (AI-019) */
			uint16_t cur_temp;
			uint8_t cur_bright;
			fade_get_current(&cur_temp, &cur_bright);

			if (final_output.color_temp != cur_temp ||
			    final_output.brightness != cur_bright) {
				LOG_INF("AI output: %dK @ %d%% → fading",
					final_output.color_temp,
					final_output.brightness);

				uint32_t duration = 2000; /* 2秒渐变 */
				if (final_output.brightness == 0) {
					duration = 1000; /* 关灯1秒 */
				}
				fade_to_target(final_output.color_temp,
					       final_output.brightness,
					       duration);
			}

			LOG_DBG("AI: hour=%.1f weather=%.2f pres=%.0f → %dK@%d%% (%uus)",
				hour, weather, presence,
				final_output.color_temp, final_output.brightness,
				last_inference_us);
		} else {
			LOG_WRN("AI inference failed: %d", ret);
		}

		k_sleep(K_MSEC(AI_INFERENCE_INTERVAL_MS));
	}
}

/* 线程控制（显式启动） */
static K_THREAD_STACK_DEFINE(ai_stack, 8192);
static struct k_thread ai_thread;
static k_tid_t ai_tid;

void ai_infer_thread_start(void)
{
	if (ai_tid) {
		return;
	}
	ai_thread_running = true;
	ai_tid = k_thread_create(&ai_thread, ai_stack,
				 K_THREAD_STACK_SIZEOF(ai_stack),
				 (k_thread_entry_t)ai_infer_thread_fn,
				 NULL, NULL, NULL,
				 5, 0, K_FOREVER);
	k_thread_name_set(ai_tid, "ai_infer");
	k_thread_start(ai_tid);
}

void ai_infer_thread_stop(void)
{
	ai_thread_running = false;
	if (ai_tid) {
		k_thread_abort(ai_tid);
		ai_tid = NULL;
	}
}
