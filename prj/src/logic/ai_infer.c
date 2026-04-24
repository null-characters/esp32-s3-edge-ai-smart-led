/*
 * AI Inference Engine Implementation - TFLM推理
 * Phase 3: AI-008 ~ AI-012, AI-014 ~ AI-020
 *
 * 功能: TFLM初始化、特征计算、推理执行、后处理、推理线程
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <string.h>

#include "ai_infer.h"
#include "model_data.h"

/* TFLM头文件 */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

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
 * TFLM内部状态 (AI-008 ~ AI-012)
 * ================================================================ */
namespace {
/* 张量工作区: 必须在内部SRAM (AI-027: 目标<8KB) */
constexpr int kTensorArenaSize = 8192;
uint8_t tensor_arena[kTensorArenaSize];

tflite::MicroErrorReporter error_reporter;
tflite::AllOpsResolver resolver;
const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input_tensor = nullptr;
TfLiteTensor *output_tensor = nullptr;

bool ai_initialized = false;
uint32_t last_inference_us = 0;
}

static inline int8_t clamp_int8(int32_t v)
{
	if (v > 127) return 127;
	if (v < -128) return -128;
	return (int8_t)v;
}

static inline float sanitize_float(float v)
{
	if (isnan(v) || isinf(v)) {
		return 0.0f;
	}
	return v;
}

/* ================================================================
 * AI-008 ~ AI-010: 初始化
 * ================================================================ */
ai_status_t ai_infer_init(void)
{
	LOG_INF("Initializing AI inference engine...");

	/* 加载模型 */
	model = tflite::GetModel(g_lighting_model_data);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		LOG_ERR("Model version mismatch: %d vs %d",
			model->version(), TFLITE_SCHEMA_VERSION);
		return AI_STATUS_ERROR_MODEL;
	}

	if (g_lighting_model_data_len == 0) {
		LOG_ERR("Model data is empty - run train_model.py first!");
		return AI_STATUS_ERROR_MODEL;
	}

	/* 创建解释器 (AI-009) */
	static tflite::MicroInterpreter static_interpreter(
		model, resolver, tensor_arena, kTensorArenaSize,
		&error_reporter);
	interpreter = &static_interpreter;

	/* 分配张量 (AI-011) */
	TfLiteStatus status = interpreter->AllocateTensors();
	if (status != kTfLiteOk) {
		LOG_ERR("AllocateTensors failed: %d", status);
		return AI_STATUS_ERROR_TENSOR;
	}

	/* 获取输入输出张量指针 (AI-011) */
	input_tensor = interpreter->input(0);
	output_tensor = interpreter->output(0);

	if (!input_tensor || !output_tensor) {
		LOG_ERR("Failed to get input/output tensors");
		return AI_STATUS_ERROR_TENSOR;
	}

	/* 打印模型信息 */
	LOG_INF("Model loaded: input=%d bytes (%d dims), output=%d bytes (%d dims)",
		input_tensor->bytes, input_tensor->dims->size,
		output_tensor->bytes, output_tensor->dims->size);
	LOG_INF("Tensor arena used: %d / %d bytes",
		interpreter->arena_used_bytes(), kTensorArenaSize);

	ai_initialized = true;
	LOG_INF("AI inference engine ready!");
	return AI_STATUS_OK;
}

bool ai_infer_is_ready(void)
{
	return ai_initialized;
}

/* ================================================================
 * AI-012, AI-017: 推理执行
 * ================================================================ */
