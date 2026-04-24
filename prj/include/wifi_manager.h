/*
 * WiFi Manager - WiFi连接管理
 * Phase 2: WF-002 ~ WF-005
 *
 * 功能: WiFi初始化、连接管理、自动重连、状态查询
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

/* WiFi状态 */
typedef enum {
	WIFI_STATE_DISCONNECTED = 0,
	WIFI_STATE_CONNECTING,
	WIFI_STATE_CONNECTED,
	WIFI_STATE_GOT_IP,
	WIFI_STATE_ERROR
} wifi_state_t;

/* 重连参数 */
#define WIFI_RECONNECT_MIN_DELAY_MS   5000    /* 初始重连延迟 5s */
#define WIFI_RECONNECT_MAX_DELAY_MS   60000   /* 最大重连延迟 60s */
#define WIFI_RECONNECT_MAX_RETRIES    0       /* 0=无限重试 */

/* SSID/密码最大长度 */
#define WIFI_SSID_MAX_LEN    32
#define WIFI_PSK_MAX_LEN     64

/**
 * @brief 初始化WiFi管理器 (WF-002)
 * @return 0=成功, 负值=失败
 */
int wifi_manager_init(void);

/**
 * @brief 连接WiFi (WF-003)
 * @param ssid WiFi名称
 * @param psk  WiFi密码
 * @return 0=成功, 负值=失败
 */
int wifi_manager_connect(const char *ssid, const char *psk);

/**
 * @brief 断开WiFi连接
 * @return 0=成功, 负值=失败
 */
int wifi_manager_disconnect(void);

/**
 * @brief 获取当前WiFi状态 (WF-005)
 * @return wifi_state_t
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief 检查WiFi是否已连接并获取IP
 * @return true=已连接且获取到IP
 */
bool wifi_manager_is_connected(void);

/**
 * @brief 获取当前IP地址字符串 (WF-005)
 * @param buf  输出缓冲区
 * @param len  缓冲区长度
 * @return 0=成功, 负值=未连接
 */
int wifi_manager_get_ip(char *buf, size_t len);

/**
 * @brief 获取WiFi信号强度RSSI (WF-005)
 * @return RSSI值(dBm), 负值=失败
 */
int wifi_manager_get_rssi(void);

/**
 * @brief 注册WiFi状态变化回调 (WF-005)
 * @param cb 回调函数
 */
typedef void (*wifi_state_cb_t)(wifi_state_t new_state);
void wifi_manager_register_state_cb(wifi_state_cb_t cb);

/**
 * @brief WiFi管理器主循环 (WF-004: 重连逻辑)
 *        在独立线程中运行,处理断线重连
 */
void wifi_manager_thread_fn(void);

/**
 * @brief 启动WiFi管理线程（显式启动）
 * @return 0=成功
 */
int wifi_manager_start(void);

/**
 * @brief 停止WiFi管理线程
 * @return 0=成功
 */
int wifi_manager_stop(void);

#endif /* WIFI_MANAGER_H */
