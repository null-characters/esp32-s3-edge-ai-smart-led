/**
 * @file wake_word.c
 * @brief 唤醒词检测模块实现 (ESP-SR 封装)
 */

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>
#include <string.h>
#include "wake_word.h"
#include "i2s_driver.h"

/* ESP-SR 头文件 */
#include <esp_wn_iface.h>
#include <esp_wn_models.h>

static const char *TAG = "wake_word";

/* 默认唤醒词模型 */
#define DEFAULT_WAKE_WORD_MODEL "wn_xiaobaitong"

/* 事件组 */
static EventGroupHandle_t wake_event_group;
#define WAKE_WORD_BIT      (1 << 0)
#define COMMAND_TIMEOUT_BIT (1 << 1)

/* 模块状态 */
static struct {
    bool initialized;
    bool running;
    bool awake;
    float threshold;
    wake_word_callback_t callback;
    void *user_data;
    TaskHandle_t task_handle;
    const esp_wn_iface_t *wakenet;
    model_iface_data_t *model_data;
    int audio_chunksize;
    SemaphoreHandle_t mutex;    /**< 状态保护 mutex */
} wake_state = {0};

/* 音频缓冲区 */
static int16_t *audio_buf = NULL;

/**
 * @brief 唤醒词检测任务
 */
static void wake_word_task(void *arg)
{
    ESP_LOGI(TAG, "Wake word task started, chunksize=%d", wake_state.audio_chunksize);
    
    while (wake_state.running) {
        /* 读取音频数据 */
        size_t bytes_read = 0;
        esp_err_t ret = i2s_read(audio_buf, wake_state.audio_chunksize, &bytes_read);
        
        if (ret == ESP_OK && bytes_read > 0) {
            /* 送入 WakeNet 检测 */
            wakenet_state_t wake_state_result = wake_state.wakenet->detect(
                wake_state.model_data, audio_buf);
            
            if (wake_state_result == WAKENET_DETECTED) {
                ESP_LOGI(TAG, "Wake word detected!");
                wake_state.awake = true;
                
                /* 设置事件位 */
                xEventGroupSetBits(wake_event_group, WAKE_WORD_BIT);
                
                /* 回调通知 */
                if (wake_state.callback) {
                    wake_state.callback(WAKE_WORD_DETECTED, wake_state.user_data);
                }
            }
        }
        
        vTaskDelay(1);
    }
    
    ESP_LOGI(TAG, "Wake word task stopped");
    vTaskDelete(NULL);
}

int wake_word_init(const wake_word_config_t *config)
{
    if (wake_state.initialized) {
        return 0;
    }
    
    /* 创建状态保护 mutex */
    wake_state.mutex = xSemaphoreCreateMutex();
    if (!wake_state.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return -1;
    }
    
    /* 创建事件组 */
    wake_event_group = xEventGroupCreate();
    if (!wake_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        vSemaphoreDelete(wake_state.mutex);
        return -1;
    }
    
    /* 设置配置 */
    wake_state.threshold = config ? config->threshold : 0.5f;
    wake_state.callback = config ? config->callback : NULL;
    wake_state.user_data = config ? config->user_data : NULL;
    
    /* 获取 WakeNet 接口 */
    wake_state.wakenet = esp_wn_handle_from_name(DEFAULT_WAKE_WORD_MODEL);
    if (!wake_state.wakenet) {
        ESP_LOGE(TAG, "Failed to get wakenet handle for %s", DEFAULT_WAKE_WORD_MODEL);
        vEventGroupDelete(wake_event_group);
        vSemaphoreDelete(wake_state.mutex);
        return -1;
    }
    
    /* 创建模型数据 */
    wake_state.model_data = wake_state.wakenet->create(DEFAULT_WAKE_WORD_MODEL, DET_MODE_90);
    if (!wake_state.model_data) {
        ESP_LOGE(TAG, "Failed to create wakenet model");
        vEventGroupDelete(wake_event_group);
        vSemaphoreDelete(wake_state.mutex);
        return -1;
    }
    
    /* 获取音频块大小 */
    wake_state.audio_chunksize = wake_state.wakenet->get_samp_chunksize(wake_state.model_data);
    
    /* 分配音频缓冲区 */
    audio_buf = (int16_t*)malloc(wake_state.audio_chunksize * sizeof(int16_t));
    if (!audio_buf) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        wake_state.wakenet->destroy(wake_state.model_data);
        vEventGroupDelete(wake_event_group);
        vSemaphoreDelete(wake_state.mutex);
        return -1;
    }
    
    /* 设置阈值 */
    wake_state.wakenet->set_det_threshold(wake_state.model_data, wake_state.threshold, 1);
    
    wake_state.initialized = true;
    ESP_LOGI(TAG, "Wake word module initialized, threshold=%.2f, chunksize=%d", 
             wake_state.threshold, wake_state.audio_chunksize);
    
    return 0;
}

