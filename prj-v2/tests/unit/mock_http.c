/*
 * Mock HTTP Client - 测试桩实现
 * 替代真实网络HTTP依赖
 */

#include "http_client.h"
#include <string.h>

int http_client_init(void) { return 0; }

int http_get(const char *host, const char *path, const char *port,
	     http_response_t *response)
{
	if (!response || !response->body) return -1;

	/* 模拟wttr.in响应 */
	if (host && strstr(host, "wttr.in")) {
		const char *mock_weather = "Sunny +22°C";
		strncpy(response->body, mock_weather, response->buf_size - 1);
		response->body_len = strlen(mock_weather);
		response->status_code = 200;
		return 0;
	}

	/* 模拟sunrise-sunset.org响应 */
	if (host && strstr(host, "sunrise-sunset")) {
		const char *mock_json =
			"{\"status\":\"OK\",\"results\":{"
			"\"sunrise\":\"2026-04-24T22:30:00+00:00\","
			"\"sunset\":\"2026-04-24T10:30:00+00:00\","
			"\"solar_noon\":\"2026-04-24T04:30:00+00:00\","
			"\"day_length\":\"12:00:00\","
			"\"civil_twilight_begin\":\"2026-04-24T22:05:00+00:00\","
			"\"civil_twilight_end\":\"2026-04-24T10:55:00+00:00\"}}";
		strncpy(response->body, mock_json, response->buf_size - 1);
		response->body_len = strlen(mock_json);
		response->status_code = 200;
		return 0;
	}

	return -1;
}

void http_response_init(http_response_t *response, char *buf, size_t buf_size)
{
	if (!response || !buf) return;
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
	}
}
