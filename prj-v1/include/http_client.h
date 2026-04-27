/*
 * HTTP Client - HTTP请求模块
 * Phase 2: WF-011 ~ WF-015
 *
 * 功能: HTTP GET请求、DNS解析、响应缓冲、超时处理
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdint.h>
#include <stddef.h>

/* HTTP响应缓冲区大小 */
#define HTTP_RESPONSE_BUF_SIZE  4096

/* HTTP超时 */
#define HTTP_REQUEST_TIMEOUT_S  5

/* HTTP状态码 */
#define HTTP_STATUS_OK          200

/* HTTP响应结构体 */
typedef struct {
	int status_code;                   /* HTTP状态码 */
	char *body;                        /* 响应体 (动态分配或外部缓冲区) */
	size_t body_len;                   /* 响应体长度 */
	size_t buf_size;                   /* 缓冲区总大小 */
} http_response_t;

/**
 * @brief 初始化HTTP客户端 (WF-011)
 * @return 0=成功, 负值=失败
 */
int http_client_init(void);

/**
 * @brief 执行HTTP GET请求 (WF-013)
 * @param host     主机名 (如 "wttr.in")
 * @param path     路径 (如 "/Beijing?format=3")
 * @param port     端口 ("80" 或 "443")
 * @param response 输出响应结构体 (调用者分配body缓冲区)
 * @return 0=成功, 负值=失败
 */
int http_get(const char *host, const char *path, const char *port,
	     http_response_t *response);

/**
 * @brief 初始化HTTP响应结构体 (WF-014)
 * @param response 响应结构体
 * @param buf      外部提供的缓冲区 (至少HTTP_RESPONSE_BUF_SIZE)
 * @param buf_size 缓冲区大小
 */
void http_response_init(http_response_t *response, char *buf, size_t buf_size);

/**
 * @brief 清理HTTP响应
 * @param response 响应结构体
 */
void http_response_cleanup(http_response_t *response);

#endif /* HTTP_CLIENT_H */
