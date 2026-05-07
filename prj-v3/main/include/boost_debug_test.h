/**
 * @file boost_debug_test.h
 * @brief 电源板调试测试 - Boost升压 + 色温控制
 * 
 * 使用方法:
 * 1. 设置 TEST_STAGE 选择测试阶段
 * 2. 烧录后观察串口输出
 */

#ifndef BOOST_DEBUG_TEST_H
#define BOOST_DEBUG_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ========================================================================
 * 调试宏配置 - 修改此处选择测试阶段
 * ======================================================================== */
#ifndef TEST_STAGE
#define TEST_STAGE       4   /* 0=禁用, 1-3=Boost测试, 4=色温渐变测试 */
#endif

/* Boot按键GPIO (ESP32-S3 DevKitC) */
#define BOOST_DEBUG_BTN_GPIO    0

/* Boost阶段参数 */
#define BOOST_DEBUG_FIXED_DUTY  50      /* 阶段1: 固定占空比 % */
#define BOOST_DEBUG_TARGET_MV   23000  /* 目标电压 mV */
#define BOOST_DEBUG_WINDOW_MV    200    /* 滞环窗口 ±mV */

/* 色温测试参数 */
#define CCT_TEST_MIN_KELVIN     2700    /* 暖白 */
#define CCT_TEST_MAX_KELVIN     6500    /* 冷白 */
#define CCT_TEST_STEP_MS        50      /* 渐变步进间隔 */
#define CCT_TEST_CYCLE_MS       5000    /* 单次渐变周期 */

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief 初始化并启动调试测试
 */
esp_err_t boost_debug_test_init(void);

/**
 * @brief 检查调试测试是否启用
 */
static inline bool boost_debug_is_enabled(void) {
    return TEST_STAGE > 0;
}

#endif /* BOOST_DEBUG_TEST_H */
