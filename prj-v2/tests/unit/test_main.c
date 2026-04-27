/*
 * Unit Test Suite Main Entry Point
 * Phase 1 TDD Test Runner
 */

#include <zephyr/ztest.h>

/* Test suites are registered automatically by ZTEST macros */
/* This file serves as the entry point for the test runner */

/* 
 * The test suites included in this build:
 * - uart_frame_suite: HW-010, HW-011, HW-012 (frame protocol)
 * - time_period_suite: HW-018 (time period judgment)
 * - rule_engine_suite: HW-019 (rule-based lighting)
 * - lighting_params_suite: HW-013 (parameter validation)
 */

/* No additional code needed - ZTEST handles test registration */
