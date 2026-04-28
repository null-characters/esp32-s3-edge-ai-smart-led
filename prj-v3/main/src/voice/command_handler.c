/**
 * @file command_handler.c
 * @brief 命令处理器实现
 */

#include "command_handler.h"
#include "led_pwm.h"
#include "tts_engine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "CMD_HANDLER";

/* ================================================================
 * 场景预设表
 * ================================================================ */

static const scene_preset_t g_scene_presets[] = {
    /* 专注模式: 冷白光, 60% 亮度 */
    [CMD_SCENE_FOCUS - CMD_SCENE_BASE] = {
        .name = "专注模式",
        .params = { .brightness = 60, .color_temp = 5000, .main_light_on = true, .aux_light_on = false }
    },
    /* 会议模式: 自然光, 70% 亮度 */
    [CMD_SCENE_MEETING - CMD_SCENE_BASE] = {
        .name = "会议模式",
        .params = { .brightness = 70, .color_temp = 4000, .main_light_on = true, .aux_light_on = true }
    },
    /* 演示模式: 主灯 30%, 辅助灯亮 */
    [CMD_SCENE_PRESENTATION - CMD_SCENE_BASE] = {
        .name = "演示模式",
        .params = { .brightness = 30, .color_temp = 4000, .main_light_on = true, .aux_light_on = true }
    },
    /* 休息模式: 暖光, 30% 亮度 */
    [CMD_SCENE_RELAX - CMD_SCENE_BASE] = {
        .name = "休息模式",
        .params = { .brightness = 30, .color_temp = 2700, .main_light_on = true, .aux_light_on = false }
    },
    /* 明亮模式: 冷白光, 100% 亮度 */
    [CMD_SCENE_BRIGHT - CMD_SCENE_BASE] = {
        .name = "明亮模式",
        .params = { .brightness = 100, .color_temp = 6500, .main_light_on = true, .aux_light_on = true }
    },
    /* 夜间模式: 暖光, 10% 亮度 */
    [CMD_SCENE_NIGHT - CMD_SCENE_BASE] = {
        .name = "夜间模式",
        .params = { .brightness = 10, .color_temp = 2700, .main_light_on = true, .aux_light_on = false }
    },
};

#define SCENE_PRESET_COUNT (sizeof(g_scene_presets) / sizeof(g_scene_presets[0]))

/* ================================================================
 * 模块状态
 * ================================================================ */

static system_state_t g_state = {
    .mode = MODE_AUTO,
    .current_scene = CMD_INVALID,
    .light = { .brightness = 50, .color_temp = 4000, .main_light_on = true, .aux_light_on = false },
    .voice_active = false,
    .last_command_time = 0,
};

static command_callback_t g_callback = NULL;
static void *g_user_data = NULL;
static SemaphoreHandle_t g_state_mutex = NULL;  /**< 状态保护 mutex */

/* ================================================================
 * 内部函数
 * ================================================================ */

/**
 * @brief 执行灯光控制
 */
static int execute_light_control(const light_control_t *params)
{
    if (!params) {
        return -1;
    }
    
    /* 调用实际硬件驱动 */
    if (params->main_light_on) {
        led_set_brightness(params->brightness);
        led_set_color_temp(params->color_temp);
    } else {
        led_set_brightness(0);
    }
    
    /* 更新状态 (mutex 保护) */
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&g_state.light, params, sizeof(light_control_t));
        xSemaphoreGive(g_state_mutex);
    }
    
    ESP_LOGI(TAG, "灯光控制: 亮度=%d%%, 色温=%dK, 主灯=%s, 辅助灯=%s",
             params->brightness, params->color_temp,
             params->main_light_on ? "开" : "关",
             params->aux_light_on ? "开" : "关");
    
    return 0;
}

/**
 * @brief 处理场景切换命令
 */
static command_result_t handle_scene_command(int command_id)
{
    int scene_index = command_id - CMD_SCENE_BASE;
    if (scene_index < 0 || scene_index >= (int)SCENE_PRESET_COUNT) {
        return CMD_RESULT_INVALID;
    }
    
    const scene_preset_t *preset = &g_scene_presets[scene_index];
    if (execute_light_control(&preset->params) == 0) {
        if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_state.current_scene = command_id;
            xSemaphoreGive(g_state_mutex);
        }
        ESP_LOGI(TAG, "切换场景: %s", preset->name);
        return CMD_RESULT_OK;
    }
    
    return CMD_RESULT_FAILED;
}

/**
 * @brief 处理亮度调节命令
 */
