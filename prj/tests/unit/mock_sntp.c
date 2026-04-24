/*
 * Mock SNTP Time Sync - 测试桩实现
 * 替代真实SNTP网络依赖
 */

#include <zephyr/kernel.h>
#include <stdbool.h>
#include "sntp_time_sync.h"

static bool mock_synced = false;
static int64_t mock_timestamp = 1745500800; /* 2026-04-24 08:00:00 UTC */

int sntp_time_init(void) { return 0; }
int sntp_time_sync(void) { mock_synced = true; return 0; }
bool sntp_time_is_synced(void) { return mock_synced; }
int64_t sntp_time_get_timestamp(void) { return mock_synced ? mock_timestamp : 0; }

void sntp_time_utc_to_local(int64_t timestamp, local_time_t *local)
{
	if (!local) return;
	/* UTC+8: 08:00 → 16:00 */
	time_t lt = (time_t)(timestamp + 8 * 3600);
	struct tm *t = gmtime(&lt);
	if (t) {
		local->year = t->tm_year + 1900;
		local->month = t->tm_mon + 1;
		local->day = t->tm_mday;
		local->hour = t->tm_hour;
		local->minute = t->tm_min;
		local->second = t->tm_sec;
		local->weekday = t->tm_wday;
	}
}

int sntp_time_get_local(local_time_t *local)
{
	if (!local) return -1;
	if (!mock_synced) return -1;
	sntp_time_utc_to_local(mock_timestamp, local);
	return 0;
}

int sntp_time_get_hour(void)
{
	local_time_t local;
	if (sntp_time_get_local(&local) == 0) return local.hour;
	return -1;
}

int sntp_time_update_rtc(void) { return 0; }
void sntp_time_thread_fn(void) { /* no-op in test */ }
