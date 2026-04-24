/*
 * Weather API Implementation - 天气数据获取
 * Phase 2: WF-016 ~ WF-020
 *
 * 功能: wttr.in API、天气解析、编码转换、缓存管理
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "weather_api.h"
#include "http_client.h"
#include "sntp_time_sync.h"
#include "wifi_manager.h"

LOG_MODULE_REGISTER(weather, CONFIG_LOG_DEFAULT_LEVEL);

static inline char ascii_tolower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool contains_case_insensitive_ascii(const char *haystack, const char *needle)
{
	if (!haystack || !needle || needle[0] == '\0') {
		return false;
	}

	size_t nlen = strlen(needle);
	if (nlen == 0) {
		return false;
	}

	for (const char *h = haystack; *h; h++) {
		size_t i = 0;
		while (i < nlen && h[i] &&
		       ascii_tolower(h[i]) == ascii_tolower(needle[i])) {
			i++;
		}
		if (i == nlen) {
			return true;
		}
	}
	return false;
}

/* ================================================================
 * 内部状态
 * ================================================================ */
static weather_cache_t weather_cache = {0};
static char response_buf[HTTP_RESPONSE_BUF_SIZE];

/* 天气关键词映射表 (WF-018) */
typedef struct {
	const char *keyword;
	weather_type_t type;
	float code;
} weather_keyword_t;

