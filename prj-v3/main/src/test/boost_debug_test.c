/**
 * @file boost_debug_test.c
 * @brief Boost升压调试测试实现
 * 
 * 分阶段测试Boost升压电路
 */

#include "boost_debug_test.h"
#include "led_pwm.h"
#include "config_constants.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdlib.h>

static const char *TAG = "BOOST_DEBUG";

/* ================================================================
 * 私有变量
 * ================================================================ */

static struct {
    bool initialized;
    bool running;
    boost_test_stage_t stage;
    
    /* 阶段1: 固定PWM */
    uint8_t fixed_duty;
    
    /* 阶段2: 滞环控制 */
    uint16_t hysteresis_target_mv;
    uint16_t hysteresis_window_mv;
    uint8_t hysteresis_duty;
    
    /* 测试结果 */
    uint16_t voltage_min;
    uint16_t voltage_max;
    uint16_t voltage_sum;
    uint32_t sample_count;
    
    /* 任务 */
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
} g_test = {0};

/* ================================================================
 * LEDC 配置 (与 led_pwm.c 保持一致)
 * ================================================================ */

#define BOOST_LEDC_TIMER        LEDC_TIMER_0
#define BOOST_LEDC_CHANNEL      LEDC_CHANNEL_0
#define BOOST_LEDC_DUTY_RES     LEDC_TIMER_10_BIT
#define BOOST_LEDC_MODE         LEDC_LOW_SPEED_MODE

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 设置Boost PWM占空比 (直接操作LEDC)
 */
static void boost_set_pwm_duty(uint8_t duty_percent)
{
    if (duty_percent > 100) duty_percent = 100;
    
    /* 限制在安全范围内 */
    if (duty_percent < BOOST_MIN_DUTY_PERCENT) {
        duty_percent = BOOST_MIN_DUTY_PERCENT;
    }
    if (duty_percent > BOOST_MAX_DUTY_PERCENT) {
        duty_percent = BOOST_MAX_DUTY_PERCENT;
    }
    
    uint32_t duty_raw = (duty_percent * 1023) / 100;
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, duty_raw);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
    
    ESP_LOGD(TAG, "PWM duty set to %d%% (raw=%lu)", duty_percent, duty_raw);
}

/**
 * @brief 读取Boost输出电压
 */
static uint16_t boost_read_voltage_simple(void)
{
    return boost_read_voltage();
}

/**
 * @brief 更新电压统计
 */
static void update_voltage_stats(uint16_t voltage)
{
    if (voltage < g_test.voltage_min || g_test.sample_count == 0) {
        g_test.voltage_min = voltage;
    }
    if (voltage > g_test.voltage_max || g_test.sample_count == 0) {
        g_test.voltage_max = voltage;
    }
    g_test.voltage_sum += voltage;
    g_test.sample_count++;
}

/* ================================================================
 * 测试任务
 * ================================================================ */

/**
 * @brief 阶段1: 固定PWM测试任务
 */
static void test_stage1_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段1: 固定PWM测试开始 ===");
    ESP_LOGI(TAG, "固定占空比: %d%%", g_test.fixed_duty);
    ESP_LOGI(TAG, "测试持续时间: %d ms", BOOST_TEST_FIXED_DURATION_MS);
    ESP_LOGI(TAG, "请用万用表测量Boost输出电压");
    ESP_LOGI(TAG, "理论输出: 20V输入 × 占空比系数 ≈ 预期电压");
    ESP_LOGI(TAG, "");
    
    /* 启动PWM */
    boost_set_pwm_duty(g_test.fixed_duty);
    
    int64_t start_time = esp_timer_get_time() / 1000;
    uint32_t log_interval = 0;
    
    while (g_test.running) {
        uint16_t voltage = boost_read_voltage_simple();
        update_voltage_stats(voltage);
        
        /* 每500ms打印一次 */
        if (++log_interval >= 5) {
            log_interval = 0;
            ESP_LOGI(TAG, "[Stage1] Duty=%d%%, Voltage=%d mV", 
                     g_test.fixed_duty, voltage);
        }
        
        /* 检查是否超时 */
        int64_t elapsed = (esp_timer_get_time() / 1000) - start_time;
        if (elapsed >= BOOST_TEST_FIXED_DURATION_MS) {
            ESP_LOGI(TAG, "测试时间到达，自动停止");
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(BOOST_TEST_SAMPLE_INTERVAL_MS));
    }
    
    /* 停止PWM */
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, 0);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
    
    ESP_LOGI(TAG, "=== 阶段1测试结束 ===");
    boost_debug_print_summary();
    
    g_test.running = false;
    g_test.task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief 阶段2: 滞环控制测试任务
 */
