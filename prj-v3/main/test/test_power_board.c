/**
 * @file test_power_board.c
 * @brief 电源板驱动单元测试
 * 
 * 测试范围:
 * - GPIO配置验证
 * - PWM频率和分辨率
 * - 色温计算逻辑
 * - 电压读取和转换
 * - 边界条件处理
 */

#include "unity.h"
#include "led_pwm.h"
#include <math.h>

/* ================================================================
 * GPIO 配置测试
 * ================================================================ */

void test_gpio_definitions(void)
{
    /* 验证GPIO引脚定义正确 */
    TEST_ASSERT_EQUAL_INT(4, POWER_BOARD_GPIO_BOOST);
    TEST_ASSERT_EQUAL_INT(5, POWER_BOARD_GPIO_WARM);
    TEST_ASSERT_EQUAL_INT(6, POWER_BOARD_GPIO_COLD);
    TEST_ASSERT_EQUAL_INT(7, POWER_BOARD_GPIO_VFB);
}

void test_gpio_no_conflict(void)
{
    /* 验证GPIO无冲突 */
    TEST_ASSERT_TRUE(POWER_BOARD_GPIO_BOOST != POWER_BOARD_GPIO_WARM);
    TEST_ASSERT_TRUE(POWER_BOARD_GPIO_BOOST != POWER_BOARD_GPIO_COLD);
    TEST_ASSERT_TRUE(POWER_BOARD_GPIO_BOOST != POWER_BOARD_GPIO_VFB);
    TEST_ASSERT_TRUE(POWER_BOARD_GPIO_WARM != POWER_BOARD_GPIO_COLD);
    TEST_ASSERT_TRUE(POWER_BOARD_GPIO_WARM != POWER_BOARD_GPIO_VFB);
    TEST_ASSERT_TRUE(POWER_BOARD_GPIO_COLD != POWER_BOARD_GPIO_VFB);
}

/* ================================================================
 * PWM 频率配置测试
 * ================================================================ */

void test_pwm_frequency_definitions(void)
{
    /* Boost PWM: 50kHz */
    TEST_ASSERT_EQUAL_INT(50000, BOOST_PWM_FREQUENCY_HZ);
    
    /* 色温PWM: 3kHz */
    TEST_ASSERT_EQUAL_INT(3000, CCT_PWM_FREQUENCY_HZ);
}

void test_boost_pwm_frequency_range(void)
{
    /* Boost PWM频率应在合理范围 (10kHz - 200kHz) */
    TEST_ASSERT_TRUE(BOOST_PWM_FREQUENCY_HZ >= 10000);
    TEST_ASSERT_TRUE(BOOST_PWM_FREQUENCY_HZ <= 200000);
}

void test_cct_pwm_frequency_range(void)
{
    /* 色温PWM频率应在合理范围 (100Hz - 20kHz) */
    TEST_ASSERT_TRUE(CCT_PWM_FREQUENCY_HZ >= 100);
    TEST_ASSERT_TRUE(CCT_PWM_FREQUENCY_HZ <= 20000);
}

/* ================================================================
 * 电压参数测试
 * ================================================================ */

void test_voltage_definitions(void)
{
    /* 目标电压: 24V */
    TEST_ASSERT_EQUAL_INT(24000, BOOST_TARGET_VOLTAGE_MV);
    
    /* 输入电压: 20V */
    TEST_ASSERT_EQUAL_INT(20000, BOOST_INPUT_VOLTAGE_MV);
}

void test_voltage_divider_ratio(void)
{
    /* 分压比: 10k/(68k+10k) = 10/78 ≈ 0.1282 */
    float expected = 10.0f / 78.0f;
    float actual = VOLTAGE_DIVIDER_RATIO;
    
    /* 允许1%误差 */
    TEST_ASSERT_TRUE(fabsf(actual - expected) < 0.01f);
}

void test_adc_resolution(void)
{
    /* 12-bit ADC: 0-4095 */
    TEST_ASSERT_EQUAL_INT(4095, ADC_RESOLUTION);
}

void test_adc_vref(void)
{
    /* ADC参考电压: 3.3V */
    TEST_ASSERT_EQUAL_INT(3300, ADC_VREF_MV);
}

/* ================================================================
 * 色温参数测试
 * ================================================================ */

