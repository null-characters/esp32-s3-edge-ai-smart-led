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
 * P1: Min-Max 归一化参数（与 generate_data.py 一致）
 * ================================================================ */
#define COLOR_TEMP_MIN     2700
#define COLOR_TEMP_MAX     6500
#define COLOR_TEMP_RANGE   3800   /* 6500 - 2700 */
#define BRIGHTNESS_MIN     0
#define BRIGHTNESS_MAX     100
#define BRIGHTNESS_RANGE   100

/* P1: Sigmoid 输出的死区阈值 */
#define DEADZONE_LOW       0.02f  /* 2% 以下视为 0 */
#define DEADZONE_HIGH      0.98f  /* 98% 以上视为 1 */

/* ================================================================
 * 内部状态
 * ================================================================ */
static bool ai_initialized = false;
static uint32_t last_inference_us = 0;

/* ================================================================
 * P0: 非对称 Debounce 状态机
 * ================================================================
 * 设计原则：开灯要快（上升沿 0ms），关灯要慢（下降沿 5 分钟）
 * 解决问题：PIR 传感器抖动导致灯光闪烁
 */
static struct {
	bool stable_presence;      /* 稳定状态（喂给 AI） */
	bool last_raw_presence;    /* 上一次原始状态 */
	int64_t falling_edge_ts;   /* 下降沿时间戳 (ms) */
} presence_sm = {
	.stable_presence = false,
	.last_raw_presence = false,
	.falling_edge_ts = 0
};

float ai_update_stable_presence(bool raw_presence, int64_t now_ms)
{
	/* 上升沿：立即响应（开灯要快） */
	if (raw_presence && !presence_sm.last_raw_presence) {
		presence_sm.stable_presence = true;
		presence_sm.falling_edge_ts = 0;
		LOG_DBG("Presence: rising edge, immediate ON");
	}
	/* 下降沿：开始计时 */
	else if (!raw_presence && presence_sm.last_raw_presence) {
		presence_sm.falling_edge_ts = now_ms;
		LOG_DBG("Presence: falling edge, start hold timer");
	}
	/* 持续低电平：检查滞后时间（关灯要慢） */
	else if (!raw_presence && presence_sm.stable_presence) {
		if (presence_sm.falling_edge_ts > 0) {
			int64_t elapsed = now_ms - presence_sm.falling_edge_ts;
			if (elapsed >= PRESENCE_FALL_DELAY_MS) {
				presence_sm.stable_presence = false;
				LOG_INF("Presence: hold time expired (%lld ms), OFF",
					elapsed);
			}
		}
	}
	/* 持续高电平：重置下降沿计时器（用户中途活动） */
	else if (raw_presence && presence_sm.stable_presence) {
		presence_sm.falling_edge_ts = 0;
	}

	presence_sm.last_raw_presence = raw_presence;
	return presence_sm.stable_presence ? 1.0f : 0.0f;
}

float ai_get_stable_presence(void)
{
	return presence_sm.stable_presence ? 1.0f : 0.0f;
}

void ai_presence_reset(void)
{
	presence_sm.stable_presence = false;
	presence_sm.last_raw_presence = false;
	presence_sm.falling_edge_ts = 0;
	LOG_INF("Presence state machine reset");
}

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
 * ================================================================
 *
 * P1 改造：Stub 返回归一化值 [0, 1]，与真实 TFLM 输出一致
 * 后处理函数负责反归一化到物理单位
 */
