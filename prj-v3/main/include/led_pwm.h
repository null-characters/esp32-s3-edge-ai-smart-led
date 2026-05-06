/**
 * @file led_pwm.h
 * @brief 电源板控制驱动接口
 * 
 * 控制定制电源板的Boost升压、双色温PWM调光和电压反馈检测
 * - GPIO4: Boost升压PWM控制 (50kHz)
 * - GPIO5: W-暖色温PWM控制 (3kHz)
 * - GPIO6: C-冷色温PWM控制 (3kHz)
 * - GPIO7: ADC电压反馈检测 (ADC1_CH6)
 */

#ifndef LED_PWM_H
#define LED_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * GPIO 引脚定义
 * ================================================================ */

#define POWER_BOARD_GPIO_BOOST       4    /**< Boost升压PWM控制 */
#define POWER_BOARD_GPIO_WARM       5    /**< W-暖色温PWM控制 */
#define POWER_BOARD_GPIO_COLD       6    /**< C-冷色温PWM控制 */
#define POWER_BOARD_GPIO_VFB        7    /**< ADC电压反馈检测 */

/* ================================================================
 * PWM 频率配置
 * ================================================================ */

#define BOOST_PWM_FREQUENCY_HZ      50000    /**< Boost PWM频率: 50kHz */
#define CCT_PWM_FREQUENCY_HZ        3000     /**< 色温PWM频率: 3kHz */

/* ================================================================
 * 电压参数配置
 * ================================================================ */

#define BOOST_TARGET_VOLTAGE_MV     24000    /**< Boost目标电压: 24V */
#define BOOST_INPUT_VOLTAGE_MV      20000    /**< Boost输入电压: 20V */
#define ADC_VREF_MV                 3300     /**< ADC参考电压: 3.3V */
#define ADC_RESOLUTION              4095     /**< ADC 12-bit分辨率 */

/* 分压电阻比 (R2/(R1+R2)) */
#define VOLTAGE_DIVIDER_RATIO       0.1282f  /**< 10kΩ/(68kΩ+10kΩ) ≈ 0.1282 */

/* ================================================================
 * 色温参数配置
 * ================================================================ */

#define CCT_MIN_KELVIN              2700     /**< 最小色温: 暖白光 */
#define CCT_MAX_KELVIN              6500     /**< 最大色温: 冷白光 */
#define CCT_DEFAULT_KELVIN          4000     /**< 默认色温 */

/* Boost PWM占空比范围 */
#define BOOST_MIN_DUTY_PERCENT       17     /**< 最小占空比 (%) */
#define BOOST_MAX_DUTY_PERCENT       85     /**< 最大占空比 (%) */

/* ================================================================
 * PID 控制参数配置
 * ================================================================ */

/**
 * @brief Boost PID控制器配置
 */
typedef struct {
    float kp;                   /**< 比例系数 */
    float ki;                   /**< 积分系数 */
    float kd;                   /**< 微分系数 */
    float integral_limit;       /**< 积分限幅 */
    float output_limit;         /**< 输出限幅 */
    uint32_t sample_interval_ms; /**< 采样间隔 (ms) */
} boost_pid_config_t;

/* 默认PID参数 (需根据实际硬件调试) */
#define BOOST_PID_DEFAULT_KP          0.5f
#define BOOST_PID_DEFAULT_KI          0.1f
#define BOOST_PID_DEFAULT_KD          0.05f
#define BOOST_PID_INTEGRAL_LIMIT       20.0f
#define BOOST_PID_OUTPUT_LIMIT         30.0f
#define BOOST_PID_SAMPLE_INTERVAL_MS   10

/* ================================================================
 * 数据结构
 * ================================================================ */

/**
 * @brief 电源板状态结构
 */
typedef struct {
    uint16_t boost_voltage_mv;      /**< Boost输出电压 (mV) */
    uint8_t warm_duty_percent;      /**< 暖色温占空比 (0-100%) */
    uint8_t cold_duty_percent;      /**< 冷色温占空比 (0-100%) */
    uint16_t color_temp_kelvin;     /**< 当前色温 (Kelvin) */
    uint8_t brightness_percent;    /**< 当前亮度 (0-100%) */
    uint8_t boost_duty_percent;    /**< Boost PWM占空比 (0-100%) */
    int16_t pid_error_mv;          /**< PID误差 (mV) */
    bool boost_enabled;             /**< Boost是否启用 */
    bool led_enabled;               /**< LED是否点亮 */
    bool pid_running;               /**< PID控制是否运行中 */
} power_board_state_t;

/* ================================================================
 * 初始化与释放
 * ================================================================ */

/**
 * @brief 初始化电源板控制驱动
 * 
 * 配置GPIO4-7用于电源板控制：
 * - GPIO4: Boost PWM (50kHz)
 * - GPIO5: W- PWM (3kHz)
 * - GPIO6: C- PWM (3kHz)
 * - GPIO7: ADC电压反馈
 * 
 * @return ESP_OK 成功
 */
esp_err_t power_board_init(void);

