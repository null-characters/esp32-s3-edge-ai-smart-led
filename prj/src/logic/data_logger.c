/*
 * Data Logger Implementation - 照明事件记录
 * Phase 3: AI-021 ~ AI-025
 *
 * 功能: PSRAM环形缓冲区、事件记录、时间范围查询
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

#include "data_logger.h"
#include "sntp_time_sync.h"
#include "weather_api.h"
#include "sunrise_sunset.h"
#include "ai_infer.h"

LOG_MODULE_REGISTER(data_log, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态 (AI-022)
 * ================================================================ */

/* 环形缓冲区结构 */
struct ring_buffer {
	lighting_event_t *buffer;     /* 缓冲区指针 */
	uint32_t capacity;            /* 容量 */
	uint32_t head;                /* 写入位置 */
	uint32_t tail;                /* 读取位置 */
	uint32_t count;               /* 当前数量 */
	bool full;                    /* 是否已满 */
	struct k_mutex lock;          /* 互斥锁 */
};

static struct ring_buffer event_buf = {0};
static bool logger_initialized = false;

/* ================================================================
 * AI-022: PSRAM缓冲区分配
 * ================================================================ */
int data_logger_init(void)
{
	LOG_INF("Initializing data logger...");

	/* 分配PSRAM缓冲区 */
	/* 注意: Zephyr中PSRAM分配需要配置heap */
	/* 这里使用k_malloc, 实际会从PSRAM分配 */
	event_buf.buffer = (lighting_event_t *)k_malloc(EVENT_BUFFER_SIZE);

	if (!event_buf.buffer) {
		LOG_ERR("Failed to allocate event buffer (%d bytes)", EVENT_BUFFER_SIZE);
		/* 降级: 使用较小的静态缓冲区 */
		static lighting_event_t static_buf[1000];
		event_buf.buffer = static_buf;
		event_buf.capacity = 1000;
		LOG_WRN("Using smaller static buffer (%d events)", 1000);
	} else {
		event_buf.capacity = EVENT_BUFFER_CAPACITY;
		LOG_INF("Allocated event buffer: %d bytes (%d events)",
			EVENT_BUFFER_SIZE, EVENT_BUFFER_CAPACITY);
	}

	event_buf.head = 0;
	event_buf.tail = 0;
	event_buf.count = 0;
	event_buf.full = false;

	k_mutex_init(&event_buf.lock);

	logger_initialized = true;
	LOG_INF("Data logger ready");
	return 0;
}

/* ================================================================
 * AI-023: 记录写入
 * ================================================================ */
int data_logger_log(const lighting_event_t *event)
{
	if (!logger_initialized || !event) {
		return -EINVAL;
	}

	k_mutex_lock(&event_buf.lock, K_FOREVER);

	/* 写入事件 */
	event_buf.buffer[event_buf.head] = *event;

	/* 更新指针 */
	event_buf.head = (event_buf.head + 1) % event_buf.capacity;
	event_buf.count++;

	/* 处理满缓冲区 (FIFO覆盖) */
	if (event_buf.full) {
		/* 覆盖最旧数据, 移动tail */
		event_buf.tail = (event_buf.tail + 1) % event_buf.capacity;
	}

	if (event_buf.count >= event_buf.capacity) {
		event_buf.full = true;
		event_buf.count = event_buf.capacity;
	}

	k_mutex_unlock(&event_buf.lock);

	LOG_DBG("Logged event type=%d at index=%u", event->event_type,
		event_buf.head == 0 ? event_buf.capacity - 1 : event_buf.head - 1);

	return 0;
}

