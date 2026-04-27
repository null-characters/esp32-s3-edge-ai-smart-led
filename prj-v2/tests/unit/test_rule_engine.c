/*
 * Unit Tests for Rule Engine (HW-019)
 * TDD Test Suite for rule-based lighting calculations
 */

#include <zephyr/ztest.h>
#include "rule_engine.h"
#include "time_period.h"
#include "presence_state.h"

/* ================================================================
 * Test: rule_get_period_settings
 * ================================================================ */
ZTEST(rule_engine_suite, test_period_settings_morning)
{
	lighting_settings_t settings;
	rule_get_period_settings(PERIOD_MORNING, &settings);
	
	zassert_equal(settings.color_temp, MORNING_COLOR_TEMP,
		"Morning color temp should be %dK", MORNING_COLOR_TEMP);
	zassert_equal(settings.brightness, MORNING_BRIGHTNESS,
		"Morning brightness should be %d%%", MORNING_BRIGHTNESS);
}

ZTEST(rule_engine_suite, test_period_settings_afternoon)
{
	lighting_settings_t settings;
	rule_get_period_settings(PERIOD_AFTERNOON, &settings);
	
	zassert_equal(settings.color_temp, AFTERNOON_COLOR_TEMP,
		"Afternoon color temp should be %dK", AFTERNOON_COLOR_TEMP);
	zassert_equal(settings.brightness, AFTERNOON_BRIGHTNESS,
		"Afternoon brightness should be %d%%", AFTERNOON_BRIGHTNESS);
}

ZTEST(rule_engine_suite, test_period_settings_evening)
{
	lighting_settings_t settings;
	rule_get_period_settings(PERIOD_EVENING, &settings);
	
	zassert_equal(settings.color_temp, EVENING_COLOR_TEMP,
		"Evening color temp should be %dK", EVENING_COLOR_TEMP);
	zassert_equal(settings.brightness, EVENING_BRIGHTNESS,
		"Evening brightness should be %d%%", EVENING_BRIGHTNESS);
}

/* ================================================================
 * Test: rule_calculate_settings with presence
 * ================================================================ */
ZTEST(rule_engine_suite, test_calculate_with_presence)
{
	lighting_settings_t settings;
	
	/* When someone is present, use period settings */
	rule_calculate_settings(PRESENCE_YES, PERIOD_MORNING, &settings);
	zassert_equal(settings.color_temp, MORNING_COLOR_TEMP);
	zassert_equal(settings.brightness, MORNING_BRIGHTNESS);
	
	rule_calculate_settings(PRESENCE_YES, PERIOD_AFTERNOON, &settings);
	zassert_equal(settings.color_temp, AFTERNOON_COLOR_TEMP);
	zassert_equal(settings.brightness, AFTERNOON_BRIGHTNESS);
	
	rule_calculate_settings(PRESENCE_YES, PERIOD_EVENING, &settings);
	zassert_equal(settings.color_temp, EVENING_COLOR_TEMP);
	zassert_equal(settings.brightness, EVENING_BRIGHTNESS);
}

ZTEST(rule_engine_suite, test_calculate_no_presence_recent)
{
	/* Just left (PRESENCE_NO) but not yet timeout - lights stay on */
	lighting_settings_t settings;
	
	rule_calculate_settings(PRESENCE_NO, PERIOD_AFTERNOON, &settings);
	zassert_equal(settings.color_temp, AFTERNOON_COLOR_TEMP);
	zassert_equal(settings.brightness, AFTERNOON_BRIGHTNESS);
}

ZTEST(rule_engine_suite, test_calculate_timeout_lights_off)
{
	/* Timeout - lights should turn off */
	lighting_settings_t settings;
	
	rule_calculate_settings(PRESENCE_TIMEOUT, PERIOD_MORNING, &settings);
	zassert_equal(settings.brightness, LIGHTS_OFF_BRIGHTNESS,
		"Brightness should be 0 when timeout");
	zassert_equal(settings.color_temp, LIGHTS_OFF_COLOR_TEMP,
		"Color temp should be minimum when timeout");
}

ZTEST(rule_engine_suite, test_calculate_timeout_all_periods)
{
	/* Timeout should turn off lights regardless of period */
	lighting_settings_t settings;
	time_period_t periods[] = {PERIOD_MORNING, PERIOD_AFTERNOON, PERIOD_EVENING};
	
	for (int i = 0; i < 3; i++) {
		rule_calculate_settings(PRESENCE_TIMEOUT, periods[i], &settings);
		zassert_equal(settings.brightness, 0,
			"Lights should be off during timeout in period %d", i);
	}
}

/* ================================================================
 * Test: Null pointer handling
 * ================================================================ */
ZTEST(rule_engine_suite, test_null_settings_period)
{
	/* Should not crash with null pointer */
	rule_get_period_settings(PERIOD_MORNING, NULL);
	/* If we reach here without crash, test passes */
	zassert_true(true, "Null pointer handled correctly");
}

ZTEST(rule_engine_suite, test_null_settings_calculate)
{
	/* Should not crash with null pointer */
	rule_calculate_settings(PRESENCE_YES, PERIOD_MORNING, NULL);
	/* If we reach here without crash, test passes */
	zassert_true(true, "Null pointer handled correctly");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
static void *rule_engine_setup(void)
{
	rule_engine_init();
	return NULL;
}

ZTEST_SUITE(rule_engine_suite, NULL, rule_engine_setup, NULL, NULL, NULL);
