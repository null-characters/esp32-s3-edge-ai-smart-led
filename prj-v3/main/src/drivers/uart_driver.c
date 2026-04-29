/**
 * @file uart_driver.c
 * @brief UART 驱动实现
 */

#include "uart_driver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "UART_DRV";

/* UART 配置 */
#define UART_NUM_RADAR      UART_NUM_1
#define UART_TX_GPIO        8   /* 改为 GPIO8，避免与 DAC (GPIO17/18) 冲突 */
#define UART_RX_GPIO        9   /* 改为 GPIO9，避免与 DAC (GPIO17/18) 冲突 */
#define UART_RX_BUF_SIZE    256
#define UART_TX_BUF_SIZE    256

static bool g_initialized = false;
static SemaphoreHandle_t s_uart_mutex = NULL;

esp_err_t uart_init(uint32_t baudrate)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "UART 已初始化");
        return ESP_OK;
    }
    
    /* 创建互斥锁 */
    s_uart_mutex = xSemaphoreCreateMutex();
    if (!s_uart_mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    uart_config_t uart_conf = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(UART_NUM_RADAR, &uart_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 参数配置失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_uart_mutex);
        s_uart_mutex = NULL;
        return ret;
    }

    ret = uart_set_pin(UART_NUM_RADAR, UART_TX_GPIO, UART_RX_GPIO, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 引脚配置失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_uart_mutex);
        s_uart_mutex = NULL;
        return ret;
    }

    ret = uart_driver_install(UART_NUM_RADAR, UART_RX_BUF_SIZE, 
                              UART_TX_BUF_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 驱动安装失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_uart_mutex);
        s_uart_mutex = NULL;
        return ret;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "UART initialized: port=%d, baud=%lu, tx=%d, rx=%d", 
             UART_NUM_RADAR, baudrate, UART_TX_GPIO, UART_RX_GPIO);
    return ESP_OK;
}

esp_err_t uart_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }
    
    g_initialized = false;
    
    SemaphoreHandle_t mutex_to_delete = s_uart_mutex;
    s_uart_mutex = NULL;
    
    if (mutex_to_delete) {
        xSemaphoreTake(mutex_to_delete, portMAX_DELAY);
        uart_driver_delete(UART_NUM_RADAR);
        xSemaphoreGive(mutex_to_delete);
        vSemaphoreDelete(mutex_to_delete);
    }
    
    ESP_LOGI(TAG, "UART 已释放");
    return ESP_OK;
}

int uart_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!g_initialized || data == NULL) {
        return -1;
    }

    int bytes_read = uart_read_bytes(UART_NUM_RADAR, data, len, 
                                     pdMS_TO_TICKS(timeout_ms));
    return bytes_read;
}

int uart_write(const uint8_t *data, size_t len)
{
    if (!g_initialized || data == NULL) {
        return -1;
    }

    int bytes_written = uart_write_bytes(UART_NUM_RADAR, (const char *)data, len);
    return bytes_written;
}

int uart_read_line(uint8_t *buf, size_t buf_size, uint32_t timeout_ms)
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

esp_err_t uart_clear_buffer(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return uart_flush_input(UART_NUM_RADAR);
}

size_t uart_available(void)
{
    if (!g_initialized) {
        return 0;
    }
    
    size_t available = 0;
    uart_get_buffered_data_len(UART_NUM_RADAR, &available);
    return available;
}

/* 兼容旧 API (deprecated) */
esp_err_t my_uart_init(uint32_t baudrate) __attribute__((deprecated("use uart_init instead")));
int my_uart_read(uint8_t *data, size_t len, uint32_t timeout_ms) __attribute__((deprecated("use uart_read instead")));
int my_uart_write(const uint8_t *data, size_t len) __attribute__((deprecated("use uart_write instead")));
int my_uart_read_line(uint8_t *buf, size_t buf_size, uint32_t timeout_ms) __attribute__((deprecated("use uart_read_line instead")));
esp_err_t my_uart_flush(void) __attribute__((deprecated("use uart_clear_buffer instead")));
size_t my_uart_available(void) __attribute__((deprecated("use uart_available instead")));

esp_err_t my_uart_init(uint32_t baudrate) { return uart_init(baudrate); }
int my_uart_read(uint8_t *data, size_t len, uint32_t timeout_ms) { return uart_read(data, len, timeout_ms); }
int my_uart_write(const uint8_t *data, size_t len) { return uart_write(data, len); }
int my_uart_read_line(uint8_t *buf, size_t buf_size, uint32_t timeout_ms) { return uart_read_line(buf, buf_size, timeout_ms); }
esp_err_t my_uart_flush(void) { return uart_clear_buffer(); }
size_t my_uart_available(void) { return uart_available(); }
