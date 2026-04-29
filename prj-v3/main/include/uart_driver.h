/**
 * @file uart_driver.h
 * @brief UART 驱动接口
 * 
 * 用于雷达通信和调试输出
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/**
 * @brief 初始化 UART 驱动
 * 
 * @param baudrate 波特率 (默认 115200)
 * @return ESP_OK 成功
 */
esp_err_t uart_init(uint32_t baudrate);

/**
 * @brief 释放 UART 驱动
 * 
 * @return ESP_OK 成功
 */
esp_err_t uart_deinit(void);

/**
 * @brief 读取 UART 数据
 * 
 * @param data 数据缓冲区
 * @param len 要读取的字节数
 * @param timeout_ms 超时时间 (毫秒)
 * @return 实际读取的字节数，负值表示错误
 */
int uart_read(uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief 写入 UART 数据
 * 
 * @param data 数据缓冲区
 * @param len 要写入的字节数
 * @return 实际写入的字节数，负值表示错误
 */
int uart_write(const uint8_t *data, size_t len);

/**
 * @brief 读取一行数据 (直到换行符)
 * 
 * @param buf 数据缓冲区
 * @param buf_size 缓冲区大小
 * @param timeout_ms 超时时间 (毫秒)
 * @return 实际读取的字节数，负值表示错误
 */
int uart_read_line(uint8_t *buf, size_t buf_size, uint32_t timeout_ms);

/**
 * @brief 清空 UART 接收缓冲区
 * @note 函数名改为 uart_clear_buffer，避免与 ESP-IDF 5.4+ uart_flush() 冲突
 * @return ESP_OK 成功
 */
esp_err_t uart_clear_buffer(void);

/**
 * @brief 获取可用数据长度
 * 
 * @return RX 缓冲区中的数据字节数
 */
size_t uart_available(void);

#endif /* UART_DRIVER_H */
