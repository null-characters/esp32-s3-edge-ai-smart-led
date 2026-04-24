/*
 * Environment-Aware Dimming - 环境感知调光算法
 * Phase 2: WF-026 ~ WF-030
 *
 * 功能: 日落临近度计算、天气补偿、日落渐变、增强调光引擎
 */

#ifndef ENV_DIMMING_H
#define ENV_DIMMING_H

#include <stdint.h>
#include <stdbool.h>
#include "rule_engine.h"
#include "weather_api.h"
#include "sunrise_sunset.h"

/* 补偿参数 */
#define WEATHER_COMP_MAX       25    /* 天气补偿最大增加 25% */
#define SUNSET_PROX_MAX        15    /* 日落临近最大增加 15% */
#define SUNSET_PROX_RANGE_HR   2.0f  /* 日落临近范围: 前后2小时 */

/* 增强调光参数结构体 */
typedef struct {
	lighting_settings_t base;      /* 基础参数 (时段规则) */
	float weather_comp;           /* 天气补偿量 (%) */
	float sunset_comp;            /* 日落补偿量 (%) */
	lighting_settings_t final_;    /* 最终参数 */
} env_dimming_result_t;

/**
 * @brief 初始化环境感知调光模块 (WF-029)
 * @return 0=成功
 */
int env_dimming_init(void);

/**
 * @brief 计算日落临近度 (WF-026)
 * @return 0.0 ~ 1.0 (日落时刻为1.0)
 */
float env_calc_sunset_proximity(void);

/**
 * @brief 计算天气补偿量 (WF-027)
 * @param weather_code 天气编码 (0.0 ~ 1.0)
 * @return 补偿量 (%), 阴天增加最多
 */
float env_calc_weather_compensation(float weather_code);

/**
 * @brief 计算日落渐变补偿量 (WF-028)
 * @param proximity 日落临近度
 * @return 补偿量 (%)
 */
float env_calc_sunset_compensation(float proximity);

/**
 * @brief 增强调光引擎: 融合天气+日落补偿 (WF-029)
 * @param presence 人员状态
 * @param period   时段
 * @param result   输出结果
 * @return 0=成功
 */
int env_dimming_calculate(int presence, int period, env_dimming_result_t *result);

/**
 * @brief 获取增强后的照明参数 (便捷函数)
 * @param settings 输出照明参数
 * @return 0=成功
 */
int env_get_enhanced_settings(lighting_settings_t *settings);

/**
 * @brief 联调测试 (WF-030)
 * 打印各种场景下的调光参数
 */
void env_dimming_test(void);

#endif /* ENV_DIMMING_H */
