/**
 * @file boost_pi_q15.h
 * @brief 简化位置式PI控制器 - PWM tick精度
 * 
 * 输出范围：0-1023 (10-bit PWM tick值)
 * 参数含义：
 * - Kp: 每1000mV误差调整Kp个PWM tick
 * - Ki: 每1000mV*ms积分调整Ki个PWM tick
 */

#ifndef BOOST_PI_Q15_H
#define BOOST_PI_Q15_H

#include <stdint.h>

/* PI控制器结构体 */
typedef struct {
    int16_t Kp;                   /**< 比例系数：每1000mV误差调整Kp个tick */
    int16_t Ki;                   /**< 积分系数：每1000mV*ms积分调整Ki个tick */
    int32_t Integral;             /**< 积分累积值（误差*mV） */
    int16_t OutMax;               /**< 输出上限（PWM tick，如1023） */
    int16_t OutMin;               /**< 输出下限（PWM tick，如30） */
    int16_t IntegralSepThreshold; /**< 积分分离阈值（mV） */
} boost_pi_context_t;

/**
 * @brief 初始化PI控制器
 * 
 * @param pi        PI控制器实例
 * @param Kp        比例系数（每1000mV误差调整Kp个tick）
 * @param Ki        积分系数（每1000mV*ms积分调整Ki个tick）
 * @param OutMax    输出上限（PWM tick，如1023）
 * @param OutMin    输出下限（PWM tick，如30）
 * @param Threshold 积分分离阈值（mV）
 */
void boost_pi_init(boost_pi_context_t *pi, 
                   int16_t Kp, int16_t Ki,
                   int16_t OutMax, int16_t OutMin,
                   int16_t Threshold);

/**
 * @brief 执行PI控制计算
 * 
 * @param pi         PI控制器实例
 * @param target     目标值（mV）
 * @param current    当前值（mV）
 * @return 目标PWM tick值（0-1023）
 */
int16_t boost_pi_calculate(boost_pi_context_t *pi, int16_t target, int16_t current);

/**
 * @brief 重置PI控制器
 */
void boost_pi_reset(boost_pi_context_t *pi);

#endif /* BOOST_PI_Q15_H */