void test_color_temp_range(void)
{
    /* 色温范围: 2700K - 6500K */
    TEST_ASSERT_EQUAL_INT(2700, CCT_MIN_KELVIN);
    TEST_ASSERT_EQUAL_INT(6500, CCT_MAX_KELVIN);
    TEST_ASSERT_EQUAL_INT(4000, CCT_DEFAULT_KELVIN);
}

void test_color_temp_valid_range(void)
{
    /* 最小值应小于最大值 */
    TEST_ASSERT_TRUE(CCT_MIN_KELVIN < CCT_MAX_KELVIN);
    
    /* 默认值应在范围内 */
    TEST_ASSERT_TRUE(CCT_DEFAULT_KELVIN >= CCT_MIN_KELVIN);
    TEST_ASSERT_TRUE(CCT_DEFAULT_KELVIN <= CCT_MAX_KELVIN);
}

/* ================================================================
 * 色温计算逻辑测试 (纯计算，无硬件依赖)
 * ================================================================ */

/**
 * @brief 计算色温对应的暖光占空比
 * 
 * 2700K: warm=100%, cold=0%
 * 6500K: warm=0%, cold=100%
 */
static uint8_t calc_warm_duty(uint16_t kelvin)
{
    if (kelvin < CCT_MIN_KELVIN) kelvin = CCT_MIN_KELVIN;
    if (kelvin > CCT_MAX_KELVIN) kelvin = CCT_MAX_KELVIN;
    
    float ratio = (float)(kelvin - CCT_MIN_KELVIN) / (CCT_MAX_KELVIN - CCT_MIN_KELVIN);
    return (uint8_t)((1.0f - ratio) * 100);
}

static uint8_t calc_cold_duty(uint16_t kelvin)
{
    if (kelvin < CCT_MIN_KELVIN) kelvin = CCT_MIN_KELVIN;
    if (kelvin > CCT_MAX_KELVIN) kelvin = CCT_MAX_KELVIN;
    
    float ratio = (float)(kelvin - CCT_MIN_KELVIN) / (CCT_MAX_KELVIN - CCT_MIN_KELVIN);
    return (uint8_t)(ratio * 100);
}

void test_cct_warm_at_min(void)
{
    /* 2700K: 暖光100% */
    TEST_ASSERT_EQUAL_INT(100, calc_warm_duty(2700));
}

void test_cct_warm_at_max(void)
{
    /* 6500K: 暖光0% */
    TEST_ASSERT_EQUAL_INT(0, calc_warm_duty(6500));
}

void test_cct_cold_at_min(void)
{
    /* 2700K: 冷光0% */
    TEST_ASSERT_EQUAL_INT(0, calc_cold_duty(2700));
}

void test_cct_cold_at_max(void)
{
    /* 6500K: 冷光100% */
    TEST_ASSERT_EQUAL_INT(100, calc_cold_duty(6500));
}

void test_cct_midpoint(void)
{
    /* 4600K (中点): 暖光约50%, 冷光约50% */
    uint8_t warm = calc_warm_duty(4600);
    uint8_t cold = calc_cold_duty(4600);
    
    /* 允许±5%误差 */
    TEST_ASSERT_TRUE(warm >= 45 && warm <= 55);
    TEST_ASSERT_TRUE(cold >= 45 && cold <= 55);
}

void test_cct_sum_to_100(void)
{
    /* 任意色温下，暖光+冷光应≈100% */
    for (uint16_t k = 2700; k <= 6500; k += 100) {
        uint8_t warm = calc_warm_duty(k);
        uint8_t cold = calc_cold_duty(k);
        TEST_ASSERT_TRUE(warm + cold >= 95 && warm + cold <= 105);
    }
}

void test_cct_boundary_clamp_low(void)
{
    /* 低于最小值应被限制到最小值 */
    uint8_t warm = calc_warm_duty(1000);  /* 太低 */
    TEST_ASSERT_EQUAL_INT(100, warm);  /* 应限制到2700K的结果 */
}

void test_cct_boundary_clamp_high(void)
{
    /* 高于最大值应被限制到最大值 */
    uint8_t cold = calc_cold_duty(10000);  /* 太高 */
    TEST_ASSERT_EQUAL_INT(100, cold);  /* 应限制到6500K的结果 */
}

/* ================================================================
 * 电压计算测试
 * ================================================================ */

/**
 * @brief 计算实际电压 (模拟ADC读数转换)
 */
static uint16_t calc_actual_voltage(int adc_raw)
{
    int voltage_mv;
    if (adc_raw < 0) adc_raw = 0;
    if (adc_raw > 4095) adc_raw = 4095;
    
    voltage_mv = (adc_raw * ADC_VREF_MV) / ADC_RESOLUTION;
    return (uint16_t)(voltage_mv / VOLTAGE_DIVIDER_RATIO);
}