ai_status_t ai_infer_run(const ai_features_t *features, ai_output_t *output)
{
	if (!ai_initialized || !interpreter) {
		return AI_STATUS_ERROR_INIT;
	}

	if (!features || !output) {
		return AI_STATUS_ERROR_INPUT;
	}

	/* 填充输入张量 */
	if (input_tensor->type == kTfLiteInt8) {
		/* INT8量化输入 */
		float input_scale = input_tensor->params.scale;
		int input_zero_point = input_tensor->params.zero_point;

		if (!(input_scale > 0.0f)) {
			LOG_ERR("Invalid input scale: %f", (double)input_scale);
			return AI_STATUS_ERROR_TENSOR;
		}

		float input_values[AI_FEATURE_COUNT] = {
			sanitize_float(features->hour_sin),
			sanitize_float(features->hour_cos),
			sanitize_float(features->sunset_proximity),
			sanitize_float(features->sunrise_hour_norm),
			sanitize_float(features->sunset_hour_norm),
			sanitize_float(features->weather),
			sanitize_float(features->presence)
		};

		for (int i = 0; i < AI_FEATURE_COUNT; i++) {
			float q = input_values[i] / input_scale + (float)input_zero_point;
			int32_t qi = (int32_t)lrintf(q);
			input_tensor->data.int8[i] = clamp_int8(qi);
		}
	} else {
		/* Float32输入 */
		input_tensor->data.f[0] = sanitize_float(features->hour_sin);
		input_tensor->data.f[1] = sanitize_float(features->hour_cos);
		input_tensor->data.f[2] = sanitize_float(features->sunset_proximity);
		input_tensor->data.f[3] = sanitize_float(features->sunrise_hour_norm);
		input_tensor->data.f[4] = sanitize_float(features->sunset_hour_norm);
		input_tensor->data.f[5] = sanitize_float(features->weather);
		input_tensor->data.f[6] = sanitize_float(features->presence);
	}

	/* 执行推理并计时 */
	int64_t start = k_cyc_to_ns(k_cycle_get_32()) / 1000;
	TfLiteStatus status = interpreter->Invoke();
	int64_t end = k_cyc_to_ns(k_cycle_get_32()) / 1000;

	if (status != kTfLiteOk) {
		LOG_ERR("Invoke failed: %d", status);
		return AI_STATUS_ERROR_INVOKE;
	}

	last_inference_us = (uint32_t)(end - start);

	/* 读取输出 */
	float color_temp, brightness;

	if (output_tensor->type == kTfLiteInt8) {
		float output_scale = output_tensor->params.scale;
		int output_zero_point = output_tensor->params.zero_point;

		if (!(output_scale > 0.0f)) {
			LOG_ERR("Invalid output scale: %f", (double)output_scale);
			return AI_STATUS_ERROR_TENSOR;
		}

		color_temp = (output_tensor->data.int8[0] - output_zero_point)
			     * output_scale;
		brightness = (output_tensor->data.int8[1] - output_zero_point)
			     * output_scale;
	} else {
		color_temp = output_tensor->data.f[0];
		brightness = output_tensor->data.f[1];
	}

	/* 限制范围 */
	if (color_temp < COLOR_TEMP_MIN) color_temp = COLOR_TEMP_MIN;
	if (color_temp > COLOR_TEMP_MAX) color_temp = COLOR_TEMP_MAX;
	if (brightness < 0) brightness = 0;
	if (brightness > 100) brightness = 100;

	output->color_temp = (uint16_t)color_temp;
	output->brightness = (uint8_t)brightness;

	LOG_DBG("AI infer: %dK @ %d%% (%u us)",
		output->color_temp, output->brightness, last_inference_us);

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

	constexpr float PI = 3.14159265358979f;
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
	LOG_INF("AI inference thread started");

	/* 等待AI初始化完成 */
	while (!ai_initialized) {
		k_sleep(K_SECONDS(1));
	}

	ai_thread_running = true;

	while (ai_thread_running) {
		/* 获取当前时间 */
		int hour_int = sntp_time_get_hour();
		if (hour_int < 0) {
			/* 时间未同步,跳过 */
			k_sleep(K_SECONDS(5));
			continue;
		}

		float hour = (float)hour_int +
			     (float)(k_uptime_get() % 3600000) / 3600000.0f;

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
