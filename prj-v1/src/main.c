/*
 * ESP32-S3 Edge AI Gateway
 * Main Application Entry Point
 * Phase 3: AI推理系统模块
 *
 * 功能: PIR感应 + UART控制 + WiFi数据 + 环境感知 + AI推理 + 数据记录
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Phase 1: 基础硬件通信 */
#include "pir.h"
#include "uart_driver.h"
#include "lighting.h"
#include "presence_state.h"
#include "time_period.h"
#include "rule_engine.h"
#include "fade_control.h"

/* Phase 2: WiFi数据服务 */
#include "wifi_manager.h"
#include "sntp_time_sync.h"
#include "http_client.h"
#include "weather_api.h"
#include "sunrise_sunset.h"
#include "env_dimming.h"

/* Phase 3: AI推理系统 */
#include "ai_infer.h"
#include "data_logger.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * WiFi状态回调
 * ================================================================ */
static void wifi_state_callback(wifi_state_t state)
{
	switch (state) {
	case WM_WIFI_STATE_GOT_IP:
		LOG_INF("WiFi connected, starting data services...");
		/* 记录事件 */
		data_logger_log_lighting(EVENT_WEATHER_UPDATE, 0, 0, 0);
		break;
	case WM_WIFI_STATE_DISCONNECTED:
		LOG_WRN("WiFi disconnected, using offline mode");
		break;
	default:
		break;
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
	LOG_INF("Phase 3: AI Inference System");
	LOG_INF("========================================");

	/* ============================================================
	 * Phase 1: 基础硬件初始化
	 * ============================================================ */

	ret = pir_init();
	if (ret < 0) { LOG_ERR("PIR init failed"); return ret; }

	ret = uart_driver_init();
	if (ret < 0) { LOG_ERR("UART init failed"); return ret; }

	ret = lighting_init();
	if (ret < 0) { LOG_ERR("Lighting init failed"); }

	ret = presence_state_init();
	if (ret < 0) { LOG_ERR("Presence FSM init failed"); }

	ret = rule_engine_init();
	if (ret < 0) { LOG_ERR("Rule engine init failed"); }

	ret = fade_control_init();
	if (ret < 0) { LOG_ERR("Fade control init failed"); }

	LOG_INF("Phase 1 complete!");

	/* ============================================================
	 * Phase 2: WiFi数据服务初始化
	 * ============================================================ */

	ret = wifi_manager_init();
	if (ret < 0) { LOG_ERR("WiFi manager init failed"); }
	wifi_manager_register_state_cb(wifi_state_callback);
	if (ret == 0) {
		wifi_manager_start();
	}

	ret = sntp_time_init();
	if (ret < 0) { LOG_ERR("SNTP init failed"); }
	if (ret == 0) {
		sntp_time_start();
	}

	ret = http_client_init();
	if (ret < 0) { LOG_ERR("HTTP client init failed"); }

	ret = weather_api_init();
	if (ret < 0) { LOG_ERR("Weather API init failed"); }
	if (ret == 0) {
		weather_api_start();
	}

	ret = sun_api_init();
	if (ret < 0) { LOG_ERR("Sunrise-sunset API init failed"); }
	if (ret == 0) {
		sun_api_start();
	}

	ret = env_dimming_init();
	if (ret < 0) { LOG_ERR("Environment dimming init failed"); }

	LOG_INF("Phase 2 complete!");

	/* ============================================================
	 * Phase 3: AI推理系统初始化
	 * ============================================================ */

	/* AI-021 ~ AI-022: 数据记录器初始化 */
	ret = data_logger_init();
	if (ret < 0) {
		LOG_ERR("Data logger init failed");
	}

	/* AI-008 ~ AI-010: AI推理引擎初始化 */
	ret = ai_infer_init();
	if (ret != AI_STATUS_OK) {
		LOG_WRN("AI inference init failed (err %d) - falling back to rules", ret);
		/* 继续运行, 使用规则引擎作为后备 */
	} else {
		LOG_INF("AI inference engine loaded!");

		/* AI-020: 启动AI推理线程 */
		ai_infer_thread_start();
	}

	LOG_INF("Phase 3 complete!");
	LOG_INF("System ready - entering main control loop");

	/* ================================================================
	 * 主控制循环
	 * ================================================================
	 * 周期: 每1秒执行一次
	 * 功能:
	 *   1. 更新人员状态机
	 *   2. 检查WiFi连接状态
	 *   3. AI推理或规则引擎计算
	 *   4. 渐变控制
	 *   5. 事件记录
	 */
	lighting_settings_t settings;
	lighting_settings_t prev_settings = {0, 0};
	env_dimming_result_t env_result;

	while (1) {
		/* 更新人员状态机 */
		presence_state_t presence = presence_state_update();

		/* 获取当前时段 */
		time_period_t period = time_period_get_current();

		/* 优先使用AI推理, 否则使用规则引擎 */
		if (ai_infer_is_ready()) {
			/* AI推理已在独立线程中运行, 这里只检查状态 */
			uint16_t cur_temp;
			uint8_t cur_bright;
			fade_get_current(&cur_temp, &cur_bright);

			settings.color_temp = cur_temp;
			settings.brightness = cur_bright;
		} else {
			/* 后备: 使用环境感知调光 (Phase 2) */
			ret = env_dimming_calculate(presence, period, &env_result);
			if (ret == 0) {
				settings = env_result.final_;
			} else {
				/* 最终后备: 基础规则引擎 */
				rule_calculate_settings(presence, period, &settings);
			}

			/* 执行渐变控制 */
			if (settings.color_temp != prev_settings.color_temp ||
			    settings.brightness != prev_settings.brightness) {

				LOG_INF("Rule: %s/%s → %dK @ %d%%",
					presence == PRESENCE_TIMEOUT ? "OFF" : "ON",
					time_period_name(period),
					settings.color_temp, settings.brightness);

				if (settings.brightness == 0) {
					fade_to_target(settings.color_temp, 0, 1000);
				} else {
					fade_to_target(settings.color_temp, settings.brightness, 2000);
				}

				/* 记录事件 */
				data_logger_log_lighting(
					settings.brightness == 0 ? EVENT_LIGHTS_OFF : EVENT_LIGHTS_ON,
					settings.color_temp, settings.brightness, 0);

				prev_settings = settings;
			}
		}

		/* WiFi离线处理 */
		if (!wifi_manager_is_connected()) {
			static bool offline_logged = false;
			if (!offline_logged) {
				LOG_WRN("WiFi offline - using defaults");
				offline_logged = true;
			}
		}

		/* 调试日志 */
		if (presence == PRESENCE_YES) {
			LOG_DBG("PRESENT | %s | %dK @ %d%%",
				time_period_name(period),
				settings.color_temp, settings.brightness);
		} else if (presence == PRESENCE_NO) {
			uint32_t sec = presence_get_absence_seconds();
			LOG_DBG("ABSENT %ds | %s", sec, time_period_name(period));
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
