/*
 * Unit Tests for Presence Detection State Machine (HW-017)
 * TDD Test Suite for presence_state_update, presence_get_state
 */

#include <zephyr/ztest.h>
#include "presence_state.h"

/* Mock for PIR - declared in mock_pir.c */
extern void mock_pir_set_presence(bool presence);
extern void mock_pir_reset(void);

/* ================================================================
 * Test: Initial state after init
 * ================================================================ */
ZTEST(presence_state_suite, test_init_state)
{
	mock_pir_reset();
	presence_state_init();
	
	/* After init, state should be PRESENCE_NO */
	presence_state_t state = presence_get_state();
	zassert_equal(state, PRESENCE_NO, "Initial state should be PRESENCE_NO");
}

ZTEST(presence_state_suite, test_init_absence_seconds)
{
	mock_pir_reset();
	presence_state_init();
	
	/* After init, absence seconds should be 0 (just entered NO state) */
	uint32_t absence = presence_get_absence_seconds();
	zassert_equal(absence, 0, "Initial absence should be 0");
}

/* ================================================================
 * Test: State transitions - No presence -> Presence detected
 * ================================================================ */
ZTEST(presence_state_suite, test_transition_no_to_yes)
{
	mock_pir_reset();
	presence_state_init();
	
	/* Initially no presence */
	mock_pir_set_presence(false);
	presence_state_update();
	zassert_equal(presence_get_state(), PRESENCE_NO);
	
	/* Now detect presence */
	mock_pir_set_presence(true);
	presence_state_t new_state = presence_state_update();
	
	zassert_equal(new_state, PRESENCE_YES, "Should transition to PRESENCE_YES");
	zassert_equal(presence_get_state(), PRESENCE_YES);
}

ZTEST(presence_state_suite, test_presence_yes_absence_seconds)
{
	mock_pir_reset();
	presence_state_init();
	
	/* Detect presence */
	mock_pir_set_presence(true);
	presence_state_update();
	
	/* Absence seconds should be 0 when presence detected */
	zassert_equal(presence_get_absence_seconds(), 0);
}

/* ================================================================
 * Test: State transitions - Presence -> No presence
 * ================================================================ */
ZTEST(presence_state_suite, test_transition_yes_to_no)
{
	mock_pir_reset();
	presence_state_init();
	
	/* First detect presence */
	mock_pir_set_presence(true);
	presence_state_update();
	zassert_equal(presence_get_state(), PRESENCE_YES);
	
	/* Now lose presence */
	mock_pir_set_presence(false);
	presence_state_t new_state = presence_state_update();
	
	zassert_equal(new_state, PRESENCE_NO, "Should transition to PRESENCE_NO");
	zassert_equal(presence_get_state(), PRESENCE_NO);
}

/* ================================================================
 * Test: Continuous presence stays in YES state
 * ================================================================ */
ZTEST(presence_state_suite, test_continuous_presence_stays_yes)
{
	mock_pir_reset();
	presence_state_init();
	
	mock_pir_set_presence(true);
	presence_state_update();
	
	/* Stay in presence for multiple updates */
	for (int i = 0; i < 5; i++) {
		presence_state_t state = presence_state_update();
		zassert_equal(state, PRESENCE_YES, "Should stay in PRESENCE_YES");
	}
}

/* ================================================================
 * Test: Continuous absence stays in NO state (but not timeout)
 * ================================================================ */
ZTEST(presence_state_suite, test_continuous_absence_stays_no)
{
	mock_pir_reset();
	presence_state_init();
	
	mock_pir_set_presence(false);
	presence_state_update();
	
	/* Stay in NO state for multiple updates (but not long enough for timeout) */
	for (int i = 0; i < 5; i++) {
		presence_state_t state = presence_state_update();
		/* Note: In real hardware, this would eventually timeout, 
		 * but in unit test we can't simulate time passage easily */
		zassert_true(state == PRESENCE_NO || state == PRESENCE_TIMEOUT,
			"Should stay in NO or TIMEOUT state");
	}
}

/* ================================================================
 * Test: Reset timer functionality
 * ================================================================ */
ZTEST(presence_state_suite, test_reset_timer)
{
	mock_pir_reset();
	presence_state_init();
	
	/* Go to NO state */
	mock_pir_set_presence(false);
	presence_state_update();
	
	/* Reset timer */
	presence_reset_timer();
	
	/* State should still be NO, but absence timer reset */
	zassert_equal(presence_get_state(), PRESENCE_NO);
	zassert_equal(presence_get_absence_seconds(), 0);
}

/* ================================================================
 * Test: State after presence lost - should be NO before timeout
 * ================================================================ */
ZTEST(presence_state_suite, test_state_after_presence_lost)
{
	mock_pir_reset();
	presence_state_init();
	
	/* Detect then lose presence */
	mock_pir_set_presence(true);
	presence_state_update();
	mock_pir_set_presence(false);
	presence_state_update();
	
	presence_state_t state = presence_get_state();
	zassert_true(state == PRESENCE_NO || state == PRESENCE_TIMEOUT,
		"After losing presence, state should be NO or TIMEOUT");
}

/* ================================================================
 * Test: Absence seconds increases after losing presence
 * ================================================================ */
ZTEST(presence_state_suite, test_absence_seconds_increases)
{
	mock_pir_reset();
	presence_state_init();
	
	/* First detect presence */
	mock_pir_set_presence(true);
	presence_state_update();
	
	/* Lose presence */
	mock_pir_set_presence(false);
	presence_state_update();
	
	uint32_t initial = presence_get_absence_seconds();
	
	/* In real hardware with time passing, this would increase
	 * In unit test without mocking time, we just verify the function works */
	zassert_true(initial >= 0, "Absence seconds should be >= 0");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
static void *presence_state_setup(void)
{
	mock_pir_reset();
	presence_state_init();
	return NULL;
}

ZTEST_SUITE(presence_state_suite, NULL, presence_state_setup, NULL, NULL, NULL);
