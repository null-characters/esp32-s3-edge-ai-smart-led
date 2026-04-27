#pragma once

#include <stdint.h>
#include "esp_err.h"

#define TFLM_ARENA_SIZE (100 * 1024)  // 100KB

/**
 * @brief 初始化 TFLM 内存分配器
 * 
 * 在 PSRAM 中分配 Arena 内存
 * 
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t tflm_allocator_init(void);

/**
 * @brief 获取 TFLM Arena 内存指针
 * 
 * @return uint8_t* Arena 内存起始地址
 */
uint8_t* tflm_get_arena(void);

/**
 * @brief 获取 TFLM Arena 内存大小
 * 
 * @return size_t Arena 大小（字节）
 */
size_t tflm_get_arena_size(void);
