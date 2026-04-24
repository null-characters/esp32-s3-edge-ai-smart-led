/*
 * ESP32-S3 Edge AI Gateway
 * Main Application Entry Point
 * Phase 1: 基础硬件通信模块 (HW-001 ~ HW-010)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "pir.h"
#include "uart_driver.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* 测试辅助: 构建一条CMD_SET_COLOR_TEMP(0x01)命令 */
static void test_frame_build(void)
{
	uint8_t buf[FRAME_MAX_TOTAL];
	uint8_t data[] = {0x0A, 0x74}; /* 色温2676K (小端) */

	int len = frame_build(0x01, data, sizeof(data), buf);
	if (len < 0) {
		LOG_ERR("frame_build failed: %d", len);
		return;
	}

	LOG_HEXDUMP_INF(buf, len, "Test frame CMD_SET_COLOR_TEMP");

	/* 验证校验和:
	 * CMD=0x01, LEN=0x02, DATA[]={0x0A,0x74}
	 * sum = 0x01 + 0x02 + 0x0A + 0x74 = 0x81
	 */
	uint8_t sum = 0x01 + 0x02 + 0x0A + 0x74;
	LOG_INF("Expected checksum: 0x%02X", sum & 0xFF);
}

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

	/* HW-009: UART驱动初始化 */
	ret = uart_driver_init();
	if (ret < 0) {
		LOG_ERR("UART init failed (err %d) - halt", ret);
		return ret;
	}

	/* HW-010: 帧封装测试 */
	LOG_INF("Running frame build test...");
	test_frame_build();

	LOG_INF("Phase 1 initialization complete!");
	LOG_INF("Entering main loop - PIR polling @ 1Hz");

	/* 主循环: 每秒轮询一次PIR状态 */
	while (1) {
		bool presence = pir_get_status();
		LOG_INF("PIR: %s | raw=%d",
			presence ? "PRESENCE" : "NO_PRESENCE",
			pir_get_raw());

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
