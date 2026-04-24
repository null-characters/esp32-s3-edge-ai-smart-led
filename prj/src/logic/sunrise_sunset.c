/*
 * Sunrise-Sunset API Implementation - 日出日落时间
 * Phase 2: WF-021 ~ WF-025
 *
 * 功能: API请求、JSON解析、时间转换、缓存管理
 */
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sunrise_sunset.h"
#include "http_client.h"
#include "sntp_time_sync.h"
#include "wifi_manager.h"

LOG_MODULE_REGISTER(sun_api, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static sun_cache_t sun_cache = {0};
static char response_buf[HTTP_RESPONSE_BUF_SIZE];

/* ================================================================
 * 初始化 (WF-021)
 * ================================================================ */
int sun_api_init(void)
{
	memset(&sun_cache, 0, sizeof(sun_cache));
	sun_cache.valid = false;
	LOG_INF("Sunrise-sunset API initialized");
	return 0;
}

/* ================================================================
 * 获取日出日落数据 (WF-021)
 * ================================================================ */
int sun_fetch(double lat, double lng)
{
	/* 构建API URL: https://api.sunrise-sunset.org/json?lat=39.9&lng=116.4 */
	char path[256];
	snprintf(path, sizeof(path),
		 "/json?lat=%.4f&lng=%.4f&formatted=0", lat, lng);

	http_response_t response;
	http_response_init(&response, response_buf, sizeof(response_buf));

	int ret = http_get("api.sunrise-sunset.org", path, "80", &response);
	if (ret != 0) {
		LOG_ERR("Sunrise-sunset fetch failed: %d", ret);
		return ret;
	}

	/* 解析JSON */
	ret = sun_parse_json(response.body);
	if (ret != 0) {
		LOG_ERR("Sunrise-sunset parse failed: %d", ret);
		return ret;
	}

	return 0;
}

/* ================================================================
 * JSON解析结构体 (WF-022)
 * ================================================================ */
struct sun_results {
	char sunrise[32];
	char sunset[32];
	char solar_noon[32];
	char day_length[16];
	char civil_twilight_begin[32];
	char civil_twilight_end[32];
};

struct sun_response {
	char status[16];
	struct sun_results results;
};

static const struct json_obj_descr results_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct sun_results, sunrise, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct sun_results, sunset, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct sun_results, solar_noon, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct sun_results, day_length, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct sun_results, civil_twilight_begin, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct sun_results, civil_twilight_end, JSON_TOK_STRING),
};

static const struct json_obj_descr response_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct sun_response, status, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct sun_response, results, results_descr),
};

/* ================================================================
 * JSON解析 (WF-022)
 * ================================================================ */
int sun_parse_json(const char *json_response)
{
	if (!json_response || strlen(json_response) == 0) {
		return -EINVAL;
	}

	LOG_DBG("Parsing sunrise-sunset JSON: %.100s...", json_response);

	struct sun_response resp;
	memset(&resp, 0, sizeof(resp));

	int ret = json_obj_parse(json_response, strlen(json_response),
				 response_descr, ARRAY_SIZE(response_descr), &resp);
	if (ret < 0) {
		LOG_ERR("JSON parse failed: %d", ret);
		return ret;
	}

	/* 检查状态 */
	if (strcmp(resp.status, "OK") != 0) {
		LOG_ERR("API returned status: %s", resp.status);
		return -EBADMSG;
	}

	/* 转换时间 */
	sun_cache.sunrise_hour = sun_iso_to_hour(resp.results.sunrise);
	sun_cache.sunset_hour = sun_iso_to_hour(resp.results.sunset);
	sun_cache.timestamp = sntp_time_get_timestamp();
	sun_cache.valid = true;

	/* 计算day_of_year */
	local_time_t local;
	if (sntp_time_get_local(&local) == 0) {
		sun_cache.day_of_year = local.month * 100 + local.day;
	}

	LOG_INF("Sunrise-sunset parsed: sunrise=%.2f, sunset=%.2f",
		sun_cache.sunrise_hour, sun_cache.sunset_hour);
	return 0;
}

/* ================================================================
 * ISO8601时间转小时数 (WF-023)
 * 格式: "2026-04-24T06:30:00+00:00" 或 "2026-04-24T22:15:00+00:00"
 * ================================================================ */