void test_voltage_at_24v(void)
{
    /* 24V时ADC读数计算
     * V_adc = 24V * 0.1282 = 3.077V
     * ADC_raw = 3.077V / 3.3V * 4095 ≈ 3818
     */
    uint16_t voltage = calc_actual_voltage(3818);
    TEST_ASSERT_TRUE(voltage >= 23500 && voltage <= 24500);
}

void test_voltage_at_20v(void)
{
    /* 20V时ADC读数计算
     * V_adc = 20V * 0.1282 = 2.564V
     * ADC_raw = 2.564V / 3.3V * 4095 ≈ 3182
     */
    uint16_t voltage = calc_actual_voltage(3182);
    TEST_ASSERT_TRUE(voltage >= 19500 && voltage <= 20500);
}

void test_voltage_adc_zero(void)
{
    /* ADC=0时电压应为0 */
    TEST_ASSERT_EQUAL_INT(0, calc_actual_voltage(0));
}

void test_voltage_adc_max(void)
{
    /* ADC=4095时电压应约为25.7V (3.3V / 0.1282) */
    uint16_t voltage = calc_actual_voltage(4095);
    TEST_ASSERT_TRUE(voltage > 25000);
}

/* ================================================================
 * 数据结构测试
 * ================================================================ */

void test_state_struct_size(void)
{
    /* 验证状态结构体大小合理 */
    power_board_state_t state;
    TEST_ASSERT_TRUE(sizeof(state) <= 32);  /* 应小于32字节 */
}

void test_state_struct_fields(void)
{
    power_board_state_t state = {0};
    
    /* 初始化后字段应为0 */
    TEST_ASSERT_EQUAL_INT(0, state.boost_voltage_mv);
    TEST_ASSERT_EQUAL_INT(0, state.warm_duty_percent);
    TEST_ASSERT_EQUAL_INT(0, state.cold_duty_percent);
    TEST_ASSERT_EQUAL_INT(0, state.color_temp_kelvin);
    TEST_ASSERT_EQUAL_INT(0, state.brightness_percent);
    TEST_ASSERT_EQUAL_INT(0, state.boost_duty_percent);
    TEST_ASSERT_EQUAL_INT(0, state.pid_error_mv);
    TEST_ASSERT_FALSE(state.boost_enabled);
    TEST_ASSERT_FALSE(state.led_enabled);
    TEST_ASSERT_FALSE(state.pid_running);
}

/* ================================================================
 * PID 参数测试
 * ================================================================ */

void test_pid_default_params(void)
{
    /* 验证默认PID参数合理 */
    TEST_ASSERT_TRUE(BOOST_PID_DEFAULT_KP > 0);
    TEST_ASSERT_TRUE(BOOST_PID_DEFAULT_KI >= 0);
    TEST_ASSERT_TRUE(BOOST_PID_DEFAULT_KD >= 0);
    TEST_ASSERT_TRUE(BOOST_PID_INTEGRAL_LIMIT > 0);
    TEST_ASSERT_TRUE(BOOST_PID_OUTPUT_LIMIT > 0);
    TEST_ASSERT_TRUE(BOOST_PID_SAMPLE_INTERVAL_MS >= 1);
}

void test_pid_integral_limit(void)
{
    /* 积分限幅应小于输出限幅的倍数 */
    TEST_ASSERT_TRUE(BOOST_PID_INTEGRAL_LIMIT <= BOOST_PID_OUTPUT_LIMIT * 3);
}

void test_pid_output_limit_range(void)
{
    /* 输出限幅应在合理范围 (不能超过PWM占空比范围) */
    TEST_ASSERT_TRUE(BOOST_PID_OUTPUT_LIMIT <= BOOST_MAX_DUTY_PERCENT - BOOST_MIN_DUTY_PERCENT);
}

/* ================================================================
 * 亮度控制测试
 * ================================================================ */

/**
 * @brief 计算亮度调整后的实际占空比
 */
static uint8_t calc_actual_duty(uint8_t base_duty, uint8_t brightness)
{
    return (base_duty * brightness) / 100;
}

void test_brightness_full(void)
{
    /* 亮度100%时，实际占空比等于基础占空比 */
    TEST_ASSERT_EQUAL_INT(50, calc_actual_duty(50, 100));
    TEST_ASSERT_EQUAL_INT(100, calc_actual_duty(100, 100));
}

