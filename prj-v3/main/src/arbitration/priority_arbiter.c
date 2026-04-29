/**
 * @file priority_arbiter.c
 * @brief 双轨优先级仲裁器实现
 * 
 * 仲裁策略: 语音 > 自动 > 默认
 */

#include "priority_arbiter.h"
#include "status_led.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "ARBITER";

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    arbiter_config_t config;
    
    /* 当前状态 */
    system_mode_t current_mode;
    led_decision_t current_decision;
    
    /* 输入缓冲 */
    voice_input_t voice_input;
    auto_input_t auto_input;
    
    /* 超时管理 */
    int64_t mode_start_time_ms;
    int64_t last_voice_time_ms;
    
    /* 回调 */
    arbiter_state_callback_t callback;
    void *user_data;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} arbiter_state_t;

static arbiter_state_t g_arbiter = {0};

/* ================================================================
 * 默认配置
 * ================================================================ */

const arbiter_config_t arbiter_default_config = {
    .manual_timeout_ms = 30000,  /* 30 秒 */
    .persist_state = true,
    .default_mode = MODE_AUTO,
};

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 获取当前时间戳 (毫秒)
 */
static int64_t get_timestamp_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/**
 * @brief 创建默认决策
 */
static led_decision_t create_default_decision(void)
{
    led_decision_t decision = {
        .power = false,
        .brightness = 50,
        .color_temp = 4000,
        .scene_id = 0,
        .source = DECISION_SOURCE_DEFAULT,
        .timestamp_ms = get_timestamp_ms(),
    };
    return decision;
}

/**
 * @brief 根据语音命令 ID 创建决策
 * @param command_id 命令 ID (对应 voice_commands.h)
 * @return 对应的控制决策
 */
static led_decision_t create_decision_from_command(int command_id)
{
    led_decision_t decision = {
        .source = DECISION_SOURCE_VOICE,
        .timestamp_ms = get_timestamp_ms(),
    };
    
    /* 根据命令 ID 映射到控制参数
     * 命令 ID 定义参考 voice_commands.h
     * 1-10: 开关/亮度控制
     * 11-20: 色温控制
     * 21-30: 场景控制
     */
    switch (command_id) {
        case 1:  /* 开灯 */
            decision.power = true;
            decision.brightness = 50;
            decision.color_temp = 4000;
            break;
        case 2:  /* 关灯 */
            decision.power = false;
            break;
        case 3:  /* 亮度调高 */
            decision.power = true;
            decision.brightness = 80;
            decision.color_temp = 4000;
            break;
        case 4:  /* 亮度调低 */
            decision.power = true;
            decision.brightness = 20;
            decision.color_temp = 4000;
            break;
        case 5:  /* 最亮 */
            decision.power = true;
            decision.brightness = 100;
            decision.color_temp = 4000;
            break;
        case 6:  /* 最暗 */
            decision.power = true;
            decision.brightness = 5;
            decision.color_temp = 4000;
            break;
        case 11:  /* 暖光 */
            decision.power = true;
            decision.brightness = 50;
            decision.color_temp = 2700;
            break;
        case 12:  /* 冷光 */
            decision.power = true;
            decision.brightness = 50;
            decision.color_temp = 6500;
            break;
        case 13:  /* 自然光 */
            decision.power = true;
            decision.brightness = 50;
            decision.color_temp = 4000;
            break;
        case 21:  /* 阅读模式 */
            decision.power = true;
            decision.brightness = 80;
            decision.color_temp = 5000;
            decision.scene_id = 1;
            break;
        case 22:  /* 夜灯模式 */
            decision.power = true;
            decision.brightness = 10;
            decision.color_temp = 2700;
            decision.scene_id = 2;
            break;
        case 23:  /* 放松模式 */
            decision.power = true;
            decision.brightness = 30;
            decision.color_temp = 3000;
            decision.scene_id = 3;
            break;
        default:
            /* 未知命令，使用默认值 */
            decision.power = true;
            decision.brightness = 50;
            decision.color_temp = 4000;
            ESP_LOGW(TAG, "未知命令 ID: %d，使用默认决策", command_id);
            break;
    }
    
    return decision;
}

/**
 * @brief 从 NVS 加载状态
 */
static esp_err_t load_state_from_nvs(system_mode_t *mode, led_decision_t *decision)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("arbiter", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        /* NVS 打开失败，使用默认值 */
        ESP_LOGW(TAG, "NVS 打开失败，使用默认模式");
        *mode = MODE_AUTO;
        return ret;
    }
    
    uint8_t mode_val = 0;
    esp_err_t mode_ret = nvs_get_u8(handle, "mode", &mode_val);
    if (mode_ret == ESP_OK) {
        *mode = (system_mode_t)mode_val;
    } else {
        /* 未找到存储的模式，使用默认值 */
        ESP_LOGW(TAG, "NVS 未找到模式，使用默认值");
        *mode = MODE_AUTO;
    }
    
    nvs_close(handle);
    return ESP_OK;  /* 返回成功，即使读取失败也已有默认值 */
}

