/*
 * PIR Sensor Driver Header
 * Phase 1: HW-005 ~ HW-008
 *
 * GPIO4 - PIR人体传感器
 *   GPIO_ACTIVE_HIGH: 有人=高电平, 无人=低电平
 *   内部上拉: 默认状态为高电平(无人时传感器输出低)
 */

#ifndef PIR_H
#define PIR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化PIR传感器GPIO
 *        配置GPIO4为输入模式，启用内部上拉和双边沿中断
 * @return 0=成功, 负值=失败
 */
int pir_init(void);

/**
 * @brief 获取PIR传感器当前状态(已消抖)
 * @return true=有人, false=无人
 */
bool pir_get_status(void);

/**
 * @brief 获取PIR原始引脚电平(未消抖)
 * @return true=高电平, false=低电平
 */
bool pir_get_raw(void);

#endif /* PIR_H */
