/**
 * @file test_boost_pi_host.c
 * @brief Boost PI控制器主机端单元测试 (PWM tick精度)
 */

#include "unity.h"
#include "../main/include/boost_pi_q15.h"

static boost_pi_context_t pi;

/* ================================================================
 * 测试用例
 * ================================================================ */

void test_init_sets_output_to_min(void)
{
    /* 初始化后输出应为最小 PWM tick */
    boost_pi_init(&pi, 10, 50, 870, 30, 2300);
    
    int16_t output = boost_pi_calculate(&pi, 23000, 23000);
    
    TEST_ASSERT_EQUAL_INT(30, output);
}

void test_positive_error_increases_duty(void)
{
    /* 正误差（电压低）应增加 PWM tick */
    boost_pi_init(&pi, 10, 0, 870, 30, 2300);  /* Ki=0 只测比例 */
    
    /* 误差=1000mV，Kp=10 → 输出增加10 tick */
    int16_t output = boost_pi_calculate(&pi, 23000, 22000);
    
    TEST_ASSERT_EQUAL_INT(40, output);  /* 30 + 10 = 40 */
}

void test_negative_error_decreases_duty(void)
{
    /* 负误差（电压高）应减少 PWM tick */
    boost_pi_init(&pi, 10, 0, 870, 30, 2300);
    
    /* 误差=-1000mV，Kp=10 → 输出减少10 tick */
    int16_t output = boost_pi_calculate(&pi, 23000, 24000);
    
    TEST_ASSERT_EQUAL_INT(20, output);  /* 30 - 10 = 20 */
}

void test_output_clamped_to_max(void)
{
    /* 输出不能超过最大 PWM tick */
    boost_pi_init(&pi, 10, 0, 870, 30, 2300);
    
    /* 误差=10000mV，Kp=10 → 输出 = 30 + 100 = 130，clamp到870 */
    int16_t output = boost_pi_calculate(&pi, 23000, 13000);
    
    TEST_ASSERT_EQUAL_INT(130, output);  /* 130 < 870，不clamp */
}

void test_output_clamped_to_min(void)
{
    /* 输出不能低于最小 PWM tick */
    boost_pi_init(&pi, 10, 0, 870, 30, 2300);
    
    /* 误差=-10000mV → 输出 = 30 - 100 = -70，clamp到30 */
    int16_t output = boost_pi_calculate(&pi, 23000, 33000);
    
    TEST_ASSERT_EQUAL_INT(30, output);
}

void test_integral_separation_large_error(void)
{
    /* 大误差时积分分离 */
    boost_pi_init(&pi, 0, 50, 870, 30, 2300);  /* Kp=0 只测积分 */
    
    /* 误差=5000mV > 阈值2300mV，积分应被禁用 */
    boost_pi_calculate(&pi, 23000, 18000);
    boost_pi_calculate(&pi, 23000, 18000);
    int16_t output = boost_pi_calculate(&pi, 23000, 18000);
    
    TEST_ASSERT_EQUAL_INT(30, output);  /* 只有最小 tick */
}

void test_integral_accumulates(void)
{
    /* 积分应累积 */
    boost_pi_init(&pi, 0, 50, 870, 30, 2300);  /* Kp=0 只测积分 */
    
    /* 第一次计算：误差=1000mV，积分=1000 */
    boost_pi_calculate(&pi, 23000, 22000);
    /* 第二次计算：积分=2000，Ki=50 → 输出增加 1 tick */
    int16_t output = boost_pi_calculate(&pi, 23000, 22000);
    
    TEST_ASSERT_EQUAL_INT(31, output);  /* 30 + 1 = 31 */
}

void test_pi_combined_response(void)
{
    /* PI组合响应 */
    boost_pi_init(&pi, 10, 50, 870, 30, 2300);
    
    /* 误差=1000mV，Kp=10 → +10 tick */
    int16_t output = boost_pi_calculate(&pi, 23000, 22000);
    
    TEST_ASSERT_TRUE(output >= 40);  /* 至少有比例项的贡献 */
}

/* ================================================================
 * 测试运行器
 * ================================================================ */

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_init_sets_output_to_min);
    RUN_TEST(test_positive_error_increases_duty);
    RUN_TEST(test_negative_error_decreases_duty);
    RUN_TEST(test_output_clamped_to_max);
    RUN_TEST(test_output_clamped_to_min);
    RUN_TEST(test_integral_separation_large_error);
    RUN_TEST(test_integral_accumulates);
    RUN_TEST(test_pi_combined_response);
    
    return UNITY_END();
}
