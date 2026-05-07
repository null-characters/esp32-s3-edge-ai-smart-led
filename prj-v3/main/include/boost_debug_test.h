/**
 * @file boost_debug_test.h
 * @brief Boost升压调试测试 - 简化版
 * 
 * 使用方法:
 * 1. 设置 BOOST_DEBUG_STAGE 选择测试阶段 (0=禁用, 1=固定PWM, 2=滞环, 3=PID)
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
#ifndef BOOST_DEBUG_STAGE
#define BOOST_DEBUG_STAGE       3   /* 0=禁用, 1=固定PWM, 2=滞环, 3=PID, 4=按键开关 */
#endif

/* Boot按键GPIO (ESP32-S3 DevKitC) */
#define BOOST_DEBUG_BTN_GPIO    0

/* 阶段1: 固定PWM参数 */
#define BOOST_DEBUG_FIXED_DUTY  50  /* 占空比 % */

/* 阶段2/3: 目标电压 */
#define BOOST_DEBUG_TARGET_MV   23000   /* 目标电压 mV (硬件压降补偿) */
#define BOOST_DEBUG_WINDOW_MV    200     /* 滞环窗口 ±mV (仅阶段2使用) */

/* 阶段3: PID使用led_pwm.c中的默认参数 */

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief 初始化并启动Boost调试测试
 * 
 * 当 BOOST_DEBUG_STAGE > 0 时自动调用
 */
esp_err_t boost_debug_test_init(void);

/**
 * @brief 检查调试测试是否启用
 */
static inline bool boost_debug_is_enabled(void) {
    return BOOST_DEBUG_STAGE > 0;
}

#endif /* BOOST_DEBUG_TEST_H */
