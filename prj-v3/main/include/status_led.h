/**
 * @file status_led.h
 * @brief 状态指示灯管理模块
 * 
 * 管理板载 WS2812 RGB LED 的状态显示，包括：
 * - WiFi 连接状态
 * - 系统运行模式
 * - 语音交互状态
 * - 错误/警告提示
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "wifi_sta.h"
#include "priority_arbiter.h"

/* ================================================================
 * 状态类型定义
 * ================================================================ */

/**
 * @brief 指示灯状态枚举
 */
typedef enum {
    /* WiFi 状态 (0-9) */
    STATUS_LED_WIFI_DISCONNECTED = 0,   ///< WiFi 未连接 (红色闪烁)
    STATUS_LED_WIFI_CONNECTING,          ///< WiFi 连接中 (黄色快闪)
    STATUS_LED_WIFI_CONNECTED,          ///< WiFi 已连接 (绿色常亮)
    STATUS_LED_WIFI_FAILED,             ///< WiFi 连接失败 (红色常亮)
    
    /* 系统模式状态 (10-19) */
    STATUS_LED_MODE_AUTO = 10,          ///< 自动模式 (蓝色常亮)
    STATUS_LED_MODE_MANUAL,              ///< 手动模式 (青色常亮)
    STATUS_LED_MODE_VOICE,               ///< 语音模式 (紫色常亮)
    STATUS_LED_MODE_SLEEP,               ///< 睡眠模式 (熄灭)
    
    /* 语音交互状态 (20-29) */
    STATUS_LED_VOICE_IDLE = 20,         ///< 语音待机 (跟随模式)
    STATUS_LED_VOICE_LISTENING,          ///< 正在监听 (白色呼吸)
    STATUS_LED_VOICE_PROCESSING,         ///< 正在处理 (白色快闪)
    STATUS_LED_VOICE_SUCCESS,            ///< 命令成功 (绿色快闪)
    STATUS_LED_VOICE_ERROR,              ///< 命令错误 (红色快闪)
    
    /* 特殊状态 (30+) */
    STATUS_LED_BOOTING = 30,            ///< 系统启动中 (彩虹渐变)
    STATUS_LED_ERROR,                    ///< 系统错误 (红色闪烁)
    STATUS_LED_UPDATING,                  ///< 固件更新中 (黄色呼吸)
    
} status_led_state_enum_t;

/**
 * @brief 状态指示灯配置
 */
typedef struct {
    uint8_t brightness;          ///< 默认亮度 (0-255, 默认 50)
    uint16_t blink_interval_ms;  ///< 闪烁间隔 (默认 500ms)
    uint16_t breath_period_ms;   ///< 呼吸周期 (默认 2000ms)
    bool enable_auto_update;     ///< 自动更新状态 (默认 true)
} status_led_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化状态指示灯模块
 * 
 * @param config 配置参数 (可为 NULL 使用默认值)
 * @return ESP_OK 成功
 */
esp_err_t status_led_init(const status_led_config_t *config);

/**
 * @brief 释放状态指示灯模块
 */
void status_led_deinit(void);

/**
 * @brief 设置当前状态
 * 
 * @param state 状态值
 * @return ESP_OK 成功
 */
esp_err_t status_led_set_state(status_led_state_enum_t state);

/**
 * @brief 获取当前状态
 * 
 * @return 当前状态
 */
status_led_state_enum_t status_led_get_state(void);

/**
 * @brief 根据 WiFi 状态更新指示灯
 * 
 * @param wifi_state WiFi 连接状态
 */
void status_led_update_wifi(wifi_sta_state_t wifi_state);

/**
 * @brief 根据系统模式更新指示灯
 * 
 * @param mode 系统运行模式
 */
void status_led_update_mode(system_mode_t mode);

/**
 * @brief 设置语音交互状态
 * 
 * @param listening 是否正在监听
 * @param processing 是否正在处理
 */
void status_led_set_voice(bool listening, bool processing);

/**
 * @brief 显示命令执行结果
 * 
 * @param success 是否成功
 * @param duration_ms 显示时长 (毫秒)
 */
void status_led_show_result(bool success, uint32_t duration_ms);

/**
 * @brief 设置亮度
 * 
 * @param brightness 亮度值 (0-255)
 */
void status_led_set_brightness(uint8_t brightness);

/**
 * @brief 开始后台任务 (状态更新循环)
 */
void status_led_start(void);

/**
 * @brief 停止后台任务
 */
void status_led_stop(void);

/**
 * @brief 强制刷新显示
 */
void status_led_refresh(void);

/* ================================================================
 * 辅助宏
 * ================================================================ */

/**
 * @brief 快速设置语音唤醒状态
 */
#define STATUS_LED_VOICE_WAKE()     status_led_set_voice(true, false)

/**
 * @brief 快速设置语音处理状态
 */
#define STATUS_LED_VOICE_PROCESS()  status_led_set_voice(false, true)

/**
 * @brief 快速设置语音待机状态
 */
#define STATUS_LED_VOICE_IDLE()     status_led_set_voice(false, false)

/**
 * @brief 快速显示成功结果
 */
#define STATUS_LED_SUCCESS(ms)      status_led_show_result(true, ms)

/**
 * @brief 快速显示失败结果
 */
#define STATUS_LED_ERROR(ms)        status_led_show_result(false, ms)

#endif /* STATUS_LED_H */