static command_result_t handle_brightness_command(int command_id)
{
    int brightness;
    
    /* 获取当前亮度 (mutex 保护) */
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        brightness = g_state.light.brightness;
        xSemaphoreGive(g_state_mutex);
    } else {
        brightness = 50;  /* 默认值 */
    }
    
    switch (command_id) {
        case CMD_BRIGHT_UP:
            brightness = (brightness >= 90) ? 100 : brightness + 10;
            break;
        case CMD_BRIGHT_DOWN:
            brightness = (brightness <= 10) ? 0 : brightness - 10;
            break;
        case CMD_BRIGHT_MAX:
            brightness = 100;
            break;
        case CMD_BRIGHT_MIN:
            brightness = 5;
            break;
        case CMD_BRIGHT_20:
            brightness = 20;
            break;
        case CMD_BRIGHT_40:
            brightness = 40;
            break;
        case CMD_BRIGHT_60:
            brightness = 60;
            break;
        case CMD_BRIGHT_80:
            brightness = 80;
            break;
        case CMD_BRIGHT_100:
            brightness = 100;
            break;
        default:
            return CMD_RESULT_INVALID;
    }
    
    light_control_t params;
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        params = g_state.light;
        params.brightness = brightness;
        xSemaphoreGive(g_state_mutex);
    }
    
    execute_light_control(&params);
    
    return CMD_RESULT_OK;
}

/**
 * @brief 处理色温调节命令
 */
static command_result_t handle_cct_command(int command_id)
{
    int color_temp;
    
    /* 获取当前色温 (mutex 保护) */
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        color_temp = g_state.light.color_temp;
        xSemaphoreGive(g_state_mutex);
    } else {
        color_temp = 4000;  /* 默认值 */
    }
    
    switch (command_id) {
        case CMD_CCT_WARM:
            color_temp = 2700;
            break;
        case CMD_CCT_COOL:
            color_temp = 6500;
            break;
        case CMD_CCT_WHITE:
            color_temp = 5000;
            break;
        case CMD_CCT_NATURAL:
            color_temp = 4000;
            break;
        case CMD_CCT_WARMER:
            color_temp = (color_temp <= 2800) ? 2700 : color_temp - 300;
            break;
        case CMD_CCT_COOLER:
            color_temp = (color_temp >= 6200) ? 6500 : color_temp + 300;
            break;
        default:
            return CMD_RESULT_INVALID;
    }
    
    light_control_t params;
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        params = g_state.light;
        params.color_temp = color_temp;
        xSemaphoreGive(g_state_mutex);
    }
    
    execute_light_control(&params);
    
    return CMD_RESULT_OK;
}

/**
 * @brief 处理开关控制命令
 */
static command_result_t handle_power_command(int command_id)
{
    light_control_t params;
    
    /* 获取当前状态 (mutex 保护) */
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        params = g_state.light;
        xSemaphoreGive(g_state_mutex);
    } else {
        return CMD_RESULT_FAILED;
    }
    
    switch (command_id) {
        case CMD_POWER_ON:
        case CMD_POWER_ALL_ON:
            params.main_light_on = true;
            params.aux_light_on = true;
            break;
        case CMD_POWER_OFF:
        case CMD_POWER_ALL_OFF:
            params.main_light_on = false;
            params.aux_light_on = false;
            break;
        case CMD_POWER_MAIN_ON:
            params.main_light_on = true;
            break;
        case CMD_POWER_MAIN_OFF:
            params.main_light_on = false;
            break;
        case CMD_POWER_AUX_ON:
            params.aux_light_on = true;
            break;
        case CMD_POWER_AUX_OFF:
            params.aux_light_on = false;
            break;
        default:
            return CMD_RESULT_INVALID;
    }
    
    execute_light_control(&params);
    return CMD_RESULT_OK;
}

/**
 * @brief 处理模式切换命令
 */
static command_result_t handle_mode_command(int command_id)
{
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        switch (command_id) {
            case CMD_MODE_AUTO:
                g_state.mode = MODE_AUTO;
                ESP_LOGI(TAG, "切换到自动模式");
                break;
            case CMD_MODE_MANUAL:
                g_state.mode = MODE_MANUAL;
                ESP_LOGI(TAG, "切换到手动模式");
                break;
            case CMD_MODE_VOICE:
                g_state.mode = MODE_VOICE;
                ESP_LOGI(TAG, "切换到语音模式");
                break;
            default:
                xSemaphoreGive(g_state_mutex);
                return CMD_RESULT_INVALID;
        }
        xSemaphoreGive(g_state_mutex);
    } else {
        return CMD_RESULT_FAILED;
    }
    
    return CMD_RESULT_OK;
}

/**
 * @brief 处理状态查询命令
 */
