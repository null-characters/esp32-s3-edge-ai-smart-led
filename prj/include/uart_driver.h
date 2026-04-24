/*
 * UART Driver Header - 驱动板通信
 * Phase 1: HW-009 ~ HW-010
 *
 * 物理层: UART1 (TX=GPIO17, RX=GPIO18), 115200bps, 8N1, 无流控
 * 帧格式: [AA55][CMD][LEN][DATA...][校验和][0D0A]
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stddef.h>

/* 帧格式常量 */
#define FRAME_HEADER_HI    0xAA
#define FRAME_HEADER_LO    0x55
#define FRAME_FOOTER_HI    0x0D
#define FRAME_FOOTER_LO    0x0A
#define FRAME_HEADER_LEN   2
#define FRAME_FOOTER_LEN   2
#define FRAME_OVERHEAD     (FRAME_HEADER_LEN + 1 + 1 + 1 + FRAME_FOOTER_LEN) /* 头+CMD+LEN+校验+尾 = 7 */
#define FRAME_MAX_DATA     255
#define FRAME_MAX_TOTAL    (FRAME_OVERHEAD + FRAME_MAX_DATA)  /* = 262 */

/**
 * @brief 初始化UART1设备
 *        波特率115200, 8N1, 无硬件流控
 * @return 0=成功, 负值=失败
 */
int uart_driver_init(void);

/**
 * @brief 构建通信帧
 * @param cmd     命令字 (1字节)
 * @param data    数据缓冲区指针
 * @param data_len 数据长度 (0-255字节)
 * @param out_buf 输出缓冲区 (调用者分配, 需至少 FRAME_MAX_TOTAL 字节)
 * @return 帧总长度(字节), 负值=错误
 */
int frame_build(uint8_t cmd, const uint8_t *data, uint8_t data_len,
		uint8_t *out_buf);

/**
 * @brief 计算校验和 (从CMD到DATA的累加和低8位)
 * @param buf  数据缓冲区 (从CMD开始)
 * @param len  数据长度
 * @return 校验和(低8位)
 */
uint8_t calc_checksum(const uint8_t *buf, uint8_t len);

/**
 * @brief 通过UART发送帧数据
 * @param frame 帧缓冲区
 * @param len   帧长度
 * @return 0=成功, 负值=失败
 */
int uart_send_frame(const uint8_t *frame, uint8_t len);

#endif /* UART_DRIVER_H */
