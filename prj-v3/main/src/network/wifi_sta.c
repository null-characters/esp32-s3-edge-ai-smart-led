/**
 * @file wifi_sta.c
 * @brief Wi-Fi STA 模式实现
 */

#include "wifi_sta.h"
#include "status_led.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

static const char *TAG = "WIFI_STA";

/* Wi-Fi 事件位 */
#define WIFI_CONNECTED_BIT     BIT0
#define WIFI_DISCONNECTED_BIT  BIT1

/* 配置 */
#define WIFI_MAX_RETRY         5
#define WIFI_RETRY_DELAY_MS    1000

static EventGroupHandle_t g_wifi_event_group = NULL;
static wifi_sta_state_t g_wifi_state = WIFI_STA_DISCONNECTED;
static uint8_t g_retry_count = 0;
static wifi_connected_callback_t g_connected_callback = NULL;
static wifi_disconnected_callback_t g_disconnected_callback = NULL;
static esp_timer_handle_t g_retry_timer = NULL;
static bool g_initialized = false;
static SemaphoreHandle_t g_wifi_mutex = NULL;

/* ================================================================
 * 定时器重连 (避免在事件回调中阻塞)
 * ================================================================ */

static void retry_timer_callback(void *arg)
{
    if (g_retry_count < WIFI_MAX_RETRY) {
        ESP_LOGI(TAG, "Retry connecting (%d/%d)", g_retry_count + 1, WIFI_MAX_RETRY);
        esp_wifi_connect();
        g_retry_count++;
        
        if (xSemaphoreTake(g_wifi_mutex, 0) == pdTRUE) {
            g_wifi_state = WIFI_STA_CONNECTING;
            xSemaphoreGive(g_wifi_mutex);
        }
        status_led_update_wifi(WIFI_STA_CONNECTING);
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", WIFI_MAX_RETRY);
        if (xSemaphoreTake(g_wifi_mutex, 0) == pdTRUE) {
            g_wifi_state = WIFI_STA_FAILED;
            xSemaphoreGive(g_wifi_mutex);
        }
        status_led_update_wifi(WIFI_STA_FAILED);
        xEventGroupSetBits(g_wifi_event_group, WIFI_DISCONNECTED_BIT);
    }
}

static void start_retry_timer(void)
{
    if (g_retry_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = retry_timer_callback,
            .arg = NULL,
            .name = "wifi_retry_timer",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&timer_args, &g_retry_timer);
    }
    
    /* 启动一次性定时器，延迟重连 */
    esp_timer_start_once(g_retry_timer, WIFI_RETRY_DELAY_MS * 1000);
}

/* ================================================================
 * 事件处理
 * ================================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wi-Fi STA started");
                esp_wifi_connect();
                if (xSemaphoreTake(g_wifi_mutex, 0) == pdTRUE) {
                    g_wifi_state = WIFI_STA_CONNECTING;
                    xSemaphoreGive(g_wifi_mutex);
                }
                status_led_update_wifi(WIFI_STA_CONNECTING);
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Wi-Fi connected to AP");
                g_retry_count = 0;
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "Wi-Fi disconnected");
                if (xSemaphoreTake(g_wifi_mutex, 0) == pdTRUE) {
                    g_wifi_state = WIFI_STA_DISCONNECTED;
                    xSemaphoreGive(g_wifi_mutex);
                }
                status_led_update_wifi(WIFI_STA_DISCONNECTED);
                
                if (g_disconnected_callback) {
                    g_disconnected_callback();
                }
                
                /* 使用定时器重连，避免在事件回调中阻塞 */
                start_retry_timer();
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            if (xSemaphoreTake(g_wifi_mutex, 0) == pdTRUE) {
                g_wifi_state = WIFI_STA_CONNECTED;
                g_retry_count = 0;
                xSemaphoreGive(g_wifi_mutex);
            }
            xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
            status_led_update_wifi(WIFI_STA_CONNECTED);
            
            if (g_connected_callback) {
                g_connected_callback();
            }
        }
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t wifi_sta_init(const char *ssid, const char *password)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Wi-Fi STA already initialized");
        return ESP_OK;
    }
    
    /* 创建互斥锁 */
    g_wifi_mutex = xSemaphoreCreateMutex();
    if (g_wifi_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化网络接口 */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Netif init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event loop create failed: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_netif_create_default_wifi_sta();

    /* 初始化 Wi-Fi */
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 注册事件处理 */
    g_wifi_event_group = xEventGroupCreate();
    if (g_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WIFI event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置 Wi-Fi */
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置WiFi省电模式 (降低功耗) */
    ret = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set WiFi power save mode: %s", esp_err_to_name(ret));
        /* 非致命错误，继续 */
    }

    g_initialized = true;
    ESP_LOGI(TAG, "Wi-Fi STA initialized: ssid=%s", ssid);
    return ESP_OK;
}