static command_result_t handle_query_command(int command_id)
{
    light_control_t current_light;
    system_mode_t current_mode;
    
    /* 获取当前状态 (mutex 保护) */
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_light = g_state.light;
        current_mode = g_state.mode;
        xSemaphoreGive(g_state_mutex);
    } else {
        return CMD_RESULT_FAILED;
    }
    
    ESP_LOGI(TAG, "查询命令: ID=%d", command_id);
    
    /* 根据查询类型进行 TTS 回复 */
    switch (command_id) {
        case CMD_QUERY_ALL:
            /* 查询所有状态 */
            {
                char text[64];
                snprintf(text, sizeof(text), "当前亮度%d%%，色温%dK",
                         current_light.brightness, current_light.color_temp);
                tts_speak(text);
            }
            break;
            
        case CMD_QUERY_BRIGHTNESS:
            /* 查询亮度 */
            tts_speak_brightness(current_light.brightness);
            break;
            
        case CMD_QUERY_CCT:
            /* 查询色温 */
            tts_speak_color_temp(current_light.color_temp);
            break;
            
        case CMD_QUERY_MODE:
            /* 查询模式 */
            tts_speak_mode(current_mode == MODE_AUTO);
            break;
            
        case CMD_QUERY_SCENE:
            /* 查询场景 */
            {
                int scene_id = g_state.current_scene;
                const scene_preset_t *scene = command_handler_get_scene_preset(scene_id);
                if (scene) {
                    tts_speak_scene_status(scene->name, scene->params.brightness, scene->params.color_temp);
                } else {
                    tts_speak("当前无场景");
                }
            }
            break;
            
        default:
            /* 其他查询命令 */
            tts_speak_confirm();
            break;
    }
    
    return CMD_RESULT_OK;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

int command_handler_init(void)
{
    ESP_LOGI(TAG, "初始化命令处理器");
    
    /* 创建状态保护 mutex */
    if (!g_state_mutex) {
        g_state_mutex = xSemaphoreCreateMutex();
        if (!g_state_mutex) {
            ESP_LOGE(TAG, "创建 mutex 失败");
            return -1;
        }
    }
    
    g_state.mode = MODE_AUTO;
    g_state.current_scene = CMD_INVALID;
    g_state.voice_active = false;
    g_state.last_command_time = 0;
    
    return 0;
}

command_result_t command_handler_process(int command_id)
{
    if (!command_is_valid(command_id)) {
        return CMD_RESULT_INVALID;
    }
    
    /* 更新最后命令时间 (mutex 保护) */
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state.last_command_time = (uint32_t)(esp_timer_get_time() / 1000);
        g_state.voice_active = true;
        xSemaphoreGive(g_state_mutex);
    }
    
    command_result_t result = CMD_RESULT_OK;
    command_category_t category = command_get_category(command_id);
    
    switch (category) {
        case CMD_CATEGORY_SCENE:
            result = handle_scene_command(command_id);
            break;
        case CMD_CATEGORY_BRIGHT:
            result = handle_brightness_command(command_id);
            break;
        case CMD_CATEGORY_CCT:
            result = handle_cct_command(command_id);
            break;
        case CMD_CATEGORY_POWER:
            result = handle_power_command(command_id);
            break;
        case CMD_CATEGORY_MODE:
            result = handle_mode_command(command_id);
            break;
        case CMD_CATEGORY_QUERY:
            /* 状态查询，TTS 回复 */
            handle_query_command(command_id);
            break;
        case CMD_CATEGORY_CANCEL:
            if (command_id == CMD_RESTORE_AUTO) {
                command_handler_restore_auto();
            }
            break;
        case CMD_CATEGORY_SPECIAL:
            ESP_LOGI(TAG, "特殊命令: ID=%d", command_id);
            break;
        default:
            result = CMD_RESULT_INVALID;
            break;
    }
    
    /* 调用回调 */
    if (g_callback) {
        g_callback(command_id, result, g_user_data);
    }
    
    return result;
}

void command_handler_set_callback(command_callback_t callback, void *user_data)
{
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_callback = callback;
        g_user_data = user_data;
        xSemaphoreGive(g_state_mutex);
    }
}

const system_state_t* command_handler_get_state(void)
{
    return &g_state;
}

int command_handler_set_mode(system_mode_t mode)
{
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state.mode = mode;
        xSemaphoreGive(g_state_mutex);
    }
    return 0;
}

int command_handler_restore_auto(void)
{
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state.mode = MODE_AUTO;
        g_state.voice_active = false;
        xSemaphoreGive(g_state_mutex);
    }
    ESP_LOGI(TAG, "恢复自动控制");
    return 0;
}

bool command_handler_check_timeout(uint32_t timeout_ms)
{
    bool timeout = false;
    
    if (g_state_mutex && xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!g_state.voice_active) {
            xSemaphoreGive(g_state_mutex);
            return false;
        }
        
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t elapsed = now - g_state.last_command_time;
        
        if (elapsed >= timeout_ms) {
            g_state.voice_active = false;
            timeout = true;
        }
        xSemaphoreGive(g_state_mutex);
    }
    
    return timeout;
}

int command_handler_apply_scene(int scene_id)
{
    return (handle_scene_command(scene_id) == CMD_RESULT_OK) ? 0 : -1;
}

const scene_preset_t* command_handler_get_scene_preset(int scene_id)
{
    int index = scene_id - CMD_SCENE_BASE;
    if (index < 0 || index >= (int)SCENE_PRESET_COUNT) {
        return NULL;
    }
    return &g_scene_presets[index];
}
