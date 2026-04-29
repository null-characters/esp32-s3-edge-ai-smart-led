/**
 * @file Lighting_Controller.c
 * @brief 统一照明控制器实现
 */

#include "Lighting_Controller.h"
#include "Lighting_Event.h"
#include "led_pwm.h"
#include "priority_arbiter.h"
#include "ttl_lease.h"
#include "tts_engine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "LIGHT_CTRL";

/* ================================================================
 * 配置常量
 * ================================================================ */

#define CONTROLLER_TASK_STACK_SIZE  4096
#define CONTROLLER_TASK_PRIORITY    5
#define DEFAULT_EVENT_QUEUE_SIZE    16

/* ================================================================
 * 模块状态
 * ================================================================ */

typedef struct {
    bool initialized;
    bool running;
    
    /* 事件队列 */
    QueueHandle_t event_queue;
    uint32_t queue_size;
    
    /* 任务 */
    TaskHandle_t task_handle;
    
    /* 当前状态 */
    uint8_t current_brightness;
    uint16_t current_color_temp;
    bool current_power;
    /* 注意: auto_mode 已移除，统一使用 priority_arbiter_get_mode() 获取模式 */
    
    /* 回调 */
    controller_event_callback_t callback;
    void *user_data;
    
    /* 同步 */
    SemaphoreHandle_t mutex;
} controller_state_t;

static controller_state_t g_ctrl = {0};

/* ================================================================
 * 内部函数
 * ================================================================ */

static void apply_scene(const scene_config_t *scene)
{
    if (!scene) {
        return;
    }
    
    ESP_LOGI(TAG, "应用场景: %s (亮度=%d%%, 色温=%dK)",
             scene->name, scene->brightness, scene->color_temp);
    
    if (scene->power) {
        led_set_brightness(scene->brightness);
        led_set_color_temp(scene->color_temp);
    }
    
    g_ctrl.current_brightness = scene->brightness;
    g_ctrl.current_color_temp = scene->color_temp;
    g_ctrl.current_power = scene->power;
}

static void apply_brightness(const brightness_config_t *config)
{
    uint8_t target;
    
    if (config->relative) {
        int new_val = g_ctrl.current_brightness + config->delta;
        target = (uint8_t)(new_val < 0 ? 0 : (new_val > 100 ? 100 : new_val));
    } else {
        target = config->value;
    }
    
    ESP_LOGI(TAG, "设置亮度: %d%%", target);
    led_set_brightness(target);
    g_ctrl.current_brightness = target;
}

static void apply_color_temp(const color_temp_config_t *config)
{
    uint16_t target;
    
    if (config->relative) {
        /* 暖/冷切换 */
        if (config->direction < 0) {
            target = 2700;  /* 暖光 */
        } else {
            target = 6500;  /* 冷光 */
        }
    } else {
        target = config->value;
    }
    
    ESP_LOGI(TAG, "设置色温: %dK", target);
    led_set_color_temp(target);
    g_ctrl.current_color_temp = target;
}

static void process_event(const Lighting_Event_t *event)
{
    if (!event) {
        return;
    }
    
    ESP_LOGI(TAG, "处理事件: source=%d, type=%d", event->source, event->type);
    
    /* 获取当前模式 (统一从 arbiter 获取) */
    system_mode_t current_mode = priority_arbiter_get_mode();
    
    /* 处理 TTL 租约 */
    if (event->source == EVENT_SOURCE_VOICE && event->has_lease) {
        ttl_lease_acquire(event->ttl_ms);
    } else if (event->source == EVENT_SOURCE_AUTO) {
        /* 自动控制事件：检查是否在自动模式 */
        if (current_mode != MODE_AUTO) {
            ESP_LOGD(TAG, "手动模式，忽略自动控制事件");
            return;
        }
    }
    
    /* 处理事件类型 */
    switch (event->type) {
        case EVENT_TYPE_SCENE: {
            const scene_config_t *scene = get_scene_config(event->data.scene.id);
            if (!scene) {
                scene = &event->data.scene;  /* 使用自定义场景 */
            }
            apply_scene(scene);
            break;
        }
        
        case EVENT_TYPE_BRIGHTNESS:
            apply_brightness(&event->data.brightness);
            break;
        
        case EVENT_TYPE_COLOR_TEMP:
            apply_color_temp(&event->data.color_temp);
            break;
        
        case EVENT_TYPE_POWER:
            g_ctrl.current_power = event->data.power.power;
            ESP_LOGI(TAG, "电源: %s", g_ctrl.current_power ? "开" : "关");
            break;
        
        case EVENT_TYPE_MODE_SWITCH:
            if (event->data.mode == 0) {
                /* 恢复自动模式 */
                ttl_lease_release(LEASE_RELEASE_USER_CMD);
                priority_arbiter_set_mode(MODE_AUTO);
                ESP_LOGI(TAG, "恢复自动模式");
            }
            break;
        
        case EVENT_TYPE_QUERY:
            /* 状态查询，TTS 回复 */
            tts_speak_brightness(g_ctrl.current_brightness);
            break;
        
        default:
            ESP_LOGW(TAG, "未知事件类型: %d", event->type);
            break;
    }
    
    /* 触发回调 */
    if (g_ctrl.callback) {
        g_ctrl.callback(event, g_ctrl.user_data);
    }
}