static const weather_keyword_t weather_keywords[] = {
	{"Sunny",          WEATHER_SUNNY,         WEATHER_CODE_SUNNY},
	{"Clear",          WEATHER_SUNNY,         WEATHER_CODE_SUNNY},
	{"Partly cloudy",  WEATHER_PARTLY_CLOUDY, WEATHER_CODE_PARTLY_CLOUDY},
	{"PartlyCloudy",   WEATHER_PARTLY_CLOUDY, WEATHER_CODE_PARTLY_CLOUDY},
	{"Cloudy",         WEATHER_CLOUDY,        WEATHER_CODE_CLOUDY},
	{"Overcast",       WEATHER_OVERCAST,      WEATHER_CODE_OVERCAST},
	{"Fog",            WEATHER_OVERCAST,      WEATHER_CODE_OVERCAST},
	{"Mist",           WEATHER_OVERCAST,      WEATHER_CODE_OVERCAST},
	{"Haze",           WEATHER_OVERCAST,      WEATHER_CODE_OVERCAST},
	{"Rain",           WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
	{"Drizzle",        WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
	{"Snow",           WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
	{"Thunderstorm",   WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
	{"Thunder",        WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
	{"Shower",         WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
	{"Blizzard",       WEATHER_RAIN_SNOW,     WEATHER_CODE_RAIN_SNOW},
};

/* ================================================================
 * 初始化 (WF-016)
 * ================================================================ */
int weather_api_init(void)
{
	memset(&weather_cache, 0, sizeof(weather_cache));
	weather_cache.valid = false;
	LOG_INF("Weather API initialized");
	return 0;
}

/* ================================================================
 * 获取天气数据 (WF-016)
 * ================================================================ */
int weather_fetch(const char *city)
{
	if (!city) {
		city = WEATHER_DEFAULT_CITY;
	}

	/* 构建wttr.in请求URL
	 * format=3 返回简洁格式: "Beijing: ☀️ +22°C"
	 * format=%C+%t 返回: "Sunny +22°C"
	 */
	char path[128];
	snprintf(path, sizeof(path), "/%s?format=%%C+%%t", city);

	http_response_t response;
	http_response_init(&response, response_buf, sizeof(response_buf));

	int ret = http_get("wttr.in", path, "80", &response);
	if (ret != 0) {
		LOG_ERR("Weather fetch failed: %d", ret);
		return ret;
	}

	/* 解析响应 */
	ret = weather_parse_response(response.body);
	if (ret != 0) {
		LOG_ERR("Weather parse failed: %d", ret);
		return ret;
	}

	return 0;
}

/* ================================================================
 * 解析天气响应 (WF-017)
 * wttr.in format=%C+%t 格式: "Sunny +22°C" 或 "Partly cloudy +15°C"
 * ================================================================ */
int weather_parse_response(const char *response_text)
{
	if (!response_text || strlen(response_text) == 0) {
		return -EINVAL;
	}

	LOG_INF("Parsing weather response: '%s'", response_text);

	/* 提取温度: 查找 '+' 或 '-' 后跟数字 */
	int temp = 0;
	const char *temp_ptr = strrchr(response_text, '+');
	if (!temp_ptr) {
		temp_ptr = strrchr(response_text, '-');
	}
	if (temp_ptr) {
		temp = atoi(temp_ptr);
	}

	/* 提取天气描述: 温度之前的部分 */
	char desc[64] = {0};
	if (temp_ptr) {
		size_t desc_len = temp_ptr - response_text;
		if (desc_len >= sizeof(desc)) {
			desc_len = sizeof(desc) - 1;
		}
		strncpy(desc, response_text, desc_len);
		/* 去除尾部空格 */
		while (desc_len > 0 && desc[desc_len - 1] == ' ') {
			desc[--desc_len] = '\0';
		}
	} else {
		strncpy(desc, response_text, sizeof(desc) - 1);
	}

	/* 编码转换 */
	float code = weather_encode(desc);

	/* 更新缓存 */
	int64_t now = sntp_time_get_timestamp();
	weather_cache.type = WEATHER_SUNNY; /* 会被weather_encode更新 */
	weather_cache.code = code;
	weather_cache.temperature = temp;
	weather_cache.timestamp = now;
	weather_cache.expires = now + WEATHER_CACHE_TTL_S;
	weather_cache.valid = true;
	strncpy(weather_cache.description, desc, sizeof(weather_cache.description) - 1);

	LOG_INF("Weather parsed: desc='%s' code=%.2f temp=%d°C",
		desc, code, temp);
	return 0;
}

/* ================================================================
 * 天气编码转换 (WF-018)
 * ================================================================ */
float weather_encode(const char *description)
{
	if (!description || strlen(description) == 0) {
		return WEATHER_CODE_SUNNY; /* 默认晴天 */
	}

	/* 模糊匹配: 按关键词表顺序匹配 */
	for (int i = 0; i < ARRAY_SIZE(weather_keywords); i++) {
		if (contains_case_insensitive_ascii(description, weather_keywords[i].keyword)) {
			return weather_keywords[i].code;
		}
	}

	/* 未匹配,默认晴天 */
	LOG_WRN("Unknown weather description: '%s', defaulting to sunny", description);
	return WEATHER_CODE_SUNNY;
}

/* ================================================================
 * 天气数据查询
 * ================================================================ */
float weather_get_code(void)
{
	if (!weather_cache.valid) {
		return -1.0f;
	}
	return weather_cache.code;
}

const weather_cache_t *weather_get_cache(void)
{
	if (weather_cache.valid) {
		return &weather_cache;
	}
	return NULL;
}

bool weather_cache_is_valid(void)
{
	if (!weather_cache.valid) {
		return false;
	}

	/* 检查是否过期 */
	int64_t now = sntp_time_get_timestamp();
	if (now > 0 && now > weather_cache.expires) {
		weather_cache.valid = false;
		LOG_INF("Weather cache expired");
		return false;
	}

	return true;
}

/* ================================================================
 * 天气定时更新线程 (WF-020)
 * ================================================================ */
void weather_update_thread_fn(void)
{
	LOG_INF("Weather update thread started");

	/* 等待WiFi连接 */
	while (!wifi_manager_is_connected()) {
		k_sleep(K_SECONDS(5));
	}

	/* 首次获取 */
	int ret = weather_fetch(WEATHER_DEFAULT_CITY);
	if (ret != 0) {
		LOG_WRN("Initial weather fetch failed");
	}

	while (1) {
		/* 每小时更新一次 */
		k_sleep(K_SECONDS(WEATHER_CACHE_TTL_S));

		/* 检查WiFi连接 */
		if (!wifi_manager_is_connected()) {
			LOG_WRN("WiFi not connected, skip weather update");
			continue;
		}

		/* 检查缓存是否过期 */
		if (weather_cache_is_valid()) {
			continue;
		}

		ret = weather_fetch(WEATHER_DEFAULT_CITY);
		if (ret != 0) {
			LOG_WRN("Weather update failed: %d", ret);
		}
	}
}

/* 线程控制（显式启动） */
static K_THREAD_STACK_DEFINE(weather_stack, 4096);
static struct k_thread weather_thread;
static k_tid_t weather_tid;

int weather_api_start(void)
{
	if (weather_tid) {
		return 0;
	}

	weather_tid = k_thread_create(&weather_thread, weather_stack,
				      K_THREAD_STACK_SIZEOF(weather_stack),
				      (k_thread_entry_t)weather_update_thread_fn,
				      NULL, NULL, NULL,
				      8, 0, K_FOREVER);
	k_thread_name_set(weather_tid, "weather_update");
	k_thread_start(weather_tid);
	return 0;
}

int weather_api_stop(void)
{
	if (!weather_tid) {
		return 0;
	}
	k_thread_abort(weather_tid);
	weather_tid = NULL;
	return 0;
}
