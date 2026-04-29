/**
 * @file ttl_lease.h
 * @brief TTL 租约计时器
 * 
 * 防止语音锁死：语音干预带有效期，过期自动释放
 * 支持三种释放条件：TTL 过期、环境变化、用户手动恢复
 */

#ifndef TTL_LEASE_H
#define TTL_LEASE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "Lighting_Event.h"

/* ================================================================
 * 配置常量
 * ================================================================ */

/** 默认租约时长 (2 小时) */
#define TTL_DEFAULT_MS       (2 * 60 * 60 * 1000)

/** 环境离开释放阈值 (10 分钟) */
#define TTL_ENV_EXIT_MS      (10 * 60 * 1000)

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 租约释放原因
 */
typedef enum {
    LEASE_RELEASE_NONE = 0,         /* 未释放 */
    LEASE_RELEASE_TTL_EXPIRED = 1,  /* TTL 过期 */
    LEASE_RELEASE_ENV_CHANGE = 2,   /* 环境变化 (人离开) */
    LEASE_RELEASE_USER_CMD = 3,     /* 用户说"恢复自动" */
    LEASE_RELEASE_FORCE = 4,        /* 强制释放 */
} lease_release_reason_t;

/**
 * @brief 租约状态
 */
typedef enum {
    LEASE_STATE_INACTIVE = 0,       /* 未激活 */
    LEASE_STATE_ACTIVE = 1,          /* 激活中 (语音锁持有) */
    LEASE_STATE_EXPIRED = 2,         /* 已过期 */
    LEASE_STATE_RELEASED = 3,       /* 已释放 */
} lease_state_t;

/**
 * @brief 租约释放回调
 * @param reason 释放原因
 * @param user_data 用户数据
 */
typedef void (*lease_release_callback_t)(lease_release_reason_t reason, void *user_data);

/**
 * @brief TTL 租约结构 (opaque 类型)
 * @note 内部实现细节已隐藏，用户只能通过 API 操作
 */
typedef struct ttl_lease_s ttl_lease_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 TTL 租约模块
 * @return ESP_OK 成功
 */
esp_err_t ttl_lease_init(void);

/**
 * @brief 释放 TTL 租约模块资源
 */
void ttl_lease_deinit(void);

/**
 * @brief 获取租约 (激活语音锁)
 * @param ttl_ms 租约时长 (毫秒), 0 使用默认值 TTL_DEFAULT_MS
 * @return ESP_OK 成功, ESP_ERR_INVALID_STATE 已有活跃租约
 */
esp_err_t ttl_lease_acquire(uint32_t ttl_ms);

/**
 * @brief 续约租约 (刷新 TTL 计时)
 * @param ttl_ms 新的租约时长, 0 保持原时长
 * @return ESP_OK 成功
 */
esp_err_t ttl_lease_renew(uint32_t ttl_ms);

/**
 * @brief 释放租约
 * @param reason 释放原因
 * @return ESP_OK 成功
 */
esp_err_t ttl_lease_release(lease_release_reason_t reason);

/**
 * @brief 检查租约是否过期 (需周期性调用)
 * @return true 租约已过期并已自动释放
 */
bool ttl_lease_check_expired(void);

/**
 * @brief 获取租约状态
 * @return 当前租约状态
 */
lease_state_t ttl_lease_get_state(void);

/**
 * @brief 获取租约剩余时间
 * @return 剩余毫秒数, -1 表示无活跃租约
 */
int64_t ttl_lease_get_remaining_ms(void);

/**
 * @brief 获取租约释放原因
 * @return 释放原因
 */
lease_release_reason_t ttl_lease_get_release_reason(void);

/**
 * @brief 设置租约释放回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void ttl_lease_set_callback(lease_release_callback_t callback, void *user_data);

/**
 * @brief 环境变化通知 (人离开等)
 * @param absent_ms 离开时长 (毫秒)
 * @return ESP_OK 成功, ESP_ERR_INVALID_STATE 租约未激活
 */
esp_err_t ttl_lease_notify_env_change(uint32_t absent_ms);

/**
 * @brief 用户恢复自动命令
 * @return ESP_OK 成功
 */
esp_err_t ttl_lease_user_resume(void);

#endif /* TTL_LEASE_H */
