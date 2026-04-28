/**
 * @file env_watcher.c
 * @brief 环境变化监听模块实现
 */

#include "env_watcher.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "ENV_WATCHER";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    bool running;
    
    /* 环境状态 */
    env_state_t current_state;
    uint64_t state_change_time_us;
    uint32_t absent_ms;
    
    /* 雷达数据 */
    bool radar_presence;
    float radar_distance;
    float radar_energy;
    uint64_t radar_update_time_us;
    
    /* 配置 */
    uint32_t absent_threshold_ms;
    uint32_t check_interval_ms;
    
    /* 回调 */
    env_change_callback_t callback;
    void *user_data;
    
    /* 定时器 */
    esp_timer_handle_t check_timer;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} env_watcher_state_t;

static env_watcher_state_t g_env = {0};

/* ================================================================
 * 内部函数
 * ================================================================ */

static void check_timer_callback(void *arg)
{
    /* 使用非阻塞获取，定时器回调中不应阻塞 */
    if (xSemaphoreTake(g_env.mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return;
    }
    
    if (!g_env.running) {
        xSemaphoreGive(g_env.mutex);
        return;
    }
    
    uint64_t now_us = esp_timer_get_time();
    
    /* 检查雷达数据是否过期 */
    if (now_us - g_env.radar_update_time_us > (uint64_t)g_env.check_interval_ms * 2000ULL) {
        /* 雷达数据超过 2 个检查周期未更新，视为无人 */
        g_env.radar_presence = false;
    }
    
    /* 更新环境状态 */
    env_state_t new_state = g_env.radar_presence ? ENV_STATE_OCCUPIED : ENV_STATE_EMPTY;
    
    /* 提取回调信息，在锁外调用 */
    env_change_callback_t callback = NULL;
    void *user_data = NULL;
    bool trigger_callback = false;
    
    if (new_state != g_env.current_state) {
        /* 状态变化 */
        g_env.current_state = new_state;
        g_env.state_change_time_us = now_us;
        
        if (new_state == ENV_STATE_EMPTY) {
            ESP_LOGI(TAG, "环境变化: 有人 → 无人");
        } else {
            ESP_LOGI(TAG, "环境变化: 无人 → 有人");
            g_env.absent_ms = 0;  /* 有人时重置离开时间 */
        }
    }
    
    /* 计算离开时长 */
    if (g_env.current_state == ENV_STATE_EMPTY) {
        g_env.absent_ms = (now_us - g_env.state_change_time_us) / 1000;
        
        /* 检查是否超过阈值 */
        if (g_env.absent_ms >= g_env.absent_threshold_ms) {
            ESP_LOGI(TAG, "人离开超过阈值: %lu 分钟", g_env.absent_ms / 60000);
            
            /* 准备回调信息 */
            callback = g_env.callback;
            user_data = g_env.user_data;
            trigger_callback = true;
        }
    }
    
    xSemaphoreGive(g_env.mutex);
    
    /* 在锁外调用回调，避免死锁 */
    if (trigger_callback && callback) {
        callback(ENV_STATE_EMPTY, g_env.absent_ms, user_data);
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t env_watcher_init(const env_watcher_config_t *config)
{
    if (g_env.initialized) {
        ESP_LOGW(TAG, "环境监听已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化环境监听模块");
    
    g_env.mutex = xSemaphoreCreateMutex();
    if (!g_env.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 应用配置 */
    if (config) {
        g_env.absent_threshold_ms = config->absent_threshold_ms;
        g_env.check_interval_ms = config->check_interval_ms;
        g_env.callback = config->callback;
        g_env.user_data = config->user_data;
    } else {
        g_env.absent_threshold_ms = ENV_ABSENT_THRESHOLD_MS;
        g_env.check_interval_ms = ENV_CHECK_INTERVAL_MS;
    }
    
    /* 初始化状态 */
    g_env.current_state = ENV_STATE_UNKNOWN;
    g_env.state_change_time_us = esp_timer_get_time();
    g_env.absent_ms = 0;
    g_env.radar_presence = false;
    g_env.radar_distance = 0;
    g_env.radar_energy = 0;
    g_env.radar_update_time_us = 0;
    
    /* 创建定时器 */
    esp_timer_create_args_t timer_args = {
        .callback = check_timer_callback,
        .arg = NULL,
        .name = "env_check_timer",
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &g_env.check_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建定时器失败");
        vSemaphoreDelete(g_env.mutex);
        return ret;
    }
    
    g_env.initialized = true;
    g_env.running = false;
    
    ESP_LOGI(TAG, "环境监听初始化完成: threshold=%lu 分钟", g_env.absent_threshold_ms / 60000);
    
    return ESP_OK;
}

void env_watcher_deinit(void)
{
    if (!g_env.initialized) {
        return;
    }
    
    env_watcher_stop();
    
    if (g_env.check_timer) {
        esp_timer_delete(g_env.check_timer);
    }
    
    if (g_env.mutex) {
        vSemaphoreDelete(g_env.mutex);
    }
    
    memset(&g_env, 0, sizeof(env_watcher_state_t));
    ESP_LOGI(TAG, "环境监听已释放");
}

esp_err_t env_watcher_start(void)
{
    if (!g_env.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_env.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动环境监听");
    
    esp_err_t ret = esp_timer_start_periodic(g_env.check_timer, g_env.check_interval_ms * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动定时器失败");
        return ret;
    }
    
    g_env.running = true;
    
    return ESP_OK;
}

esp_err_t env_watcher_stop(void)
{
    if (!g_env.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_env.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "停止环境监听");
    
    esp_timer_stop(g_env.check_timer);
    g_env.running = false;
    
    return ESP_OK;
}

esp_err_t env_watcher_update_radar(bool presence, float distance, float energy)
{
    if (!g_env.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_env.mutex, portMAX_DELAY);
    
    g_env.radar_presence = presence;
    g_env.radar_distance = distance;
    g_env.radar_energy = energy;
    g_env.radar_update_time_us = esp_timer_get_time();
    
    xSemaphoreGive(g_env.mutex);
    
    return ESP_OK;
}

env_state_t env_watcher_get_state(void)
{
    if (!g_env.initialized) {
        return ENV_STATE_UNKNOWN;
    }
    
    xSemaphoreTake(g_env.mutex, portMAX_DELAY);
    env_state_t state = g_env.current_state;
    xSemaphoreGive(g_env.mutex);
    
    return state;
}

uint32_t env_watcher_get_absent_time_ms(void)
{
    if (!g_env.initialized) {
        return 0;
    }
    
    xSemaphoreTake(g_env.mutex, portMAX_DELAY);
    uint32_t absent_ms = g_env.absent_ms;
    xSemaphoreGive(g_env.mutex);
    
    return absent_ms;
}

void env_watcher_set_callback(env_change_callback_t callback, void *user_data)
{
    if (!g_env.initialized) {
        return;
    }
    
    xSemaphoreTake(g_env.mutex, portMAX_DELAY);
    g_env.callback = callback;
    g_env.user_data = user_data;
    xSemaphoreGive(g_env.mutex);
}