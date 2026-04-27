#include "tflm_allocator.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "tflm_allocator";

// 在 PSRAM 中分配 Arena（需要 PSRAM 支持）
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
static uint8_t *tflm_arena = NULL;
#else
// 如果没有 PSRAM，使用内部 RAM（需要注意内存限制）
static uint8_t tflm_arena[TFLM_ARENA_SIZE] __attribute__((aligned(16)));
#endif

static bool is_initialized = false;

esp_err_t tflm_allocator_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "TFLM allocator already initialized");
        return ESP_OK;
    }

#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
    // 从 PSRAM 分配内存
    tflm_arena = (uint8_t *)heap_caps_malloc(TFLM_ARENA_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tflm_arena == NULL) {
        ESP_LOGE(TAG, "Failed to allocate TFLM arena from PSRAM");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "TFLM arena allocated from PSRAM: %d bytes", TFLM_ARENA_SIZE);
#else
    ESP_LOGI(TAG, "TFLM arena using internal RAM: %d bytes", TFLM_ARENA_SIZE);
#endif

    // 清零 Arena 内存
    memset(tflm_arena, 0, TFLM_ARENA_SIZE);

    is_initialized = true;
    ESP_LOGI(TAG, "TFLM allocator initialized successfully");
    
    return ESP_OK;
}

uint8_t* tflm_get_arena(void)
{
    if (!is_initialized) {
        ESP_LOGW(TAG, "TFLM allocator not initialized, returning NULL");
        return NULL;
    }
    return tflm_arena;
}

size_t tflm_get_arena_size(void)
{
    return TFLM_ARENA_SIZE;
}
