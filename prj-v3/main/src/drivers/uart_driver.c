/**
 * @file uart_driver.c
 * @brief UART 驱动实现
 */

#include "uart_driver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"

static const char *TAG = "UART_DRV";

/* UART 配置 */
#define UART_NUM_RADAR      UART_NUM_1
#define UART_TX_GPIO        17
#define UART_RX_GPIO        18
#define UART_RX_BUF_SIZE    256
#define UART_TX_BUF_SIZE    256

static bool g_initialized = false;

esp_err_t my_uart_init(uint32_t baudrate)
{
    uart_config_t uart_conf = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_RADAR, &uart_conf));

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_RADAR, UART_TX_GPIO, UART_RX_GPIO, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_RADAR, UART_RX_BUF_SIZE, 
                                          UART_TX_BUF_SIZE, 0, NULL, 0));

    g_initialized = true;
    ESP_LOGI(TAG, "UART initialized: port=%d, baud=%lu, tx=%d, rx=%d", 
             UART_NUM_RADAR, baudrate, UART_TX_GPIO, UART_RX_GPIO);
    return ESP_OK;
}

int my_uart_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!g_initialized || data == NULL) {
        return -1;
    }

    int bytes_read = uart_read_bytes(UART_NUM_RADAR, data, len, 
                                      pdMS_TO_TICKS(timeout_ms));
    return bytes_read;
}

int my_uart_write(const uint8_t *data, size_t len)
{
    if (!g_initialized || data == NULL) {
        return -1;
    }

    int bytes_written = uart_write_bytes(UART_NUM_RADAR, (const char *)data, len);
    return bytes_written;
}

int my_uart_read_line(uint8_t *buf, size_t buf_size, uint32_t timeout_ms)
{
    if (!g_initialized || buf == NULL || buf_size == 0) {
        return -1;
    }

    size_t total_read = 0;
    uint8_t byte;
    
    while (total_read < buf_size - 1) {
        int read = uart_read_bytes(UART_NUM_RADAR, &byte, 1, 
                                   pdMS_TO_TICKS(timeout_ms));
        if (read <= 0) {
            break;
        }
        
        buf[total_read++] = byte;
        
        if (byte == '\n' || byte == '\r') {
            break;
        }
    }
    
    buf[total_read] = '\0';
    return (int)total_read;
}

esp_err_t my_uart_flush(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return uart_flush_input(UART_NUM_RADAR);
}

size_t my_uart_available(void)
{
    if (!g_initialized) {
        return 0;
    }
    
    size_t available = 0;
    uart_get_buffered_data_len(UART_NUM_RADAR, &available);
    return available;
}