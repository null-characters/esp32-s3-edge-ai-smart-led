/**
 * @file ttl_lease.c
 * @brief TTL 租约计时器实现
 * 
 * 防止语音锁死：语音干预带有效期，过期自动释放
 */

#include "ttl_lease.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "TTL_LEASE";

/* ================================================================
 * 内部结构体定义 (隐藏实现细节)
 * ================================================================ */

/**
 * @brief TTL 租约结构体实现
 */
struct ttl_lease_s {
    lease_state_t state;            /* 租约状态 */
    uint64_t start_time_us;         /* 租约开始时间 (微秒) */
    uint32_t ttl_ms;                /* 租约时长 (毫秒) */
    lease_release_reason_t release_reason;  /* 释放原因 */
    bool already_notified;          /* 是否已触发回调 (防止重复通知) */
    
    /* 回调 */
    lease_release_callback_t on_release;
    void *user_data;
};

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    struct ttl_lease_s lease;       /* 租约实例 */
    uint32_t env_absent_ms;         /* 环境离开累计时间 */
    bool env_absent;                /* 环境是否离开 */
    SemaphoreHandle_t mutex;
} ttl_state_t;

static ttl_state_t g_ttl = {0};

/* ================================================================
 * 内部函数
 * ================================================================ */

static int64_t get_time_us(void)
{
    return esp_timer_get_time();
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t ttl_lease_init(void)
{
    if (g_ttl.initialized) {
        ESP_LOGW(TAG, "TTL 租约已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化 TTL 租约模块");
    
    g_ttl.mutex = xSemaphoreCreateMutex();
    if (!g_ttl.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    g_ttl.lease.state = LEASE_STATE_INACTIVE;
    g_ttl.lease.start_time_us = 0;
    g_ttl.lease.ttl_ms = 0;
    g_ttl.lease.release_reason = LEASE_RELEASE_NONE;
    g_ttl.lease.already_notified = false;
    g_ttl.env_absent_ms = 0;
    g_ttl.env_absent = false;
    
    g_ttl.initialized = true;
    
    ESP_LOGI(TAG, "TTL 租约模块初始化完成");
    return ESP_OK;
}

void ttl_lease_deinit(void)
{
    if (!g_ttl.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "释放 TTL 租约模块资源");
    
    /* 删除 mutex */
    if (g_ttl.mutex) {
        vSemaphoreDelete(g_ttl.mutex);
        g_ttl.mutex = NULL;
    }
    
    /* 清空回调 */
    g_ttl.lease.on_release = NULL;
    g_ttl.lease.user_data = NULL;
    
    /* 重置状态 */
    g_ttl.lease.state = LEASE_STATE_INACTIVE;
    g_ttl.lease.start_time_us = 0;
    g_ttl.lease.ttl_ms = 0;
    g_ttl.lease.release_reason = LEASE_RELEASE_NONE;
    g_ttl.lease.already_notified = false;
    g_ttl.env_absent_ms = 0;
    g_ttl.env_absent = false;
    
    g_ttl.initialized = false;
    
    ESP_LOGI(TAG, "TTL 租约模块已释放");
}

esp_err_t ttl_lease_acquire(uint32_t ttl_ms)
{
    if (!g_ttl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    
    if (g_ttl.lease.state == LEASE_STATE_ACTIVE) {
        ESP_LOGW(TAG, "已有活跃租约，拒绝新租约");
        xSemaphoreGive(g_ttl.mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (ttl_ms == 0) {
        ttl_ms = TTL_DEFAULT_MS;  /* 默认 2 小时 */
    }
    
    g_ttl.lease.state = LEASE_STATE_ACTIVE;
    g_ttl.lease.start_time_us = get_time_us();
    g_ttl.lease.ttl_ms = ttl_ms;
    g_ttl.lease.release_reason = LEASE_RELEASE_NONE;
    g_ttl.lease.already_notified = false;  /* 重置通知标志 */
    g_ttl.env_absent_ms = 0;  /* 重置环境离开时间 */
    
    ESP_LOGI(TAG, "获取租约: ttl=%lu 分钟", ttl_ms / 60000);
    
    xSemaphoreGive(g_ttl.mutex);
    
    return ESP_OK;
}

esp_err_t ttl_lease_renew(uint32_t ttl_ms)
{
    if (!g_ttl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    
    if (g_ttl.lease.state != LEASE_STATE_ACTIVE) {
        xSemaphoreGive(g_ttl.mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* 刷新开始时间 */
    g_ttl.lease.start_time_us = get_time_us();
    
    /* 更新 TTL 时长 */
    if (ttl_ms > 0) {
        g_ttl.lease.ttl_ms = ttl_ms;
    }
    
    /* 重置环境离开时间 */
    g_ttl.env_absent_ms = 0;
    
    ESP_LOGI(TAG, "续约租约: ttl=%lu 分钟", g_ttl.lease.ttl_ms / 60000);
    
    xSemaphoreGive(g_ttl.mutex);
    
    return ESP_OK;
}

esp_err_t ttl_lease_release(lease_release_reason_t reason)
{
    if (!g_ttl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    
    if (g_ttl.lease.state != LEASE_STATE_ACTIVE) {
        xSemaphoreGive(g_ttl.mutex);
        return ESP_OK;  /* 无活跃租约，直接返回成功 */
    }
    
    g_ttl.lease.state = LEASE_STATE_RELEASED;
    g_ttl.lease.release_reason = reason;
    
    /* 提取回调信息，在锁外调用避免死锁 */
    lease_release_callback_t callback = g_ttl.lease.on_release;
    void *user_data = g_ttl.lease.user_data;
    
    ESP_LOGI(TAG, "释放租约: reason=%d", reason);
    
    xSemaphoreGive(g_ttl.mutex);
    
    /* 在锁外调用回调 */
    if (callback) {
        callback(reason, user_data);
    }
    
    return ESP_OK;
}

bool ttl_lease_check_expired(void)
{
    if (!g_ttl.initialized) {
        return false;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    
    if (g_ttl.lease.state != LEASE_STATE_ACTIVE) {
        xSemaphoreGive(g_ttl.mutex);
        return false;
    }
    
    /* 提取回调信息 */
    lease_release_callback_t callback = g_ttl.lease.on_release;
    void *user_data = g_ttl.lease.user_data;
    bool expired = false;
    lease_release_reason_t reason = LEASE_RELEASE_NONE;
    
    /* 检查 TTL 过期 */
    int64_t elapsed_ms = (get_time_us() - g_ttl.lease.start_time_us) / 1000;
    
    if (elapsed_ms >= g_ttl.lease.ttl_ms) {
        ESP_LOGI(TAG, "TTL 过期: elapsed=%lld 分钟, ttl=%lu 分钟", 
                 elapsed_ms / 60000, g_ttl.lease.ttl_ms / 60000);
        
        g_ttl.lease.state = LEASE_STATE_EXPIRED;
        g_ttl.lease.release_reason = LEASE_RELEASE_TTL_EXPIRED;
        reason = LEASE_RELEASE_TTL_EXPIRED;
        expired = true;
    }
    /* 检查环境变化释放 */
    else if (g_ttl.env_absent && g_ttl.env_absent_ms >= TTL_ENV_EXIT_MS) {
        ESP_LOGI(TAG, "环境变化释放: absent=%lu 分钟", g_ttl.env_absent_ms / 60000);
        
        g_ttl.lease.state = LEASE_STATE_RELEASED;
        g_ttl.lease.release_reason = LEASE_RELEASE_ENV_CHANGE;
        reason = LEASE_RELEASE_ENV_CHANGE;
        expired = true;
    }
    
    /* 仅在首次过期时触发回调 */
    if (expired && !g_ttl.lease.already_notified) {
        g_ttl.lease.already_notified = true;
    } else if (expired) {
        /* 已通知过，不重复回调 */
        callback = NULL;
    }
    
    xSemaphoreGive(g_ttl.mutex);
    
    /* 在锁外调用回调 */
    if (callback) {
        callback(reason, user_data);
    }
    
    return expired;
}

lease_state_t ttl_lease_get_state(void)
{
    if (!g_ttl.initialized) {
        return LEASE_STATE_INACTIVE;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    lease_state_t state = g_ttl.lease.state;
    xSemaphoreGive(g_ttl.mutex);
    
    return state;
}

int64_t ttl_lease_get_remaining_ms(void)
{
    if (!g_ttl.initialized || g_ttl.lease.state != LEASE_STATE_ACTIVE) {
        return -1;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    
    int64_t elapsed_ms = (get_time_us() - g_ttl.lease.start_time_us) / 1000;
    int64_t remaining = g_ttl.lease.ttl_ms - elapsed_ms;
    
    xSemaphoreGive(g_ttl.mutex);
    
    return remaining > 0 ? remaining : 0;
}

lease_release_reason_t ttl_lease_get_release_reason(void)
{
    if (!g_ttl.initialized) {
        return LEASE_RELEASE_NONE;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    lease_release_reason_t reason = g_ttl.lease.release_reason;
    xSemaphoreGive(g_ttl.mutex);
    
    return reason;
}

void ttl_lease_set_callback(lease_release_callback_t callback, void *user_data)
{
    if (!g_ttl.initialized) {
        return;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    g_ttl.lease.on_release = callback;
    g_ttl.lease.user_data = user_data;
    xSemaphoreGive(g_ttl.mutex);
}

esp_err_t ttl_lease_notify_env_change(uint32_t absent_ms)
{
    if (!g_ttl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ttl.mutex, portMAX_DELAY);
    
    if (g_ttl.lease.state != LEASE_STATE_ACTIVE) {
        xSemaphoreGive(g_ttl.mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    g_ttl.env_absent_ms = absent_ms;
    g_ttl.env_absent = (absent_ms > 0);
    
    ESP_LOGD(TAG, "环境变化通知: absent=%lu 分钟", absent_ms / 60000);
    
    xSemaphoreGive(g_ttl.mutex);
    
    return ESP_OK;
}

esp_err_t ttl_lease_user_resume(void)
{
    return ttl_lease_release(LEASE_RELEASE_USER_CMD);
}