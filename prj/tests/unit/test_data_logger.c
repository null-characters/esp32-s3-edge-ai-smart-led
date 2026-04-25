/*
 * Data Logger Unit Tests
 * Phase 3: AI-021 ~ AI-025 TDD
 */

#include <zephyr/ztest.h>
#include "data_logger.h"

/* ================================================================
 * AI-021: 事件结构测试
 * ================================================================ */
ZTEST(data_logger, test_event_size)
{
	/* 验证结构体大小为32字节 */
	zassert_equal(32, sizeof(lighting_event_t),
		"Event struct should be exactly 32 bytes");
}

/* ================================================================
 * AI-023: 记录写入测试
 * ================================================================ */
ZTEST(data_logger, test_log_single_event)
{
	int ret = data_logger_init();
	zassert_equal(0, ret, "Init should succeed");

	lighting_event_t event = {
		.timestamp = 1745500800,
		.event_type = EVENT_LIGHTS_ON,
		.color_temp = 4000,
		.brightness = 65,
	};

	ret = data_logger_log(&event);
	zassert_equal(0, ret, "Log should succeed");

	uint32_t count, capacity;
	ret = data_logger_get_status(&count, &capacity);
	zassert_equal(0, ret, "Get status should succeed");
	zassert_equal(1, count, "Count should be 1");
}

ZTEST(data_logger, test_log_multiple_events)
{
	data_logger_init();

	lighting_event_t event = {0};
	event.timestamp = 1745500800;

	for (int i = 0; i < 100; i++) {
		event.event_type = i % 8;
		event.brightness = i;
		data_logger_log(&event);
	}

	uint32_t count, capacity;
	data_logger_get_status(&count, &capacity);
	zassert_equal(100, count, "Should have 100 events");
}

/* ================================================================
 * AI-024: 环形缓冲区测试
 * ================================================================ */
ZTEST(data_logger, test_ring_buffer_overflow)
{
	data_logger_init();

	uint32_t count, capacity;
	data_logger_get_status(&count, &capacity);

	/* 填充超过容量 */
	lighting_event_t event = {0};
	event.timestamp = 1745500800;

	for (uint32_t i = 0; i < capacity + 100; i++) {
		event.timestamp = i;
		data_logger_log(&event);
	}

	data_logger_get_status(&count, &capacity);
	zassert_equal(capacity, count, "Should be at capacity after overflow");
	zassert_true(data_logger_is_full(), "Should be full");
}

/* ================================================================
 * AI-025: 记录查询测试
 * ================================================================ */
ZTEST(data_logger, test_query_by_time_range)
{
	data_logger_init();

	/* 写入不同时间戳的事件 */
	lighting_event_t event = {0};
	for (int i = 0; i < 50; i++) {
		event.timestamp = 1745500800 + i * 60;  /* 每分钟一个事件 */
		event.event_type = i % 8;
		data_logger_log(&event);
	}

	/* 查询时间范围 */
	lighting_event_t results[20];
	uint32_t actual_count;

	int ret = data_logger_query(1745500800 + 10 * 60,  /* 10分钟后 */
				    1745500800 + 25 * 60,  /* 25分钟后 */
				    results, 20, &actual_count);

	zassert_equal(0, ret, "Query should succeed");
	zassert_equal(16, actual_count, "Should find 16 events in range (indices 10-25 inclusive)");
}

ZTEST(data_logger, test_get_recent)
{
	data_logger_init();

	lighting_event_t event = {0};
	event.brightness = 50;
	for (int i = 0; i < 100; i++) {
		event.timestamp = i;
		event.brightness = i;
		data_logger_log(&event);
	}

	lighting_event_t recent[10];
	uint32_t actual_count;

	int ret = data_logger_get_recent(recent, 10, &actual_count);

	zassert_equal(0, ret, "Get recent should succeed");
	zassert_equal(10, actual_count, "Should get 10 events");

	/* 最新的事件brightness应该是99 */
	zassert_equal(99, recent[9].brightness, "Most recent should be last");
}

ZTEST_SUITE(data_logger, NULL, NULL, NULL, NULL, NULL);
