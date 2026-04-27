/*
 * AI Feature Engineering Unit Tests
 * Phase 3: AI-014 ~ AI-016 TDD
 */

#include <zephyr/ztest.h>
#include <math.h>
#include "ai_infer.h"

#define PI 3.14159265358979f
#define FLOAT_TOLERANCE 0.01f

/* ================================================================
 * AI-014: 时间特征计算测试
 * ================================================================ */
ZTEST(ai_features, test_time_features_morning)
{
	ai_features_t features;
	ai_calc_time_features(6.0f, &features);

	/* 6:00 → sin(6*2π/24) = sin(π/2) ≈ 1.0 */
	zassert_within(1.0f, features.hour_sin, FLOAT_TOLERANCE,
		"6AM sin should be ~1.0");
	/* cos(π/2) = 0 */
	zassert_within(0.0f, features.hour_cos, FLOAT_TOLERANCE,
		"6AM cos should be ~0.0");
}

ZTEST(ai_features, test_time_features_noon)
{
	ai_features_t features;
	ai_calc_time_features(12.0f, &features);

	/* 12:00 → sin(π) = 0, cos(π) = -1 */
	zassert_within(0.0f, features.hour_sin, FLOAT_TOLERANCE,
		"Noon sin should be ~0.0");
	zassert_within(-1.0f, features.hour_cos, FLOAT_TOLERANCE,
		"Noon cos should be ~-1.0");
}

ZTEST(ai_features, test_time_features_midnight)
{
	ai_features_t features;
	ai_calc_time_features(0.0f, &features);

	/* 0:00 → sin(0) = 0, cos(0) = 1 */
	zassert_within(0.0f, features.hour_sin, FLOAT_TOLERANCE,
		"Midnight sin should be ~0.0");
	zassert_within(1.0f, features.hour_cos, FLOAT_TOLERANCE,
		"Midnight cos should be ~1.0");
}

ZTEST(ai_features, test_time_features_evening)
{
	ai_features_t features;
	ai_calc_time_features(18.0f, &features);

	/* 18:00 → sin(3π/2) = -1, cos(3π/2) = 0 */
	zassert_within(-1.0f, features.hour_sin, FLOAT_TOLERANCE,
		"6PM sin should be ~-1.0");
	zassert_within(0.0f, features.hour_cos, FLOAT_TOLERANCE,
		"6PM cos should be ~0.0");
}

/* ================================================================
 * AI-015: 日落临近度计算测试
 * ================================================================ */
ZTEST(ai_features, test_sunset_proximity_far)
{
	/* 日落前3小时 */
	float prox = ai_calc_sunset_proximity(15.0f, 18.0f);
	zassert_within(0.0f, prox, FLOAT_TOLERANCE,
		"3 hours before sunset should be 0");
}

ZTEST(ai_features, test_sunset_proximity_approaching)
{
	/* 日落前1小时 */
	float prox = ai_calc_sunset_proximity(17.0f, 18.0f);
	zassert_within(0.5f, prox, FLOAT_TOLERANCE,
		"1 hour before sunset should be 0.5");
}

ZTEST(ai_features, test_sunset_proximity_at_sunset)
{
	/* 日落时刻 */
	float prox = ai_calc_sunset_proximity(18.0f, 18.0f);
	zassert_within(1.0f, prox, FLOAT_TOLERANCE,
		"At sunset should be 1.0");
}

ZTEST(ai_features, test_sunset_proximity_after)
{
	/* 日落后1小时 */
	float prox = ai_calc_sunset_proximity(19.0f, 18.0f);
	zassert_within(1.0f, prox, FLOAT_TOLERANCE,
		"1 hour after sunset should still be 1.0");
}

ZTEST(ai_features, test_sunset_proximity_late_night)
{
	/* 日落后3小时 (深夜) */
	float prox = ai_calc_sunset_proximity(21.0f, 18.0f);
	/* 21 - 18 = 3, -2 range, should be 0.5 */
	zassert_within(0.5f, prox, FLOAT_TOLERANCE,
		"Late night should decay to 0.5");
}

/* ================================================================
 * AI-016: 特征向量组装测试
 * ================================================================ */
ZTEST(ai_features, test_assemble_complete)
{
	ai_features_t features;
	ai_assemble_features(14.0f, 0.5f, 1.0f, 6.0f, 18.5f, &features);

	/* 验证时间特征 */
	float expected_sin = sinf(14.0f * 2.0f * PI / 24.0f);
	float expected_cos = cosf(14.0f * 2.0f * PI / 24.0f);
	zassert_within(expected_sin, features.hour_sin, FLOAT_TOLERANCE,
		"Hour sin mismatch");
	zassert_within(expected_cos, features.hour_cos, FLOAT_TOLERANCE,
		"Hour cos mismatch");

	/* 验证其他特征 */
	zassert_within(0.5f, features.weather, FLOAT_TOLERANCE,
		"Weather should be 0.5");
	zassert_within(1.0f, features.presence, FLOAT_TOLERANCE,
		"Presence should be 1.0");
	zassert_within(6.0f/24.0f, features.sunrise_hour_norm, FLOAT_TOLERANCE,
		"Sunrise norm should be 6/24");
	zassert_within(18.5f/24.0f, features.sunset_hour_norm, FLOAT_TOLERANCE,
		"Sunset norm should be 18.5/24");
}

ZTEST(ai_features, test_assemble_no_presence)
{
	ai_features_t features;
	ai_assemble_features(10.0f, 0.0f, 0.0f, 6.0f, 18.0f, &features);

	zassert_within(0.0f, features.presence, FLOAT_TOLERANCE,
		"Presence should be 0.0");
	zassert_within(0.0f, features.weather, FLOAT_TOLERANCE,
		"Sunny weather should be 0.0");
}

ZTEST_SUITE(ai_features, NULL, NULL, NULL, NULL, NULL);
