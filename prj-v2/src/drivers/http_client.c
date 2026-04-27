/*
 * HTTP Client Implementation - HTTP请求
 * Phase 2: WF-011 ~ WF-015
 *
 * 功能: HTTP GET、DNS解析、响应缓冲、超时处理
 */
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

#include "http_client.h"

LOG_MODULE_REGISTER(http_cli, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * 内部状态
 * ================================================================ */
static bool initialized = false;

/* ================================================================
 * HTTP客户端初始化 (WF-011)
 * ================================================================ */
int http_client_init(void)
{
	LOG_INF("HTTP client initialized");
	initialized = true;
	return 0;
}

/* ================================================================
 * 响应结构体操作 (WF-014)
 * ================================================================ */
void http_response_init(http_response_t *response, char *buf, size_t buf_size)
{
	if (!response || !buf) {
		return;
	}
	response->body = buf;
	response->body_len = 0;
	response->buf_size = buf_size;
	response->status_code = 0;
	memset(buf, 0, buf_size);
}

void http_response_cleanup(http_response_t *response)
{
	if (response) {
		response->body = NULL;
		response->body_len = 0;
		response->buf_size = 0;
		response->status_code = 0;
	}
}

/* ================================================================
 * HTTP回调: 接收响应数据
 * ================================================================ */
static int http_response_cb(int sock, struct http_response *resp,
			    enum http_final_call final_data, void *user_data)
{
	http_response_t *response = (http_response_t *)user_data;

	if (!response || !response->body) {
		return -EINVAL;
	}

	response->status_code = resp->http_status_code;

	/* 追加响应体数据 */
	size_t avail = response->buf_size - response->body_len - 1;
	size_t to_copy = resp->data_len < avail ? resp->data_len : avail;

	if (to_copy > 0) {
		memcpy(response->body + response->body_len, resp->recv_buf, to_copy);
		response->body_len += to_copy;
		response->body[response->body_len] = '\0';
	}

	/* 检查缓冲区溢出 (WF-014) */
	if (resp->data_len > avail) {
		LOG_WRN("HTTP response truncated: %zu bytes dropped",
			resp->data_len - avail);
	}

	return 0;
}

/* ================================================================
 * HTTP GET实现 (WF-012, WF-013, WF-015)
 * ================================================================ */
int http_get(const char *host, const char *path, const char *port,
	     http_response_t *response)
{
	if (!host || !path || !port || !response) {
		return -EINVAL;
	}

	int sock = -1;
	int ret;

	/* DNS解析 (WF-012) */
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res = NULL;

	LOG_INF("DNS resolving: %s", host);

	ret = zsock_getaddrinfo(host, port, &hints, &res);
	if (ret != 0) {
		LOG_ERR("DNS resolve failed for %s: %d", host, ret);
		return -EHOSTUNREACH;
	}

	/* 创建socket */
	sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
		LOG_ERR("Socket creation failed: %d", -errno);
		zsock_freeaddrinfo(res);
		return -ENETDOWN;
	}

	/* 设置超时 (WF-015) */
	struct zsock_timeval timeout = {
		.tv_sec = HTTP_REQUEST_TIMEOUT_S,
		.tv_usec = 0,
	};
	zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	zsock_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	/* 连接服务器 */
	ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
	zsock_freeaddrinfo(res);

	if (ret < 0) {
		LOG_ERR("Connect to %s:%s failed: %d", host, port, -errno);
		zsock_close(sock);
		return -ECONNREFUSED;
	}

	LOG_INF("Connected to %s:%s", host, port);

	/* 构建HTTP GET请求 */
	struct http_request req = {
		.method = HTTP_GET,
		.url = path,
		.host = host,
		.protocol = "HTTP/1.1",
		.response = http_response_cb,
		.recv_buf = response->body + response->body_len,
		.recv_buf_len = response->buf_size - response->body_len,
	};

	/* 发送请求并接收响应 */
	ret = http_client_req(sock, &req, 5 * MSEC_PER_SEC, response);
	if (ret < 0) {
		LOG_ERR("HTTP request failed: %d", ret);
		zsock_close(sock);
		return ret;
	}

	zsock_close(sock);

	LOG_INF("HTTP GET %s%s → status %d, body %zu bytes",
		host, path, response->status_code, response->body_len);

	return (response->status_code == HTTP_STATUS_OK) ? 0 : -EBADMSG;
}
