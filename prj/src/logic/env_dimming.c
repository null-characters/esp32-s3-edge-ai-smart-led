/*
 * Environment-Aware Dimming Implementation - 环境感知调光
 * Phase 2: WF-026 ~ WF-030
 *
 * 功能: 日落临近度、天气补偿、日落渐变、融合算法
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include "env_dimming.h"
#include "weather_api.h"
#include "sunrise_sunset.h"
#include "sntp_time_sync.h"
#include "presence_state.h"
#include "time_period.h"

LOG_MODULE_REGISTER(env_dim, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 初始化 (WF-029)
 * ================================================================ */
int env_dimming_init(void)
{
	LOG_INF("Environment-aware dimming initialized");
	return 0;
}

/* ================================================================
 * 日落临近度计算 (WF-026)
 * 返回值: 0.0 = 远离日落, 1.0 = 日落时刻
 * ================================================================ */
float env_calc_sunset_proximity(void)
{
	/* 使用sunrise_sunset模块的计算 */
	return sun_get_sunset_proximity();
}

/* ================================================================
 * 天气补偿计算 (WF-027)
 * 晴天: 0% 补偿
 * 阴天: +18% 补偿
 * 雨雪: +25% 补偿
 * ================================================================ */
float env_calc_weather_compensation(float weather_code)
{
	if (weather_code < 0.0f) {
		return 0.0f; /* 无效数据,不补偿 */
	}

	/* 线性映射: weather_code 0~1 → compensation 0~WEATHER_COMP_MAX */
	return weather_code * WEATHER_COMP_MAX;
}

/* ================================================================
 * 日落渐变补偿计算 (WF-028)
 * 日落前2小时: 渐变增强
 * 日落后: 逐渐恢复正常
 * ================================================================ */
float env_calc_sunset_compensation(float proximity)
{
	if (proximity <= 0.0f) {
		return 0.0f;
	}

	/* proximity 0~1 → compensation 0~SUNSET_PROX_MAX */
	/* 日落时刻 proximity=1, 补偿最大 */
	return proximity * SUNSET_PROX_MAX;
}

/* ================================================================
 * 增强调光引擎 (WF-029)
 * 融合: 基础时段参数 + 天气补偿 + 日落补偿
 * ================================================================ */
int env_dimming_calculate(int presence, int period, env_dimming_result_t *result)
{
	if (!result) {
		return -EINVAL;
	}

	/* 1. 获取基础时段参数 */
	lighting_settings_t base;
	rule_get_period_settings((time_period_t)period, &base);
	result->base = base;

	/* 2. 如果无人, 直接返回关灯参数 */
	if (presence == PRESENCE_TIMEOUT) {
		result->final_.color_temp = LIGHTS_OFF_COLOR_TEMP;
		result->final_.brightness = LIGHTS_OFF_BRIGHTNESS;
		result->weather_comp = 0.0f;
		result->sunset_comp = 0.0f;
		return 0;
	}

	/* 3. 获取天气补偿 */
	float weather_code = weather_get_code();
	if (weather_code < 0.0f) {
		weather_code = 0.0f; /* 无效数据,使用默认晴天 */
	}
	result->weather_comp = env_calc_weather_compensation(weather_code);

	/* 4. 获取日落补偿 */
	float proximity = env_calc_sunset_proximity();
	result->sunset_comp = env_calc_sunset_compensation(proximity);

	/* 5. 计算最终参数 */
	result->final_.color_temp = base.color_temp;

	/* 光强 = 基础 + 天气补偿 + 日落补偿 */
	int total_brightness = (int)base.brightness +
			       (int)result->weather_comp +
			       (int)result->sunset_comp;

	/* 限制范围 0-100 */
	if (total_brightness > BRIGHTNESS_MAX) {
		total_brightness = BRIGHTNESS_MAX;
	}
	if (total_brightness < BRIGHTNESS_MIN) {
		total_brightness = BRIGHTNESS_MIN;
	}

	result->final_.brightness = (uint8_t)total_brightness;

	LOG_DBG("Dimming: base=%d%% + weather=%.1f%% + sunset=%.1f%% → %d%%",
		base.brightness, result->weather_comp, result->sunset_comp,
		result->final_.brightness);

	return 0;
}

/* ================================================================
 * 便捷函数: 获取增强后的照明参数
 * ================================================================ */
int env_get_enhanced_settings(lighting_settings_t *settings)
{
	if (!settings) {
		return -EINVAL;
	}

	env_dimming_result_t result;
	int presence = presence_get_state();
	int period = time_period_get_current();

	int ret = env_dimming_calculate(presence, period, &result);
	if (ret == 0) {
		*settings = result.final_;
	}

	return ret;
}

/* ================================================================
 * 联调测试 (WF-030)
 * 模拟不同天气和时段,验证调光正确性
 * ================================================================ */
void env_dimming_test(void)
{
	LOG_INF("=== Environment Dimming Test ===");

	/* 测试场景 */
	struct test_case {
		const char *name;
		float weather_code;
		float sunset_proximity;
		int base_brightness;
	};

	struct test_case cases[] = {
		{"晴天上午",    WEATHER_CODE_SUNNY,         0.0f, 65},
		{"阴天上午",    WEATHER_CODE_OVERCAST,      0.0f, 65},
		{"雨天下午",    WEATHER_CODE_RAIN_SNOW,     0.0f, 60},
		{"晴天日落前",  WEATHER_CODE_SUNNY,         0.5f, 55},
		{"阴天日落",    WEATHER_CODE_OVERCAST,      1.0f, 55},
		{"晴天日落后",  WEATHER_CODE_SUNNY,         0.75f, 50},
	};

	for (int i = 0; i < ARRAY_SIZE(cases); i++) {
		float weather_comp = env_calc_weather_compensation(cases[i].weather_code);
		float sunset_comp = env_calc_sunset_compensation(cases[i].sunset_proximity);
		int final_brightness = cases[i].base_brightness +
				       (int)weather_comp + (int)sunset_comp;
		if (final_brightness > 100) final_brightness = 100;

		LOG_INF("场景: %s", cases[i].name);
		LOG_INF("  天气编码: %.2f → 补偿 %.1f%%", cases[i].weather_code, weather_comp);
		LOG_INF("  日落临近: %.2f → 补偿 %.1f%%", cases[i].sunset_proximity, sunset_comp);
		LOG_INF("  基础光强: %d%% → 最终: %d%%", cases[i].base_brightness, final_brightness);
	}

	LOG_INF("=== Test Complete ===");
}
