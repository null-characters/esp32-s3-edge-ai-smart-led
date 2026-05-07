/**
 * @file boost_pi_q15.c
 * @brief 简化位置式PI控制器实现 - PWM tick精度
 */

#include "boost_pi_q15.h"
#include <stddef.h>

/* 积分限幅范围（防止积分饱和） */
#define INTEGRAL_MAX  1000000
#define INTEGRAL_MIN -1000000

void boost_pi_init(boost_pi_context_t *pi, 
                   int16_t Kp, int16_t Ki,
                   int16_t OutMax, int16_t OutMin,
                   int16_t Threshold)
{
    if (pi == NULL) return;

    pi->Kp = Kp;
    pi->Ki = Ki;
    pi->OutMax = OutMax;
    pi->OutMin = OutMin;
    pi->IntegralSepThreshold = Threshold;
    pi->Integral = 0;
}

int16_t boost_pi_calculate(boost_pi_context_t *pi, int16_t target, int16_t current)
{
    if (pi == NULL) return pi ? pi->OutMin : 0;

    int16_t error = target - current;  /* 正值表示电压低，需要增加占空比 */
    
    /* 积分分离：大误差时禁用积分 */
    int16_t abs_error = (error >= 0) ? error : -error;
    if (abs_error < pi->IntegralSepThreshold) {
        pi->Integral += error;
        if (pi->Integral > INTEGRAL_MAX) pi->Integral = INTEGRAL_MAX;
        if (pi->Integral < INTEGRAL_MIN) pi->Integral = INTEGRAL_MIN;
    }
    
    /* 位置式PI: output = OutMin + Kp*error/1000 + Ki*Integral/100000 */
    int32_t output = pi->OutMin;
    output += (int32_t)error * pi->Kp / 1000;
    output += pi->Integral * pi->Ki / 100000;
    
    /* 输出限幅 */
    if (output > pi->OutMax) output = pi->OutMax;
    if (output < pi->OutMin) output = pi->OutMin;
    
    return (int16_t)output;
}

void boost_pi_reset(boost_pi_context_t *pi)
{
    if (pi == NULL) return;
    pi->Integral = 0;
}