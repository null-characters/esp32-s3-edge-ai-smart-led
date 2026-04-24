/*
 * Unit Tests for Time Period Module (HW-018)
 * TDD Test Suite for time_period_get
 */

#include <zephyr/ztest.h>
#include "time_period.h"

/* ================================================================
 * Test: time_period_get for all hours of the day
 * ================================================================ */
ZTEST(time_period_suite, test_morning_hours)
{
	/* Morning: 6:00 - 11:59 */
	for (uint8_t hour = 6; hour < 12; hour++) {
		time_period_t period = time_period_get(hour);
		zassert_equal(period, PERIOD_MORNING,
			"Hour %d should be MORNING", hour);
	}
}

ZTEST(time_period_suite, test_afternoon_hours)
{
	/* Afternoon: 12:00 - 17:59 */
	for (uint8_t hour = 12; hour < 18; hour++) {
		time_period_t period = time_period_get(hour);
		zassert_equal(period, PERIOD_AFTERNOON,
			"Hour %d should be AFTERNOON", hour);
	}
}

ZTEST(time_period_suite, test_evening_hours_early)
{
	/* Evening: 18:00 - 23:59 */
	for (uint8_t hour = 18; hour < 24; hour++) {
		time_period_t period = time_period_get(hour);
		zassert_equal(period, PERIOD_EVENING,
			"Hour %d should be EVENING", hour);
	}
}

ZTEST(time_period_suite, test_evening_hours_late)
{
	/* Evening: 00:00 - 05:59 */
	for (uint8_t hour = 0; hour < 6; hour++) {
		time_period_t period = time_period_get(hour);
		zassert_equal(period, PERIOD_EVENING,
			"Hour %d should be EVENING", hour);
	}
}

/* ================================================================
 * Test: Boundary values
 * ================================================================ */
ZTEST(time_period_suite, test_boundary_morning_start)
{
	/* 6:00 is the start of morning */
	zassert_equal(time_period_get(6), PERIOD_MORNING);
}

ZTEST(time_period_suite, test_boundary_afternoon_start)
{
	/* 12:00 is the start of afternoon */
	zassert_equal(time_period_get(12), PERIOD_AFTERNOON);
}

ZTEST(time_period_suite, test_boundary_evening_start)
{
	/* 18:00 is the start of evening */
	zassert_equal(time_period_get(18), PERIOD_EVENING);
}

ZTEST(time_period_suite, test_boundary_midnight)
{
	/* 0:00 (midnight) is evening */
	zassert_equal(time_period_get(0), PERIOD_EVENING);
}

/* ================================================================
 * Test: time_period_name
 * ================================================================ */
ZTEST(time_period_suite, test_period_names)
{
	zassert_str_equal(time_period_name(PERIOD_MORNING), "Morning");
	zassert_str_equal(time_period_name(PERIOD_AFTERNOON), "Afternoon");
	zassert_str_equal(time_period_name(PERIOD_EVENING), "Evening");
}

ZTEST(time_period_suite, test_period_name_invalid)
{
	/* Invalid period value should return "Unknown" */
	const char *name = time_period_name((time_period_t)99);
	zassert_str_equal(name, "Unknown");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
ZTEST_SUITE(time_period_suite, NULL, NULL, NULL, NULL, NULL);
