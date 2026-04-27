/*
 * Weather API - 天气数据获取模块
 * Phase 2: WF-016 ~ WF-020
 *
 * 功能: wttr.in API封装、天气解析、编码转换、缓存、定时更新
 */

#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <stdint.h>
#include <stdbool.h>

/* 天气编码定义 (WF-018) */
typedef enum {
	WEATHER_SUNNY       = 0,    /* 晴天   → 编码 0.0 */
	WEATHER_PARTLY_CLOUDY = 1,  /* 少云   → 编码 0.25 */
	WEATHER_CLOUDY      = 2,    /* 多云   → 编码 0.5 */
	WEATHER_OVERCAST    = 3,    /* 阴天   → 编码 0.75 */
	WEATHER_RAIN_SNOW   = 4     /* 雨/雪  → 编码 1.0 */
} weather_type_t;

/* 天气编码值 (0.0 ~ 1.0) */
#define WEATHER_CODE_SUNNY          0.0f
#define WEATHER_CODE_PARTLY_CLOUDY  0.25f
#define WEATHER_CODE_CLOUDY         0.5f
#define WEATHER_CODE_OVERCAST       0.75f
#define WEATHER_CODE_RAIN_SNOW      1.0f

/* 天气缓存结构体 (WF-019) */
typedef struct {
	weather_type_t type;      /* 天气类型 */
	float code;               /* 0-1编码值 */
	int temperature;          /* 温度(℃) */
	int64_t timestamp;        /* 获取时间(Unix秒) */
	int64_t expires;          /* 过期时间(Unix秒) */
	bool valid;               /* 缓存是否有效 */
	char description[64];     /* 天气描述文本 */
} weather_cache_t;

/* 缓存有效期: 1小时 */
#define WEATHER_CACHE_TTL_S  3600

/* 默认城市 */
#define WEATHER_DEFAULT_CITY  "Beijing"

/**
 * @brief 初始化天气模块 (WF-016)
 * @return 0=成功
 */
int weather_api_init(void);

/**
 * @brief 获取天气数据 (WF-016)
 * @param city 城市名 (如 "Beijing"), NULL=使用默认
 * @return 0=成功, 负值=失败
 */
int weather_fetch(const char *city);

/**
 * @brief 解析天气响应文本 (WF-017)
 * @param response_text wttr.in原始响应
 * @return 0=成功, 负值=失败
 */
int weather_parse_response(const char *response_text);

/**
 * @brief 天气文本转数值编码 (WF-018)
 * @param description 天气描述文本
 * @return 编码值 (0.0 ~ 1.0)
 */
float weather_encode(const char *description);

/**
 * @brief 获取当前天气编码值 (0-1)
 * @return 编码值, -1.0=无效
 */
float weather_get_code(void);

/**
 * @brief 获取天气缓存 (WF-019)
 * @return 缓存指针 (只读), NULL=无效
 */
const weather_cache_t *weather_get_cache(void);

/**
 * @brief 检查缓存是否有效 (WF-019)
 * @return true=有效
 */
bool weather_cache_is_valid(void);

/**
 * @brief 天气定时更新线程 (WF-020)
 */
void weather_update_thread_fn(void);

/**
 * @brief 启动天气更新线程（显式启动）
 * @return 0=成功
 */
int weather_api_start(void);

/**
 * @brief 停止天气更新线程
 * @return 0=成功
 */
int weather_api_stop(void);

#endif /* WEATHER_API_H */
