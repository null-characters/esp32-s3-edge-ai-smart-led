/**
 * @file boost_debug_test.c
 * @brief Boost升压调试测试 - 简化实现
 */

#include "boost_debug_test.h"
#include "led_pwm.h"
#include "config_constants.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BOOST_DBG";

/* LEDC配置 (与led_pwm.c一致) */
#define BOOST_LEDC_TIMER    LEDC_TIMER_0
#define BOOST_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BOOST_LEDC_MODE     LEDC_LOW_SPEED_MODE

/* ========================================================================
 * 内部函数
 * ======================================================================== */

#if BOOST_DEBUG_STAGE > 0
static void set_duty(uint8_t percent)
{
    if (percent > 100) percent = 100;
    if (percent < BOOST_MIN_DUTY_PERCENT) percent = BOOST_MIN_DUTY_PERCENT;
    if (percent > BOOST_MAX_DUTY_PERCENT) percent = BOOST_MAX_DUTY_PERCENT;
    
    uint32_t raw = (percent * 1023) / 100;
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, raw);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
}
#endif

/* ========================================================================
 * 测试任务
 * ======================================================================== */

#if BOOST_DEBUG_STAGE == 1
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

#if BOOST_DEBUG_STAGE == 2
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

#if BOOST_DEBUG_STAGE == 3
static void stage3_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段3: PID控制 ===");
    ESP_LOGI(TAG, "目标: %d mV", BOOST_DEBUG_TARGET_MV);
    
    boost_set_voltage(BOOST_DEBUG_TARGET_MV);
    boost_pid_start();
    
    while (1) {
        uint16_t v;
        uint8_t d;
        int16_t e;
        boost_pid_get_status(&v, &d, &e);
        ESP_LOGI(TAG, "V=%d mV, Err=%d mV, Duty=%d%%", v, e, d);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif

/* ========================================================================
 * 公开接口
 * ======================================================================== */

esp_err_t boost_debug_test_init(void)
{
#if BOOST_DEBUG_STAGE == 0
    ESP_LOGI(TAG, "Boost调试测试已禁用 (BOOST_DEBUG_STAGE=0)");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "初始化Boost调试测试，阶段=%d", BOOST_DEBUG_STAGE);
    
    esp_err_t ret = power_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "电源板初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* 根据阶段创建任务 */
#if BOOST_DEBUG_STAGE == 1
    xTaskCreate(stage1_task, "boost_s1", 3072, NULL, 5, NULL);
#elif BOOST_DEBUG_STAGE == 2
    xTaskCreate(stage2_task, "boost_s2", 3072, NULL, 5, NULL);
#elif BOOST_DEBUG_STAGE == 3
    xTaskCreate(stage3_task, "boost_s3", 3072, NULL, 5, NULL);
#endif

    return ESP_OK;
#endif
}
