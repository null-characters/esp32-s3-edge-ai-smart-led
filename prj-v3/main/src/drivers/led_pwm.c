/**
 * @file led_pwm.c
 * @brief 电源板控制驱动实现
 * 
 * 硬件架构:
 * - 220V AC -> 适配器(20V DC) -> LDO(5V) + Boost(24V软件PWM控制)
 * - 双色温MOSFET PWM控制 (C-冷色, W-暖色)
 * - ADC电压反馈检测Boost输出
 */

#include "led_pwm.h"
#include <stdlib.h>
#include <inttypes.h>
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "POWER_BOARD";

/* ================================================================
 * LEDC PWM 配置
 * ================================================================ */

/* Boost PWM: 50kHz, 10-bit分辨率 (0-1023) */
#define BOOST_LEDC_TIMER        LEDC_TIMER_0
#define BOOST_LEDC_CHANNEL      LEDC_CHANNEL_0
#define BOOST_LEDC_DUTY_RES     LEDC_TIMER_10_BIT
#define BOOST_LEDC_MODE         LEDC_LOW_SPEED_MODE

/* 色温PWM: 3kHz, 13-bit分辨率 (0-8191) */
#define CCT_LEDC_TIMER          LEDC_TIMER_1
#define CCT_LEDC_CHANNEL_WARM   LEDC_CHANNEL_1
#define CCT_LEDC_CHANNEL_COLD   LEDC_CHANNEL_2
#define CCT_LEDC_DUTY_RES       LEDC_TIMER_13_BIT
#define CCT_LEDC_MODE           LEDC_LOW_SPEED_MODE

/* ADC配置: GPIO7 = ADC1_CH6 */
#define VOLTAGE_ADC_UNIT        ADC_UNIT_1
#define VOLTAGE_ADC_CHANNEL     ADC_CHANNEL_6
#define VOLTAGE_ADC_ATTEN       ADC_ATTEN_DB_12

/* Boost控制参数 */
#define BOOST_VOLTAGE_TOLERANCE_MV   500

/* PID控制任务栈大小和优先级 */
#define PID_TASK_STACK_SIZE    4096
#define PID_TASK_PRIORITY      5

/* ================================================================
 * 私有变量
 * ================================================================ */

static bool g_initialized = false;
static bool g_boost_enabled = false;
static uint16_t g_target_voltage_mv = BOOST_TARGET_VOLTAGE_MV;
static uint16_t g_current_color_temp = CCT_DEFAULT_KELVIN;
static uint8_t g_brightness = 100;      /* 当前亮度 (0-100%) */
static uint8_t g_warm_duty = 50;
static uint8_t g_cold_duty = 50;
static SemaphoreHandle_t s_mutex = NULL;
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_adc_cali_handle = NULL;

/* PID控制器状态 */
static struct {
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_error;
    float integral_limit;
    float output_limit;
    uint32_t sample_interval_ms;
    uint8_t base_duty;         /* 基础占空比 */
    uint8_t current_duty;      /* 当前占空比 */
    int16_t last_error;        /* 上次误差 (mV) */
    uint16_t last_voltage;     /* 上次电压 (mV) */
    bool running;               /* PID任务是否运行 */
    TaskHandle_t task_handle;   /* PID任务句柄 */
} g_pid = {
    .kp = BOOST_PID_DEFAULT_KP,
    .ki = BOOST_PID_DEFAULT_KI,
    .kd = BOOST_PID_DEFAULT_KD,
    .integral = 0.0f,
    .prev_error = 0.0f,
    .integral_limit = BOOST_PID_INTEGRAL_LIMIT,
    .output_limit = BOOST_PID_OUTPUT_LIMIT,
    .sample_interval_ms = BOOST_PID_SAMPLE_INTERVAL_MS,
    .base_duty = BOOST_MIN_DUTY_PERCENT,
    .current_duty = BOOST_MIN_DUTY_PERCENT,
    .last_error = 0,
    .last_voltage = 0,
    .running = false,
    .task_handle = NULL,
};

/* ================================================================
 * ADC 驱动
 * ================================================================ */

static esp_err_t adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = VOLTAGE_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = VOLTAGE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(s_adc_handle, VOLTAGE_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return ret;
    }

#if CONFIG_ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = VOLTAGE_ADC_UNIT,
        .atten = VOLTAGE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &s_adc_cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration enabled");
    }
#endif

    ESP_LOGI(TAG, "ADC initialized on GPIO%d (ADC1_CH6)", POWER_BOARD_GPIO_VFB);
    return ESP_OK;
}

