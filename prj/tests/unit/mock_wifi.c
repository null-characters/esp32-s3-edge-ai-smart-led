/*
 * Mock WiFi Manager - 测试桩实现
 * 替代真实WiFi硬件依赖
 */

#include <zephyr/kernel.h>
#include <stdbool.h>
#include "wifi_manager.h"

static wifi_state_t mock_state = WIFI_STATE_DISCONNECTED;
static wifi_state_cb_t mock_cb = NULL;

int wifi_manager_init(void) { return 0; }
int wifi_manager_connect(const char *ssid, const char *psk)
{
	mock_state = WIFI_STATE_GOT_IP;
	if (mock_cb) mock_cb(mock_state);
	return 0;
}
int wifi_manager_disconnect(void)
{
	mock_state = WIFI_STATE_DISCONNECTED;
	if (mock_cb) mock_cb(mock_state);
	return 0;
}
wifi_state_t wifi_manager_get_state(void) { return mock_state; }
bool wifi_manager_is_connected(void) { return mock_state == WIFI_STATE_GOT_IP; }
int wifi_manager_get_ip(char *buf, size_t len)
{
	if (buf && len > 10) { strncpy(buf, "192.168.1.1", len); return 0; }
	return -1;
}
int wifi_manager_get_rssi(void) { return -50; }
void wifi_manager_register_state_cb(wifi_state_cb_t cb) { mock_cb = cb; }
void wifi_manager_thread_fn(void) { /* no-op in test */ }
