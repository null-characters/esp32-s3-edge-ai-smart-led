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
#include <string.h>

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

/* ================================================================
 * 帧解析 (HW-011)
 * ================================================================ */
int frame_parse(const uint8_t *buf, uint8_t buf_len,
		uint8_t *cmd, uint8_t *data, uint8_t *data_len)
{
	if (!buf || !cmd || !data || !data_len) {
		return -EINVAL;
	}

	/* 检查最小长度 */
	if (buf_len < FRAME_OVERHEAD) {
		LOG_WRN("Frame too short: %d bytes (min %d)", buf_len, FRAME_OVERHEAD);
		return FRAME_PARSE_ERR_LENGTH;
	}

	/* 验证帧头 0xAA55 */
	if (buf[0] != FRAME_HEADER_HI || buf[1] != FRAME_HEADER_LO) {
		LOG_WRN("Invalid frame header: 0x%02X%02X (expected 0xAA55)",
			buf[0], buf[1]);
		return FRAME_PARSE_ERR_HEADER;
	}

	/* 提取命令字和数据长度 */
	uint8_t received_cmd = buf[2];
	uint8_t received_len = buf[3];

	/* 验证总长度 */
	if (FRAME_OVERHEAD + received_len != buf_len) {
		LOG_WRN("Length mismatch: total=%d, claimed len=%d",
			buf_len, received_len);
		return FRAME_PARSE_ERR_LENGTH;
	}

	/* 验证帧尾 0x0D0A */
	if (buf[buf_len - 2] != FRAME_FOOTER_HI ||
	    buf[buf_len - 1] != FRAME_FOOTER_LO) {
		LOG_WRN("Invalid frame footer: 0x%02X%02X (expected 0x0D0A)",
			buf[buf_len - 2], buf[buf_len - 1]);
		return FRAME_PARSE_ERR_FOOTER;
	}

	/* 计算并验证校验和 */
	uint8_t calc_sum = calc_checksum(&buf[FRAME_HEADER_LEN],
					 1 + 1 + received_len);
	uint8_t recv_sum = buf[FRAME_HEADER_LEN + 1 + 1 + received_len];

	if (calc_sum != recv_sum) {
		LOG_WRN("Checksum error: calc=0x%02X, recv=0x%02X",
			calc_sum, recv_sum);
		return FRAME_PARSE_ERR_CSUM;
	}

	/* 提取数据 */
	*cmd = received_cmd;
	*data_len = received_len;
	for (uint8_t i = 0; i < received_len; i++) {
		data[i] = buf[FRAME_HEADER_LEN + 1 + 1 + i];
	}

	LOG_DBG("Frame parsed: cmd=0x%02X len=%d csum=0x%02X",
		received_cmd, received_len, recv_sum);
	return FRAME_PARSE_OK;
}

/* ================================================================
 * 接收响应 (HW-014)
 * ================================================================ */
int uart_receive_response(uint8_t *cmd, uint8_t *data, uint8_t *data_len)
{
	static uint8_t rx_buf[FRAME_MAX_TOTAL];
	uint8_t idx = 0;
	int64_t timeout = k_uptime_get() + UART_RECV_TIMEOUT_MS;

	/* 清除缓冲区 */
	memset(rx_buf, 0, sizeof(rx_buf));

	/* 轮询接收直到超时或收到完整帧 */
	while (k_uptime_get() < timeout) {
		uint8_t byte;
		if (uart_poll_in(uart_dev, &byte) == 0) {
			if (idx < sizeof(rx_buf)) {
				rx_buf[idx++] = byte;

				/* 尝试解析 (可能帧还不完整,会返回错误) */
				if (idx >= FRAME_OVERHEAD) {
					int ret = frame_parse(rx_buf, idx, cmd, data, data_len);
					if (ret == FRAME_PARSE_OK) {
						/* 成功接收并解析 */
						LOG_DBG("Response received: cmd=0x%02X len=%d",
							*cmd, *data_len);
						return 0;
					}
				}
			}
		}
	}

	LOG_WRN("Response timeout after %d ms", UART_RECV_TIMEOUT_MS);
	return -ETIMEDOUT;
}

/* ================================================================
 * 发送命令并等待响应 (HW-015)
 * ================================================================ */
int uart_send_with_retry(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
	static uint8_t tx_buf[FRAME_MAX_TOTAL];
	static uint8_t rx_data[256];
	uint8_t rx_cmd, rx_len;
	int ret;

	/* 构建发送帧 */
	int frame_len = frame_build(cmd, data, data_len, tx_buf);
	if (frame_len < 0) {
		LOG_ERR("Frame build failed: %d", frame_len);
		return frame_len;
	}

	/* 重试循环 */
	for (int retry = 0; retry <= UART_MAX_RETRIES; retry++) {
		if (retry > 0) {
			LOG_WRN("Retry %d/%d for cmd=0x%02X", retry, UART_MAX_RETRIES, cmd);
			k_sleep(K_MSEC(UART_RETRY_DELAY_MS));
		}

		/* 发送 */
		ret = uart_send_frame(tx_buf, frame_len);
		if (ret < 0) {
			LOG_ERR("UART send failed: %d", ret);
			continue;
		}

		/* 等待响应 */
		ret = uart_receive_response(&rx_cmd, rx_data, &rx_len);
		if (ret < 0) {
			LOG_WRN("No response received (retry %d)", retry);
			continue;
		}

		/* 检查响应内容 */
		if (rx_len >= 1) {
			if (rx_data[0] == 0x00) {
				LOG_INF("Command 0x%02X acknowledged (ACK)", cmd);
				return 0;
			} else if (rx_data[0] == 0xFF) {
				LOG_WRN("Command 0x%02X rejected (NAK)", cmd);
				return -ENOTSUP;
			}
		}

		/* 收到响应但无法识别 */
		LOG_WRN("Unexpected response to cmd 0x%02X", cmd);
		return -EBADMSG;
	}

	LOG_ERR("Command 0x%02X failed after %d retries", cmd, UART_MAX_RETRIES);
	return -EIO;
}