static void adc_deinit(void)
{
    if (s_adc_cali_handle) {
#if CONFIG_ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme(s_adc_cali_handle);
#endif
        s_adc_cali_handle = NULL;
    }
    if (s_adc_handle) {
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
    }
}

/* ================================================================
 * PWM 驱动
 * ================================================================ */

static esp_err_t pwm_init(void)
{
    ledc_timer_config_t boost_timer = {
        .speed_mode = BOOST_LEDC_MODE,
        .duty_resolution = BOOST_LEDC_DUTY_RES,
        .timer_num = BOOST_LEDC_TIMER,
        .freq_hz = BOOST_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&boost_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Boost timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t boost_channel = {
        .gpio_num = POWER_BOARD_GPIO_BOOST,
        .speed_mode = BOOST_LEDC_MODE,
        .channel = BOOST_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BOOST_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&boost_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Boost channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_timer_config_t cct_timer = {
        .speed_mode = CCT_LEDC_MODE,
        .duty_resolution = CCT_LEDC_DUTY_RES,
        .timer_num = CCT_LEDC_TIMER,
        .freq_hz = CCT_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ret = ledc_timer_config(&cct_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CCT timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t warm_channel = {
        .gpio_num = POWER_BOARD_GPIO_WARM,
        .speed_mode = CCT_LEDC_MODE,
        .channel = CCT_LEDC_CHANNEL_WARM,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = CCT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&warm_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Warm channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t cold_channel = {
        .gpio_num = POWER_BOARD_GPIO_COLD,
        .speed_mode = CCT_LEDC_MODE,
        .channel = CCT_LEDC_CHANNEL_COLD,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = CCT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&cold_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cold channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_fade_func_install(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC fade install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "PWM initialized - Boost: GPIO4 @ %dHz, CCT: GPIO5/6 @ %dHz",
             BOOST_PWM_FREQUENCY_HZ, CCT_PWM_FREQUENCY_HZ);
    return ESP_OK;
}

static void pwm_deinit(void)
{
    ledc_fade_func_uninstall();
    ledc_stop(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, 0);
    ledc_stop(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM, 0);
    ledc_stop(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD, 0);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

esp_err_t power_board_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = pwm_init();
    if (ret != ESP_OK) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    ret = adc_init();
    if (ret != ESP_OK) {
        pwm_deinit();
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    g_initialized = true;
    g_boost_enabled = false;
    g_warm_duty = 0;
    g_cold_duty = 0;
    g_current_color_temp = CCT_DEFAULT_KELVIN;

    ESP_LOGI(TAG, "Power board driver initialized");
    ESP_LOGI(TAG, "Hardware: 20V adapter -> 24V Boost, dual CCT control");
    return ESP_OK;
}

esp_err_t power_board_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    boost_disable();
    cct_turn_off();
    pwm_deinit();
    adc_deinit();

    g_initialized = false;

    xSemaphoreGive(s_mutex);
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;

    ESP_LOGI(TAG, "Power board driver deinitialized");
    return ESP_OK;
}

/* ================================================================
 * Boost 升压控制
 * ================================================================ */

esp_err_t boost_enable(void)
{
    /* 使用PID闭环控制 */
    return boost_pid_start();
}

esp_err_t boost_disable(void)
{
    /* 停止PID控制 */
    return boost_pid_stop();
}

esp_err_t boost_set_voltage(uint16_t voltage_mv)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (voltage_mv < 20000 || voltage_mv > 26000) {
        ESP_LOGE(TAG, "Voltage out of range: %d mV", voltage_mv);
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    g_target_voltage_mv = voltage_mv;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Target voltage set to %d mV", voltage_mv);
    return ESP_OK;
}

uint16_t boost_read_voltage(void)
{
    if (!g_initialized || !s_adc_handle) {
        return 0;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int adc_raw = 0;
    int voltage_mv = 0;

    int sum = 0;
    const int samples = 16;
    for (int i = 0; i < samples; i++) {
        adc_oneshot_read(s_adc_handle, VOLTAGE_ADC_CHANNEL, &adc_raw);
        sum += adc_raw;
    }
    adc_raw = sum / samples;

    if (s_adc_cali_handle) {
        adc_cali_raw_to_voltage(s_adc_cali_handle, adc_raw, &voltage_mv);
    } else {
        voltage_mv = (adc_raw * ADC_VREF_MV) / ADC_RESOLUTION;
    }

    uint16_t actual_voltage = (uint16_t)(voltage_mv / VOLTAGE_DIVIDER_RATIO);

    xSemaphoreGive(s_mutex);

    return actual_voltage;
}

bool boost_is_voltage_ok(void)
{
    uint16_t voltage = boost_read_voltage();
    int32_t diff = (int32_t)voltage - (int32_t)g_target_voltage_mv;
    return (abs(diff) <= BOOST_VOLTAGE_TOLERANCE_MV);
}

/* ================================================================
 * 色温控制
 * ================================================================ */

esp_err_t cct_set_color_temp(uint16_t kelvin)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (kelvin < CCT_MIN_KELVIN) kelvin = CCT_MIN_KELVIN;
    if (kelvin > CCT_MAX_KELVIN) kelvin = CCT_MAX_KELVIN;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 计算色温比例 */
    float ratio = (float)(kelvin - CCT_MIN_KELVIN) / (CCT_MAX_KELVIN - CCT_MIN_KELVIN);
    g_warm_duty = (uint8_t)((1.0f - ratio) * 100);
    g_cold_duty = (uint8_t)(ratio * 100);

    /* 应用亮度因子 */
    uint8_t actual_warm = (g_warm_duty * g_brightness) / 100;
    uint8_t actual_cold = (g_cold_duty * g_brightness) / 100;

    uint32_t warm_duty_raw = (actual_warm * 8191) / 100;
    uint32_t cold_duty_raw = (actual_cold * 8191) / 100;

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM, warm_duty_raw);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM);

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD, cold_duty_raw);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD);

    g_current_color_temp = kelvin;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Color temp set to %dK (warm=%d%%, cold=%d%%, brightness=%d%%)",
             kelvin, actual_warm, actual_cold, g_brightness);
    return ESP_OK;
}

/* 别名: led_set_color_temp */
esp_err_t led_set_color_temp(uint16_t kelvin)
{
    return cct_set_color_temp(kelvin);
}

esp_err_t cct_fade_to_color_temp(uint16_t target_kelvin, uint32_t duration_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (target_kelvin < CCT_MIN_KELVIN) target_kelvin = CCT_MIN_KELVIN;
    if (target_kelvin > CCT_MAX_KELVIN) target_kelvin = CCT_MAX_KELVIN;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 计算色温比例 */
    float ratio = (float)(target_kelvin - CCT_MIN_KELVIN) / (CCT_MAX_KELVIN - CCT_MIN_KELVIN);
    g_warm_duty = (uint8_t)((1.0f - ratio) * 100);
    g_cold_duty = (uint8_t)(ratio * 100);

    /* 应用亮度因子 */
    uint8_t actual_warm = (g_warm_duty * g_brightness) / 100;
    uint8_t actual_cold = (g_cold_duty * g_brightness) / 100;

    uint32_t warm_duty_raw = (actual_warm * 8191) / 100;
    uint32_t cold_duty_raw = (actual_cold * 8191) / 100;

    ledc_set_fade_time_and_start(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM,
                                  warm_duty_raw, duration_ms, LEDC_FADE_NO_WAIT);
    ledc_set_fade_time_and_start(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD,
                                  cold_duty_raw, duration_ms, LEDC_FADE_NO_WAIT);

    g_current_color_temp = target_kelvin;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Fading to %dK over %" PRIu32 " ms (brightness=%d%%)",
             target_kelvin, duration_ms, g_brightness);
    return ESP_OK;
}

