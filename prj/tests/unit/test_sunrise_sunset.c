/*
 * Sunrise-Sunset API Unit Tests
 * Phase 2: WF-021 ~ WF-025 TDD
 */

#include <zephyr/ztest.h>
#include <string.h>
#include "sunrise_sunset.h"

/* ================================================================
 * ISO8601时间转小时数测试 (WF-023)
 * ================================================================ */
ZTEST(sun_api, test_iso_to_hour_morning)
{
	float hour = sun_iso_to_hour("2026-04-24T06:30:00+00:00");
	/* UTC+8: 06:30 → 14:30 */
	zassert_within(14.5f, hour, 0.1f, "06:30 UTC should be 14:30 local");
}

ZTEST(sun_api, test_iso_to_hour_evening)
{
	float hour = sun_iso_to_hour("2026-04-24T10:15:00+00:00");
	/* UTC+8: 10:15 → 18:15 */
	zassert_within(18.25f, hour, 0.1f, "10:15 UTC should be 18:15 local");
}

ZTEST(sun_api, test_iso_to_hour_midnight)
{
	float hour = sun_iso_to_hour("2026-04-24T16:00:00+00:00");
	/* UTC+8: 16:00 → 00:00 (次日) */
	zassert_within(0.0f, hour, 0.1f, "16:00 UTC should be 00:00 local");
}

ZTEST(sun_api, test_iso_to_hour_invalid)
{
	zassert_true(sun_iso_to_hour(NULL) < 0, "NULL should return error");
	zassert_true(sun_iso_to_hour("") < 0, "Empty should return error");
	zassert_true(sun_iso_to_hour("invalid") < 0, "Invalid format should return error");
}

/* ================================================================
 * 日落临近度计算测试 (WF-026)
 * 注意: 这些测试需要设置模拟时间
 * ================================================================ */
ZTEST(sun_api, test_sunset_proximity_range)
{
	/* 初始化 */
	sun_api_init();

	/* 手动设置缓存数据 */
	extern sun_cache_t sun_cache;
	sun_cache.sunset_hour = 18.5f; /* 18:30 */
	sun_cache.valid = true;

	/* 日落临近度应该在0-1范围内 */
	float proximity = sun_get_sunset_proximity();
	zassert_true(proximity >= 0.0f, "Proximity should be >= 0");
	zassert_true(proximity <= 1.0f, "Proximity should be <= 1");
}

ZTEST(sun_api, test_cache_validity)
{
	sun_api_init();

	/* 初始状态无效 */
	zassert_false(sun_cache_is_valid(), "Initial cache should be invalid");

	/* 设置有效 */
	extern sun_cache_t sun_cache;
	sun_cache.valid = true;
	zassert_true(sun_cache_is_valid(), "Cache should be valid after setting");
}

ZTEST_SUITE(sun_api, NULL, NULL, NULL, NULL, NULL);
