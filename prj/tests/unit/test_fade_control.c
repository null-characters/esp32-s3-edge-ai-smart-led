/*
 * Unit Tests for Fade Control Module (HW-020)
 * TDD Test Suite for fade_to_target, fade_set_immediate, fade_is_in_progress
 */

#include <zephyr/ztest.h>
#include "fade_control.h"
#include "lighting.h"

/* ================================================================
 * Test: Initialization
 * ================================================================ */
ZTEST(fade_control_suite, test_init)
{
	int ret = fade_control_init();
	zassert_equal(ret, 0, "fade_control_init should return 0");
}

/* ================================================================
 * Test: Immediate set - valid parameters
 * ================================================================ */
ZTEST(fade_control_suite, test_set_immediate_valid)
{
	fade_control_init();
	
	int ret = fade_set_immediate(4200, 60);
	zassert_equal(ret, 0, "Immediate set with valid params should succeed");
	
	uint16_t temp;
	uint8_t bright;
	fade_get_current(&temp, &bright);
	
	zassert_equal(temp, 4200, "Current temp should be set");
	zassert_equal(bright, 60, "Current brightness should be set");
}

ZTEST(fade_control_suite, test_set_immediate_min_values)
{
	fade_control_init();
	
	/* Minimum valid values */
	int ret = fade_set_immediate(COLOR_TEMP_MIN, BRIGHTNESS_MIN);
	zassert_equal(ret, 0);
	
	uint16_t temp;
	uint8_t bright;
	fade_get_current(&temp, &bright);
	
	zassert_equal(temp, COLOR_TEMP_MIN);
	zassert_equal(bright, BRIGHTNESS_MIN);
}

ZTEST(fade_control_suite, test_set_immediate_max_values)
{
	fade_control_init();
	
	/* Maximum valid values */
	int ret = fade_set_immediate(COLOR_TEMP_MAX, BRIGHTNESS_MAX);
	zassert_equal(ret, 0);
	
	uint16_t temp;
	uint8_t bright;
	fade_get_current(&temp, &bright);
	
	zassert_equal(temp, COLOR_TEMP_MAX);
	zassert_equal(bright, BRIGHTNESS_MAX);
}

/* ================================================================
 * Test: Immediate set - invalid parameters
 * ================================================================ */
ZTEST(fade_control_suite, test_set_immediate_temp_too_low)
{
	fade_control_init();
	
	int ret = fade_set_immediate(COLOR_TEMP_MIN - 1, 50);
	zassert_true(ret < 0, "Should fail with temp below minimum");
}

ZTEST(fade_control_suite, test_set_immediate_temp_too_high)
{
	fade_control_init();
	
	int ret = fade_set_immediate(COLOR_TEMP_MAX + 1, 50);
	zassert_true(ret < 0, "Should fail with temp above maximum");
}

ZTEST(fade_control_suite, test_set_immediate_brightness_too_high)
{
	fade_control_init();
	
	int ret = fade_set_immediate(4000, BRIGHTNESS_MAX + 1);
	zassert_true(ret < 0, "Should fail with brightness above maximum");
}

/* ================================================================
 * Test: Fade to target - parameter validation
 * ================================================================ */
ZTEST(fade_control_suite, test_fade_to_target_invalid_temp)
{
	fade_control_init();
	
	/* Invalid temp */
	int ret = fade_to_target(1000, 50, 100);
	zassert_true(ret < 0, "Should fail with invalid temp");
}

ZTEST(fade_control_suite, test_fade_to_target_invalid_brightness)
{
	fade_control_init();
	
	/* Invalid brightness */
	int ret = fade_to_target(4000, 150, 100);
	zassert_true(ret < 0, "Should fail with invalid brightness");
}

/* ================================================================
 * Test: Fade progress flag
 * ================================================================ */
ZTEST(fade_control_suite, test_fade_not_in_progress_after_init)
{
	fade_control_init();
	
	bool in_progress = fade_is_in_progress();
	zassert_false(in_progress, "Should not be fading after init");
}

ZTEST(fade_control_suite, test_fade_not_in_progress_after_immediate)
{
	fade_control_init();
	
	fade_set_immediate(4000, 50);
	
	bool in_progress = fade_is_in_progress();
	zassert_false(in_progress, "Should not be fading after immediate set");
}

/* ================================================================
 * Test: Get current with NULL pointers (should not crash)
 * ================================================================ */
ZTEST(fade_control_suite, test_get_current_null_temp)
{
	fade_control_init();
	fade_set_immediate(4000, 50);
	
	uint8_t bright;
	fade_get_current(NULL, &bright);
	
	zassert_equal(bright, 50, "Should still get brightness");
}

ZTEST(fade_control_suite, test_get_current_null_bright)
{
	fade_control_init();
	fade_set_immediate(4000, 50);
	
	uint16_t temp;
	fade_get_current(&temp, NULL);
	
	zassert_equal(temp, 4000, "Should still get temp");
}

ZTEST(fade_control_suite, test_get_current_both_null)
{
	fade_control_init();
	
	/* Should not crash */
	fade_get_current(NULL, NULL);
	zassert_true(true, "Function should handle NULL pointers");
}

/* ================================================================
 * Test: Zero duration fade acts like immediate
 * ================================================================ */
ZTEST(fade_control_suite, test_fade_zero_duration)
{
	fade_control_init();
	
	/* Zero duration should act like immediate set */
	int ret = fade_to_target(4200, 60, 0);
	zassert_equal(ret, 0);
	
	uint16_t temp;
	uint8_t bright;
	fade_get_current(&temp, &bright);
	
	zassert_equal(temp, 4200);
	zassert_equal(bright, 60);
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
static void *fade_control_setup(void)
{
	fade_control_init();
	return NULL;
}

ZTEST_SUITE(fade_control_suite, NULL, fade_control_setup, NULL, NULL, NULL);
