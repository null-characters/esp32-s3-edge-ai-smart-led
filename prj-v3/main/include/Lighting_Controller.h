/**
 * @file Lighting_Controller.h
 * @brief 统一照明控制器
 * 
 * 消费统一事件队列，执行照明控制
 * 集成 TTL 租约、仲裁器、LED 驱动
 */

#ifndef LIGHTING_CONTROLLER_H
#define LIGHTING_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "Lighting_Event.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 控制器状态回调
 * @param event 处理的事件
 * @param user_data 用户数据
 */
typedef void (*controller_event_callback_t)(const Lighting_Event_t *event, void *user_data);

/**
 * @brief 控制器配置
 */
typedef struct {
    uint32_t event_queue_size;              /* 事件队列大小 */
    controller_event_callback_t callback;   /* 事件回调 */
    void *user_data;                        /* 用户数据 */
} controller_config_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化照明控制器
 * @param config 配置参数 (NULL 使用默认配置)
 * @return ESP_OK 成功
 */
esp_err_t lighting_controller_init(const controller_config_t *config);

/**
 * @brief 释放照明控制器
 */
void lighting_controller_deinit(void);

/**
 * @brief 启动控制器任务
 * @return ESP_OK 成功
 */
esp_err_t lighting_controller_start(void);

/**
 * @brief 停止控制器任务
 * @return ESP_OK 成功
 */
esp_err_t lighting_controller_stop(void);

/**
 * @brief 提交事件到控制器
 * @param event 事件
 * @return ESP_OK 成功
 */
esp_err_t lighting_controller_submit_event(const Lighting_Event_t *event);

/**
 * @brief 提交语音命令事件 (便捷函数)
 * @param type 事件类型
 * @param scene_id 场景 ID (场景事件)
 * @param brightness 亮度 (亮度事件)
 * @param color_temp 色温 (色温事件)
 * @param ttl_ms TTL 租约时长
 * @return ESP_OK 成功
 */
esp_err_t lighting_controller_submit_voice_event(event_type_t type, 
                                                  scene_id_t scene_id,
                                                  uint8_t brightness,
                                                  uint16_t color_temp,
                                                  uint32_t ttl_ms);

/**
 * @brief 提交自动控制事件 (便捷函数)
 * @param scene_id 场景 ID
 * @param confidence 置信度
 * @return ESP_OK 成功
 */
esp_err_t lighting_controller_submit_auto_event(scene_id_t scene_id, float confidence);

/**
 * @brief 获取当前照明状态
 * @param brightness 亮度
 * @param color_temp 色温
 * @param power 电源状态
 */
void lighting_controller_get_state(uint8_t *brightness, uint16_t *color_temp, bool *power);

/**
 * @brief 获取当前模式
 * @return true 自动模式, false 手动模式
 */
bool lighting_controller_is_auto_mode(void);

/**
 * @brief 设置事件回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void lighting_controller_set_callback(controller_event_callback_t callback, void *user_data);

#endif /* LIGHTING_CONTROLLER_H */