void test_brightness_half(void)
{
    /* 亮度50%时，实际占空比减半 */
    TEST_ASSERT_EQUAL_INT(50, calc_actual_duty(100, 50));
    TEST_ASSERT_EQUAL_INT(25, calc_actual_duty(50, 50));
}

void test_brightness_zero(void)
{
    /* 亮度0%时，实际占空比为0 */
    TEST_ASSERT_EQUAL_INT(0, calc_actual_duty(100, 0));
    TEST_ASSERT_EQUAL_INT(0, calc_actual_duty(50, 0));
}

void test_brightness_with_cct(void)
{
    /* 色温2700K (warm=100%, cold=0%) + 亮度50% = warm=50%, cold=0% */
    uint8_t warm_actual = calc_actual_duty(100, 50);
    uint8_t cold_actual = calc_actual_duty(0, 50);
    TEST_ASSERT_EQUAL_INT(50, warm_actual);
    TEST_ASSERT_EQUAL_INT(0, cold_actual);
    
    /* 色温6500K (warm=0%, cold=100%) + 亮度30% = warm=0%, cold=30% */
    warm_actual = calc_actual_duty(0, 30);
    cold_actual = calc_actual_duty(100, 30);
    TEST_ASSERT_EQUAL_INT(0, warm_actual);
    TEST_ASSERT_EQUAL_INT(30, cold_actual);
}

void test_brightness_preserves_ratio(void)
{
    /* 亮度调整应保持色温比例 */
    uint8_t warm_base = 60;
    uint8_t cold_base = 40;
    
    for (uint8_t brightness = 10; brightness <= 100; brightness += 10) {
        uint8_t warm_actual = calc_actual_duty(warm_base, brightness);
        uint8_t cold_actual = calc_actual_duty(cold_base, brightness);
        
        /* 比例应保持 warm:cold = 60:40 = 3:2 */
        if (warm_actual > 0 && cold_actual > 0) {
            float ratio = (float)warm_actual / (float)cold_actual;
            TEST_ASSERT_TRUE(fabsf(ratio - 1.5f) < 0.1f);  /* 允许10%误差 */
        }
    }
}

/* ================================================================
 * 测试注册
 * ================================================================ */

void run_test_power_board(void)
{
    /* GPIO测试 */
    RUN_TEST(test_gpio_definitions);
    RUN_TEST(test_gpio_no_conflict);
    
    /* PWM频率测试 */
    RUN_TEST(test_pwm_frequency_definitions);
    RUN_TEST(test_boost_pwm_frequency_range);
    RUN_TEST(test_cct_pwm_frequency_range);
    
    /* 电压参数测试 */
    RUN_TEST(test_voltage_definitions);
    RUN_TEST(test_voltage_divider_ratio);
    RUN_TEST(test_adc_resolution);
    RUN_TEST(test_adc_vref);
    
    /* 色温参数测试 */
    RUN_TEST(test_color_temp_range);
    RUN_TEST(test_color_temp_valid_range);
    
    /* 色温计算测试 */
    RUN_TEST(test_cct_warm_at_min);
    RUN_TEST(test_cct_warm_at_max);
    RUN_TEST(test_cct_cold_at_min);
    RUN_TEST(test_cct_cold_at_max);
    RUN_TEST(test_cct_midpoint);
    RUN_TEST(test_cct_sum_to_100);
    RUN_TEST(test_cct_boundary_clamp_low);
    RUN_TEST(test_cct_boundary_clamp_high);
    
    /* 电压计算测试 */
    RUN_TEST(test_voltage_at_24v);
    RUN_TEST(test_voltage_at_20v);
    RUN_TEST(test_voltage_adc_zero);
    RUN_TEST(test_voltage_adc_max);
    
    /* 数据结构测试 */
    RUN_TEST(test_state_struct_size);
    RUN_TEST(test_state_struct_fields);
    
    /* PID参数测试 */
    RUN_TEST(test_pid_default_params);
    RUN_TEST(test_pid_integral_limit);
    RUN_TEST(test_pid_output_limit_range);
    
    /* 亮度控制测试 */
    RUN_TEST(test_brightness_full);
    RUN_TEST(test_brightness_half);
    RUN_TEST(test_brightness_zero);
    RUN_TEST(test_brightness_with_cct);
    RUN_TEST(test_brightness_preserves_ratio);
}
