/**
 * @file wifi_sta.h
 * @brief Wi-Fi STA 模式接口
 */

#ifndef WIFI_STA_H
#define WIFI_STA_H

#include "esp_err.h"
#include "esp_wifi.h"

/**
 * @brief Wi-Fi 连接状态
 */
typedef enum {
    WIFI_STA_DISCONNECTED,  ///< 未连接
    WIFI_STA_CONNECTING,    ///< 正在连接
    WIFI_STA_CONNECTED,     ///< 已连接
    WIFI_STA_FAILED,        ///< 连接失败
} wifi_sta_state_t;

/**
 * @brief Wi-Fi STA 配置参数 (应用层)
 */
typedef struct {
    const char *ssid;       ///< SSID
    const char *password;   ///< 密码
    bool auto_reconnect;    ///< 自动重连
    uint8_t max_retry;      ///< 最大重试次数
} wifi_sta_app_config_t;

/**
 * @brief Wi-Fi 连接回调函数类型
 */
typedef void (*wifi_connected_cb_t)(void);
typedef void (*wifi_disconnected_cb_t)(void);

/**
 * @brief 初始化 Wi-Fi STA 模式
 * 
 * @param ssid SSID
 * @param password 密码
 * @return ESP_OK 成功
 */
esp_err_t my_wifi_sta_init(const char *ssid, const char *password);

/**
 * @brief 连接 Wi-Fi
 * 
 * @param timeout_ms 超时时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t my_wifi_sta_connect(uint32_t timeout_ms);

/**
 * @brief 断开 Wi-Fi
 * 
 * @return ESP_OK 成功
 */
esp_err_t my_wifi_sta_disconnect(void);

/**
 * @brief 获取 Wi-Fi 连接状态
 * 
 * @return 连接状态
 */
wifi_sta_state_t my_wifi_sta_get_state(void);

/**
 * @brief 获取 IP 地址
 * 
 * @param ip IP 地址字符串 (输出)
 * @param ip_size IP 缓冲区大小
 * @return ESP_OK 成功
 */
esp_err_t my_wifi_sta_get_ip(char *ip, size_t ip_size);

/**
 * @brief 注册连接回调函数
 * 
 * @param connected_cb 连接成功回调
 * @param disconnected_cb 断开连接回调
 * @return ESP_OK 成功
 */
esp_err_t my_wifi_sta_register_cb(wifi_connected_cb_t connected_cb, 
                                wifi_disconnected_cb_t disconnected_cb);

/**
 * @brief 扫描 Wi-Fi 网络
 * 
 * @param ap_list AP 信息数组 (输出)
 * @param max_count 最大扫描数量
 * @param count 实际扫描数量 (输出)
 * @return ESP_OK 成功
 */
esp_err_t my_wifi_sta_scan(wifi_ap_record_t *ap_list, uint16_t max_count, 
                         uint16_t *count);

#endif /* WIFI_STA_H */