esp_err_t wifi_sta_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }
    
    /* 停止定时器 */
    if (g_retry_timer) {
        esp_timer_stop(g_retry_timer);
        esp_timer_delete(g_retry_timer);
        g_retry_timer = NULL;
    }
    
    /* 断开连接 */
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
    }
    
    /* 停止并释放 Wi-Fi */
    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi stop failed: %s", esp_err_to_name(ret));
    }
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi deinit failed: %s", esp_err_to_name(ret));
    }
    
    /* 删除事件组 */
    if (g_wifi_event_group) {
        vEventGroupDelete(g_wifi_event_group);
        g_wifi_event_group = NULL;
    }
    
    /* 删除互斥锁 */
    if (g_wifi_mutex) {
        vSemaphoreDelete(g_wifi_mutex);
        g_wifi_mutex = NULL;
    }
    
    /* 清空回调 */
    g_connected_callback = NULL;
    g_disconnected_callback = NULL;
    
    g_initialized = false;
    g_wifi_state = WIFI_STA_DISCONNECTED;
    g_retry_count = 0;
    
    ESP_LOGI(TAG, "Wi-Fi STA deinitialized");
    return ESP_OK;
}

esp_err_t wifi_sta_connect(uint32_t timeout_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    wifi_sta_state_t state;
    if (xSemaphoreTake(g_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = g_wifi_state;
        xSemaphoreGive(g_wifi_mutex);
    } else {
        return ESP_ERR_TIMEOUT;
    }
    
    if (state == WIFI_STA_CONNECTED) {
        return ESP_OK;
    }

    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
                                            pdTRUE, pdFALSE,
                                            pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else if (bits & WIFI_DISCONNECTED_BIT) {
        return ESP_ERR_WIFI_CONN;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t app_wifi_sta_disconnect(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    wifi_sta_state_t state;
    if (xSemaphoreTake(g_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = g_wifi_state;
        xSemaphoreGive(g_wifi_mutex);
    } else {
        return ESP_ERR_TIMEOUT;
    }
    
    if (state == WIFI_STA_DISCONNECTED) {
        return ESP_OK;
    }

    /* 使用普通错误检查而非 ESP_ERROR_CHECK，避免系统重启 */
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (xSemaphoreTake(g_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_wifi_state = WIFI_STA_DISCONNECTED;
        xSemaphoreGive(g_wifi_mutex);
    }
    
    ESP_LOGI(TAG, "Wi-Fi disconnected");
    return ESP_OK;
}

wifi_sta_state_t wifi_sta_get_state(void)
{
    wifi_sta_state_t state = WIFI_STA_DISCONNECTED;
    if (g_wifi_mutex && xSemaphoreTake(g_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = g_wifi_state;
        xSemaphoreGive(g_wifi_mutex);
    }
    return state;
}

esp_err_t wifi_sta_get_ip(char *ip, size_t ip_size)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    wifi_sta_state_t state = wifi_sta_get_state();
    if (state != WIFI_STA_CONNECTED) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
        return ret;
    }
    
    snprintf(ip, ip_size, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t wifi_sta_register_callback(wifi_connected_callback_t connected_callback, 
                                     wifi_disconnected_callback_t disconnected_callback)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_connected_callback = connected_callback;
    g_disconnected_callback = disconnected_callback;
    return ESP_OK;
}

esp_err_t wifi_sta_scan(wifi_ap_record_t *ap_list, uint16_t max_count, 
                        uint16_t *count)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    /* 使用普通错误检查而非 ESP_ERROR_CHECK */
    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi scan start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_scan_get_ap_records(&max_count, ap_list);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi scan get records failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    *count = max_count;
    ESP_LOGI(TAG, "Wi-Fi scan found %d APs", *count);
    return ESP_OK;
}