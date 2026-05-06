/**
 * @file boost_debug_test.h
 * @brief Boost升压调试测试接口
 * 
 * 分阶段测试Boost升压电路:
 * - 阶段1: 固定PWM占空比测试，验证电路
 * - 阶段2: 电压反馈 + 滞环控制
 * - 阶段3: PID闭环控制
 * 
 * 使用方法:
 * 1. 在 menuconfig 中启用 BOOST_DEBUG_TEST_ENABLED
 * 2. 设置 BOOST_DEBUG_TEST_STAGE 选择测试阶段
 * 3. 烧录后观察串口输出
 */

#ifndef BOOST_DEBUG_TEST_H
#define BOOST_DEBUG_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 调试测试阶段定义
 * ================================================================ */

/**
 * @brief 测试阶段枚举
 */
typedef enum {
    BOOST_TEST_STAGE_1_FIXED_PWM = 1,   /**< 阶段1: 固定PWM占空比 */
    BOOST_TEST_STAGE_2_HYSTERESIS = 2,   /**< 阶段2: 滞环控制 */
    BOOST_TEST_STAGE_3_PID = 3,          /**< 阶段3: PID闭环控制 */
} boost_test_stage_t;

/* ================================================================
 * 配置参数 (可通过 Kconfig 或运行时修改)
 * ================================================================ */

/* 阶段1: 固定PWM测试参数 */
#define BOOST_TEST_FIXED_DUTY_DEFAULT    50    /**< 默认固定占空比 (%) */
#define BOOST_TEST_FIXED_DURATION_MS     10000 /**< 测试持续时间 (ms) */

/* 阶段2: 滞环控制参数 */
#define BOOST_TEST_HYSTERESIS_TARGET_MV  24000 /**< 目标电压 (mV) */
#define BOOST_TEST_HYSTERESIS_WINDOW_MV  500   /**< 滞环窗口 (±mV) */
#define BOOST_TEST_HYSTERESIS_STEP       1     /**< 占空比调整步进 (%) */

/* 阶段3: PID控制参数 (继承 led_pwm.h 中的定义) */
#define BOOST_TEST_PID_TARGET_MV         24000 /**< PID目标电压 (mV) */

/* 采样间隔 */
#define BOOST_TEST_SAMPLE_INTERVAL_MS    100   /**< 采样间隔 (ms) */

/* ================================================================
 * API 接口
 * ================================================================ */

/**
 * @brief 初始化Boost调试测试
 * 
 * 根据配置的测试阶段自动运行相应测试
 * 
 * @param stage 测试阶段
 * @return ESP_OK 成功
 */
esp_err_t boost_debug_test_init(boost_test_stage_t stage);

/**
 * @brief 启动调试测试
 * 
 * @return ESP_OK 成功
 */
esp_err_t boost_debug_test_start(void);

/**
 * @brief 停止调试测试
 * 
 * @return ESP_OK 成功
 */
esp_err_t boost_debug_test_stop(void);

/**
 * @brief 获取测试状态
 * 
 * @param running 是否运行中
 * @param stage 当前阶段
 * @param voltage 当前电压 (mV)
 * @param duty 当前占空比 (%)
 * @return ESP_OK 成功
 */
esp_err_t boost_debug_test_get_status(bool *running, boost_test_stage_t *stage,
                                       uint16_t *voltage, uint8_t *duty);

/**
 * @brief 设置固定PWM占空比 (阶段1专用)
 * 
 * @param duty_percent 占空比 (0-100%)
 * @return ESP_OK 成功
 */
esp_err_t boost_debug_set_fixed_duty(uint8_t duty_percent);

/**
 * @brief 设置滞环控制目标电压 (阶段2专用)
 * 
 * @param target_mv 目标电压 (mV)
 * @param hysteresis_mv 滞环窗口 (mV)
 * @return ESP_OK 成功
 */
esp_err_t boost_debug_set_hysteresis_target(uint16_t target_mv, uint16_t hysteresis_mv);

/**
 * @brief 打印测试结果摘要
 */
void boost_debug_print_summary(void);

#endif /* BOOST_DEBUG_TEST_H */
