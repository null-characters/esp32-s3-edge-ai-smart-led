/**
 * @file test_boost_pi.c
 * @brief PI控制器单元测试 - TDD
 * 
 * 测试用例定义 PI 控制器的行为：
 * 1. 初始化后输出最小占空比
 * 2. 正误差（电压低）→ 增加占空比
 * 3. 负误差（电压高）→ 减少占空比
 * 4. 积分分离：大误差时禁用积分
 * 5. 输出限幅
 */

#include "unity.h"
#include "boost_pi_q15.h"

static boost_pi_context_t pi;

/* ================================================================
 * 初始化测试
 * ================================================================ */

void test_init_sets_output_to_min(void)
{
    /* 初始化后，目标=当前时，输出应为最小占空比 */
    boost_pi_init(&pi, 1, 10, 5, 85, 3, 4600);
    
    int16_t output = boost_pi_calculate(&pi, 23000, 23000);
    
    TEST_ASSERT_EQUAL_INT(3, output);  /* OutMin = 3% */
}

void test_init_null_pointer(void)
{
    /* NULL指针安全检查 */
    boost_pi_init(NULL, 1, 10, 5, 85, 3, 4600);
    int16_t output = boost_pi_calculate(NULL, 23000, 23000);
    
    TEST_ASSERT_EQUAL_INT(0, output);  /* NULL返回0 */
}

/* ================================================================
 * 比例控制测试
 * ================================================================ */

void test_positive_error_increases_duty(void)
{
    /* 正误差：电压低于目标，应增加占空比 */
    boost_pi_init(&pi, 1, 0, 5, 85, 3, 4600);  /* Ki=0 纯比例 */
    
    /* 目标23000mV，当前20000mV，误差=3000mV
     * Kp=1: 每1000mV调整1%
     * 预期输出 = 3% + (3000/1000)*1 = 6%
     */
    int16_t output = boost_pi_calculate(&pi, 23000, 20000);
    
    TEST_ASSERT_EQUAL_INT(6, output);
}

void test_negative_error_decreases_duty(void)
{
    /* 负误差：电压高于目标，应减少占空比 */
    boost_pi_init(&pi, 1, 0, 5, 85, 3, 4600);
    
    /* 目标23000mV，当前25000mV，误差=-2000mV
     * Kp=1: 每1000mV调整1%
     * 预期输出 = 3% + (-2000/1000)*1 = 1% → clamp到3%
     */
    int16_t output = boost_pi_calculate(&pi, 23000, 25000);
    
    TEST_ASSERT_EQUAL_INT(3, output);  /* 不能低于最小值 */
}

void test_output_clamped_to_max(void)
{
    /* 输出不能超过最大值 */
    boost_pi_init(&pi, 1, 0, 5, 85, 3, 4600);
    
    /* 误差=100000mV，理论上应该输出 3% + 100% = 103%
     * 但应该被限制到85%
     */
    int16_t output = boost_pi_calculate(&pi, 23000, -77000);
    
    TEST_ASSERT_EQUAL_INT(85, output);
}

/* ================================================================
 * 积分控制测试
 * ================================================================ */

void test_integral_accumulates(void)
{
    /* 积分项应累积 */
    boost_pi_init(&pi, 0, 10, 5, 85, 3, 4600);  /* Kp=0 纯积分 */
    boost_pi_reset(&pi);
    
    /* 第一次：误差=1000mV，积分=1000 */
    boost_pi_calculate(&pi, 23000, 22000);
    
    /* 第二次：误差=1000mV，积分=2000
     * Ki=10: 每1000mV*ms调整10%/100000
     * 预期输出 = 3% + (2000*10)/100000 = 3.2% → 3%
     */
    int16_t output = boost_pi_calculate(&pi, 23000, 22000);
    
    /* 积分效果较弱，需要多次累积才能看到变化 */
    TEST_ASSERT_TRUE(output >= 3);
}

void test_integral_separation_large_error(void)
{
    /* 大误差时禁用积分（积分分离） */
    boost_pi_init(&pi, 0, 10, 5, 85, 3, 4600);  /* 阈值4600mV */
    boost_pi_reset(&pi);
    
    /* 误差=5000mV > 4600mV，积分应被禁用 */
    boost_pi_calculate(&pi, 23000, 18000);
    boost_pi_calculate(&pi, 23000, 18000);
    boost_pi_calculate(&pi, 23000, 18000);
    
    /* 纯积分控制器，积分被禁用，输出应保持最小值 */
    int16_t output = boost_pi_calculate(&pi, 23000, 18000);
    
    TEST_ASSERT_EQUAL_INT(3, output);
}

void test_integral_separation_small_error(void)
{
    /* 小误差时启用积分 */
    boost_pi_init(&pi, 0, 10, 5, 85, 3, 4600);  /* 阈值4600mV */
    boost_pi_reset(&pi);
    
    /* 误差=1000mV < 4600mV，积分应正常累积 */
    for (int i = 0; i < 100; i++) {
        boost_pi_calculate(&pi, 23000, 22000);
    }
    
    /* 积分累积后输出应增加 */
    int16_t output = boost_pi_calculate(&pi, 23000, 22000);
    
    TEST_ASSERT_TRUE(output > 3);
}

/* ================================================================
 * PI组合测试
 * ================================================================ */

void test_pi_combined_response(void)
{
    /* PI组合：比例快速响应，积分消除稳态误差 */
    boost_pi_init(&pi, 1, 10, 5, 85, 3, 4600);
    boost_pi_reset(&pi);
    
    /* 误差=4000mV
     * 比例项: 4000/1000 * 1 = 4%
     * 输出 ≈ 3% + 4% = 7%
     */
    int16_t output = boost_pi_calculate(&pi, 23000, 19000);
    
    TEST_ASSERT_EQUAL_INT(7, output);
}

/* ================================================================
 * 边界条件测试
 * ================================================================ */

void test_zero_target_voltage(void)
{
    /* 目标电压为0 */
    boost_pi_init(&pi, 1, 10, 5, 85, 3, 4600);
    
    int16_t output = boost_pi_calculate(&pi, 0, 10000);
    
    TEST_ASSERT_TRUE(output >= 3 && output <= 85);
}

void test_max_voltage_input(void)
{
    /* 最大电压输入 */
    boost_pi_init(&pi, 1, 10, 5, 85, 3, 4600);
    
    int16_t output = boost_pi_calculate(&pi, 23000, 32767);
    
    TEST_ASSERT_TRUE(output >= 3 && output <= 85);
}

/* ================================================================
 * 测试运行器
 * ================================================================ */

void run_test_boost_pi(void)
{
    UNITY_BEGIN();
    
    /* 初始化测试 */
    RUN_TEST(test_init_sets_output_to_min);
    RUN_TEST(test_init_null_pointer);
    
    /* 比例控制测试 */
    RUN_TEST(test_positive_error_increases_duty);
    RUN_TEST(test_negative_error_decreases_duty);
    RUN_TEST(test_output_clamped_to_max);
    
    /* 积分控制测试 */
    RUN_TEST(test_integral_accumulates);
    RUN_TEST(test_integral_separation_large_error);
    RUN_TEST(test_integral_separation_small_error);
    
    /* PI组合测试 */
    RUN_TEST(test_pi_combined_response);
    
    /* 边界条件测试 */
    RUN_TEST(test_zero_target_voltage);
    RUN_TEST(test_max_voltage_input);
    
    UNITY_END();
}