esp_err_t cct_set_warm_duty(uint8_t percent)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (percent > 100) percent = 100;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    uint32_t duty_raw = (percent * 8191) / 100;
    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM, duty_raw);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM);

    g_warm_duty = percent;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Warm duty set to %d%%", percent);
    return ESP_OK;
}

esp_err_t cct_set_cold_duty(uint8_t percent)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (percent > 100) percent = 100;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    uint32_t duty_raw = (percent * 8191) / 100;
    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD, duty_raw);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD);

    g_cold_duty = percent;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Cold duty set to %d%%", percent);
    return ESP_OK;
}

esp_err_t cct_turn_off(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM, 0);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM);

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD, 0);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD);

    g_warm_duty = 0;
    g_cold_duty = 0;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "LED output turned off");
    return ESP_OK;
}

uint16_t cct_get_color_temp(void)
{
    uint16_t temp;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    temp = g_current_color_temp;
    xSemaphoreGive(s_mutex);
    return temp;
}

uint8_t cct_get_warm_duty(void)
{
    uint8_t duty;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    duty = g_warm_duty;
    xSemaphoreGive(s_mutex);
    return duty;
}

uint8_t cct_get_cold_duty(void)
{
    uint8_t duty;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    duty = g_cold_duty;
    xSemaphoreGive(s_mutex);
    return duty;
}