static void test_stage2_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段2: 滞环控制测试开始 ===");
    ESP_LOGI(TAG, "目标电压: %d mV", g_test.hysteresis_target_mv);
    ESP_LOGI(TAG, "滞环窗口: ±%d mV", g_test.hysteresis_window_mv);
    ESP_LOGI(TAG, "调整步进: %d%%", BOOST_TEST_HYSTERESIS_STEP);
    ESP_LOGI(TAG, "");
    
    /* 从最小占空比开始 */
    g_test.hysteresis_duty = BOOST_MIN_DUTY_PERCENT;
    boost_set_pwm_duty(g_test.hysteresis_duty);
    
    uint32_t log_interval = 0;
    
    while (g_test.running) {
        uint16_t voltage = boost_read_voltage_simple();
        update_voltage_stats(voltage);
        
        /* 滞环控制逻辑 */
        int16_t error = (int16_t)g_test.hysteresis_target_mv - (int16_t)voltage;
        
        if (error > (int16_t)g_test.hysteresis_window_mv) {
            /* 电压过低，增加占空比 */
            if (g_test.hysteresis_duty < BOOST_MAX_DUTY_PERCENT) {
                g_test.hysteresis_duty += BOOST_TEST_HYSTERESIS_STEP;
                boost_set_pwm_duty(g_test.hysteresis_duty);
            }
        } else if (error < -(int16_t)g_test.hysteresis_window_mv) {
            /* 电压过高，减少占空比 */
            if (g_test.hysteresis_duty > BOOST_MIN_DUTY_PERCENT) {
                g_test.hysteresis_duty -= BOOST_TEST_HYSTERESIS_STEP;
                boost_set_pwm_duty(g_test.hysteresis_duty);
            }
        }
        
        /* 每500ms打印一次 */
        if (++log_interval >= 5) {
            log_interval = 0;
            ESP_LOGI(TAG, "[Stage2] V=%d mV, Target=%d mV, Err=%d mV, Duty=%d%%",
                     voltage, g_test.hysteresis_target_mv, error, g_test.hysteresis_duty);
        }
        
        vTaskDelay(pdMS_TO_TICKS(BOOST_TEST_SAMPLE_INTERVAL_MS));
    }
    
    /* 停止PWM */
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, 0);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
    
    ESP_LOGI(TAG, "=== 阶段2测试结束 ===");
    boost_debug_print_summary();
    
    g_test.task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief 阶段3: PID控制测试任务
 */
