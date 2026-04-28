/**
 * @file test_model_loader.c
 * @brief 模型加载器单元测试
 */

#include "unity.h"
#include "model_loader.h"
#include "esp_log.h"

static const char *TAG = "TEST_MODEL_LOADER";

/* 加载进度回调 */
static int g_last_progress = 0;
static void test_load_callback(model_type_t type, int progress, void *user_data)
{
    g_last_progress = progress;
    ESP_LOGI(TAG, "模型 %d 加载进度: %d%%", type, progress);
}

/**
 * @brief 测试初始化
 */
TEST_CASE("model_loader_init", "[model]")
{
    esp_err_t ret = model_loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    model_loader_deinit();
}

/**
 * @brief 测试加载单个模型
 */
TEST_CASE("model_loader_load_single", "[model]")
{
    esp_err_t ret = model_loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 加载雷达分析器模型 (最小) */
    ret = model_loader_load(MODEL_TYPE_RADAR_ANALYZER, test_load_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 检查加载状态 */
    TEST_ASSERT_TRUE(model_loader_is_loaded(MODEL_TYPE_RADAR_ANALYZER));
    
    /* 获取模型数据 */
    uint8_t *data = NULL;
    size_t size = 0;
    ret = model_loader_get(MODEL_TYPE_RADAR_ANALYZER, &data, &size);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_GREATER_THAN(0, size);
    
    /* 卸载模型 */
    ret = model_loader_unload(MODEL_TYPE_RADAR_ANALYZER);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_FALSE(model_loader_is_loaded(MODEL_TYPE_RADAR_ANALYZER));
    
    model_loader_deinit();
}

/**
 * @brief 测试加载所有模型
 */
TEST_CASE("model_loader_load_all", "[model]")
{
    esp_err_t ret = model_loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 加载所有模型 */
    ret = model_loader_load(MODEL_TYPE_SOUND_CLASSIFIER, test_load_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = model_loader_load(MODEL_TYPE_RADAR_ANALYZER, test_load_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = model_loader_load(MODEL_TYPE_FUSION_MODEL, test_load_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 检查所有模型已加载 */
    TEST_ASSERT_TRUE(model_loader_is_loaded(MODEL_TYPE_SOUND_CLASSIFIER));
    TEST_ASSERT_TRUE(model_loader_is_loaded(MODEL_TYPE_RADAR_ANALYZER));
    TEST_ASSERT_TRUE(model_loader_is_loaded(MODEL_TYPE_FUSION_MODEL));
    
    /* 检查总内存占用 */
    size_t total_mem = model_loader_get_total_memory();
    ESP_LOGI(TAG, "总内存占用: %zu 字节", total_mem);
    TEST_ASSERT_GREATER_THAN(0, total_mem);
    
    /* 打印状态 */
    model_loader_print_status();
    
    model_loader_deinit();
}

/**
 * @brief 测试模型信息获取
 */
TEST_CASE("model_loader_get_info", "[model]")
{
    esp_err_t ret = model_loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 获取模型信息 */
    const model_info_t *info = model_loader_get_info(MODEL_TYPE_SOUND_CLASSIFIER);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL(MODEL_TYPE_SOUND_CLASSIFIER, info->type);
    TEST_ASSERT_EQUAL_STRING("sound_classifier", info->name);
    
    info = model_loader_get_info(MODEL_TYPE_RADAR_ANALYZER);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL(MODEL_TYPE_RADAR_ANALYZER, info->type);
    
    info = model_loader_get_info(MODEL_TYPE_FUSION_MODEL);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL(MODEL_TYPE_FUSION_MODEL, info->type);
    
    /* 无效类型 */
    info = model_loader_get_info(MODEL_TYPE_MAX);
    TEST_ASSERT_NULL(info);
    
    model_loader_deinit();
}

/**
 * @brief 测试重复加载
 */
TEST_CASE("model_loader_double_load", "[model]")
{
    esp_err_t ret = model_loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 第一次加载 */
    ret = model_loader_load(MODEL_TYPE_RADAR_ANALYZER, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 第二次加载 (应该返回成功但不重新加载) */
    ret = model_loader_load(MODEL_TYPE_RADAR_ANALYZER, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 卸载 */
    ret = model_loader_unload(MODEL_TYPE_RADAR_ANALYZER);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    model_loader_deinit();
}

/**
 * @brief 测试无效参数
 */
TEST_CASE("model_loader_invalid_args", "[model]")
{
    esp_err_t ret = model_loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    /* 无效模型类型 */
    ret = model_loader_load(MODEL_TYPE_MAX, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    ret = model_loader_unload(MODEL_TYPE_MAX);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    /* 无效指针 */
    uint8_t *data = NULL;
    size_t size = 0;
    ret = model_loader_get(MODEL_TYPE_RADAR_ANALYZER, NULL, &size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    ret = model_loader_get(MODEL_TYPE_RADAR_ANALYZER, &data, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    /* 未加载的模型 */
    ret = model_loader_get(MODEL_TYPE_SOUND_CLASSIFIER, &data, &size);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);
    
    model_loader_deinit();
}