/* ================================================================
 * 亮度控制
 * ================================================================ */

esp_err_t led_set_brightness(uint8_t percent)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (percent > 100) percent = 100;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    g_brightness = percent;

    /* 根据当前色温和亮度计算实际占空比 */
    uint8_t actual_warm = (g_warm_duty * g_brightness) / 100;
    uint8_t actual_cold = (g_cold_duty * g_brightness) / 100;

    uint32_t warm_duty_raw = (actual_warm * 8191) / 100;
    uint32_t cold_duty_raw = (actual_cold * 8191) / 100;

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM, warm_duty_raw);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM);

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD, cold_duty_raw);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD);

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Brightness set to %d%% (actual warm=%d%%, cold=%d%%)",
             percent, actual_warm, actual_cold);
    return ESP_OK;
}

esp_err_t led_fade_to_brightness(uint8_t target_percent, uint32_t duration_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (target_percent > 100) target_percent = 100;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    g_brightness = target_percent;

    uint8_t actual_warm = (g_warm_duty * target_percent) / 100;
    uint8_t actual_cold = (g_cold_duty * target_percent) / 100;

    uint32_t warm_duty_raw = (actual_warm * 8191) / 100;
    uint32_t cold_duty_raw = (actual_cold * 8191) / 100;

    ledc_set_fade_time_and_start(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_WARM,
                                  warm_duty_raw, duration_ms, LEDC_FADE_NO_WAIT);
    ledc_set_fade_time_and_start(CCT_LEDC_MODE, CCT_LEDC_CHANNEL_COLD,
                                  cold_duty_raw, duration_ms, LEDC_FADE_NO_WAIT);

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Fading to brightness %d%% over %" PRIu32 " ms",
             target_percent, duration_ms);
    return ESP_OK;
}

uint8_t led_get_brightness(void)
{
    uint8_t brightness;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    brightness = g_brightness;
    xSemaphoreGive(s_mutex);
    return brightness;
}

/* ================================================================
 * 状态查询
 * ================================================================ */

esp_err_t power_board_get_state(power_board_state_t *state)
{
    if (!g_initialized || !state) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    state->boost_voltage_mv = g_pid.last_voltage;
    state->warm_duty_percent = g_warm_duty;
    state->cold_duty_percent = g_cold_duty;
    state->color_temp_kelvin = g_current_color_temp;
    state->brightness_percent = g_brightness;
    state->boost_duty_percent = g_pid.current_duty;
    state->pid_error_mv = g_pid.last_error;
    state->boost_enabled = g_boost_enabled;
    state->led_enabled = (g_warm_duty > 0 || g_cold_duty > 0);
    state->pid_running = g_pid.running;

    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

/* ================================================================
 * PID 闭环控制
 * ================================================================ */

/**
 * @brief PID计算函数
 * 
 * @param error 误差值 (目标电压 - 实际电压, 单位: V)
 * @return PID输出调整量 (占空比调整, 单位: %)
 */
static float pid_compute(float error)
{
    /* 积分项 */
    g_pid.integral += error;
    
    /* 积分限幅 */
    if (g_pid.integral > g_pid.integral_limit) {
        g_pid.integral = g_pid.integral_limit;
    } else if (g_pid.integral < -g_pid.integral_limit) {
        g_pid.integral = -g_pid.integral_limit;
    }
    
    /* 微分项 */
    float derivative = error - g_pid.prev_error;
    
    /* PID输出 */
    float output = g_pid.kp * error + 
                   g_pid.ki * g_pid.integral + 
                   g_pid.kd * derivative;
    
    /* 输出限幅 */
    if (output > g_pid.output_limit) {
        output = g_pid.output_limit;
    } else if (output < -g_pid.output_limit) {
        output = -g_pid.output_limit;
    }
    
    /* 保存上次误差 */
    g_pid.prev_error = error;
    
    return output;
}

/**
 * @brief 设置Boost PWM占空比
 * 
 * @param duty_percent 占空比 (0-100%)
 */
static void boost_set_duty(uint8_t duty_percent)
{
    if (duty_percent < BOOST_MIN_DUTY_PERCENT) {
        duty_percent = BOOST_MIN_DUTY_PERCENT;
    }
    if (duty_percent > BOOST_MAX_DUTY_PERCENT) {
        duty_percent = BOOST_MAX_DUTY_PERCENT;
    }
    
    uint32_t duty_raw = (duty_percent * 1023) / 100;
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, duty_raw);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
    
    g_pid.current_duty = duty_percent;
}

