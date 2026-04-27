/*
 * Time Period Judgment Module
 * Phase 1: HW-018
 */

#ifndef TIME_PERIOD_H
#define TIME_PERIOD_H

#include <stdint.h>

/* 时段定义 */
typedef enum {
	PERIOD_MORNING   = 0,  /* 上午 6:00 - 11:59 */
	PERIOD_AFTERNOON = 1,  /* 下午 12:00 - 17:59 */
	PERIOD_EVENING   = 2   /* 晚上 18:00 - 5:59 */
} time_period_t;

/* 时段边界 */
#define PERIOD_MORNING_START    6
#define PERIOD_AFTERNOON_START 12
#define PERIOD_EVENING_START   18

/**
 * @brief 根据小时数判断时段
 * @param hour 当前小时 (0-23)
 * @return 时段枚举值
 */
time_period_t time_period_get(uint8_t hour);

/**
 * @brief 获取当前时段 (使用系统时间, 未同步时返回默认值)
 * @return 时段枚举值
 */
time_period_t time_period_get_current(void);

/**
 * @brief 手动设置当前小时 (用于外部时间同步)
 * @param hour 当前小时 (0-23)
 */
void time_period_set_hour(uint8_t hour);

/**
 * @brief 获取时段名称 (用于日志输出)
 * @param period 时段枚举值
 * @return 字符串描述
 */
const char *time_period_name(time_period_t period);

#endif /* TIME_PERIOD_H */
