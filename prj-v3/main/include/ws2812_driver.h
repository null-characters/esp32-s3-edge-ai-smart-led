/**
 * @file ws2812_driver.h
 * @brief WS2812 RGB LED 驱动接口
 * 
 * 使用 RMT 外设驱动板载 WS2812 LED (GPIO48)
 * 用于系统状态指示
 */

#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief RGB 颜色结构
 */
typedef struct {
    uint8_t r;  ///< 红色分量 (0-255)
    uint8_t g;  ///< 绿色分量 (0-255)
    uint8_t b;  ///< 蓝色分量 (0-255)
} ws2812_color_t;

/**
 * @brief WS2812 驱动配置
 */
typedef struct {
    int gpio_num;           ///< GPIO 引脚号 (默认 GPIO48)
    uint8_t led_count;      ///< LED 数量 (默认 1)
    uint8_t brightness;     ///< 全局亮度 (0-255, 默认 50)
} ws2812_config_t;

/**
 * @brief 初始化 WS2812 驱动
 * 
 * @param config 配置参数 (可为 NULL 使用默认值)
 * @return ESP_OK 成功
 */
esp_err_t ws2812_init(const ws2812_config_t *config);

/**
 * @brief 释放 WS2812 驱动
 */
void ws2812_deinit(void);

/**
 * @brief 设置单个 LED 颜色
 * 
 * @param index LED 索引 (0 开始)
 * @param color RGB 颜色
 * @return ESP_OK 成功
 */
esp_err_t ws2812_set_pixel(uint8_t index, ws2812_color_t color);

/**
 * @brief 设置所有 LED 颜色
 * 
 * @param color RGB 颜色
 * @return ESP_OK 成功
 */
esp_err_t ws2812_set_all(ws2812_color_t color);

/**
 * @brief 清除所有 LED (熄灭)
 * 
 * @return ESP_OK 成功
 */
esp_err_t ws2812_clear(void);

/**
 * @brief 刷新显示 (将缓冲区数据发送到 LED)
 * 
 * @return ESP_OK 成功
 */
esp_err_t ws2812_show(void);

/**
 * @brief 设置全局亮度
 * 
 * @param brightness 亮度值 (0-255)
 */
void ws2812_set_brightness(uint8_t brightness);

/**
 * @brief 获取当前全局亮度
 * 
 * @return 亮度值 (0-255)
 */
uint8_t ws2812_get_brightness(void);

/* ================================================================
 * 预定义颜色
 * ================================================================ */

#define WS2812_COLOR_BLACK      ((ws2812_color_t){0, 0, 0})
#define WS2812_COLOR_WHITE      ((ws2812_color_t){255, 255, 255})
#define WS2812_COLOR_RED        ((ws2812_color_t){255, 0, 0})
#define WS2812_COLOR_GREEN      ((ws2812_color_t){0, 255, 0})
#define WS2812_COLOR_BLUE       ((ws2812_color_t){0, 0, 255})
#define WS2812_COLOR_YELLOW     ((ws2812_color_t){255, 255, 0})
#define WS2812_COLOR_CYAN       ((ws2812_color_t){0, 255, 255})
#define WS2812_COLOR_MAGENTA    ((ws2812_color_t){255, 0, 255})
#define WS2812_COLOR_ORANGE     ((ws2812_color_t){255, 165, 0})
#define WS2812_COLOR_PURPLE     ((ws2812_color_t){128, 0, 128})

#endif /* WS2812_DRIVER_H */
