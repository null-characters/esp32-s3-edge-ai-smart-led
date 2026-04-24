/*
 * UART Driver Implementation - 驱动板通信
 * Phase 1: HW-009 ~ HW-010
 *
 * 物理参数: 115200bps, 8N1, 无硬件流控
 * UART1: TX=GPIO17, RX=GPIO18
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "uart_driver.h"

LOG_MODULE_REGISTER(uart_drv, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * UART1 设备
 * ================================================================ */
#define SENSOR_UART_NODE DT_NODELABEL(uart1)

static const struct device *uart_dev = NULL;

/* ================================================================
 * UART设备初始化 (HW-009)
 * ================================================================ */
int uart_driver_init(void)
{
	uart_dev = DEVICE_DT_GET(SENSOR_UART_NODE);
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART1 device not ready");
		return -ENODEV;
	}
	LOG_INF("UART1 device ready: %s (115200,8N1)", uart_dev->name);

	/* UART配置已在DTS overlay中设置:
	 * - 波特率: 115200
	 * - 数据位: 8 (默认)
	 * - 停止位: 1 (默认)  
	 * - 校验: 无 (默认)
	 * - 流控: 无 (默认)
	 * 
	 * 中断驱动已在prj.conf中启用(CONFIG_UART_INTERRUPT_DRIVEN)
	 */

	return 0;
}

/* ================================================================
 * 校验和计算
 * 从CMD到DATA的累加和, 取低8位
 * ================================================================ */
uint8_t calc_checksum(const uint8_t *buf, uint8_t len)
{
	uint8_t sum = 0;
	for (uint8_t i = 0; i < len; i++) {
		sum += buf[i];
	}
	return sum;
}

/* ================================================================
 * 帧封装 (HW-010)
 * 
 * 帧格式: [AA 55] [CMD] [LEN] [DATA...] [校验和] [0D 0A]
 * ┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
 * │ 帧头2B   │ 命令1B   │ 长度1B   │ 数据NB   │ 校验和1B │ 帧尾2B   │
 * │ 0xAA55   │  cmd     │ data_len │ data[]   │ checksum │ 0x0D0A   │
 * └──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
 * 
 * 校验和 = (cmd + len + data[0..n-1]) & 0xFF
 * ================================================================ */
int frame_build(uint8_t cmd, const uint8_t *data, uint8_t data_len,
		uint8_t *out_buf)
{
	if (!out_buf) {
		return -EINVAL;
	}

	if (data_len > 0 && !data) {
		return -EINVAL;
	}

	uint8_t idx = 0;

	/* 帧头 */
	out_buf[idx++] = FRAME_HEADER_HI;  /* 0xAA */
	out_buf[idx++] = FRAME_HEADER_LO;  /* 0x55 */

	/* 命令字 */
	out_buf[idx++] = cmd;

	/* 数据长度 */
	out_buf[idx++] = data_len;

	/* 数据域 */
	if (data_len > 0 && data) {
		for (uint8_t i = 0; i < data_len; i++) {
			out_buf[idx++] = data[i];
		}
	}

	/* 校验和: 从CMD到DATA的累加和低8位 */
	uint8_t checksum = calc_checksum(&out_buf[2], 2 + data_len);
	out_buf[idx++] = checksum;

	/* 帧尾 */
	out_buf[idx++] = FRAME_FOOTER_HI;  /* 0x0D */
	out_buf[idx++] = FRAME_FOOTER_LO;  /* 0x0A */

	LOG_DBG("Frame built: cmd=0x%02X len=%d total=%d csum=0x%02X",
		cmd, data_len, idx, checksum);

	return idx;  /* 返回帧总长度 */
}

/* ================================================================
 * UART发送
 * ================================================================ */
int uart_send_frame(const uint8_t *frame, uint8_t len)
{
	if (!uart_dev || !frame || len == 0) {
		return -EINVAL;
	}

	/* 轮询发送 */
	for (uint8_t i = 0; i < len; i++) {
		uart_poll_out(uart_dev, frame[i]);
	}

	LOG_DBG("UART sent %d bytes", len);
	return 0;
}