static void controller_task(void *arg)
{
    Lighting_Event_t event;
    
    ESP_LOGI(TAG, "照明控制器任务启动");
    
    while (g_ctrl.running) {
        if (xQueueReceive(g_ctrl.event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            xSemaphoreTake(g_ctrl.mutex, portMAX_DELAY);
            process_event(&event);
            xSemaphoreGive(g_ctrl.mutex);
        }
        
        /* 检查 TTL 租约 */
        ttl_lease_check_expired();
    }
    
    ESP_LOGI(TAG, "照明控制器任务退出");
    vTaskDelete(NULL);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t lighting_controller_init(const controller_config_t *config)
{
    if (g_ctrl.initialized) {
        ESP_LOGW(TAG, "照明控制器已初始化");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "初始化照明控制器");
    
    g_ctrl.mutex = xSemaphoreCreateMutex();
    if (!g_ctrl.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    /* 应用配置 */
    if (config) {
        g_ctrl.queue_size = config->event_queue_size;
        g_ctrl.callback = config->callback;
        g_ctrl.user_data = config->user_data;
    } else {
        g_ctrl.queue_size = DEFAULT_EVENT_QUEUE_SIZE;
    }
    
    /* 创建事件队列 */
    g_ctrl.event_queue = xQueueCreate(g_ctrl.queue_size, sizeof(Lighting_Event_t));
    if (!g_ctrl.event_queue) {
        ESP_LOGE(TAG, "创建事件队列失败");
        vSemaphoreDelete(g_ctrl.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    /* 初始化状态 */
    g_ctrl.current_brightness = 50;
    g_ctrl.current_color_temp = 4000;
    g_ctrl.current_power = true;
    g_ctrl.running = false;
    g_ctrl.initialized = true;
    
    ESP_LOGI(TAG, "照明控制器初始化完成");
    
    return ESP_OK;
}

void lighting_controller_deinit(void)
{
    if (!g_ctrl.initialized) {
        return;
    }
    
    lighting_controller_stop();
    
    if (g_ctrl.event_queue) {
        vQueueDelete(g_ctrl.event_queue);
    }
    
    if (g_ctrl.mutex) {
        vSemaphoreDelete(g_ctrl.mutex);
    }
    
    memset(&g_ctrl, 0, sizeof(controller_state_t));
    ESP_LOGI(TAG, "照明控制器已释放");
}

esp_err_t lighting_controller_start(void)
{
    if (!g_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_ctrl.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动照明控制器");
    
    g_ctrl.running = true;
    
    BaseType_t ret = xTaskCreate(
        controller_task,
        "light_ctrl",
        CONTROLLER_TASK_STACK_SIZE,
        NULL,
        CONTROLLER_TASK_PRIORITY,
        &g_ctrl.task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建任务失败");
        g_ctrl.running = false;
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t lighting_controller_stop(void)
{
    if (!g_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_ctrl.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "停止照明控制器");
    
    g_ctrl.running = false;
    
    if (g_ctrl.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(200));  /* 等待任务退出 */
        g_ctrl.task_handle = NULL;
    }
    
    return ESP_OK;
}

esp_err_t lighting_controller_submit_event(const Lighting_Event_t *event)
{
    if (!g_ctrl.initialized || !event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    Lighting_Event_t event_copy = *event;
    event_copy.timestamp_ms = esp_timer_get_time() / 1000;
    
    if (xQueueSend(g_ctrl.event_queue, &event_copy, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "事件队列满，丢弃事件");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t lighting_controller_submit_voice_event(event_type_t type, 
                                                  scene_id_t scene_id,
                                                  uint8_t brightness,
                                                  uint16_t color_temp,
                                                  uint32_t ttl_ms)
{
    Lighting_Event_t event = create_voice_event(type, ttl_ms);
    
    switch (type) {
        case EVENT_TYPE_SCENE:
            event.data.scene.id = scene_id;
            break;
        case EVENT_TYPE_BRIGHTNESS:
            event.data.brightness.value = brightness;
            event.data.brightness.relative = false;
            break;
        case EVENT_TYPE_COLOR_TEMP:
            event.data.color_temp.value = color_temp;
            event.data.color_temp.relative = false;
            break;
        default:
            break;
    }
    
    return lighting_controller_submit_event(&event);
}

esp_err_t lighting_controller_submit_auto_event(scene_id_t scene_id, float confidence)
{
    Lighting_Event_t event = create_auto_event(EVENT_TYPE_SCENE);
    event.data.scene.id = scene_id;
    event.confidence = confidence;
    
    return lighting_controller_submit_event(&event);
}

void lighting_controller_get_state(uint8_t *brightness, uint16_t *color_temp, bool *power)
{
    if (brightness) *brightness = g_ctrl.current_brightness;
    if (color_temp) *color_temp = g_ctrl.current_color_temp;
    if (power) *power = g_ctrl.current_power;
}

bool lighting_controller_is_auto_mode(void)
{
    return (priority_arbiter_get_mode() == MODE_AUTO);
}

void lighting_controller_set_callback(controller_event_callback_t callback, void *user_data)
{
    g_ctrl.callback = callback;
    g_ctrl.user_data = user_data;
}