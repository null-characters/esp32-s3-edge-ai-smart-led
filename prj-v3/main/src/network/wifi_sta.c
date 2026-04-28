/**
 * @file wifi_sta.c
 * @brief Wi-Fi STA 模式实现
 */

#include "wifi_sta.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

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
static wifi_connected_cb_t g_connected_cb = NULL;
static wifi_disconnected_cb_t g_disconnected_cb = NULL;
static char g_ssid[32] = {0};
static char g_password[64] = {0};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wi-Fi STA started");
                esp_wifi_connect();
                g_wifi_state = WIFI_STA_CONNECTING;
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Wi-Fi connected to AP");
                g_retry_count = 0;
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "Wi-Fi disconnected");
                g_wifi_state = WIFI_STA_DISCONNECTED;
                
                if (g_disconnected_cb) {
                    g_disconnected_cb();
                }
                
                if (g_retry_count < WIFI_MAX_RETRY) {
                    ESP_LOGI(TAG, "Retry connecting (%d/%d)", g_retry_count + 1, WIFI_MAX_RETRY);
                    vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
                    esp_wifi_connect();
                    g_retry_count++;
                    g_wifi_state = WIFI_STA_CONNECTING;
                } else {
                    ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", WIFI_MAX_RETRY);
                    g_wifi_state = WIFI_STA_FAILED;
                    xEventGroupSetBits(g_wifi_event_group, WIFI_DISCONNECTED_BIT);
                }
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            g_wifi_state = WIFI_STA_CONNECTED;
            g_retry_count = 0;
            xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
            
            if (g_connected_cb) {
                g_connected_cb();
            }
        }
    }
}

esp_err_t my_wifi_sta_init(const char *ssid, const char *password)
{
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 初始化 Wi-Fi
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    // 注册事件处理
    g_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                         &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                         &wifi_event_handler, NULL, NULL));

    // 配置 Wi-Fi
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 配置WiFi省电模式 (降低功耗) */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));

    // 保存配置
    strncpy(g_ssid, ssid, sizeof(g_ssid) - 1);
    strncpy(g_password, password, sizeof(g_password) - 1);

    ESP_LOGI(TAG, "Wi-Fi STA initialized: ssid=%s", ssid);
    return ESP_OK;
}

esp_err_t my_wifi_sta_connect(uint32_t timeout_ms)
{
    if (g_wifi_state == WIFI_STA_CONNECTED) {
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

esp_err_t my_wifi_sta_disconnect(void)
{
    if (g_wifi_state == WIFI_STA_DISCONNECTED) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    g_wifi_state = WIFI_STA_DISCONNECTED;
    
    ESP_LOGI(TAG, "Wi-Fi disconnected");
    return ESP_OK;
}

wifi_sta_state_t my_wifi_sta_get_state(void)
{
    return g_wifi_state;
}

esp_err_t my_wifi_sta_get_ip(char *ip, size_t ip_size)
{
    if (g_wifi_state != WIFI_STA_CONNECTED) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
    
    snprintf(ip, ip_size, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t my_wifi_sta_register_cb(wifi_connected_cb_t connected_cb, 
                                wifi_disconnected_cb_t disconnected_cb)
{
    g_connected_cb = connected_cb;
    g_disconnected_cb = disconnected_cb;
    return ESP_OK;
}

esp_err_t my_wifi_sta_scan(wifi_ap_record_t *ap_list, uint16_t max_count, 
                         uint16_t *count)
{
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_cfg, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_count, ap_list));
    *count = max_count;

    ESP_LOGI(TAG, "Wi-Fi scan found %d APs", *count);
    return ESP_OK;
}