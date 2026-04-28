/**
 * @file mock_esp.h
 * @brief ESP-IDF API 模拟层
 * 
 * 用于在主机上运行 Unity 测试，无需真实硬件
 */

#ifndef MOCK_ESP_H
#define MOCK_ESP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ================================================================
 * ESP-IDF 类型模拟
 * ================================================================ */

typedef int esp_err_t;
#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM  0x101
#define ESP_ERR_NOT_FOUND 0x104
#define ESP_ERR_INVALID_ARG 0x105

/* 日志模拟 */
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E/%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W/%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I/%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("D/%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) printf("V/%s: " fmt "\n", tag, ##__VA_ARGS__)

/* ================================================================
 * FreeRTOS 模拟
 * ================================================================ */

typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  pdTRUE
#define pdFAIL  pdFALSE

/* 队列模拟 */
static inline QueueHandle_t xQueueCreate(int len, int size) {
    return malloc(len * size + sizeof(int) * 2);
}

static inline void vQueueDelete(QueueHandle_t q) {
    free(q);
}

static inline int xQueueSend(QueueHandle_t q, const void *item, int timeout) {
    return pdTRUE;
}

static inline int xQueueReceive(QueueHandle_t q, void *item, int timeout) {
    return pdFALSE;
}

/* 互斥锁模拟 */
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));
    if (mutex) pthread_mutex_init(mutex, NULL);
    return mutex;
}

static inline int xSemaphoreTake(SemaphoreHandle_t sem, int timeout) {
    if (sem) pthread_mutex_lock((pthread_mutex_t*)sem);
    return pdTRUE;
}

static inline int xSemaphoreGive(SemaphoreHandle_t sem) {
    if (sem) pthread_mutex_unlock((pthread_mutex_t*)sem);
    return pdTRUE;
}

static inline void vSemaphoreDelete(SemaphoreHandle_t sem) {
    if (sem) {
        pthread_mutex_destroy((pthread_mutex_t*)sem);
        free(sem);
    }
}

/* 任务模拟 */
static inline int xTaskCreate(void (*func)(void*), const char *name, int stack, void *arg, int prio, TaskHandle_t *handle) {
    return pdPASS;
}

/* ================================================================
 * 内存管理模拟
 * ================================================================ */

#define MALLOC_CAP_8BIT    (1 << 0)
#define MALLOC_CAP_SPIRAM  (1 << 1)
#define MALLOC_CAP_INTERNAL (1 << 2)

static inline void* heap_caps_malloc(size_t size, uint32_t caps) {
    return malloc(size);
}

static inline void heap_caps_free(void *ptr) {
    free(ptr);
}

static inline size_t heap_caps_get_free_size(uint32_t caps) {
    return 1024 * 1024;  /* 模拟 1MB 空闲 */
}

static inline size_t heap_caps_get_total_size(uint32_t caps) {
    return 8 * 1024 * 1024;  /* 模拟 8MB 总量 */
}

/* ================================================================
 * SPIFFS 模拟
 * ================================================================ */

typedef struct {
    const char *base_path;
    const char *partition_label;
    int max_files;
    bool format_if_mount_failed;
} esp_vfs_spiffs_conf_t;

static inline int esp_vfs_spiffs_register(esp_vfs_spiffs_conf_t *conf) {
    return ESP_OK;
}

static inline int esp_vfs_spiffs_unregister(const char *label) {
    return ESP_OK;
}

/* ================================================================
 * 文件操作模拟
 * ================================================================ */

/* 模拟文件系统 */
typedef struct {
    char path[256];
    uint8_t *data;
    size_t size;
} mock_file_t;

#define MOCK_MAX_FILES 16
static mock_file_t mock_files[MOCK_MAX_FILES];
static int mock_file_count = 0;

static inline void mock_file_register(const char *path, const uint8_t *data, size_t size) {
    if (mock_file_count < MOCK_MAX_FILES) {
        strncpy(mock_files[mock_file_count].path, path, 255);
        mock_files[mock_file_count].data = malloc(size);
        memcpy(mock_files[mock_file_count].data, data, size);
        mock_files[mock_file_count].size = size;
        mock_file_count++;
    }
}

static inline FILE* mock_fopen(const char *path, const char *mode) {
    for (int i = 0; i < mock_file_count; i++) {
        if (strstr(path, mock_files[i].path)) {
            /* 返回内存文件指针 */
            return (FILE*)&mock_files[i];
        }
    }
    return NULL;
}

static inline size_t mock_fread(void *ptr, size_t size, size_t nmemb, FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    if (mf && mf->data) {
        size_t to_copy = size * nmemb;
        if (to_copy > mf->size) to_copy = mf->size;
        memcpy(ptr, mf->data, to_copy);
        return to_copy;
    }
    return 0;
}

static inline int mock_fseek(FILE *fp, long offset, int whence) {
    return 0;
}

static inline long mock_ftell(FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    return mf ? mf->size : 0;
}

static inline int mock_fclose(FILE *fp) {
    return 0;
}

/* 重定向标准文件操作 */
#undef fopen
#undef fread
#undef fseek
#undef ftell
#undef fclose
#define fopen(path, mode) mock_fopen(path, mode)
#define fread(ptr, size, nmemb, fp) mock_fread(ptr, size, nmemb, fp)
#define fseek(fp, offset, whence) mock_fseek(fp, offset, whence)
#define ftell(fp) mock_ftell(fp)
#define fclose(fp) mock_fclose(fp)

/* ================================================================
 * NVS 模拟
 * ================================================================ */

typedef void* nvs_handle_t;

static inline int nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    *handle = malloc(1);
    return ESP_OK;
}

static inline void nvs_close(nvs_handle_t handle) {
    free(handle);
}

static inline int nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *value) {
    *value = 0;  /* 默认值 */
    return ESP_OK;
}

static inline int nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value) {
    return ESP_OK;
}

/* ================================================================
 * GPIO 模拟
 * ================================================================ */

#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT  2

static inline int gpio_set_direction(int pin, int mode) {
    return ESP_OK;
}

static inline int gpio_set_level(int pin, int level) {
    return ESP_OK;
}

static inline int gpio_get_level(int pin) {
    return 0;
}

/* ================================================================
 * PWM 模拟
 * ================================================================ */

static inline int ledc_set_duty(int mode, int channel, uint32_t duty) {
    return ESP_OK;
}

static inline int ledc_update_duty(int mode, int channel) {
    return ESP_OK;
}

/* ================================================================
 * 测试辅助宏
 * ================================================================ */

#define TEST_ASSERT_ESP_OK(err) TEST_ASSERT_EQUAL_INT(ESP_OK, (err))
#define TEST_ASSERT_NOT_NULL(ptr) TEST_ASSERT_TRUE((ptr) != NULL)

#endif /* MOCK_ESP_H */
