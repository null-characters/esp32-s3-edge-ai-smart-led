/*
 * AI Post-Processing Unit Tests
 * Phase 3: AI-018 TDD
 */

#include <zephyr/ztest.h>
#include "ai_infer.h"
#include "lighting.h"

/* ================================================================
 * AI-018: 后处理测试
 * ================================================================ */
ZTEST(ai_postprocess, test_no_presence_force_off)
{
	ai_output_t raw = {4000, 80};  /* 4000K @ 80% */
	ai_output_t final;

	ai_post_process(&raw, 0.0f, 10.0f, &final);

	zassert_equal(0, final.brightness, "No presence should force off");
	zassert_equal(COLOR_TEMP_MIN, final.color_temp, "Color temp should be min");
}

ZTEST(ai_postprocess, test_presence_keeps_output)
{
	ai_output_t raw = {4500, 65};
	ai_output_t final;

	ai_post_process(&raw, 1.0f, 14.0f, &final);

	zassert_equal(65, final.brightness, "Brightness should be preserved");
	zassert_equal(4500, final.color_temp, "Color temp should be preserved");
}

ZTEST(ai_postprocess, test_late_night_limit)
{
	ai_output_t raw = {3500, 80};  /* 22:30, AI建议80% */
	ai_output_t final;

	ai_post_process(&raw, 1.0f, 22.5f, &final);

	/* 深夜应限制到50% */
	zassert_equal(50, final.brightness, "Late night should limit to 50%");
}

ZTEST(ai_postprocess, test_before_late_night_no_limit)
{
	ai_output_t raw = {4000, 75};
	ai_output_t final;

	ai_post_process(&raw, 1.0f, 21.5f, &final);

	/* 21:30不限制 */
	zassert_equal(75, final.brightness, "Before 22:00 should not limit");
}

ZTEST(ai_postprocess, test_output_range_limit)
{
	/* 测试超范围输出 */
	ai_output_t raw = {7000, 150};  /* 超范围 */
	ai_output_t final;

	ai_post_process(&raw, 1.0f, 10.0f, &final);

	zassert_true(final.color_temp <= COLOR_TEMP_MAX, "Color temp should be capped");
	zassert_true(final.brightness <= 100, "Brightness should be capped at 100");
}

ZTEST(ai_postprocess, test_negative_output)
{
	ai_output_t raw = {2000, -10};  /* 负值 */
	ai_output_t final;

	ai_post_process(&raw, 1.0f, 10.0f, &final);

	zassert_true(final.color_temp >= COLOR_TEMP_MIN, "Color temp should be floored");
	zassert_true(final.brightness >= 0, "Brightness should be >= 0");
}

ZTEST_SUITE(ai_postprocess, NULL, NULL, NULL, NULL, NULL);
