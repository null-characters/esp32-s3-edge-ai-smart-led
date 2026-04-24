/*
 * Data Logger - 照明事件记录模块
 * Phase 3: AI-021 ~ AI-025
 *
 * 功能: 事件结构定义、PSRAM环形缓冲区、记录写入/查询
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/* ================================================================
 * AI-021: 事件结构定义
 * ================================================================ */

/* 事件类型 */
typedef enum {
	EVENT_LIGHTS_ON = 0,      /* 开灯 */
	EVENT_LIGHTS_OFF = 1,     /* 关灯 */
	EVENT_PRESENCE_ENTER = 2, /* 人员进入 */
	EVENT_PRESENCE_LEAVE = 3, /* 人员离开 */
	EVENT_AI_UPDATE = 4,      /* AI调光更新 */
	EVENT_MANUAL = 5,         /* 手动调节 */
	EVENT_WEATHER_UPDATE = 6, /* 天气数据更新 */
	EVENT_SUN_UPDATE = 7,     /* 日出日落更新 */
} event_type_t;

/* 照明事件结构 (32字节) */
typedef struct {
	int64_t timestamp;        /* Unix时间戳 (8B) */
	uint8_t event_type;       /* 事件类型 (1B) */
	uint8_t presence;         /* 人员状态 (1B) */
	uint8_t brightness;       /* 光强 (1B) */
	uint8_t reserved;         /* 保留对齐 (1B) */
	uint16_t color_temp;      /* 色温 (2B) */
	float weather_code;       /* 天气编码 (4B) */
	float sunset_proximity;   /* 日落临近度 (4B) */
	uint16_t sunrise_hour;    /* 日出时间 x100 (2B) */
	uint16_t sunset_hour;     /* 日落时间 x100 (2B) */
	uint16_t ai_latency_us;   /* AI推理延迟 (2B) */
	uint8_t source;           /* 来源: 0=规则, 1=AI (1B) */
	uint8_t padding[3];       /* 填充到32B */
} __packed lighting_event_t;

/* 结构大小检查 */
#define LIGHTING_EVENT_SIZE 32
BUILD_ASSERT(sizeof(lighting_event_t) == LIGHTING_EVENT_SIZE,
	     "lighting_event_t must be 32 bytes");

/* ================================================================
 * AI-022 ~ AI-024: PSRAM环形缓冲区
 * ================================================================ */

/* 缓冲区配置 */
#define EVENT_BUFFER_CAPACITY  10000   /* 10K条记录 */
#define EVENT_BUFFER_SIZE      (EVENT_BUFFER_CAPACITY * LIGHTING_EVENT_SIZE)  /* 320KB */

/**
 * @brief 初始化数据记录器 (AI-022)
 * @return 0=成功
 */
int data_logger_init(void);

/**
 * @brief 记录事件 (AI-023)
 * @param event 事件数据
 * @return 0=成功
 */
int data_logger_log(const lighting_event_t *event);

/**
 * @brief 记录照明状态变化 (便捷函数)
 * @param type 事件类型
 * @param color_temp 色温
 * @param brightness 光强
 * @param source 来源
 */
void data_logger_log_lighting(event_type_t type, uint16_t color_temp,
			      uint8_t brightness, uint8_t source);

/**
 * @brief 获取缓冲区状态 (AI-024)
 * @param count 输出: 当前记录数
 * @param capacity 输出: 总容量
 * @return 0=成功
 */
int data_logger_get_status(uint32_t *count, uint32_t *capacity);

/**
 * @brief 检查缓冲区是否已满
 * @return true=已满
 */
bool data_logger_is_full(void);

/* ================================================================
 * AI-025: 记录查询
 * ================================================================ */

/**
 * @brief 按时间范围查询记录 (AI-025)
 * @param start_time 起始时间戳
 * @param end_time 结束时间戳
 * @param events 输出缓冲区
 * @param max_count 最大返回数量
 * @param actual_count 实际返回数量
 * @return 0=成功
 */
int data_logger_query(int64_t start_time, int64_t end_time,
		      lighting_event_t *events, uint32_t max_count,
		      uint32_t *actual_count);

/**
 * @brief 获取最近N条记录
 * @param events 输出缓冲区
 * @param count 请求数量
 * @param actual_count 实际返回数量
 * @return 0=成功
 */
int data_logger_get_recent(lighting_event_t *events, uint32_t count,
			   uint32_t *actual_count);

/**
 * @brief 打印统计信息 (调试用)
 */
void data_logger_print_stats(void);

#endif /* DATA_LOGGER_H */
