/*
 * SNTP Time Sync Implementation - 时间同步
 * Phase 2: WF-006 ~ WF-010
 *
 * 功能: SNTP同步、UTC+8时区转换、RTC更新
 */
#include <zephyr/kernel.h>
#include <zephyr/net/sntp.h>
#include <zephyr/posix/time.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "sntp_time_sync.h"
#include "time_period.h"

LOG_MODULE_REGISTER(sntp_sync, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static bool time_synced = false;
static int64_t sync_timestamp = 0;   /* 上次同步的Unix时间戳 */

/* ================================================================
 * SNTP初始化 (WF-006, WF-007)
 * ================================================================ */
int sntp_time_init(void)
{
	LOG_INF("Initializing SNTP client...");

	/* Zephyr SNTP通过socket API工作,无需额外初始化
	 * NTP服务器在prj.conf中通过CONFIG_SNTP配置
	 * 时区转换由本模块统一处理(UTC -> UTC+8)
	 */
	LOG_INF("SNTP client initialized (timezone offset: +8h)");
	return 0;
}

/* ================================================================
 * 时间同步 (WF-008)
 * ================================================================ */
int sntp_time_sync(void)
{
	struct sntp_time sntp_time;
	int64_t timeout_ms = SNTP_SYNC_TIMEOUT_S * 1000;
	int ret;

	/* 尝试多个NTP服务器 */
	const char *servers[] = {
		NTP_SERVER_1,
		NTP_SERVER_2,
		NTP_SERVER_3
	};

	for (int i = 0; i < ARRAY_SIZE(servers); i++) {
		LOG_INF("Trying NTP server: %s", servers[i]);

		ret = sntp_simple(servers[i], timeout_ms, &sntp_time);
		if (ret == 0) {
			sync_timestamp = sntp_time.seconds;
			time_synced = true;

			/* 更新系统时钟 */
			struct timespec ts = {
				.tv_sec = sntp_time.seconds,
				.tv_nsec = sntp_time.microseconds * 1000,
			};
			clock_settime(CLOCK_REALTIME, &ts);

			/* 更新RTC */
			sntp_time_update_rtc();

			/* 更新时段判断模块的小时数 */
			local_time_t local;
			sntp_time_utc_to_local(sync_timestamp, &local);
			time_period_set_hour(local.hour);

			LOG_INF("SNTP sync OK: timestamp=%lld, local=%04d-%02d-%02d %02d:%02d:%02d",
				sync_timestamp,
				local.year, local.month, local.day,
				local.hour, local.minute, local.second);
			return 0;
		}

		LOG_WRN("NTP server %s failed: %d", servers[i], ret);
	}

	LOG_ERR("All NTP servers failed");
	time_synced = false;
	return -ETIMEDOUT;
}

/* ================================================================
 * 状态查询
 * ================================================================ */
bool sntp_time_is_synced(void)
{
	return time_synced;
}

int64_t sntp_time_get_timestamp(void)
{
	if (!time_synced) {
		return 0;
	}

	/* 从系统时钟获取当前时间 */
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (int64_t)ts.tv_sec;
}

/* ================================================================
 * UTC转本地时间 (WF-009)
 * ================================================================ */
void sntp_time_utc_to_local(int64_t timestamp, local_time_t *local)
{
	if (!local) {
		return;
	}

	/* 加上时区偏移 */
	time_t local_time = (time_t)(timestamp + TIMEZONE_OFFSET_SECONDS);
	struct tm *tm_info = gmtime(&local_time);

	if (tm_info) {
		local->year = tm_info->tm_year + 1900;
		local->month = tm_info->tm_mon + 1;
		local->day = tm_info->tm_mday;
		local->hour = tm_info->tm_hour;
		local->minute = tm_info->tm_min;
		local->second = tm_info->tm_sec;
		local->weekday = tm_info->tm_wday;
	}
}

/* ================================================================
 * 获取本地时间
 * ================================================================ */
int sntp_time_get_local(local_time_t *local)
{
	if (!local) {
		return -EINVAL;
	}

	if (!time_synced) {
		return -ENODATA;
	}

	int64_t ts = sntp_time_get_timestamp();
	sntp_time_utc_to_local(ts, local);
	return 0;
}

/* ================================================================
 * 获取当前小时 (用于时段判断)
 * ================================================================ */
int sntp_time_get_hour(void)
{
	if (!time_synced) {
		return -ENODATA;
	}

	local_time_t local;
	if (sntp_time_get_local(&local) == 0) {
		return local.hour;
	}
	return -1;
}

float sntp_time_get_local_hour_f(void)
{
	if (!time_synced) {
		return -1.0f;
	}

	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
		return -1.0f;
	}

	/* 转为本地秒（UTC+8） */
	int64_t local_sec = (int64_t)ts.tv_sec + TIMEZONE_OFFSET_SECONDS;

	/* 取当天秒数 */
	int64_t sod = local_sec % 86400;
	if (sod < 0) {
		sod += 86400;
	}

	double sec_f = (double)sod + (double)ts.tv_nsec / 1e9;
	double hour = sec_f / 3600.0;

	/* 规范到 [0,24) */
	if (hour < 0.0) {
		hour += 24.0;
	} else if (hour >= 24.0) {
		hour -= 24.0;
	}

	return (float)hour;
}

/* ================================================================
 * 更新RTC (WF-010)
 * ================================================================ */
int sntp_time_update_rtc(void)
{
	/* Zephyr中clock_settime已经更新了系统RTC
	 * 对于ESP32-S3, 硬件RTC由Zephyr底层驱动管理
	 */
	LOG_INF("RTC updated from SNTP");
	return 0;
}

/* ================================================================
 * SNTP周期同步线程
 * - 注意: 不再使用 K_THREAD_DEFINE 自动启动，避免“名义未启动但实际在跑”
 * ================================================================ */
void sntp_time_thread_fn(void)
{
	LOG_INF("SNTP sync thread started");

	/* 首次同步 */
	int ret = sntp_time_sync();
	if (ret != 0) {
		LOG_WRN("Initial SNTP sync failed, will retry");
	}

	while (1) {
		/* 等待同步间隔 */
		k_sleep(K_SECONDS(SNTP_SYNC_INTERVAL_S));

		/* 定期重新同步 */
		ret = sntp_time_sync();
		if (ret != 0) {
			LOG_WRN("Periodic SNTP sync failed, retry next cycle");
		}
	}
}

/* 线程控制（显式启动） */
static K_THREAD_STACK_DEFINE(sntp_sync_stack, 2048);
static struct k_thread sntp_sync_thread;
static k_tid_t sntp_sync_tid;

int sntp_time_start(void)
{
	if (sntp_sync_tid) {
		return 0;
	}

	sntp_sync_tid = k_thread_create(&sntp_sync_thread, sntp_sync_stack,
					K_THREAD_STACK_SIZEOF(sntp_sync_stack),
					(k_thread_entry_t)sntp_time_thread_fn,
					NULL, NULL, NULL,
					8, 0, K_FOREVER);
	k_thread_name_set(sntp_sync_tid, "sntp_sync");
	k_thread_start(sntp_sync_tid);
	return 0;
}

int sntp_time_stop(void)
{
	if (!sntp_sync_tid) {
		return 0;
	}
	k_thread_abort(sntp_sync_tid);
	sntp_sync_tid = NULL;
	return 0;
}
