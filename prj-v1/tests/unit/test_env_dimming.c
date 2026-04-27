/*
 * Environment-Aware Dimming Unit Tests
 * Phase 2: WF-026 ~ WF-030 TDD
 */

#include <zephyr/ztest.h>
#include "env_dimming.h"
#include "weather_api.h"
#include "sunrise_sunset.h"
#include "rule_engine.h"
#include "presence_state.h"

/* ================================================================
 * 天气补偿计算测试 (WF-027)
 * ================================================================ */
ZTEST(env_dimming, test_weather_compensation_sunny)
{
	float comp = env_calc_weather_compensation(WEATHER_CODE_SUNNY);
	zassert_within(0.0f, comp, 0.1f, "Sunny should have 0% compensation");
}

ZTEST(env_dimming, test_weather_compensation_overcast)
{
	float comp = env_calc_weather_compensation(WEATHER_CODE_OVERCAST);
	zassert_within(18.75f, comp, 0.1f, "Overcast should have ~19% compensation");
}

ZTEST(env_dimming, test_weather_compensation_rain)
{
	float comp = env_calc_weather_compensation(WEATHER_CODE_RAIN_SNOW);
	zassert_within(25.0f, comp, 0.1f, "Rain should have 25% compensation");
}

ZTEST(env_dimming, test_weather_compensation_linear)
{
	/* 验证线性关系 */
	for (float code = 0.0f; code <= 1.0f; code += 0.25f) {
		float comp = env_calc_weather_compensation(code);
		float expected = code * WEATHER_COMP_MAX;
		zassert_within(expected, comp, 0.5f, "Compensation should be linear");
	}
}

/* ================================================================
 * 日落补偿计算测试 (WF-028)
 * ================================================================ */
ZTEST(env_dimming, test_sunset_compensation_zero)
{
	float comp = env_calc_sunset_compensation(0.0f);
	zassert_within(0.0f, comp, 0.1f, "Zero proximity should have 0% compensation");
}

ZTEST(env_dimming, test_sunset_compensation_max)
{
	float comp = env_calc_sunset_compensation(1.0f);
	zassert_within(15.0f, comp, 0.1f, "Max proximity should have 15% compensation");
}

ZTEST(env_dimming, test_sunset_compensation_half)
{
	float comp = env_calc_sunset_compensation(0.5f);
	zassert_within(7.5f, comp, 0.5f, "Half proximity should have ~8% compensation");
}

/* ================================================================
 * 增强调光计算测试 (WF-029)
 * ================================================================ */
ZTEST(env_dimming, test_dimming_offline_mode)
{
	env_dimming_init();

	env_dimming_result_t result;
	int ret = env_dimming_calculate(PRESENCE_YES, PERIOD_MORNING, &result);

	zassert_equal(0, ret, "Calculate should succeed");
	/* 无网络数据时, 使用默认值 */
	zassert_true(result.final_.brightness > 0, "Brightness should be positive");
}

ZTEST(env_dimming, test_dimming_timeout_mode)
{
	env_dimming_init();

	env_dimming_result_t result;
	int ret = env_dimming_calculate(PRESENCE_TIMEOUT, PERIOD_MORNING, &result);

	zassert_equal(0, ret, "Calculate should succeed");
	zassert_equal(0, result.final_.brightness, "Timeout should turn off lights");
	zassert_equal(LIGHTS_OFF_COLOR_TEMP, result.final_.color_temp, "Color temp should be min");
}

ZTEST(env_dimming, test_dimming_compensation_limits)
{
	env_dimming_init();

	/* 设置极端天气和日落补偿 */
	env_dimming_result_t result;

	/* 即使补偿很大, 最终光强也不应超过100% */
	for (int period = 0; period <= 2; period++) {
		for (int presence = 0; presence <= 2; presence++) {
			int ret = env_dimming_calculate(presence, period, &result);
			if (ret == 0 && presence != PRESENCE_TIMEOUT) {
				zassert_true(result.final_.brightness <= 100,
					     "Brightness should not exceed 100");
				zassert_true(result.final_.brightness >= 0,
					     "Brightness should not be negative");
			}
		}
	}
}

ZTEST(env_dimming, test_dimming_integration)
{
	env_dimming_init();

	/* 测试完整计算流程 */
	env_dimming_result_t result;
	int ret = env_dimming_calculate(PRESENCE_YES, PERIOD_AFTERNOON, &result);

	zassert_equal(0, ret, "Calculate should succeed");

	/* 验证结构体完整性 */
	zassert_true(result.base.brightness > 0, "Base brightness should be set");
	zassert_true(result.weather_comp >= 0, "Weather comp should be >= 0");
	zassert_true(result.sunset_comp >= 0, "Sunset comp should be >= 0");
	zassert_true(result.final_.brightness >= result.base.brightness ||
		     result.weather_comp + result.sunset_comp == 0,
		     "Final brightness should be >= base (if compensation present)");
}

ZTEST_SUITE(env_dimming, NULL, NULL, NULL, NULL, NULL);
