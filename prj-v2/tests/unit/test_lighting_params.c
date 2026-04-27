/*
 * Unit Tests for Lighting Parameter Validation (HW-013)
 * TDD Test Suite for parameter range checking
 */

#include <zephyr/ztest.h>
#include "lighting.h"

/* ================================================================
 * Test: Color temperature range validation
 * Valid range: 2700K - 6500K
 * ================================================================ */
ZTEST(lighting_params_suite, test_color_temp_min_boundary)
{
	/* Minimum valid value */
	zassert_true(COLOR_TEMP_MIN == 2700, "Minimum color temp should be 2700K");
}

ZTEST(lighting_params_suite, test_color_temp_max_boundary)
{
	/* Maximum valid value */
	zassert_true(COLOR_TEMP_MAX == 6500, "Maximum color temp should be 6500K");
}

ZTEST(lighting_params_suite, test_color_temp_valid_range)
{
	/* Test some valid values within range */
	uint16_t valid_temps[] = {2700, 3000, 4000, 5000, 6000, 6500};
	
	for (int i = 0; i < ARRAY_SIZE(valid_temps); i++) {
		uint16_t temp = valid_temps[i];
		zassert_true(temp >= COLOR_TEMP_MIN && temp <= COLOR_TEMP_MAX,
			"Temp %d should be in valid range", temp);
	}
}

/* ================================================================
 * Test: Brightness range validation
 * Valid range: 0% - 100%
 * ================================================================ */
ZTEST(lighting_params_suite, test_brightness_min_boundary)
{
	zassert_true(BRIGHTNESS_MIN == 0, "Minimum brightness should be 0%");
}

ZTEST(lighting_params_suite, test_brightness_max_boundary)
{
	zassert_true(BRIGHTNESS_MAX == 100, "Maximum brightness should be 100%");
}

ZTEST(lighting_params_suite, test_brightness_valid_range)
{
	uint8_t valid_brights[] = {0, 25, 50, 75, 100};
	
	for (int i = 0; i < ARRAY_SIZE(valid_brights); i++) {
		uint8_t bright = valid_brights[i];
		zassert_true(bright >= BRIGHTNESS_MIN && bright <= BRIGHTNESS_MAX,
			"Brightness %d should be in valid range", bright);
	}
}

/* ================================================================
 * Test: Command codes
 * ================================================================ */
ZTEST(lighting_params_suite, test_command_codes)
{
	/* Verify command code definitions */
	zassert_equal(CMD_SET_COLOR_TEMP, 0x01, "SET_COLOR_TEMP should be 0x01");
	zassert_equal(CMD_SET_BRIGHTNESS, 0x02, "SET_BRIGHTNESS should be 0x02");
	zassert_equal(CMD_SET_BOTH, 0x03, "SET_BOTH should be 0x03");
	zassert_equal(CMD_GET_STATUS, 0x04, "GET_STATUS should be 0x04");
}

ZTEST(lighting_params_suite, test_response_codes)
{
	/* Verify response code definitions */
	zassert_equal(RESP_ACK, 0x00, "ACK should be 0x00");
	zassert_equal(RESP_NAK, 0xFF, "NAK should be 0xFF");
}

/* ================================================================
 * Test: Settings structure
 * ================================================================ */
ZTEST(lighting_params_suite, test_settings_structure)
{
	lighting_settings_t settings;
	
	/* Test assignment */
	settings.color_temp = 4200;
	settings.brightness = 60;
	
	zassert_equal(settings.color_temp, 4200);
	zassert_equal(settings.brightness, 60);
}

/* ================================================================
 * Test: Period default values
 * ================================================================ */
ZTEST(lighting_params_suite, test_morning_defaults)
{
	zassert_equal(MORNING_COLOR_TEMP, 4800, "Morning color temp");
	zassert_equal(MORNING_BRIGHTNESS, 65, "Morning brightness");
}

ZTEST(lighting_params_suite, test_afternoon_defaults)
{
	zassert_equal(AFTERNOON_COLOR_TEMP, 4200, "Afternoon color temp");
	zassert_equal(AFTERNOON_BRIGHTNESS, 60, "Afternoon brightness");
}

ZTEST(lighting_params_suite, test_evening_defaults)
{
	zassert_equal(EVENING_COLOR_TEMP, 3500, "Evening color temp");
	zassert_equal(EVENING_BRIGHTNESS, 55, "Evening brightness");
}

ZTEST(lighting_params_suite, test_lights_off_defaults)
{
	zassert_equal(LIGHTS_OFF_COLOR_TEMP, 2700, "Off color temp");
	zassert_equal(LIGHTS_OFF_BRIGHTNESS, 0, "Off brightness");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
ZTEST_SUITE(lighting_params_suite, NULL, NULL, NULL, NULL, NULL);
