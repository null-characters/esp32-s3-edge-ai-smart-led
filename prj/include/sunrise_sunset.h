/*
 * Sunrise-Sunset API - 日出日落时间获取模块
 * Phase 2: WF-021 ~ WF-025
 *
 * 功能: sunrise-sunset.org API封装、JSON解析、时间转换、缓存、定时更新
 */

#ifndef SUNRISE_SUNSET_H
#define SUNRISE_SUNSET_H

#include <stdint.h>
#include <stdbool.h>

/* 太阳数据缓存结构体 (WF-024) */
typedef struct {
	float sunrise_hour;      /* 日出时间 (小时, 如 6.5 = 6:30) */
	float sunset_hour;       /* 日落时间 (小时, 如 18.75 = 18:45) */
	int day_of_year;         /* 数据对应的日期 (用于判断是否需要更新) */
	int64_t timestamp;       /* 获取时间 */
	bool valid;              /* 数据是否有效 */
} sun_cache_t;

/* 默认经纬度 (北京) */
#define SUN_DEFAULT_LATITUDE   39.9042
#define SUN_DEFAULT_LONGITUDE  116.4074

/* 缓存有效期: 24小时 */
#define SUN_CACHE_TTL_S  86400

/**
 * @brief 初始化日出日落模块 (WF-021)
 * @return 0=成功
 */
int sun_api_init(void);

/**
 * @brief 获取日出日落数据 (WF-021)
 * @param lat 纬度
 * @param lng 经度
 * @return 0=成功, 负值=失败
 */
int sun_fetch(double lat, double lng);

/**
 * @brief 解析JSON响应 (WF-022)
 * @param json_response API返回的JSON
 * @return 0=成功, 负值=失败
 */
int sun_parse_json(const char *json_response);

/**
 * @brief ISO8601时间转小时数 (WF-023)
 * @param iso_time ISO8601格式时间字符串 (如 "2026-04-24T06:30:00+00:00")
 * @return 小时数 (0-24), 如 6.5 表示 6:30
 */
float sun_iso_to_hour(const char *iso_time);

/**
 * @brief 获取日落临近度 (WF-026)
 * 返回当前时间距离日落的归一化值
 * @return 0.0=远离日落, 1.0=日落时刻, >1.0=日落后
 */
float sun_get_sunset_proximity(void);

/**
 * @brief 获取日出时间 (小时)
 * @return 日出小时数 (0-24), -1=无效
 */
float sun_get_sunrise_hour(void);

/**
 * @brief 获取日落时间 (小时)
 * @return 日落小时数 (0-24), -1=无效
 */
float sun_get_sunset_hour(void);

/**
 * @brief 获取太阳数据缓存 (WF-024)
 * @return 缓存指针 (只读)
 */
const sun_cache_t *sun_get_cache(void);

/**
 * @brief 检查缓存是否有效
 * @return true=有效
 */
bool sun_cache_is_valid(void);

/**
 * @brief 日出日落定时更新线程 (WF-025)
 */
void sun_update_thread_fn(void);

#endif /* SUNRISE_SUNSET_H */