/**
 * @brief 释放电源板控制驱动
 * 
 * @return ESP_OK 成功
 */
esp_err_t power_board_deinit(void);

/* ================================================================
 * Boost升压控制
 * ================================================================ */

/**
 * @brief 启用Boost升压
 * 
 * 开始Boost PWM输出，通过ADC反馈闭环控制到目标电压
 * 
 * @return ESP_OK 成功
 */
esp_err_t boost_enable(void);

/**
 * @brief 禁用Boost升压
 * 
 * 停止Boost PWM输出
 * 
 * @return ESP_OK 成功
 */
esp_err_t boost_disable(void);

/**
 * @brief 设置Boost目标电压
 * 
 * @param voltage_mv 目标电压 (mV)，范围: 20000-26000
 * @return ESP_OK 成功
 */
esp_err_t boost_set_voltage(uint16_t voltage_mv);

/**
 * @brief 读取Boost输出电压
 * 
 * 通过ADC读取分压后的电压并计算实际输出电压
 * 
 * @return Boost输出电压 (mV)
 */
uint16_t boost_read_voltage(void);

/**
 * @brief 检查Boost电压是否在目标范围内
 * 
 * @return true 电压正常
 * @return false 电压异常
 */
bool boost_is_voltage_ok(void);

/**
 * @brief 配置Boost PID参数
 * 
 * @param config PID配置参数
 * @return ESP_OK 成功
 */
esp_err_t boost_pid_configure(const boost_pid_config_t *config);

/**
 * @brief 启动Boost PID闭环控制任务
 * 
 * 创建后台任务定期采样电压并调整PWM占空比
 * 
 * @return ESP_OK 成功
 */
esp_err_t boost_pid_start(void);

/**
 * @brief 停止Boost PID闭环控制任务
 * 
 * @return ESP_OK 成功
 */
esp_err_t boost_pid_stop(void);

/**
 * @brief 获取Boost PID控制状态
 * 
 * @param voltage 当前电压 (mV)
 * @param duty 当前占空比 (%)
 * @param error 误差 (mV)
 * @return ESP_OK 成功
 */
esp_err_t boost_pid_get_status(uint16_t *voltage, uint8_t *duty, int16_t *error);

/* ================================================================
 * 亮度控制
 * ================================================================ */

/**
 * @brief 设置亮度
 * 
 * 同时调整暖光和冷光占空比，保持当前色温比例
 * - 亮度100%: 按色温比例全输出
 * - 亮度50%: 暖光和冷光占空比各减半
 * - 亮度0%: 关闭输出
 * 
 * @param percent 亮度 (0-100%)
 * @return ESP_OK 成功
 */
esp_err_t led_set_brightness(uint8_t percent);

/**
 * @brief 渐变到目标亮度
 * 
 * @param target_percent 目标亮度 (0-100%)
 * @param duration_ms 渐变时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t led_fade_to_brightness(uint8_t target_percent, uint32_t duration_ms);

/**
 * @brief 获取当前亮度
 * 
 * @return 亮度 (0-100%)
 */
uint8_t led_get_brightness(void);

/* ================================================================
 * 色温控制
 * ================================================================ */

/**
 * @brief 设置色温
 * 
 * 根据色温值计算暖光和冷光的占空比
 * - 2700K: 暖光100%，冷光0%
 * - 6500K: 暖光0%，冷光100%
 * 
 * @param kelvin 色温值 (2700-6500K)
 * @return ESP_OK 成功
 */
esp_err_t led_set_color_temp(uint16_t kelvin);

/**
 * @brief 渐变到目标色温
 * 
 * @param target_kelvin 目标色温 (Kelvin)
 * @param duration_ms 渐变时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t cct_fade_to_color_temp(uint16_t target_kelvin, uint32_t duration_ms);

/**
 * @brief 设置暖光占空比
 * 
 * @param percent 占空比 (0-100%)
 * @return ESP_OK 成功
 */
esp_err_t cct_set_warm_duty(uint8_t percent);

/**
 * @brief 设置冷光占空比
 * 
 * @param percent 占空比 (0-100%)
 * @return ESP_OK 成功
 */
esp_err_t cct_set_cold_duty(uint8_t percent);

/**
 * @brief 关闭LED输出
 * 
 * 将暖光和冷光占空比都设为0
 * 
 * @return ESP_OK 成功
 */
esp_err_t cct_turn_off(void);

/**
 * @brief 获取当前色温
 * 
 * @return 色温值 (Kelvin)
 */
uint16_t cct_get_color_temp(void);

/**
 * @brief 获取暖光占空比
 * 
 * @return 占空比 (0-100%)
 */
uint8_t cct_get_warm_duty(void);

/**
 * @brief 获取冷光占空比
 * 
 * @return 占空比 (0-100%)
 */
uint8_t cct_get_cold_duty(void);

/* ================================================================
 * 状态查询
 * ================================================================ */

/**
 * @brief 获取电源板状态
 * 
 * @param state 状态结构体指针
 * @return ESP_OK 成功
 */
esp_err_t power_board_get_state(power_board_state_t *state);

#endif /* LED_PWM_H */
