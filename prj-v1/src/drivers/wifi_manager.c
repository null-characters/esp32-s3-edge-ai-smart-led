/*
 * WiFi Manager Implementation - WiFi连接管理
 * Phase 2: WF-002 ~ WF-005
 *
 * 功能: WiFi初始化、STA连接、自动重连、状态查询
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "wifi_manager.h"

LOG_MODULE_REGISTER(wifi_mgr, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static wifi_state_t current_state = WM_WIFI_STATE_DISCONNECTED;
static wifi_state_cb_t state_cb = NULL;

/* 保存连接凭证 (用于自动重连) */
static char stored_ssid[WIFI_SSID_MAX_LEN + 1];
static char stored_psk[WIFI_PSK_MAX_LEN + 1];
static bool has_credentials = false;

/* 重连状态 */
static bool reconnect_enabled = false;
static uint32_t reconnect_delay_ms = WIFI_RECONNECT_MIN_DELAY_MS;
static int reconnect_attempts = 0;

/* 网络管理事件 */
static struct net_mgmt_event_callback mgmt_cb;

/* 网络接口 */
static struct net_if *wifi_iface = NULL;

/* 信号量: 等待连接/断开事件 */
static K_SEM_DEFINE(wifi_connected, 0, 1);
static K_SEM_DEFINE(wifi_got_ip, 0, 1);

/* ================================================================
 * 状态更新
 * ================================================================ */
static void set_state(wifi_state_t new_state)
{
	if (current_state != new_state) {
		LOG_INF("WiFi state: %d → %d", current_state, new_state);
		current_state = new_state;
		if (state_cb) {
			state_cb(new_state);
		}
	}
}

/* ================================================================
 * 网络管理事件回调 (WF-003, WF-004)
 * ================================================================ */
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("WiFi connected event");
		set_state(WM_WIFI_STATE_CONNECTED);
		k_sem_give(&wifi_connected);
		reconnect_attempts = 0;
		reconnect_delay_ms = WIFI_RECONNECT_MIN_DELAY_MS;
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("WiFi disconnected event");
		set_state(WM_WIFI_STATE_DISCONNECTED);
		reconnect_enabled = true;
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		LOG_INF("IPv4 address added");
		set_state(WM_WIFI_STATE_GOT_IP);
		k_sem_give(&wifi_got_ip);
		break;

	case NET_EVENT_IPV4_ADDR_DEL:
		LOG_WRN("IPv4 address removed");
		if (current_state == WM_WIFI_STATE_GOT_IP) {
			set_state(WM_WIFI_STATE_CONNECTED);
		}
		break;

	default:
		break;
	}
}

/* ================================================================
 * WiFi初始化 (WF-002)
 * ================================================================ */
int wifi_manager_init(void)
{
	LOG_INF("Initializing WiFi manager...");

	/* 获取WiFi网络接口 */
	wifi_iface = net_if_get_wifi_sta();
	if (!wifi_iface) {
		LOG_ERR("No WiFi STA interface found");
		return -ENODEV;
	}

	/* 注册网络管理事件回调 */
	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT |
				     NET_EVENT_IPV4_ADDR_ADD |
				     NET_EVENT_IPV4_ADDR_DEL);
	net_mgmt_add_event_callback(&mgmt_cb);

	set_state(WM_WIFI_STATE_DISCONNECTED);
	LOG_INF("WiFi manager initialized");
	return 0;
}

/* ================================================================
 * WiFi连接 (WF-003)
 * ================================================================ */
int wifi_manager_connect(const char *ssid, const char *psk)
{
	if (!ssid || !psk) {
		return -EINVAL;
	}

	/* 保存凭证 (用于重连) */
	strncpy(stored_ssid, ssid, sizeof(stored_ssid) - 1);
	stored_ssid[sizeof(stored_ssid) - 1] = '\0';
	strncpy(stored_psk, psk, sizeof(stored_psk) - 1);
	stored_psk[sizeof(stored_psk) - 1] = '\0';
	has_credentials = true;

	/* 构建连接参数 */
	struct wifi_connect_req_params params = {
		.ssid = stored_ssid,
		.ssid_length = strlen(stored_ssid),
		.psk = stored_psk,
		.psk_length = strlen(stored_psk),
		.security = WIFI_SECURITY_TYPE_PSK,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
	};

	set_state(WM_WIFI_STATE_CONNECTING);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface,
			   &params, sizeof(params));
	if (ret) {
		LOG_ERR("WiFi connect request failed: %d", ret);
		set_state(WM_WIFI_STATE_ERROR);
		return ret;
	}

	LOG_INF("WiFi connecting to SSID: %s...", ssid);

	/* 等待连接成功 (最多30秒) */
	ret = k_sem_take(&wifi_connected, K_SECONDS(30));
	if (ret) {
		LOG_ERR("WiFi connect timeout");
		set_state(WM_WIFI_STATE_ERROR);
		return -ETIMEDOUT;
	}

	/* 等待获取IP (最多15秒) */
	ret = k_sem_take(&wifi_got_ip, K_SECONDS(15));
	if (ret) {
		LOG_WRN("WiFi connected but no IP (DHCP timeout)");
		return -ETIMEDOUT;
	}

	LOG_INF("WiFi connected and got IP");
	reconnect_enabled = true;
	return 0;
}

