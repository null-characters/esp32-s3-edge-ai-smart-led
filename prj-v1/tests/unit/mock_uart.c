/*
 * Mock UART Driver - 测试桩实现
 * 替代真实 UART 硬件依赖
 */

#include <zephyr/kernel.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "uart_driver.h"
#include "lighting.h"  /* for RESP_ACK, RESP_NAK */

static bool mock_uart_ready = true;
static uint8_t mock_response_byte = RESP_ACK;  /* 默认返回 ACK */

/* 帧封装 - 与真实实现相同 (纯逻辑,无硬件依赖) */
uint8_t calc_checksum(const uint8_t *buf, uint8_t len)
{
	uint8_t sum = 0;
	for (uint8_t i = 0; i < len; i++) {
		sum += buf[i];
	}
	return sum;
}

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
	out_buf[idx++] = FRAME_HEADER_HI;
	out_buf[idx++] = FRAME_HEADER_LO;
	out_buf[idx++] = cmd;
	out_buf[idx++] = data_len;

	if (data_len > 0 && data) {
		for (uint8_t i = 0; i < data_len; i++) {
			out_buf[idx++] = data[i];
		}
	}

	uint8_t checksum = calc_checksum(&out_buf[2], 2 + data_len);
	out_buf[idx++] = checksum;
	out_buf[idx++] = FRAME_FOOTER_HI;
	out_buf[idx++] = FRAME_FOOTER_LO;

	return idx;
}

int frame_parse(const uint8_t *buf, uint8_t buf_len,
		uint8_t *cmd, uint8_t *data, uint8_t *data_len)
{
	if (!buf || !cmd || !data || !data_len) {
		return -EINVAL;
	}
	if (buf_len < FRAME_OVERHEAD) {
		return FRAME_PARSE_ERR_LENGTH;
	}
	if (buf[0] != FRAME_HEADER_HI || buf[1] != FRAME_HEADER_LO) {
		return FRAME_PARSE_ERR_HEADER;
	}

	uint8_t received_cmd = buf[2];
	uint8_t received_len = buf[3];

	if (FRAME_OVERHEAD + received_len != buf_len) {
		return FRAME_PARSE_ERR_LENGTH;
	}
	if (buf[buf_len - 2] != FRAME_FOOTER_HI ||
	    buf[buf_len - 1] != FRAME_FOOTER_LO) {
		return FRAME_PARSE_ERR_FOOTER;
	}

	uint8_t calc_sum = calc_checksum(&buf[FRAME_HEADER_LEN], 1 + 1 + received_len);
	uint8_t recv_sum = buf[FRAME_HEADER_LEN + 1 + 1 + received_len];
	if (calc_sum != recv_sum) {
		return FRAME_PARSE_ERR_CSUM;
	}

	*cmd = received_cmd;
	*data_len = received_len;
	for (uint8_t i = 0; i < received_len; i++) {
		data[i] = buf[FRAME_HEADER_LEN + 1 + 1 + i];
	}
	return FRAME_PARSE_OK;
}

/* Mock 实现 - 模拟成功 */
int uart_driver_init(void)
{
	mock_uart_ready = true;
	return 0;
}

int uart_send_frame(const uint8_t *frame, uint8_t len)
{
	if (!mock_uart_ready || !frame || len == 0) {
		return -EINVAL;
	}
	return 0;
}

int uart_receive_response(uint8_t *cmd, uint8_t *data, uint8_t *data_len)
{
	if (!cmd || !data || !data_len) {
		return -EINVAL;
	}
	/* 模拟 ACK 响应 */
	*cmd = 0x00;
	data[0] = mock_response_byte;
	*data_len = 1;
	return 0;
}

int uart_send_with_retry(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
	if (!mock_uart_ready) {
		return -ENODEV;
	}

	/* 构建帧验证参数 */
	uint8_t buf[FRAME_MAX_TOTAL];
	int frame_len = frame_build(cmd, data, data_len, buf);
	if (frame_len < 0) {
		return frame_len;
	}

	/* 模拟发送成功 */
	int ret = uart_send_frame(buf, frame_len);
	if (ret < 0) {
		return ret;
	}

	/* 模拟接收响应 */
	uint8_t rx_cmd, rx_data[256], rx_len;
	ret = uart_receive_response(&rx_cmd, rx_data, &rx_len);
	if (ret < 0) {
		return ret;
	}

	/* 检查响应 */
	if (rx_len >= 1) {
		if (rx_data[0] == RESP_ACK) {
			return 0;
		} else if (rx_data[0] == RESP_NAK) {
			return -ENOTSUP;
		}
	}
	return -EBADMSG;
}
