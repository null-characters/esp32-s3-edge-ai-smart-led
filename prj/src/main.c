/*
 * ESP32-S3 Edge AI Gateway
 * Main Application Entry Point
 * Phase 1: 基础硬件通信模块 (HW-001 ~ HW-023)
 *
 * 功能: PIR人体感应 + UART驱动板控制 + 规则调光 + 渐变控制
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "pir.h"
#include "uart_driver.h"
#include "lighting.h"
#include "presence_state.h"
#include "time_period.h"
#include "rule_engine.h"
#include "fade_control.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 测试辅助: 验证帧解析功能 (HW-011)
 * ================================================================ */
static void test_frame_parse(void)
{
	/* 构建一个测试帧, 然后解析它 */
	uint8_t buf[FRAME_MAX_TOTAL];
	uint8_t test_data[] = {0x0A, 0x74}; /* 色温2676K */
	
	int len = frame_build(0x01, test_data, sizeof(test_data), buf);
	if (len < 0) {
		LOG_ERR("Test frame build failed: %d", len);
		return;
	}

	/* 解析这个帧 */
	uint8_t parsed_cmd, parsed_data[256], parsed_len;
	int ret = frame_parse(buf, len, &parsed_cmd, parsed_data, &parsed_len);
	
	if (ret == FRAME_PARSE_OK) {
		LOG_INF("Frame parse test: PASS (cmd=0x%02X, len=%d)",
			parsed_cmd, parsed_len);
	} else {
		LOG_ERR("Frame parse test: FAIL (err=%d)", ret);
	}
}

/* ================================================================
 * 初始化流程
 * ================================================================ */
int main(void)
{
	int ret;

	LOG_INF("========================================");
	LOG_INF("ESP32-S3 Edge AI Gateway Starting...");
	LOG_INF("Phase 1: Basic Hardware Communication");
	LOG_INF("========================================");

	/* HW-005 ~ HW-008: PIR传感器初始化 */
	ret = pir_init();
	if (ret < 0) {
		LOG_ERR("PIR init failed (err %d) - halt", ret);
		return ret;
	}

	/* HW-009 ~ HW-010: UART驱动初始化 */
	ret = uart_driver_init();
	if (ret < 0) {
		LOG_ERR("UART init failed (err %d) - halt", ret);
		return ret;
	}

	/* HW-013: 照明控制初始化 */
	ret = lighting_init();
	if (ret < 0) {
		LOG_ERR("Lighting init failed (err %d)", ret);
	}

	/* HW-017: 人员状态机初始化 */
	ret = presence_state_init();
	if (ret < 0) {
		LOG_ERR("Presence FSM init failed (err %d)", ret);
	}

	/* HW-019: 规则引擎初始化 */
	ret = rule_engine_init();
	if (ret < 0) {
		LOG_ERR("Rule engine init failed (err %d)", ret);
	}

	/* HW-020: 渐变控制初始化 */
	ret = fade_control_init();
	if (ret < 0) {
		LOG_ERR("Fade control init failed (err %d)", ret);
	}

	/* HW-011: 帧解析测试 */
	LOG_INF("Running frame parse test...");
	test_frame_parse();

	LOG_INF("Phase 1 initialization complete!");
	LOG_INF("Entering main control loop...");

	/* ================================================================
	 * 主控制循环 (HW-021)
	 * ================================================================
	 * 周期: 每1秒执行一次
	 * 功能:
	 *   1. 更新人员状态机 (检测是否超时无人)
	 *   2. 根据状态+时段计算照明参数
	 *   3. 渐变到目标设置
	 */
	lighting_settings_t settings;
	lighting_settings_t prev_settings = {0, 0};
	
	while (1) {
		/* 更新人员状态机 (HW-017) */
		presence_state_t presence = presence_state_update();
		
		/* 获取当前时段 (HW-018) */
		time_period_t period = time_period_get_current();
		
		/* 计算照明参数 (HW-019) */
		rule_calculate_settings(presence, period, &settings);
		
		/* 如果设置变化,执行渐变 (HW-020) */
		if (settings.color_temp != prev_settings.color_temp ||
		    settings.brightness != prev_settings.brightness) {
			
			LOG_INF("Settings change: %s/%s → %dK @ %d%%",
				presence == PRESENCE_TIMEOUT ? "OFF" : "ON",
				time_period_name(period),
				settings.color_temp,
				settings.brightness);
			
			if (settings.brightness == 0) {
				/* 关灯: 渐变到关闭 */
				fade_to_target(settings.color_temp, 0, 1000);
			} else {
				/* 开灯: 渐变到目标 */
				fade_to_target(settings.color_temp, settings.brightness, 2000);
			}
			
			prev_settings = settings;
		}

		/* 调试日志 */
		if (presence == PRESENCE_YES) {
			LOG_DBG("State: PRESENT | Period: %s | %dK @ %d%%",
				time_period_name(period),
				settings.color_temp, settings.brightness);
		} else if (presence == PRESENCE_NO) {
			uint32_t sec = presence_get_absence_seconds();
			LOG_DBG("State: ABSENT %ds | Period: %s | %dK @ %d%%",
				sec, time_period_name(period),
				settings.color_temp, settings.brightness);
		} else {
			LOG_DBG("State: TIMEOUT (lights off)");
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