/**
 * @brief PID控制任务
 * 
 * 定期采样电压并调整PWM占空比
 */
static void pid_control_task(void *arg)
{
    ESP_LOGI(TAG, "PID control task started");
    
    while (g_pid.running) {
        /* 读取当前电压 */
        uint16_t voltage_mv = boost_read_voltage();
        g_pid.last_voltage = voltage_mv;
        
        /* 计算误差 (mV -> V) */
        int16_t error_mv = (int16_t)g_target_voltage_mv - (int16_t)voltage_mv;
        g_pid.last_error = error_mv;
        float error_v = (float)error_mv / 1000.0f;
        
        /* PID计算 */
        float adjustment = pid_compute(error_v);
        
        /* 计算新占空比 */
        uint8_t new_duty = g_pid.base_duty + (uint8_t)adjustment;
        
        /* 应用新占空比 */
        boost_set_duty(new_duty);
        
        /* 日志输出 (每100次打印一次) */
        static int log_counter = 0;
        if (++log_counter >= 100) {
            log_counter = 0;
            ESP_LOGI(TAG, "PID: V=%dmV, Err=%dmV, Adj=%.2f%%, Duty=%d%%",
                     voltage_mv, error_mv, adjustment, g_pid.current_duty);
        }
        
        /* 等待下一次采样 */
        vTaskDelay(pdMS_TO_TICKS(g_pid.sample_interval_ms));
    }
    
    ESP_LOGI(TAG, "PID control task stopped");
    g_pid.task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t boost_pid_configure(const boost_pid_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    
    g_pid.kp = config->kp;
    g_pid.ki = config->ki;
    g_pid.kd = config->kd;
    g_pid.integral_limit = config->integral_limit;
    g_pid.output_limit = config->output_limit;
    g_pid.sample_interval_ms = config->sample_interval_ms;
    
    /* 重置PID状态 */
    g_pid.integral = 0.0f;
    g_pid.prev_error = 0.0f;
    
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "PID configured: Kp=%.2f, Ki=%.2f, Kd=%.2f, IntLim=%.2f, OutLim=%.2f",
             config->kp, config->ki, config->kd, config->integral_limit, config->output_limit);
    return ESP_OK;
}

esp_err_t boost_pid_start(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_pid.running) {
        ESP_LOGW(TAG, "PID already running");
        return ESP_OK;
    }
    
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    
    /* 重置PID状态 */
    g_pid.integral = 0.0f;
    g_pid.prev_error = 0.0f;
    g_pid.running = true;
    g_pid.base_duty = BOOST_MIN_DUTY_PERCENT;
    g_pid.current_duty = BOOST_MIN_DUTY_PERCENT;
    
    /* 启动Boost PWM */
    boost_set_duty(g_pid.base_duty);
    g_boost_enabled = true;
    
    xSemaphoreGive(s_mutex);
    
    /* 创建PID控制任务 */
    BaseType_t ret = xTaskCreate(pid_control_task, "boost_pid",
                                  PID_TASK_STACK_SIZE, NULL,
                                  PID_TASK_PRIORITY, &g_pid.task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create PID task");
        g_pid.running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Boost PID control started, target voltage: %dmV", g_target_voltage_mv);
    return ESP_OK;
}

esp_err_t boost_pid_stop(void)
{
    if (!g_pid.running) {
        return ESP_OK;
    }
    
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    
    g_pid.running = false;
    
    /* 等待任务结束 */
    if (g_pid.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(g_pid.sample_interval_ms * 2));
    }
    
    /* 关闭Boost PWM */
    ledc_set_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL, 0);
    ledc_update_duty(BOOST_LEDC_MODE, BOOST_LEDC_CHANNEL);
    g_boost_enabled = false;
    
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Boost PID control stopped");
    return ESP_OK;
}

esp_err_t boost_pid_get_status(uint16_t *voltage, uint8_t *duty, int16_t *error)
{
    if (!voltage || !duty || !error) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    
    *voltage = g_pid.last_voltage;
    *duty = g_pid.current_duty;
    *error = g_pid.last_error;
    
    xSemaphoreGive(s_mutex);
    
    return ESP_OK;
}