/* 便捷函数: 记录照明状态变化 */
void data_logger_log_lighting(event_type_t type, uint16_t color_temp,
			      uint8_t brightness, uint8_t source)
{
	lighting_event_t event = {0};

	/* 时间戳 */
	event.timestamp = sntp_time_get_timestamp();

	/* 事件类型 */
	event.event_type = (uint8_t)type;

	/* 人员状态 */
	event.presence = (presence_get_state() == PRESENCE_YES) ? 1 : 0;

	/* 照明参数 */
	event.color_temp = color_temp;
	event.brightness = brightness;

	/* 天气和日落 */
	event.weather_code = weather_get_code();
	event.sunset_proximity = sun_get_sunset_proximity();

	float sunrise = sun_get_sunrise_hour();
	float sunset = sun_get_sunset_hour();
	event.sunrise_hour = (uint16_t)(sunrise * 100);
	event.sunset_hour = (uint16_t)(sunset * 100);

	/* AI延迟 */
	event.ai_latency_us = (uint16_t)ai_infer_get_latency_us();

	/* 来源 */
	event.source = source;

	data_logger_log(&event);
}

/* ================================================================
 * AI-024: 缓冲区状态
 * ================================================================ */
int data_logger_get_status(uint32_t *count, uint32_t *capacity)
{
	if (!count || !capacity) {
		return -EINVAL;
	}

	k_mutex_lock(&event_buf.lock, K_FOREVER);
	*count = event_buf.count;
	*capacity = event_buf.capacity;
	k_mutex_unlock(&event_buf.lock);

	return 0;
}

bool data_logger_is_full(void)
{
	return event_buf.full;
}

/* ================================================================
 * AI-025: 记录查询
 * ================================================================ */
int data_logger_query(int64_t start_time, int64_t end_time,
		      lighting_event_t *events, uint32_t max_count,
		      uint32_t *actual_count)
{
	if (!events || !actual_count || max_count == 0) {
		return -EINVAL;
	}

	*actual_count = 0;

	k_mutex_lock(&event_buf.lock, K_FOREVER);

	/* 从tail开始遍历 */
	uint32_t idx = event_buf.tail;
	uint32_t found = 0;

	for (uint32_t i = 0; i < event_buf.count && found < max_count; i++) {
		lighting_event_t *e = &event_buf.buffer[idx];

		/* 时间范围过滤 */
		if (e->timestamp >= start_time && e->timestamp <= end_time) {
			events[found] = *e;
			found++;
		}

		idx = (idx + 1) % event_buf.capacity;
	}

	*actual_count = found;
	k_mutex_unlock(&event_buf.lock);

	LOG_DBG("Query: %lld ~ %lld, found %u records", start_time, end_time, found);
	return 0;
}

int data_logger_get_recent(lighting_event_t *events, uint32_t count,
			   uint32_t *actual_count)
{
	if (!events || !actual_count || count == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&event_buf.lock, K_FOREVER);

	/* 计算实际可返回数量 */
	uint32_t available = event_buf.count < count ? event_buf.count : count;
	*actual_count = available;

	/* 从head往前读取 */
	uint32_t idx = event_buf.head;
	for (uint32_t i = 0; i < available; i++) {
		if (idx == 0) {
			idx = event_buf.capacity - 1;
		} else {
			idx--;
		}
		events[available - 1 - i] = event_buf.buffer[idx];
	}

	k_mutex_unlock(&event_buf.lock);

	return 0;
}

/* ================================================================
 * 调试: 打印统计
 * ================================================================ */
void data_logger_print_stats(void)
{
	k_mutex_lock(&event_buf.lock, K_FOREVER);

	LOG_INF("=== Data Logger Stats ===");
	LOG_INF("  Buffer: %p", event_buf.buffer);
	LOG_INF("  Capacity: %u events (%u bytes)",
		event_buf.capacity, event_buf.capacity * LIGHTING_EVENT_SIZE);
	LOG_INF("  Count: %u events", event_buf.count);
	LOG_INF("  Full: %s", event_buf.full ? "yes" : "no");
	LOG_INF("  Head: %u, Tail: %u", event_buf.head, event_buf.tail);

	/* 最近一条记录 */
	if (event_buf.count > 0) {
		uint32_t last_idx = event_buf.head == 0 ?
				    event_buf.capacity - 1 : event_buf.head - 1;
		lighting_event_t *e = &event_buf.buffer[last_idx];
		LOG_INF("  Last event: type=%d ts=%lld %dK@%d%%",
			e->event_type, e->timestamp,
			e->color_temp, e->brightness);
	}

	k_mutex_unlock(&event_buf.lock);
}
