/*
 * Weather API Unit Tests
 * Phase 2: WF-016 ~ WF-020 TDD
 */

#include <zephyr/ztest.h>
#include <string.h>
#include "weather_api.h"

/* ================================================================
 * 天气编码转换测试 (WF-018)
 * ================================================================ */
ZTEST(weather_api, test_encode_sunny)
{
	zassert_equal(WEATHER_CODE_SUNNY, weather_encode("Sunny"),
		"Sunny should encode to 0.0");
	zassert_equal(WEATHER_CODE_SUNNY, weather_encode("Clear"),
		"Clear should encode to 0.0");
	zassert_equal(WEATHER_CODE_SUNNY, weather_encode("sUnNy"),
		"Case-insensitive sunny should encode to 0.0");
}

ZTEST(weather_api, test_encode_cloudy)
{
	zassert_equal(WEATHER_CODE_CLOUDY, weather_encode("Cloudy"),
		"Cloudy should encode to 0.5");
	zassert_equal(WEATHER_CODE_PARTLY_CLOUDY, weather_encode("Partly cloudy"),
		"Partly cloudy should encode to 0.25");
}

ZTEST(weather_api, test_encode_overcast)
{
	zassert_equal(WEATHER_CODE_OVERCAST, weather_encode("Overcast"),
		"Overcast should encode to 0.75");
	zassert_equal(WEATHER_CODE_OVERCAST, weather_encode("Fog"),
		"Fog should encode to 0.75");
	zassert_equal(WEATHER_CODE_OVERCAST, weather_encode("Mist"),
		"Mist should encode to 0.75");
	zassert_equal(WEATHER_CODE_OVERCAST, weather_encode("mIsT"),
		"Case-insensitive mist should encode to 0.75");
}

ZTEST(weather_api, test_encode_rain_snow)
{
	zassert_equal(WEATHER_CODE_RAIN_SNOW, weather_encode("Rain"),
		"Rain should encode to 1.0");
	zassert_equal(WEATHER_CODE_RAIN_SNOW, weather_encode("Snow"),
		"Snow should encode to 1.0");
	zassert_equal(WEATHER_CODE_RAIN_SNOW, weather_encode("Thunderstorm"),
		"Thunderstorm should encode to 1.0");
	zassert_equal(WEATHER_CODE_RAIN_SNOW, weather_encode("Light rain"),
		"Light rain should encode to 1.0");
}

ZTEST(weather_api, test_encode_unknown)
{
	/* 未知天气默认返回晴天 */
	zassert_equal(WEATHER_CODE_SUNNY, weather_encode("Unknown"),
		"Unknown should default to 0.0");
	zassert_equal(WEATHER_CODE_SUNNY, weather_encode(""),
		"Empty should default to 0.0");
	zassert_equal(WEATHER_CODE_SUNNY, weather_encode(NULL),
		"NULL should default to 0.0");
}

/* ================================================================
 * 天气响应解析测试 (WF-017)
 * ================================================================ */
ZTEST(weather_api, test_parse_sunny)
{
	weather_api_init();
	int ret = weather_parse_response("Sunny +22°C");
	zassert_equal(0, ret, "Parse should succeed");

	const weather_cache_t *cache = weather_get_cache();
	zassert_not_null(cache, "Cache should be valid");
	zassert_equal(WEATHER_CODE_SUNNY, cache->code, "Code should be 0.0");
	zassert_equal(22, cache->temperature, "Temp should be 22");
}

ZTEST(weather_api, test_parse_overcast)
{
	weather_api_init();
	int ret = weather_parse_response("Overcast +15°C");
	zassert_equal(0, ret, "Parse should succeed");

	const weather_cache_t *cache = weather_get_cache();
	zassert_not_null(cache, "Cache should be valid");
	zassert_equal(WEATHER_CODE_OVERCAST, cache->code, "Code should be 0.75");
	zassert_equal(15, cache->temperature, "Temp should be 15");
}

ZTEST(weather_api, test_parse_negative_temp)
{
	weather_api_init();
	int ret = weather_parse_response("Snow -5°C");
	zassert_equal(0, ret, "Parse should succeed");

	const weather_cache_t *cache = weather_get_cache();
	zassert_not_null(cache, "Cache should be valid");
	zassert_equal(-5, cache->temperature, "Temp should be -5");
}

ZTEST(weather_api, test_parse_partly_cloudy)
{
	weather_api_init();
	int ret = weather_parse_response("Partly cloudy +18°C");
	zassert_equal(0, ret, "Parse should succeed");

	const weather_cache_t *cache = weather_get_cache();
	zassert_not_null(cache, "Cache should be valid");
	zassert_equal(WEATHER_CODE_PARTLY_CLOUDY, cache->code, "Code should be 0.25");
}

ZTEST_SUITE(weather_api, NULL, NULL, NULL, NULL, NULL);
