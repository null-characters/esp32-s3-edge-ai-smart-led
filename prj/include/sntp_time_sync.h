/*
 * SNTP Time Sync - 时间同步模块
 * Phase 2: WF-006 ~ WF-010
 *
 * 功能: SNTP初始化、时间同步、UTC+8时区转换、RTC更新
 */

#ifndef SNTP_TIME_SYNC_H
#define SNTP_TIME_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/posix/time.h>

/* 时区偏移: UTC+8 (中国标准时间) */
#define TIMEZONE_OFFSET_SECONDS  (8 * 3600)

/* NTP服务器 */
#define NTP_SERVER_1  "ntp.aliyun.com"
#define NTP_SERVER_2  "ntp.tencent.com"
#define NTP_SERVER_3  "pool.ntp.org"

/* 同步间隔 */
#define SNTP_SYNC_INTERVAL_S  3600   /* 每小时同步一次 */

/* 同步超时 */
#define SNTP_SYNC_TIMEOUT_S   30     /* 30秒超时 */

/* 本地时间结构体 */
typedef struct {
	int year;     /* 2026 */
	int month;    /* 1-12 */
	int day;      /* 1-31 */
	int hour;     /* 0-23 */
	int minute;   /* 0-59 */
	int second;   /* 0-59 */
	int weekday;  /* 0=Sunday, 1=Monday, ... */
} local_time_t;

/**
 * @brief 初始化SNTP客户端 (WF-006, WF-007)
 * @return 0=成功, 负值=失败
 */
int sntp_time_init(void);

/**
 * @brief 执行一次时间同步 (WF-008)
 * @return 0=成功, 负值=失败
 */
int sntp_time_sync(void);

/**
 * @brief 检查时间是否已同步
 * @return true=已同步
 */
bool sntp_time_is_synced(void);

/**
 * @brief 获取当前Unix时间戳 (秒)
 * @return 时间戳, 0=未同步
 */
int64_t sntp_time_get_timestamp(void);

/**
 * @brief UTC时间戳转本地时间 (WF-009)
 * @param timestamp Unix时间戳(秒)
 * @param local     输出本地时间结构体
 */
void sntp_time_utc_to_local(int64_t timestamp, local_time_t *local);

/**
 * @brief 获取当前本地时间 (便捷函数)
 * @param local 输出本地时间
 * @return 0=成功, 负值=未同步
 */
int sntp_time_get_local(local_time_t *local);

/**
 * @brief 获取当前本地小时数 (用于时段判断)
 * @return 小时数(0-23), 负值=未同步
 */
int sntp_time_get_hour(void);

/**
 * @brief 更新系统RTC (WF-010)
 * @return 0=成功, 负值=失败
 */
int sntp_time_update_rtc(void);

/**
 * @brief SNTP周期同步线程 (WF-010)
 */
void sntp_time_thread_fn(void);

/**
 * @brief 启动SNTP周期同步线程（显式启动）
 * @return 0=成功
 */
int sntp_time_start(void);

/**
 * @brief 停止SNTP周期同步线程
 * @return 0=成功
 */
int sntp_time_stop(void);

#endif /* SNTP_TIME_SYNC_H */
