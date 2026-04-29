/**
 * @file mock_esp.h
 * @brief ESP-IDF API 模拟层
 * 
 * 用于在主机上运行 Unity 测试，无需真实硬件
 * 支持完整的文件系统模拟、队列数据传输和定时器
 */

#ifndef MOCK_ESP_H
#define MOCK_ESP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* ================================================================
 * ESP-IDF 类型模拟
 * ================================================================ */

typedef int esp_err_t;
#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM  0x101
#define ESP_ERR_NOT_FOUND 0x104
#define ESP_ERR_INVALID_ARG 0x105
#define ESP_ERR_TIMEOUT 0x107

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
#define portMAX_DELAY 0xFFFFFFFF

/* 环形缓冲区队列模拟 */
typedef struct {
    uint8_t *buffer;       /* 数据缓冲区 */
    size_t item_size;      /* 单项大小 */
    size_t capacity;       /* 容量 */
    size_t head;           /* 头指针 */
    size_t tail;           /* 尾指针 */
    size_t count;          /* 当前数量 */
    pthread_mutex_t mutex; /* 互斥锁 */
} mock_queue_t;

static inline QueueHandle_t xQueueCreate(int len, int size) {
    mock_queue_t *q = (mock_queue_t*)malloc(sizeof(mock_queue_t));
    if (!q) return NULL;
    
    q->buffer = (uint8_t*)malloc(len * size);
    if (!q->buffer) {
        free(q);
        return NULL;
    }
    
    q->item_size = size;
    q->capacity = len;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    
    return (QueueHandle_t)q;
}

static inline void vQueueDelete(QueueHandle_t handle) {
    if (!handle) return;
    mock_queue_t *q = (mock_queue_t*)handle;
    pthread_mutex_destroy(&q->mutex);
    free(q->buffer);
    free(q);
}

static inline int xQueueSend(QueueHandle_t handle, const void *item, int timeout) {
    if (!handle || !item) return pdFALSE;
    mock_queue_t *q = (mock_queue_t*)handle;
    
    pthread_mutex_lock(&q->mutex);
    
    if (q->count >= q->capacity) {
        pthread_mutex_unlock(&q->mutex);
        return pdFALSE; /* 队列已满 */
    }
    
    /* 复制数据到尾部 */
    size_t offset = q->tail * q->item_size;
    memcpy(q->buffer + offset, item, q->item_size);
    
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    
    pthread_mutex_unlock(&q->mutex);
    return pdTRUE;
}

static inline int xQueueReceive(QueueHandle_t handle, void *item, int timeout) {
    if (!handle || !item) return pdFALSE;
    mock_queue_t *q = (mock_queue_t*)handle;
    
    pthread_mutex_lock(&q->mutex);
    
    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return pdFALSE; /* 队列为空 */
    }
    
    /* 从头部复制数据 */
    size_t offset = q->head * q->item_size;
    memcpy(item, q->buffer + offset, q->item_size);
    
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    
    pthread_mutex_unlock(&q->mutex);
    return pdTRUE;
}

static inline int uxQueueMessagesWaiting(QueueHandle_t handle) {
    if (!handle) return 0;
    mock_queue_t *q = (mock_queue_t*)handle;
    pthread_mutex_lock(&q->mutex);
    int count = q->count;
    pthread_mutex_unlock(&q->mutex);
    return count;
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
 * 定时器模拟
 * ================================================================ */

typedef void* esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void* arg);

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    uint64_t period_us;
    bool one_shot;
    bool running;
    uint64_t next_fire_us;
} mock_timer_t;

static mock_timer_t mock_timers[16];
static int mock_timer_count = 0;
static uint64_t mock_time_us = 0;

/* 获取模拟时间 */
static inline uint64_t esp_timer_get_time(void) {
    return mock_time_us;
}

/* 推进模拟时间 */
static inline void mock_timer_advance(uint64_t us) {
    mock_time_us += us;
    
    /* 检查并触发定时器 */
    for (int i = 0; i < mock_timer_count; i++) {
        mock_timer_t *t = &mock_timers[i];
        if (t->running && mock_time_us >= t->next_fire_us) {
            if (t->callback) {
                t->callback(t->arg);
            }
            if (t->one_shot) {
                t->running = false;
            } else {
                t->next_fire_us = mock_time_us + t->period_us;
            }
        }
    }
}

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    const char *name;
} esp_timer_create_args_t;