int wake_word_start(void)
{
    if (!wake_state.initialized) {
        ESP_LOGE(TAG, "Wake word not initialized");
        return -1;
    }
    
    if (wake_state.running) {
        return 0;
    }
    
    wake_state.running = true;
    
    /* 创建检测任务 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        wake_word_task,
        "wake_word",
        4096,
        NULL,
        5,
        &wake_state.task_handle,
        1  /* 固定到核心1 */
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wake word task");
        wake_state.running = false;
        return -1;
    }
    
    ESP_LOGI(TAG, "Wake word detection started");
    return 0;
}

int wake_word_stop(void)
{
    if (!wake_state.running) {
        return 0;
    }
    
    wake_state.running = false;
    
    /* 等待任务结束 */
    if (wake_state.task_handle) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        wake_state.task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Wake word detection stopped");
    return 0;
}

void wake_word_deinit(void)
{
    if (!wake_state.initialized) {
        return;
    }
    
    /* 停止检测任务 */
    wake_word_stop();
    
    /* 释放音频缓冲区 */
    if (audio_buf) {
        free(audio_buf);
        audio_buf = NULL;
    }
    
    /* 释放 WakeNet 资源 */
    if (wake_state.model_data && wake_state.wakenet) {
        wake_state.wakenet->destroy(wake_state.model_data);
        wake_state.model_data = NULL;
    }
    
    /* 删除事件组 */
    if (wake_event_group) {
        vEventGroupDelete(wake_event_group);
        wake_event_group = NULL;
    }
    
    /* 删除 mutex */
    SemaphoreHandle_t mutex_to_delete = wake_state.mutex;
    memset(&wake_state, 0, sizeof(wake_state));
    
    if (mutex_to_delete) {
        vSemaphoreDelete(mutex_to_delete);
    }
    
    ESP_LOGI(TAG, "Wake word module deinitialized");
}

bool wake_word_is_awake(void)
{
    if (!wake_state.initialized) {
        return false;
    }
    
    xSemaphoreTake(wake_state.mutex, portMAX_DELAY);
    bool awake = wake_state.awake;
    xSemaphoreGive(wake_state.mutex);
    
    return awake;
}

void wake_word_reset(void)
{
    if (!wake_state.initialized) {
        return;
    }
    
    xSemaphoreTake(wake_state.mutex, portMAX_DELAY);
    wake_state.awake = false;
    xSemaphoreGive(wake_state.mutex);
    
    xEventGroupClearBits(wake_event_group, WAKE_WORD_BIT);
}

void wake_word_set_threshold(float threshold)
{
    xSemaphoreTake(wake_state.mutex, portMAX_DELAY);
    wake_state.threshold = threshold;
    if (wake_state.model_data && wake_state.wakenet) {
        wake_state.wakenet->set_det_threshold(wake_state.model_data, threshold, 1);
    }
    xSemaphoreGive(wake_state.mutex);
}

float wake_word_get_threshold(void)
{
    if (!wake_state.initialized) {
        return 0.0f;
    }
    
    xSemaphoreTake(wake_state.mutex, portMAX_DELAY);
    float threshold = wake_state.threshold;
    xSemaphoreGive(wake_state.mutex);
    
    return threshold;
}