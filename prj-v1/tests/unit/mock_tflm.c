/*
 * Mock TFLM (TensorFlow Lite Micro) - 测试桩实现
 * 替代真实TFLM库依赖
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "ai_infer.h"

/* 占位模型数据 (测试用) */
static bool mock_ai_ready = false;
static uint32_t mock_latency_us = 1500;  /* 模拟1.5ms推理延迟 */

ai_status_t ai_infer_init(void)
{
	mock_ai_ready = true;
	return AI_STATUS_OK;
}

bool ai_infer_is_ready(void)
{
	return mock_ai_ready;
}

ai_status_t ai_infer_run(const ai_features_t *features, ai_output_t *output)
{
	if (!mock_ai_ready) return AI_STATUS_ERROR_INIT;
	if (!features || !output) return AI_STATUS_ERROR_INPUT;

	/* 模拟推理: 简化规则 */
	if (features->presence < 0.5f) {
		output->color_temp = 2700;
		output->brightness = 0;
	} else {
		/* 基于时段的简化输出 */
		float hour = 0;
		/* 反推小时 (近似) */
		if (features->hour_cos > 0.5f) hour = 8;
		else if (features->hour_cos < -0.5f) hour = 14;
		else hour = 18;

		if (hour < 12) {
			output->color_temp = 4800;
			output->brightness = 65;
		} else if (hour < 17) {
			output->color_temp = 4200;
			output->brightness = 60;
		} else {
			output->color_temp = 3500;
			output->brightness = 55;
		}

		/* 天气补偿 */
		output->brightness += (uint8_t)(features->weather * 25);

		/* 日落补偿 */
		output->brightness += (uint8_t)(features->sunset_proximity * 15);

		/* 限制 */
		if (output->brightness > 100) output->brightness = 100;
	}

	mock_latency_us = 1200 + (uint32_t)(features->weather * 300);
	return AI_STATUS_OK;
}

uint32_t ai_infer_get_latency_us(void)
{
	return mock_latency_us;
}

void ai_infer_thread_start(void) {}
void ai_infer_thread_stop(void) {}
