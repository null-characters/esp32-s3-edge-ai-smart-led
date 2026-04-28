/**
 * @file model_loader.h
 * @brief TFLM 模型加载器
 * 
 * 支持从 Flash 或 SPIFFS 加载 TFLite 模型
 * 实现分步加载机制，减少 RAM 占用
 */

#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 模型类型
 */
typedef enum {
    MODEL_TYPE_SOUND_CLASSIFIER,   /* 声音分类器 (~30KB) */
    MODEL_TYPE_RADAR_ANALYZER,     /* 雷达分析器 (~2KB) */
    MODEL_TYPE_FUSION_MODEL,       /* 融合决策器 (~3KB) */
    MODEL_TYPE_MAX,
} model_type_t;

/**
 * @brief 模型信息
 */
typedef struct {
    model_type_t type;             /* 模型类型 */
    const char *name;              /* 模型名称 */
    const char *path;              /* 模型路径 (SPIFFS) */
    size_t size;                   /* 模型大小 (字节) */
    bool is_loaded;                /* 是否已加载 */
    uint8_t *data;                 /* 模型数据指针 */
} model_info_t;

/**
 * @brief 模型加载回调
 */
typedef void (*model_load_callback_t)(model_type_t type, int progress, void *user_data);

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化模型加载器
 * @return ESP_OK 成功
 */
esp_err_t model_loader_init(void);

/**
 * @brief 释放模型加载器资源
 */
void model_loader_deinit(void);

/**
 * @brief 加载模型到内存
 * @param type 模型类型
 * @param callback 加载进度回调 (可选)
 * @param user_data 用户数据
 * @return ESP_OK 成功
 */
esp_err_t model_loader_load(model_type_t type, model_load_callback_t callback, void *user_data);

/**
 * @brief 卸载模型 (释放内存)
 * @param type 模型类型
 * @return ESP_OK 成功
 */
esp_err_t model_loader_unload(model_type_t type);

/**
 * @brief 获取模型数据指针
 * @param type 模型类型
 * @param data 输出数据指针
 * @param size 输出数据大小
 * @return ESP_OK 成功
 */
esp_err_t model_loader_get(model_type_t type, uint8_t **data, size_t *size);

/**
 * @brief 检查模型是否已加载
 * @param type 模型类型
 * @return true 已加载
 */
bool model_loader_is_loaded(model_type_t type);

/**
 * @brief 获取模型信息
 * @param type 模型类型
 * @return 模型信息指针
 */
const model_info_t* model_loader_get_info(model_type_t type);

/**
 * @brief 获取所有模型的内存占用
 * @return 总内存占用 (字节)
 */
size_t model_loader_get_total_memory(void);

/**
 * @brief 打印模型加载状态
 */
void model_loader_print_status(void);

#endif /* MODEL_LOADER_H */