/**
 * @brief 保存状态到 NVS
 */
static esp_err_t save_state_to_nvs(system_mode_t mode)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("arbiter", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_u8(handle, "mode", (uint8_t)mode);
    nvs_commit(handle);
    nvs_close(handle);
    
    return ret;
}

/**
 * @brief 触发状态回调
 */
static void trigger_callback(const led_decision_t *decision)
{
    if (g_arbiter.callback) {
        g_arbiter.callback(g_arbiter.current_mode, decision, g_arbiter.user_data);
    }
}

/**
 * @brief 检查并处理超时
 */
static void check_timeout(void)
{
    if (g_arbiter.current_mode != MODE_MANUAL) {
        return;
    }
    
    int64_t elapsed = get_timestamp_ms() - g_arbiter.last_voice_time_ms;
    if (elapsed >= g_arbiter.config.manual_timeout_ms) {
        ESP_LOGI(TAG, "手动模式超时，恢复自动模式");
        g_arbiter.current_mode = MODE_AUTO;
        g_arbiter.current_decision = create_default_decision();
        
        if (g_arbiter.config.persist_state) {
            save_state_to_nvs(MODE_AUTO);
        }
        
        trigger_callback(&g_arbiter.current_decision);
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t priority_arbiter_init(const arbiter_config_t *config)
{
    if (g_arbiter.initialized) {
        ESP_LOGW(TAG, "仲裁器已初始化");
        return ESP_OK;
    }
    
    if (!config) {
        config = &arbiter_default_config;
    }
    
    ESP_LOGI(TAG, "初始化优先级仲裁器");
    
    /* 创建互斥锁 */
    g_arbiter.mutex = xSemaphoreCreateMutex();
    if (!g_arbiter.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 保存配置 */
    memcpy(&g_arbiter.config, config, sizeof(arbiter_config_t));
    
    /* 初始化状态 */
    g_arbiter.current_mode = config->default_mode;
    g_arbiter.current_decision = create_default_decision();
    g_arbiter.mode_start_time_ms = get_timestamp_ms();
    g_arbiter.last_voice_time_ms = 0;
    
    memset(&g_arbiter.voice_input, 0, sizeof(voice_input_t));
    memset(&g_arbiter.auto_input, 0, sizeof(auto_input_t));
    
    /* 从 NVS 加载状态 */
    if (config->persist_state) {
        system_mode_t saved_mode;
        if (load_state_from_nvs(&saved_mode, NULL) == ESP_OK) {
            g_arbiter.current_mode = saved_mode;
            ESP_LOGI(TAG, "从 NVS 恢复模式: %d", saved_mode);
        }
    }
    
    g_arbiter.initialized = true;
    
    ESP_LOGI(TAG, "仲裁器初始化完成: mode=%d, timeout=%lums",
             g_arbiter.current_mode, config->manual_timeout_ms);
    
    return ESP_OK;
}

void priority_arbiter_deinit(void)
{
    if (!g_arbiter.initialized) {
        return;
    }
    
    if (g_arbiter.mutex) {
        vSemaphoreDelete(g_arbiter.mutex);
    }
    
    memset(&g_arbiter, 0, sizeof(arbiter_state_t));
    ESP_LOGI(TAG, "仲裁器已释放");
}

decision_source_t priority_arbiter_decide(const voice_input_t *voice, 
                                          const auto_input_t *auto_input,
                                          led_decision_t *output)
{
    if (!g_arbiter.initialized || !output) {
        return DECISION_SOURCE_NONE;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    
    decision_source_t source = DECISION_SOURCE_DEFAULT;
    
    /* 优先级: 语音 > 自动 > 默认 */
    if (voice && voice->valid && voice->confidence > 0.5f) {
        /* 语音命令优先 */
        *output = voice->decision;
        output->source = DECISION_SOURCE_VOICE;
        source = DECISION_SOURCE_VOICE;
        
        /* 切换到手动模式 */
        if (g_arbiter.current_mode != MODE_MANUAL) {
            g_arbiter.current_mode = MODE_MANUAL;
            ESP_LOGI(TAG, "切换到手动模式 (语音命令)");
        }
        g_arbiter.last_voice_time_ms = get_timestamp_ms();
        
    } else if (auto_input && auto_input->valid && g_arbiter.current_mode == MODE_AUTO) {
        /* 自动控制 */
        *output = auto_input->decision;
        output->source = DECISION_SOURCE_AUTO;
        source = DECISION_SOURCE_AUTO;
        
    } else {
        /* 默认决策 */
        *output = create_default_decision();
        source = DECISION_SOURCE_DEFAULT;
    }
    
    /* 更新当前决策 */
    g_arbiter.current_decision = *output;
    
    /* 检查超时 */
    check_timeout();
    
    xSemaphoreGive(g_arbiter.mutex);
    
    return source;
}

esp_err_t priority_arbiter_submit_voice(int command_id, float confidence)
{
    if (!g_arbiter.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    
    /* 根据命令 ID 创建完整的控制决策 */
    g_arbiter.voice_input.valid = true;
    g_arbiter.voice_input.command_id = command_id;
    g_arbiter.voice_input.confidence = confidence;
    g_arbiter.voice_input.decision = create_decision_from_command(command_id);
    
    /* 切换到手动模式 */
    g_arbiter.current_mode = MODE_MANUAL;
    g_arbiter.last_voice_time_ms = get_timestamp_ms();
    
    /* 更新当前决策 */
    g_arbiter.current_decision = g_arbiter.voice_input.decision;
    
    ESP_LOGI(TAG, "语音命令: id=%d, conf=%.2f, power=%d, brightness=%d, color_temp=%d",
             command_id, confidence,
             g_arbiter.voice_input.decision.power,
             g_arbiter.voice_input.decision.brightness,
             g_arbiter.voice_input.decision.color_temp);
    
    xSemaphoreGive(g_arbiter.mutex);
    
    return ESP_OK;
}

esp_err_t priority_arbiter_submit_auto(uint8_t scene_id, float confidence)
{
    if (!g_arbiter.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    
    /* 仅在自动模式下接受自动控制 */
    if (g_arbiter.current_mode == MODE_AUTO) {
        g_arbiter.auto_input.valid = true;
        g_arbiter.auto_input.scene_id = scene_id;
        g_arbiter.auto_input.tflm_confidence = confidence;
        g_arbiter.auto_input.decision.scene_id = scene_id;
        g_arbiter.auto_input.decision.source = DECISION_SOURCE_AUTO;
        g_arbiter.auto_input.decision.timestamp_ms = get_timestamp_ms();
        
        ESP_LOGD(TAG, "自动控制: scene=%d, conf=%.2f", scene_id, confidence);
    }
    
    xSemaphoreGive(g_arbiter.mutex);
    
    return ESP_OK;
}

system_mode_t priority_arbiter_get_mode(void)
{
    return g_arbiter.current_mode;
}

bool priority_arbiter_get_decision(led_decision_t *output)
{
    if (!g_arbiter.initialized || !output) {
        return false;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    *output = g_arbiter.current_decision;
    xSemaphoreGive(g_arbiter.mutex);
    
    return true;
}

void priority_arbiter_set_callback(arbiter_state_callback_t callback, void *user_data)
{
    g_arbiter.callback = callback;
    g_arbiter.user_data = user_data;
}

esp_err_t priority_arbiter_set_mode(system_mode_t mode)
{
    if (!g_arbiter.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    
    g_arbiter.current_mode = mode;
    g_arbiter.mode_start_time_ms = get_timestamp_ms();
    
    if (mode == MODE_MANUAL) {
        g_arbiter.last_voice_time_ms = get_timestamp_ms();
    }
    
    if (g_arbiter.config.persist_state) {
        save_state_to_nvs(mode);
    }
    
    /* 更新状态指示灯 */
    status_led_update_mode(mode);
    
    ESP_LOGI(TAG, "模式切换: %d", mode);
    
    xSemaphoreGive(g_arbiter.mutex);
    
    return ESP_OK;
}

void priority_arbiter_reset_timeout(void)
{
    if (!g_arbiter.initialized) {
        return;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    g_arbiter.last_voice_time_ms = get_timestamp_ms();
    xSemaphoreGive(g_arbiter.mutex);
}

int32_t priority_arbiter_get_timeout_remaining(void)
{
    if (!g_arbiter.initialized || g_arbiter.current_mode != MODE_MANUAL) {
        return -1;
    }
    
    xSemaphoreTake(g_arbiter.mutex, portMAX_DELAY);
    int64_t elapsed = get_timestamp_ms() - g_arbiter.last_voice_time_ms;
    int32_t remaining = (int32_t)(g_arbiter.config.manual_timeout_ms - elapsed);
    xSemaphoreGive(g_arbiter.mutex);
    
    return remaining > 0 ? remaining : 0;
}