ai_status_t ai_infer_run(const ai_features_t *features, ai_output_t *output)
{
	if (!ai_initialized) {
		return AI_STATUS_ERROR_INIT;
	}

	if (!features || !output) {
		return AI_STATUS_ERROR_INPUT;
	}

	/* Stub: 使用简单规则计算归一化输出 [0, 1] */
	float color_temp_norm, brightness_norm;

	/* 无人时关灯 */
	if (features->presence < 0.5f) {
		color_temp_norm = 0.0f;  /* 2700K */
		brightness_norm = 0.0f;  /* 0% */
	} else {
		/* 根据时间设置 */
		float hour = atan2f(features->hour_sin, features->hour_cos) * 24.0f / (2.0f * 3.14159265f);
		if (hour < 0) hour += 24.0f;

		/* 白天: 冷白光, 高亮度 */
		if (hour >= 6.0f && hour < 18.0f) {
			color_temp_norm = 0.605f;   /* ~5000K */
			brightness_norm = 1.0f;     /* 100% */
		}
		/* 傍晚: 暖白光, 中等亮度 */
		else if (hour >= 18.0f && hour < 22.0f) {
			color_temp_norm = 0.079f;   /* ~3000K */
			brightness_norm = 0.7f;    /* 70% */
		}
		/* 深夜: 暖黄光, 低亮度 */
		else {
			color_temp_norm = 0.0f;    /* 2700K */
			brightness_norm = 0.3f;    /* 30% */
		}

		/* 日落临近时降低色温 */
		if (features->sunset_proximity > 0.5f) {
			color_temp_norm = 0.0f;    /* 2700K */
			brightness_norm *= 0.8f;
		}

		/* 阴天时增加亮度 */
		if (features->weather > 0.5f) {
			brightness_norm = fminf(1.0f, brightness_norm * 1.2f);
		}
	}

	/* P1: 存储归一化值（通过 float 指针传递给 post_process） */
	/* 注意：这里我们临时使用 output 结构体的内存来存储归一化值 */
	/* 因为 ai_post_process 会重新解释这些值 */
	float raw_output[2] = { color_temp_norm, brightness_norm };
	memcpy(output, raw_output, sizeof(raw_output));

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

/**
 * P1: 反归一化函数 - 将 [0, 1] 输出转换为物理单位
 *
 * @param output_norm 模型输出（归一化值 [0, 1]）
 * @param result 输出结果（物理单位）
 *
 * 注意：Sigmoid 输出永远在 (0, 1) 内，但两端梯度消失
 *       使用死区处理确保精确的 0% 和 100%
 */
static void denormalize_output(const float *output_norm, ai_output_t *result)
{
	float color_temp_norm = sanitize_float(output_norm[0]);
	float brightness_norm = sanitize_float(output_norm[1]);

	/* P1: 死区处理 - 解决 Sigmoid 梯度消失问题 */
	if (brightness_norm < DEADZONE_LOW) {
		result->brightness = BRIGHTNESS_MIN;  /* 0% */
	} else if (brightness_norm > DEADZONE_HIGH) {
		result->brightness = BRIGHTNESS_MAX;  /* 100% */
	} else {
		result->brightness = (uint8_t)(brightness_norm * BRIGHTNESS_RANGE + BRIGHTNESS_MIN);
	}

	/* 色温同理 */
	if (color_temp_norm < DEADZONE_LOW) {
		result->color_temp = COLOR_TEMP_MIN;  /* 2700K */
	} else if (color_temp_norm > DEADZONE_HIGH) {
		result->color_temp = COLOR_TEMP_MAX;  /* 6500K */
	} else {
		result->color_temp = (uint16_t)(color_temp_norm * COLOR_TEMP_RANGE + COLOR_TEMP_MIN);
	}
}

void ai_post_process(const ai_output_t *raw_output, float presence,
		     float hour, ai_output_t *final_output)
{
	if (!raw_output || !final_output) return;

	/* P1: 反归一化到物理单位 */
	denormalize_output((const float *)raw_output, final_output);

	/* 无人强制关灯（使用稳定 presence） */
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

	/* P1: 范围保护（理论上不再需要，但保留作为防御性编程） */
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

		/* 获取原始 PIR 状态 */
		presence_state_t pstate = presence_get_state();
		bool raw_presence = (pstate == PRESENCE_YES);

		/* P0: 通过非对称 Debounce 状态机获取稳定 presence */
		int64_t now_ms = k_uptime_get();
		float stable_presence = ai_update_stable_presence(raw_presence, now_ms);

		/* 获取日出日落时间 */
		float sunrise_hour = sun_get_sunrise_hour();
		float sunset_hour = sun_get_sunset_hour();
		if (sunrise_hour < 0) sunrise_hour = 6.0f; /* 默认 */
		if (sunset_hour < 0) sunset_hour = 18.5f;  /* 默认 */

		/* 组装特征（使用稳定 presence） */
		ai_features_t features;
		ai_assemble_features(hour, weather, stable_presence,
				     sunrise_hour, sunset_hour, &features);

		/* 执行推理 */
		ai_output_t raw_output;
		ai_status_t ret = ai_infer_run(&features, &raw_output);

		if (ret == AI_STATUS_OK) {
			/* 后处理（使用稳定 presence） */
			ai_output_t final_output;
			ai_post_process(&raw_output, stable_presence, hour, &final_output);

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
				hour, weather, stable_presence,
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
