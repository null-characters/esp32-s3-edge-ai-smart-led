/**
 * @file model_loader.c
 * @brief TFLM 模型加载器实现
 */

#include "model_loader.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MODEL_LOADER";

/* ================================================================
 * 模型配置表
 * ================================================================ */

/* 模型文件路径 */
#define MODEL_PATH_SOUND    "/spiffs/models/sound_classifier.tflite"
#define MODEL_PATH_RADAR    "/spiffs/models/radar_analyzer.tflite"
#define MODEL_PATH_FUSION   "/spiffs/models/fusion_model.tflite"

/* 模型信息表 */
static model_info_t g_models[MODEL_TYPE_MAX] = {
    [MODEL_TYPE_SOUND_CLASSIFIER] = {
        .type = MODEL_TYPE_SOUND_CLASSIFIER,
        .name = "sound_classifier",
        .path = MODEL_PATH_SOUND,
        .size = 30 * 1024,  /* ~30KB */
        .is_loaded = false,
        .data = NULL,
    },
    [MODEL_TYPE_RADAR_ANALYZER] = {
        .type = MODEL_TYPE_RADAR_ANALYZER,
        .name = "radar_analyzer",
        .path = MODEL_PATH_RADAR,
        .size = 2 * 1024,   /* ~2KB */
        .is_loaded = false,
        .data = NULL,
    },
    [MODEL_TYPE_FUSION_MODEL] = {
        .type = MODEL_TYPE_FUSION_MODEL,
        .name = "fusion_model",
        .path = MODEL_PATH_FUSION,
        .size = 3 * 1024,   /* ~3KB */
        .is_loaded = false,
        .data = NULL,
    },
};

/* SPIFFS 是否已挂载 */
static bool g_spiffs_mounted = false;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 挂载 SPIFFS 文件系统
 */
static esp_err_t mount_spiffs(void)
{
    if (g_spiffs_mounted) {
        return ESP_OK;
    }
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "挂载 SPIFFS 失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    g_spiffs_mounted = true;
    ESP_LOGI(TAG, "SPIFFS 已挂载");
    return ESP_OK;
}

/**
 * @brief 从 SPIFFS 加载模型文件
 */
static esp_err_t load_model_from_spiffs(model_info_t *model, model_load_callback_t callback, void *user_data)
{
    FILE *fp = fopen(model->path, "rb");
    if (!fp) {
        ESP_LOGW(TAG, "模型文件不存在: %s (使用占位符)", model->path);
        
        /* 使用占位符数据 (全零) */
        model->data = heap_caps_malloc(model->size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
        if (!model->data) {
            ESP_LOGE(TAG, "分配模型内存失败: %zu 字节", model->size);
            return ESP_ERR_NO_MEM;
        }
        memset(model->data, 0, model->size);
        model->is_loaded = true;
        
        ESP_LOGI(TAG, "使用占位符模型: %s (%zu 字节)", model->name, model->size);
        return ESP_OK;
    }
    
    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* 分配内存 */
    model->data = heap_caps_malloc(file_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (!model->data) {
        fclose(fp);
        ESP_LOGE(TAG, "分配模型内存失败: %zu 字节", file_size);
        return ESP_ERR_NO_MEM;
    }
    
    /* 分块读取文件 */
    size_t chunk_size = 4096;
    size_t bytes_read = 0;
    int progress = 0;
    
    while (bytes_read < file_size) {
        size_t to_read = (file_size - bytes_read > chunk_size) ? chunk_size : (file_size - bytes_read);
        size_t n = fread(model->data + bytes_read, 1, to_read, fp);
        if (n == 0) break;
        bytes_read += n;
        
        /* 更新进度 */
        int new_progress = (bytes_read * 100) / file_size;
        if (callback && new_progress != progress) {
            progress = new_progress;
            callback(model->type, progress, user_data);
        }
    }
    
    fclose(fp);
    
    model->size = bytes_read;
    model->is_loaded = true;
    
    ESP_LOGI(TAG, "模型加载成功: %s (%zu 字节)", model->name, model->size);
    return ESP_OK;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t model_loader_init(void)
{
    ESP_LOGI(TAG, "初始化模型加载器");
    
    /* 挂载 SPIFFS */
    esp_err_t ret = mount_spiffs();
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* 打印模型信息 */
    for (int i = 0; i < MODEL_TYPE_MAX; i++) {
        ESP_LOGI(TAG, "  [%d] %s: %zu 字节, 路径: %s",
                 i, g_models[i].name, g_models[i].size, g_models[i].path);
    }
    
    return ESP_OK;
}

void model_loader_deinit(void)
{
    /* 卸载所有模型 */
    for (int i = 0; i < MODEL_TYPE_MAX; i++) {
        if (g_models[i].data) {
            heap_caps_free(g_models[i].data);
            g_models[i].data = NULL;
            g_models[i].is_loaded = false;
        }
    }
    
    /* 卸载 SPIFFS */
    if (g_spiffs_mounted) {
        esp_vfs_spiffs_unregister(NULL);
        g_spiffs_mounted = false;
    }
    
    ESP_LOGI(TAG, "模型加载器已释放");
}

esp_err_t model_loader_load(model_type_t type, model_load_callback_t callback, void *user_data)
{
    if (type >= MODEL_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    model_info_t *model = &g_models[type];
    
    if (model->is_loaded) {
        ESP_LOGW(TAG, "模型已加载: %s", model->name);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "加载模型: %s", model->name);
    
    return load_model_from_spiffs(model, callback, user_data);
}

esp_err_t model_loader_unload(model_type_t type)
{
    if (type >= MODEL_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    model_info_t *model = &g_models[type];
    
    if (!model->is_loaded) {
        return ESP_OK;
    }
    
    if (model->data) {
        heap_caps_free(model->data);
        model->data = NULL;
    }
    
    model->is_loaded = false;
    ESP_LOGI(TAG, "模型已卸载: %s", model->name);
    
    return ESP_OK;
}

esp_err_t model_loader_get(model_type_t type, uint8_t **data, size_t *size)
{
    if (type >= MODEL_TYPE_MAX || !data || !size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    model_info_t *model = &g_models[type];
    
    if (!model->is_loaded) {
        return ESP_ERR_NOT_FOUND;
    }
    
    *data = model->data;
    *size = model->size;
    
    return ESP_OK;
}

bool model_loader_is_loaded(model_type_t type)
{
    if (type >= MODEL_TYPE_MAX) {
        return false;
    }
    return g_models[type].is_loaded;
}

const model_info_t* model_loader_get_info(model_type_t type)
{
    if (type >= MODEL_TYPE_MAX) {
        return NULL;
    }
    return &g_models[type];
}

size_t model_loader_get_total_memory(void)
{
    size_t total = 0;
    for (int i = 0; i < MODEL_TYPE_MAX; i++) {
        if (g_models[i].is_loaded && g_models[i].data) {
            total += g_models[i].size;
        }
    }
    return total;
}

void model_loader_print_status(void)
{
    ESP_LOGI(TAG, "=== 模型加载状态 ===");
    for (int i = 0; i < MODEL_TYPE_MAX; i++) {
        ESP_LOGI(TAG, "  %s: %s (%zu 字节)",
                 g_models[i].name,
                 g_models[i].is_loaded ? "已加载" : "未加载",
                 g_models[i].size);
    }
    ESP_LOGI(TAG, "总内存占用: %zu 字节", model_loader_get_total_memory());
}