float sun_iso_to_hour(const char *iso_time)
{
	if (!iso_time || strlen(iso_time) < 19) {
		return -1.0f;
	}

	/* 找到时间部分 "T" 之后 */
	const char *time_start = strchr(iso_time, 'T');
	if (!time_start) {
		return -1.0f;
	}
	time_start++; /* 跳过 'T' */

	/* 解析 HH:MM:SS */
	int hour, minute, second;
	if (sscanf(time_start, "%d:%d:%d", &hour, &minute, &second) != 3) {
		return -1.0f;
	}

	/* 检查时区偏移 */
	/* API返回UTC时间,需要转换为本地时间 (UTC+8) */
	hour += 8;
	if (hour >= 24) {
		hour -= 24;
	}

	/* 返回小时浮点数 */
	return (float)hour + (float)minute / 60.0f + (float)second / 3600.0f;
}

/* ================================================================
 * 日落临近度计算 (WF-026)
 * 日落前2小时开始变化, 日落后2小时逐渐恢复
 * ================================================================ */
float sun_get_sunset_proximity(void)
{
	if (!sun_cache.valid) {
		return 0.0f;
	}

	/* 获取当前小时 */
	int current_hour = sntp_time_get_hour();
	if (current_hour < 0) {
		return 0.0f;
	}

	float current = (float)current_hour +
			(float)(k_uptime_get() % 3600000) / 3600000.0f;
	float sunset = sun_cache.sunset_hour;

	/* 计算距离日落的时间差 */
	float diff = current - sunset;

	/* 日落前2小时: proximity从0渐变到1
	 * 日落后2小时: proximity从1渐变到0.5 (深夜限幅)
	 */
	if (diff >= -2.0f && diff <= 0.0f) {
		/* 日落前2小时内: 0 → 1 */
		return (diff + 2.0f) / 2.0f;
	} else if (diff > 0.0f && diff <= 2.0f) {
		/* 日落后2小时内: 1 → 0.5 */
		return 1.0f - diff / 4.0f;
	} else if (diff > 2.0f) {
		/* 日落后超过2小时 (深夜) */
		return 0.5f;
	}

	/* 远离日落 */
	return 0.0f;
}

/* ================================================================
 * 数据查询
 * ================================================================ */
float sun_get_sunrise_hour(void)
{
	if (!sun_cache.valid) {
		return -1.0f;
	}
	return sun_cache.sunrise_hour;
}

float sun_get_sunset_hour(void)
{
	if (!sun_cache.valid) {
		return -1.0f;
	}
	return sun_cache.sunset_hour;
}

const sun_cache_t *sun_get_cache(void)
{
	return &sun_cache;
}

bool sun_cache_is_valid(void)
{
	return sun_cache.valid;
}

/* ================================================================
 * 日出日落定时更新线程 (WF-025)
 * 每天凌晨更新一次
 * ================================================================ */
void sun_update_thread_fn(void)
{
	LOG_INF("Sunrise-sunset update thread started");

	/* 等待WiFi连接 */
	while (!wifi_manager_is_connected()) {
		k_sleep(K_SECONDS(5));
	}

	/* 首次获取 */
	int ret = sun_fetch(SUN_DEFAULT_LATITUDE, SUN_DEFAULT_LONGITUDE);
	if (ret != 0) {
		LOG_WRN("Initial sunrise-sunset fetch failed");
	}

	while (1) {
		/* 等待到凌晨3点更新 */
		/* 简化: 每24小时更新一次 */
		k_sleep(K_SECONDS(SUN_CACHE_TTL_S));

		/* 检查WiFi连接 */
		if (!wifi_manager_is_connected()) {
			LOG_WRN("WiFi not connected, skip sunrise-sunset update");
			continue;
		}

		ret = sun_fetch(SUN_DEFAULT_LATITUDE, SUN_DEFAULT_LONGITUDE);
		if (ret != 0) {
			LOG_WRN("Sunrise-sunset update failed: %d", ret);
		}
	}
}

/* 日出日落更新线程定义 */
K_THREAD_DEFINE(sun_thread, 4096,
		sun_update_thread_fn, NULL, NULL, NULL,
		8, 0, 0);
