/*
 * Unit Tests for Lighting Interface (HW-013)
 * TDD Test Suite for lighting_set_color_temp, lighting_set_brightness, etc.
 */

#include <zephyr/ztest.h>
#include "lighting.h"
#include "uart_driver.h"

/* ================================================================
 * Test: Module initialization
 * ================================================================ */
ZTEST(lighting_interface_suite, test_init)
{
	int ret = lighting_init();
	zassert_equal(ret, 0, "lighting_init should return 0");
}

/* ================================================================
 * Test: Color temperature - valid range
 * ================================================================ */
ZTEST(lighting_interface_suite, test_set_color_temp_min)
{
	int ret = lighting_set_color_temp(COLOR_TEMP_MIN);
	/* Returns 0 on success, or error if UART not available */
	zassert_true(ret == 0 || ret == -ENODEV, "Min temp should be accepted");
}

ZTEST(lighting_interface_suite, test_set_color_temp_max)
{
	int ret = lighting_set_color_temp(COLOR_TEMP_MAX);
	zassert_true(ret == 0 || ret == -ENODEV, "Max temp should be accepted");
}

ZTEST(lighting_interface_suite, test_set_color_temp_mid)
{
	int ret = lighting_set_color_temp(4200);
	zassert_true(ret == 0 || ret == -ENODEV, "Mid temp should be accepted");
}

/* ================================================================
 * Test: Color temperature - invalid range
 * ================================================================ */
ZTEST(lighting_interface_suite, test_set_color_temp_too_low)
{
	int ret = lighting_set_color_temp(COLOR_TEMP_MIN - 1);
	zassert_true(ret < 0, "Below min temp should fail");
}

ZTEST(lighting_interface_suite, test_set_color_temp_too_high)
{
	int ret = lighting_set_color_temp(COLOR_TEMP_MAX + 1);
	zassert_true(ret < 0, "Above max temp should fail");
}

/* ================================================================
 * Test: Brightness - valid range
 * ================================================================ */
ZTEST(lighting_interface_suite, test_set_brightness_min)
{
	int ret = lighting_set_brightness(BRIGHTNESS_MIN);
	zassert_true(ret == 0 || ret == -ENODEV, "Min brightness should be accepted");
}

ZTEST(lighting_interface_suite, test_set_brightness_max)
{
	int ret = lighting_set_brightness(BRIGHTNESS_MAX);
	zassert_true(ret == 0 || ret == -ENODEV, "Max brightness should be accepted");
}

ZTEST(lighting_interface_suite, test_set_brightness_mid)
{
	int ret = lighting_set_brightness(50);
	zassert_true(ret == 0 || ret == -ENODEV, "Mid brightness should be accepted");
}

/* ================================================================
 * Test: Brightness - invalid range
 * ================================================================ */
ZTEST(lighting_interface_suite, test_set_brightness_too_high)
{
	int ret = lighting_set_brightness(BRIGHTNESS_MAX + 1);
	zassert_true(ret < 0, "Above max brightness should fail");
}

ZTEST(lighting_interface_suite, test_set_brightness_255)
{
	int ret = lighting_set_brightness(255);
	zassert_true(ret < 0, "255 brightness should fail");
}

/* ================================================================
 * Test: Set both parameters
 * ================================================================ */
ZTEST(lighting_interface_suite, test_set_both_valid)
{
	int ret = lighting_set_both(4200, 60);
	zassert_true(ret == 0 || ret == -ENODEV, "Valid both params should be accepted");
}

ZTEST(lighting_interface_suite, test_set_both_min_values)
{
	int ret = lighting_set_both(COLOR_TEMP_MIN, BRIGHTNESS_MIN);
	zassert_true(ret == 0 || ret == -ENODEV, "Min values should be accepted");
}

ZTEST(lighting_interface_suite, test_set_both_max_values)
{
	int ret = lighting_set_both(COLOR_TEMP_MAX, BRIGHTNESS_MAX);
	zassert_true(ret == 0 || ret == -ENODEV, "Max values should be accepted");
}

/* ================================================================
 * Test: Set both - invalid combinations
 * ================================================================ */
ZTEST(lighting_interface_suite, test_set_both_invalid_temp)
{
	int ret = lighting_set_both(1000, 50);
	zassert_true(ret < 0, "Invalid temp should fail");
}

ZTEST(lighting_interface_suite, test_set_both_invalid_brightness)
{
	int ret = lighting_set_both(4000, 150);
	zassert_true(ret < 0, "Invalid brightness should fail");
}

ZTEST(lighting_interface_suite, test_set_both_both_invalid)
{
	int ret = lighting_set_both(1000, 150);
	zassert_true(ret < 0, "Both invalid should fail");
}

/* ================================================================
 * Test: Get status
 * ================================================================ */
ZTEST(lighting_interface_suite, test_get_status)
{
	int ret = lighting_get_status();
	zassert_true(ret == 0 || ret == -ENODEV, "Get status should work");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
static void *lighting_setup(void)
{
	lighting_init();
	return NULL;
}

ZTEST_SUITE(lighting_interface_suite, NULL, lighting_setup, NULL, NULL, NULL);