static inline esp_err_t esp_timer_create(esp_timer_create_args_t *args, esp_timer_handle_t *handle) {
    if (mock_timer_count >= 16) return ESP_ERR_NO_MEM;
    
    mock_timer_t *t = &mock_timers[mock_timer_count];
    t->callback = args->callback;
    t->arg = args->arg;
    t->running = false;
    t->one_shot = true;
    t->next_fire_us = 0;
    
    *handle = (esp_timer_handle_t)t;
    mock_timer_count++;
    return ESP_OK;
}

static inline esp_err_t esp_timer_delete(esp_timer_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    mock_timer_t *t = (mock_timer_t*)handle;
    t->running = false;
    return ESP_OK;
}

static inline esp_err_t esp_timer_start_once(esp_timer_handle_t handle, uint64_t timeout_us) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    mock_timer_t *t = (mock_timer_t*)handle;
    t->one_shot = true;
    t->next_fire_us = mock_time_us + timeout_us;
    t->running = true;
    return ESP_OK;
}

static inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t handle, uint64_t period_us) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    mock_timer_t *t = (mock_timer_t*)handle;
    t->one_shot = false;
    t->period_us = period_us;
    t->next_fire_us = mock_time_us + period_us;
    t->running = true;
    return ESP_OK;
}

static inline esp_err_t esp_timer_stop(esp_timer_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    mock_timer_t *t = (mock_timer_t*)handle;
    t->running = false;
    return ESP_OK;
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
 * 文件操作模拟 (支持读写位置)
 * ================================================================ */

/* 模拟文件系统 */
typedef struct {
    char path[256];
    uint8_t *data;
    size_t size;
    size_t pos;          /* 当前读写位置 */
    bool in_use;         /* 是否被打开 */
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
        mock_files[mock_file_count].pos = 0;
        mock_files[mock_file_count].in_use = false;
        mock_file_count++;
    }
}

static inline void mock_file_clear(void) {
    for (int i = 0; i < mock_file_count; i++) {
        if (mock_files[i].data) {
            free(mock_files[i].data);
            mock_files[i].data = NULL;
        }
    }
    mock_file_count = 0;
}

static inline FILE* mock_fopen(const char *path, const char *mode) {
    for (int i = 0; i < mock_file_count; i++) {
        if (strstr(path, mock_files[i].path) && !mock_files[i].in_use) {
            mock_files[i].pos = 0;
            mock_files[i].in_use = true;
            return (FILE*)&mock_files[i];
        }
    }
    return NULL;
}

static inline size_t mock_fread(void *ptr, size_t size, size_t nmemb, FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    if (!mf || !mf->data) return 0;
    
    size_t to_read = size * nmemb;
    size_t avail = mf->size - mf->pos;
    if (to_read > avail) to_read = avail;
    
    memcpy(ptr, mf->data + mf->pos, to_read);
    mf->pos += to_read;
    
    return to_read / size;
}

static inline size_t mock_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    if (!mf || !mf->data) return 0;
    
    size_t to_write = size * nmemb;
    size_t avail = mf->size - mf->pos;
    if (to_write > avail) to_write = avail;
    
    memcpy(mf->data + mf->pos, ptr, to_write);
    mf->pos += to_write;
    
    return to_write / size;
}

static inline int mock_fseek(FILE *fp, long offset, int whence) {
    mock_file_t *mf = (mock_file_t*)fp;
    if (!mf) return -1;
    
    long new_pos;
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = mf->pos + offset;
            break;
        case SEEK_END:
            new_pos = mf->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_pos < 0) new_pos = 0;
    if ((size_t)new_pos > mf->size) new_pos = mf->size;
    
    mf->pos = new_pos;
    return 0;
}

static inline long mock_ftell(FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    return mf ? (long)mf->pos : 0;
}

static inline int mock_fclose(FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    if (mf) mf->in_use = false;
    return 0;
}

static inline int mock_feof(FILE *fp) {
    mock_file_t *mf = (mock_file_t*)fp;
    return mf ? (mf->pos >= mf->size) : 1;
}

/* 重定向标准文件操作 */
#undef fopen
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef fclose
#undef feof
#define fopen(path, mode) mock_fopen(path, mode)
#define fread(ptr, size, nmemb, fp) mock_fread(ptr, size, nmemb, fp)
#define fwrite(ptr, size, nmemb, fp) mock_fwrite(ptr, size, nmemb, fp)
#define fseek(fp, offset, whence) mock_fseek(fp, offset, whence)
#define ftell(fp) mock_ftell(fp)
#define fclose(fp) mock_fclose(fp)
#define feof(fp) mock_feof(fp)

/* ================================================================
 * NVS 模拟
 * ================================================================ */

typedef void* nvs_handle_t;

#define NVS_READONLY  0
#define NVS_READWRITE 1

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

static inline int nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value) {
    *value = 0;
    return ESP_OK;
}

static inline int nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
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