static void test_stage3_task(void *arg)
{
    ESP_LOGI(TAG, "=== 阶段3: PID闭环控制测试开始 ===");
    ESP_LOGI(TAG, "目标电压: %d mV", BOOST_TEST_PID_TARGET_MV);
    ESP_LOGI(TAG, "PID参数: Kp=%.2f, Ki=%.2f, Kd=%.2f",
             BOOST_PID_DEFAULT_KP, BOOST_PID_DEFAULT_KI, BOOST_PID_DEFAULT_KD);
    ESP_LOGI(TAG, "");
    
    /* 使用现有的PID控制 */
    boost_set_voltage(BOOST_TEST_PID_TARGET_MV);
    esp_err_t ret = boost_pid_start();
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动PID控制失败: %s", esp_err_to_name(ret));
        g_test.running = false;
        g_test.task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    /* 监控PID运行状态 */
    uint32_t log_interval = 0;
    
    while (g_test.running) {
        uint16_t voltage;
        uint8_t duty;
        int16_t error;
        
        boost_pid_get_status(&voltage, &duty, &error);
        update_voltage_stats(voltage);
        
        /* 每500ms打印一次 */
        if (++log_interval >= 5) {
            log_interval = 0;
            ESP_LOGI(TAG, "[Stage3-PID] V=%d mV, Err=%d mV, Duty=%d%%",
                     voltage, error, duty);
        }
        
        vTaskDelay(pdMS_TO_TICKS(BOOST_TEST_SAMPLE_INTERVAL_MS));
    }
    
    /* 停止PID控制 */
    boost_pid_stop();
    
    ESP_LOGI(TAG, "=== 阶段3测试结束 ===");
    boost_debug_print_summary();
    
    g_test.task_handle = NULL;
    vTaskDelete(NULL);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

esp_err_t boost_debug_test_init(boost_test_stage_t stage)
{
    if (g_test.initialized) {
        ESP_LOGW(TAG, "已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化Boost调试测试模块");
    
    g_test.mutex = xSemaphoreCreateMutex();
    if (!g_test.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 初始化电源板驱动 */
    esp_err_t ret = power_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化电源板驱动失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_test.mutex);
        return ret;
    }
    
    /* 设置默认参数 */
    g_test.stage = stage;
    g_test.fixed_duty = BOOST_TEST_FIXED_DUTY_DEFAULT;
    g_test.hysteresis_target_mv = BOOST_TEST_HYSTERESIS_TARGET_MV;
    g_test.hysteresis_window_mv = BOOST_TEST_HYSTERESIS_WINDOW_MV;
    g_test.hysteresis_duty = BOOST_MIN_DUTY_PERCENT;
    
    /* 重置统计 */
    g_test.voltage_min = 0;
    g_test.voltage_max = 0;
    g_test.voltage_sum = 0;
    g_test.sample_count = 0;
    
    g_test.initialized = true;
    g_test.running = false;
    
    ESP_LOGI(TAG, "初始化完成，测试阶段: %d", stage);
    return ESP_OK;
}

esp_err_t boost_debug_test_start(void)
{
    if (!g_test.initialized) {
        ESP_LOGE(TAG, "未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_test.running) {
        ESP_LOGW(TAG, "测试已在运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动Boost调试测试，阶段: %d", g_test.stage);
    
    /* 重置统计 */
    g_test.voltage_min = 0;
    g_test.voltage_max = 0;
    g_test.voltage_sum = 0;
    g_test.sample_count = 0;
    
    g_test.running = true;
    
    /* 根据阶段创建不同任务 */
    BaseType_t ret = pdFAIL;
    
    switch (g_test.stage) {
        case BOOST_TEST_STAGE_1_FIXED_PWM:
            ret = xTaskCreate(test_stage1_task, "boost_test_s1",
                              4096, NULL, 5, &g_test.task_handle);
            break;
            
        case BOOST_TEST_STAGE_2_HYSTERESIS:
            ret = xTaskCreate(test_stage2_task, "boost_test_s2",
                              4096, NULL, 5, &g_test.task_handle);
            break;
            
        case BOOST_TEST_STAGE_3_PID:
            ret = xTaskCreate(test_stage3_task, "boost_test_s3",
                              4096, NULL, 5, &g_test.task_handle);
            break;
            
        default:
            ESP_LOGE(TAG, "未知测试阶段: %d", g_test.stage);
            g_test.running = false;
            return ESP_ERR_INVALID_ARG;
    }
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建测试任务失败");
        g_test.running = false;
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t boost_debug_test_stop(void)
{
    if (!g_test.initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "停止Boost调试测试");
    
    g_test.running = false;
    
    /* 等待任务结束 */
    if (g_test.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(200));
        g_test.task_handle = NULL;
    }
    
    /* 确保PWM关闭 */
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, 0);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
    
    return ESP_OK;
}

esp_err_t boost_debug_test_get_status(bool *running, boost_test_stage_t *stage,
                                       uint16_t *voltage, uint8_t *duty)
{
    if (!running || !stage || !voltage || !duty) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_test.mutex, pdMS_TO_TICKS(DEFAULT_MUTEX_TIMEOUT_MS));
    
    *running = g_test.running;
    *stage = g_test.stage;
    *voltage = g_test.sample_count > 0 ? 
               (uint16_t)(g_test.voltage_sum / g_test.sample_count) : 0;
    *duty = g_test.hysteresis_duty;
    
    xSemaphoreGive(g_test.mutex);
    
    return ESP_OK;
}

esp_err_t boost_debug_set_fixed_duty(uint8_t duty_percent)
{
    if (duty_percent > 100) {
        duty_percent = 100;
    }
    
    xSemaphoreTake(g_test.mutex, pdMS_TO_TICKS(DEFAULT_MUTEX_TIMEOUT_MS));
    g_test.fixed_duty = duty_percent;
    xSemaphoreGive(g_test.mutex);
    
    ESP_LOGI(TAG, "设置固定占空比: %d%%", duty_percent);
    return ESP_OK;
}

esp_err_t boost_debug_set_hysteresis_target(uint16_t target_mv, uint16_t hysteresis_mv)
{
    xSemaphoreTake(g_test.mutex, pdMS_TO_TICKS(DEFAULT_MUTEX_TIMEOUT_MS));
    g_test.hysteresis_target_mv = target_mv;
    g_test.hysteresis_window_mv = hysteresis_mv;
    xSemaphoreGive(g_test.mutex);
    
    ESP_LOGI(TAG, "设置滞环目标: %d mV, 窗口: ±%d mV", target_mv, hysteresis_mv);
    return ESP_OK;
}

void boost_debug_print_summary(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "====== 测试结果摘要 ======");
    ESP_LOGI(TAG, "测试阶段: %d", g_test.stage);
    ESP_LOGI(TAG, "采样次数: %lu", g_test.sample_count);
    
    if (g_test.sample_count > 0) {
        uint16_t avg = (uint16_t)(g_test.voltage_sum / g_test.sample_count);
        ESP_LOGI(TAG, "电压最小: %d mV", g_test.voltage_min);
        ESP_LOGI(TAG, "电压最大: %d mV", g_test.voltage_max);
        ESP_LOGI(TAG, "电压平均: %d mV", avg);
        ESP_LOGI(TAG, "电压波动: %d mV (峰峰值)", 
                 g_test.voltage_max - g_test.voltage_min);
    }
    
    if (g_test.stage == BOOST_TEST_STAGE_1_FIXED_PWM) {
        ESP_LOGI(TAG, "固定占空比: %d%%", g_test.fixed_duty);
    } else if (g_test.stage == BOOST_TEST_STAGE_2_HYSTERESIS) {
        ESP_LOGI(TAG, "最终占空比: %d%%", g_test.hysteresis_duty);
        ESP_LOGI(TAG, "目标电压: %d mV", g_test.hysteresis_target_mv);
    }
    
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, "");
}
