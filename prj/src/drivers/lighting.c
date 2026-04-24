/*
 * Lighting Command Implementation
 * Phase 1: HW-013
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "lighting.h"
#include "uart_driver.h"

LOG_MODULE_REGISTER(lighting, CONFIG_LOG_DEFAULT_LEVEL);

static uint8_t tx_buf[FRAME_MAX_TOTAL];

/* ================================================================
 * 模块初始化
 * ================================================================ */
int lighting_init(void)
{
	LOG_INF("Lighting module initialized");
	return 0;
}

/* ================================================================
 * 设置色温
 * ================================================================ */
int lighting_set_color_temp(uint16_t color_temp)
{
	if (color_temp < COLOR_TEMP_MIN || color_temp > COLOR_TEMP_MAX) {
		LOG_ERR("Invalid color temp: %d (range %d-%d)",
			color_temp, COLOR_TEMP_MIN, COLOR_TEMP_MAX);
		return -EINVAL;
	}

	/* 小端字节序: 低字节在前 */
	uint8_t data[2] = {
		color_temp & 0xFF,
		(color_temp >> 8) & 0xFF
	};

	int ret = uart_send_with_retry(CMD_SET_COLOR_TEMP, data, sizeof(data));
	if (ret < 0) {
		LOG_ERR("UART send_with_retry failed: %d", ret);
		return ret;
	}

	LOG_INF("Set color temp: %dK", color_temp);
	return 0;
}

/* ================================================================
 * 设置光强
 * ================================================================ */
int lighting_set_brightness(uint8_t brightness)
{
	if (brightness > BRIGHTNESS_MAX) {
		LOG_ERR("Invalid brightness: %d (max %d)",
			brightness, BRIGHTNESS_MAX);
		return -EINVAL;
	}

	uint8_t data[1] = { brightness };

	int ret = uart_send_with_retry(CMD_SET_BRIGHTNESS, data, sizeof(data));
	if (ret < 0) {
		LOG_ERR("UART send_with_retry failed: %d", ret);
		return ret;
	}

	LOG_INF("Set brightness: %d%%", brightness);
	return 0;
}

/* ================================================================
 * 同时设置色温和光强
 * ================================================================ */
int lighting_set_both(uint16_t color_temp, uint8_t brightness)
{
	if (color_temp < COLOR_TEMP_MIN || color_temp > COLOR_TEMP_MAX) {
		LOG_ERR("Invalid color temp: %d", color_temp);
		return -EINVAL;
	}
	if (brightness > BRIGHTNESS_MAX) {
		LOG_ERR("Invalid brightness: %d", brightness);
		return -EINVAL;
	}

	/* 数据: [色温低8, 色温高8, 光强] */
	uint8_t data[3] = {
		color_temp & 0xFF,
		(color_temp >> 8) & 0xFF,
		brightness
	};

	int ret = uart_send_with_retry(CMD_SET_BOTH, data, sizeof(data));
	if (ret < 0) {
		LOG_ERR("UART send_with_retry failed: %d", ret);
		return ret;
	}

	LOG_INF("Set both: %dK @ %d%%", color_temp, brightness);
	return 0;
}

/* ================================================================
 * 查询状态
 * ================================================================ */
int lighting_get_status(void)
{
	int ret = uart_send_with_retry(CMD_GET_STATUS, NULL, 0);
	if (ret < 0) {
		LOG_ERR("UART send_with_retry failed: %d", ret);
		return ret;
	}

	LOG_INF("Get status request sent");
	return 0;
}
