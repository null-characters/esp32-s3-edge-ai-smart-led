/**
 * @file boost_debug_test.c
 * @brief 电源板调试测试 - Boost升压 + 色温控制
 */

#include "boost_debug_test.h"
#include "led_pwm.h"
#include "config_constants.h"
#include "boost_pi_q15.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "POWER_DBG";

/* LEDC配置 (与led_pwm.c一致) */
#define BOOST_LEDC_TIMER    LEDC_TIMER_0
#define BOOST_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BOOST_LEDC_MODE     LEDC_LOW_SPEED_MODE

/* ========================================================================
 * 内部函数
 * ======================================================================== */

#if TEST_STAGE > 0 && TEST_STAGE < 4
/* PWM tick 范围 (10-bit: 0-1023) */
#define BOOST_PWM_TICK_MAX   1023
#define BOOST_PWM_TICK_MIN   30    /* ≈3% */

static void set_duty_tick(uint16_t tick)
{
    if (tick > BOOST_PWM_TICK_MAX) tick = BOOST_PWM_TICK_MAX;
    if (tick < BOOST_PWM_TICK_MIN && tick > 0) tick = BOOST_PWM_TICK_MIN;
    
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, tick);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
}

/* 兼容旧阶段：百分比转tick */
static void set_duty(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint16_t tick = (percent * 1023) / 100;
    set_duty_tick(tick);
}
#endif

/* ========================================================================
 * Boost测试任务 (阶段1-3)
 * ======================================================================== */

#if TEST_STAGE == 1
static void stage1_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段1: 固定PWM测试 ===");
    ESP_LOGI(TAG, "占空比: %d%%, 请用万用表测量Boost输出", BOOST_DEBUG_FIXED_DUTY);
    
    set_duty(BOOST_DEBUG_FIXED_DUTY);
    
    while (1) {
        uint16_t v = boost_read_voltage();
        ESP_LOGI(TAG, "V=%d mV, Duty=%d%%", v, BOOST_DEBUG_FIXED_DUTY);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

#if TEST_STAGE == 2
static void stage2_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段2: 滞环控制 ===");
    ESP_LOGI(TAG, "目标: %d mV ± %d mV", BOOST_DEBUG_TARGET_MV, BOOST_DEBUG_WINDOW_MV);
    
    uint8_t duty = BOOST_MIN_DUTY_PERCENT;
    set_duty(duty);
    
    while (1) {
        uint16_t v = boost_read_voltage();
        int16_t err = BOOST_DEBUG_TARGET_MV - v;
        
        if (err > BOOST_DEBUG_WINDOW_MV && duty < BOOST_MAX_DUTY_PERCENT) {
            duty++;
            set_duty(duty);
        } else if (err < -BOOST_DEBUG_WINDOW_MV && duty > BOOST_MIN_DUTY_PERCENT) {
            duty--;
            set_duty(duty);
        }
        
        ESP_LOGI(TAG, "V=%d mV, Err=%d mV, Duty=%d%%", v, err, duty);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif

#if TEST_STAGE == 3
static boost_pi_context_t g_boost_pi;

static void stage3_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段3: PI控制 (PWM tick精度) ===");
    ESP_LOGI(TAG, "目标: %d mV", BOOST_DEBUG_TARGET_MV);
    
    /* PWM tick 精度控制 */
    boost_pi_init(&g_boost_pi, 10, 50, 870, 30, 2300);
    
    uint16_t duty_tick = BOOST_PWM_TICK_MIN;
    set_duty_tick(duty_tick);
    
    while (1) {
        uint16_t v = boost_read_voltage();
        duty_tick = (uint16_t)boost_pi_calculate(&g_boost_pi, BOOST_DEBUG_TARGET_MV, v);
        set_duty_tick(duty_tick);
        
        uint8_t duty_percent = (duty_tick * 100) / 1023;
        ESP_LOGI(TAG, "V=%d mV, Err=%d mV, Tick=%d (%d%%)", 
                 v, BOOST_DEBUG_TARGET_MV - v, duty_tick, duty_percent);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif

/* ========================================================================
 * 色温渐变测试 (阶段4)
 * ======================================================================== */

#if TEST_STAGE == 4
static void cct_test_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段4: 色温渐变测试 ===");
    ESP_LOGI(TAG, "范围: %dK ~ %dK, 周期: %dms", 
             CCT_TEST_MIN_KELVIN, CCT_TEST_MAX_KELVIN, CCT_TEST_CYCLE_MS);
    
    /* 启用Boost升压 */
    boost_enable();
    ESP_LOGI(TAG, "Boost已启用，目标电压: %d mV", BOOST_DEBUG_TARGET_MV);
    
    /* 等待电压稳定 */
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    uint16_t current_kelvin = CCT_TEST_MIN_KELVIN;
    int16_t step = 10;  /* 每50ms变化10K */
    
    while (1) {
        /* 设置色温 */
        led_set_color_temp(current_kelvin);
        
        /* 读取电压和当前色温 */
        uint16_t v = boost_read_voltage();
        uint8_t warm = cct_get_warm_duty();
        uint8_t cold = cct_get_cold_duty();
        
        ESP_LOGI(TAG, "V=%d mV, CCT=%dK, W=%d%%, C=%d%%", v, current_kelvin, warm, cold);
        
        /* 渐变：暖白 → 冷白 → 暖白 */
        current_kelvin += step;
        
        /* 到达边界时反向 */
        if (current_kelvin >= CCT_TEST_MAX_KELVIN) {
            current_kelvin = CCT_TEST_MAX_KELVIN;
            step = -step;
        } else if (current_kelvin <= CCT_TEST_MIN_KELVIN) {
            current_kelvin = CCT_TEST_MIN_KELVIN;
            step = -step;
        }
        
        vTaskDelay(pdMS_TO_TICKS(CCT_TEST_STEP_MS));
    }
}
#endif

/* ========================================================================
 * 公开接口
 * ======================================================================== */

esp_err_t boost_debug_test_init(void)
{
#if TEST_STAGE == 0
    ESP_LOGI(TAG, "调试测试已禁用 (TEST_STAGE=0)");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "初始化调试测试，阶段=%d", TEST_STAGE);
    
    esp_err_t ret = power_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "电源板初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* 根据阶段创建任务 */
#if TEST_STAGE == 1
    xTaskCreate(stage1_task, "test_s1", 3072, NULL, 5, NULL);
#elif TEST_STAGE == 2
    xTaskCreate(stage2_task, "test_s2", 3072, NULL, 5, NULL);
#elif TEST_STAGE == 3
    xTaskCreate(stage3_task, "test_s3", 3072, NULL, 5, NULL);
#elif TEST_STAGE == 4
    xTaskCreate(cct_test_task, "test_cct", 3072, NULL, 5, NULL);
#endif

    return ESP_OK;
#endif
}