/* ================================================================
 * WiFi断开
 * ================================================================ */
int wifi_manager_disconnect(void)
{
	reconnect_enabled = false;

	int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
	if (ret) {
		LOG_ERR("WiFi disconnect request failed: %d", ret);
		return ret;
	}

	LOG_INF("WiFi disconnected");
	return 0;
}

/* ================================================================
 * 状态查询 (WF-005)
 * ================================================================ */
wifi_state_t wifi_manager_get_state(void)
{
	return current_state;
}

bool wifi_manager_is_connected(void)
{
	return current_state == WM_WIFI_STATE_GOT_IP;
}

int wifi_manager_get_ip(char *buf, size_t len)
{
	if (!buf || len == 0) {
		return -EINVAL;
	}

	if (!wifi_iface) {
		return -ENODEV;
	}

	/* 获取接口的IPv4地址 */
	struct net_if_addr *ifaddr = net_if_ipv4_get_global_addr(wifi_iface, NET_ADDR_PREFERRED);
	if (ifaddr) {
		net_addr_ntop(AF_INET, &ifaddr->address.in_addr, buf, len);
		return 0;
	}

	/* 尝试获取链接本地地址 */
	ARRAY_FOR_EACH(wifi_iface->config.ip.ipv4->unicast, i) {
		struct net_if_addr *addr = &wifi_iface->config.ip.ipv4->unicast[i];
		if (addr->is_used && addr->address.family == AF_INET) {
			net_addr_ntop(AF_INET, &addr->address.in_addr, buf, len);
			return 0;
		}
	}

	return -ENOENT;
}

int wifi_manager_get_rssi(void)
{
	/* 使用WiFi管理请求获取信号强度 */
	struct wifi_iface_status status = {0};

	int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, wifi_iface,
			   &status, sizeof(status));
	if (ret) {
		LOG_WRN("Failed to get WiFi status: %d", ret);
		return ret;
	}

	return status.rssi;
}

/* ================================================================
 * 回调注册 (WF-005)
 * ================================================================ */
void wifi_manager_register_state_cb(wifi_state_cb_t cb)
{
	state_cb = cb;
}

/* ================================================================
 * 自动重连线程 (WF-004)
 * 指数退避重试: 5s → 10s → 20s → 40s → 60s (最大)
 * ================================================================ */
void wifi_manager_thread_fn(void)
{
	LOG_INF("WiFi manager thread started");

	while (1) {
		/* 检查是否需要重连 */
		if (reconnect_enabled && has_credentials &&
		    current_state == WM_WIFI_STATE_DISCONNECTED) {

			reconnect_attempts++;
			LOG_INF("Reconnect attempt %d (delay %dms)",
				reconnect_attempts, reconnect_delay_ms);

			k_sleep(K_MSEC(reconnect_delay_ms));

			/* 尝试重连 */
			int ret = wifi_manager_connect(stored_ssid, stored_psk);
			if (ret == 0) {
				LOG_INF("Reconnect succeeded");
			} else {
				LOG_WRN("Reconnect failed: %d", ret);
				/* 指数退避 */
				reconnect_delay_ms *= 2;
				if (reconnect_delay_ms > WIFI_RECONNECT_MAX_DELAY_MS) {
					reconnect_delay_ms = WIFI_RECONNECT_MAX_DELAY_MS;
				}
			}
		}

		k_sleep(K_SECONDS(2));
	}
}

/* 线程控制（显式启动） */
static K_THREAD_STACK_DEFINE(wifi_mgr_stack, 4096);
static struct k_thread wifi_mgr_thread;
static k_tid_t wifi_mgr_tid;

int wifi_manager_start(void)
{
	if (wifi_mgr_tid) {
		return 0;
	}

	wifi_mgr_tid = k_thread_create(&wifi_mgr_thread, wifi_mgr_stack,
				       K_THREAD_STACK_SIZEOF(wifi_mgr_stack),
				       (k_thread_entry_t)wifi_manager_thread_fn,
				       NULL, NULL, NULL,
				       7, 0, K_FOREVER);
	k_thread_name_set(wifi_mgr_tid, "wifi_mgr");
	k_thread_start(wifi_mgr_tid);
	return 0;
}

int wifi_manager_stop(void)
{
	if (!wifi_mgr_tid) {
		return 0;
	}
	k_thread_abort(wifi_mgr_tid);
	wifi_mgr_tid = NULL;
	return 0;
}
