/**
 * @file boost_debug_test.c
 * @brief Boost升压调试测试 - 简化实现
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

static const char *TAG = "BOOST_DBG";

/* LEDC配置 (与led_pwm.c一致) */
#define BOOST_LEDC_TIMER    LEDC_TIMER_0
#define BOOST_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BOOST_LEDC_MODE     LEDC_LOW_SPEED_MODE

/* ========================================================================
 * 内部函数
 * ======================================================================== */

#if BOOST_DEBUG_STAGE > 0
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
static boost_pi_context_t g_boost_pi;

static void stage3_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段3: PI控制 (PWM tick精度) ===");
    ESP_LOGI(TAG, "目标: %d mV", BOOST_DEBUG_TARGET_MV);
    
    /* PWM tick 精度控制
     * OutMin=30 (≈3%), OutMax=870 (≈85%)
     * Kp=10: 每1000mV误差调整10 tick (≈1%)
     * Ki=50: 积分作用
     */
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

#if BOOST_DEBUG_STAGE == 4
/* 按键开关状态 */
static bool g_boost_enabled = false;
static bool g_btn_pressed = false;
static uint32_t g_btn_press_time = 0;

static void stage4_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段4: 按键开关模式 ===");
    ESP_LOGI(TAG, "短按Boot按键(GPIO%d)切换输出状态", BOOST_DEBUG_BTN_GPIO);
    ESP_LOGI(TAG, "初始状态: 输出关闭");
    
    /* 初始化Boot按键GPIO */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << BOOST_DEBUG_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);
    
    /* 初始状态：关闭输出 */
    set_duty(0);
    
    while (1) {
        /* 检测按键 (按下时GPIO为低电平) */
        int btn_level = gpio_get_level(BOOST_DEBUG_BTN_GPIO);
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (btn_level == 0 && !g_btn_pressed) {
            /* 按键按下 */
            g_btn_pressed = true;
            g_btn_press_time = now;
            ESP_LOGI(TAG, "按键按下...");
        } else if (btn_level == 1 && g_btn_pressed) {
            /* 按键释放 */
            g_btn_pressed = false;
            uint32_t press_duration = now - g_btn_press_time;
            
            /* 短按 (<1秒): 切换开关状态 */
            if (press_duration < 1000) {
                g_boost_enabled = !g_boost_enabled;
                
                if (g_boost_enabled) {
                    ESP_LOGI(TAG, ">>> 输出开启 (滞环控制)");
                    ESP_LOGI(TAG, "目标: %d mV ± %d mV", BOOST_DEBUG_TARGET_MV, BOOST_DEBUG_WINDOW_MV);
                } else {
                    ESP_LOGI(TAG, ">>> 输出关闭");
                    set_duty(0);
                }
            }
        }
        
        /* 如果开启，执行滞环控制 */
        if (g_boost_enabled) {
            uint16_t v = boost_read_voltage();
            int16_t err = BOOST_DEBUG_TARGET_MV - v;
            uint8_t duty;
            
            /* 获取当前占空比 */
            duty = (ledc_get_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL) * 100) / 1023;
            
            if (err > BOOST_DEBUG_WINDOW_MV && duty < BOOST_MAX_DUTY_PERCENT) {
                duty++;
                set_duty(duty);
            } else if (err < -BOOST_DEBUG_WINDOW_MV && duty > BOOST_MIN_DUTY_PERCENT) {
                duty--;
                set_duty(duty);
            }
            
            ESP_LOGI(TAG, "V=%d mV, Err=%d mV, Duty=%d%%, 状态=%s", 
                     v, err, duty, g_boost_enabled ? "ON" : "OFF");
        }
        
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
#elif BOOST_DEBUG_STAGE == 4
    xTaskCreate(stage4_task, "boost_s4", 3072, NULL, 5, NULL);
#endif

    return ESP_OK;
#endif
}
