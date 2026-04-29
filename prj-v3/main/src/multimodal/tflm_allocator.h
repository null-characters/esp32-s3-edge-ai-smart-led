#pragma once

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief TFLM Arena 大小配置
 * @note 可通过 menuconfig 配置，默认 100KB
 *       sound_classifier CNN 模型可能需要更大 Arena
 */
#ifndef CONFIG_TFLM_ARENA_SIZE
#define TFLM_ARENA_SIZE (100 * 1024)  /* 默认 100KB */
#else
#define TFLM_ARENA_SIZE CONFIG_TFLM_ARENA_SIZE
#endif

/**
 * @brief 初始化 TFLM 内存分配器
 * 
 * 在 PSRAM 中分配 Arena 内存
 * 
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t tflm_allocator_init(void);

/**
 * @brief 释放 TFLM Arena 内存
 * 
 * 释放 PSRAM 分配的内存，用于资源清理或重启场景
 */
void tflm_allocator_deinit(void);

/**
 * @brief 获取 TFLM Arena 内存指针
 * 
 * @return uint8_t* Arena 内存起始地址，未初始化返回 NULL
 */
uint8_t* tflm_get_arena(void);

/**
 * @brief 获取 TFLM Arena 内存大小
 * 
 * @return size_t Arena 大小（字节）
 */
size_t tflm_get_arena_size(void);

/**
 * @brief 获取 Arena 已使用内存大小
 * 
 * @return size_t 已使用大小（字节），未初始化返回 0
 * @note 需要在推理后调用，返回 MicroInterpreter::GetArenaUsedBytes()
 */
size_t tflm_get_arena_used(void);

/**
 * @brief 设置 Arena 已使用内存大小
 * 
 * @param used 已使用大小（字节）
 * @note 由 TFLM 推理引擎调用更新
 */
void tflm_set_arena_used(size_t used);
