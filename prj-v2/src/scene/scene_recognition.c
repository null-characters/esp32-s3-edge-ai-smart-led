/**
 * @file scene_recognition.c
 * @brief 场景智能识别实现
 * 
 * Phase 3: 场景智能模块
 * 任务 SC-001 ~ SC-012
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>
#include "scene_recognition.h"

LOG_MODULE_REGISTER(scene, LOG_LEVEL_INF);

/* 场景名称 */
static const char *scene_names[] = {
    "会议高潮",
    "专注工作",
    "汇报演示",
    "休息放松",
    "打扫卫生",
    "未知场景"
};

/* 默认场景配置 */
static scene_config_t scene_configs[] = {
    /* 会议高潮: 全亮冷白 */
    { .color_temp = 6500, .brightness = 100, .fade_time_ms = 2000 },
    /* 专注工作: 暖光中等 */
    { .color_temp = 4000, .brightness = 60, .fade_time_ms = 3000 },
    /* 汇报演示: 主灯暗 */
    { .color_temp = 4000, .brightness = 30, .fade_time_ms = 2000 },
    /* 休息放松: 暖光暗 */
    { .color_temp = 2700, .brightness = 30, .fade_time_ms = 5000 },
    /* 打扫卫生: 基础照明 */
    { .color_temp = 4000, .brightness = 50, .fade_time_ms = 1000 },
};

/* 场景特征映射表 */
static scene_features_t scene_features[] = {
    /* 会议高潮: 多人、多人声 */
    {
        .distance_min = 0.2f, .distance_max = 0.8f,
        .velocity_min = -0.5f, .velocity_max = 0.5f,
        .energy_min = 0.3f, .energy_max = 1.0f,
        .target_count_min = 2, .target_count_max = 10,
        .sound_class = SOUND_MULTI_VOICE,
        .sound_prob_min = 0.6f,
        .hour_start = 9, .hour_end = 18
    },
    /* 专注工作: 单人、键盘声 */
    {
        .distance_min = 0.3f, .distance_max = 0.6f,
        .velocity_min = -0.2f, .velocity_max = 0.2f,
        .energy_min = 0.1f, .energy_max = 0.4f,
        .target_count_min = 1, .target_count_max = 1,
        .sound_class = SOUND_KEYBOARD,
        .sound_prob_min = 0.5f,
        .hour_start = 9, .hour_end = 22
    },
    /* 汇报演示: 单人、单人讲话 */
    {
        .distance_min = 0.4f, .distance_max = 0.7f,
        .velocity_min = -0.3f, .velocity_max = 0.3f,
        .energy_min = 0.2f, .energy_max = 0.5f,
        .target_count_min = 1, .target_count_max = 1,
        .sound_class = SOUND_SINGLE_VOICE,
        .sound_prob_min = 0.6f,
        .hour_start = 9, .hour_end = 18
    },
    /* 休息放松: 单人、寂静 */
    {
        .distance_min = 0.2f, .distance_max = 0.5f,
        .velocity_min = -0.1f, .velocity_max = 0.1f,
        .energy_min = 0.0f, .energy_max = 0.2f,
        .target_count_min = 1, .target_count_max = 1,
        .sound_class = SOUND_SILENCE,
        .sound_prob_min = 0.5f,
        .hour_start = 22, .hour_end = 6
    },
    /* 打扫卫生: 不规则移动 */
    {
        .distance_min = 0.0f, .distance_max = 1.0f,
        .velocity_min = -1.0f, .velocity_max = 1.0f,
        .energy_min = 0.3f, .energy_max = 1.0f,
        .target_count_min = 1, .target_count_max = 2,
        .sound_class = SOUND_OTHER_NOISE,
        .sound_prob_min = 0.3f,
        .hour_start = 0, .hour_end = 24
    }
};

/* 场景状态 */
static scene_state_t scene_state = {
    .current_scene = SCENE_UNKNOWN,
    .prev_scene = SCENE_UNKNOWN,
    .last_switch_time = 0,
    .switch_count = 0,
};

/* 过渡状态 */
static scene_transition_t transition = {0};

/* 切换阈值 */
static float min_confidence = 0.7f;
static uint32_t min_switch_interval = 5000;  /* 5秒 */

int scene_init(void)
{
    scene_state.current_scene = SCENE_UNKNOWN;
    scene_state.prev_scene = SCENE_UNKNOWN;
    scene_state.last_switch_time = 0;
    scene_state.switch_count = 0;
    scene_state.history_idx = 0;
    
    for (int i = 0; i < 10; i++) {
        scene_state.confidence_history[i] = 0;
    }
    
    transition.active = false;
    
    LOG_INF("Scene recognition initialized");
    return 0;
}

int scene_recognize(const multimodal_output_t *output,
                    scene_id_t *scene_id, float *confidence)
{
    float max_score = 0;
    scene_id_t best_scene = SCENE_UNKNOWN;
    
    /* 遍历场景，计算匹配分数 */
    for (int i = 0; i < 5; i++) {
        const scene_features_t *sf = &scene_features[i];
        float score = 0;
        int match_count = 0;
        
        /* 雷达特征匹配 */
        if (output->radar_features[0] >= sf->distance_min &&
            output->radar_features[0] <= sf->distance_max) {
            score += 0.3f;
            match_count++;
        }
        
        /* 声音类别匹配 */
        if (output->sound_class == sf->sound_class &&
            output->sound_confidence >= sf->sound_prob_min) {
            score += 0.5f;
            match_count++;
        }
        
        /* 归一化分数 */
        if (match_count > 0) {
            score /= match_count;
        }
        
        if (score > max_score) {
            max_score = score;
            best_scene = (scene_id_t)i;
        }
    }
    
    *scene_id = best_scene;
    *confidence = max_score;
    
    /* 更新置信度历史 */
    scene_state.confidence_history[scene_state.history_idx] = max_score;
    scene_state.history_idx = (scene_state.history_idx + 1) % 10;
    
    return 0;
}

int scene_get_config(scene_id_t scene_id, scene_config_t *config)
{
    if (scene_id >= 5) {
        return -EINVAL;
    }
    
    *config = scene_configs[scene_id];
    return 0;
}

int scene_set_config(scene_id_t scene_id, const scene_config_t *config)
{
    if (scene_id >= 5) {
        return -EINVAL;
    }
    
    scene_configs[scene_id] = *config;
    return 0;
}

bool scene_check_switch(scene_id_t new_scene, float confidence)
{
    uint32_t now = k_uptime_get();
    
    /* 检查置信度 */
    if (confidence < min_confidence) {
        return false;
    }
    
    /* 检查切换间隔 */
    if (now - scene_state.last_switch_time < min_switch_interval) {
        return false;
    }
    
    /* 检查是否同一场景 */
    if (new_scene == scene_state.current_scene) {
        return false;
    }
    
    return true;
}

int scene_start_transition(scene_id_t target_scene, uint32_t duration_ms)
{
    if (target_scene >= 5) {
        return -EINVAL;
    }
    
    scene_config_t *cfg = &scene_configs[target_scene];
    
    transition.current_color_temp = transition.target_color_temp;
    transition.current_brightness = transition.target_brightness;
    transition.target_color_temp = cfg->color_temp;
    transition.target_brightness = cfg->brightness;
    transition.start_time = k_uptime_get();
    transition.duration_ms = duration_ms > 0 ? duration_ms : cfg->fade_time_ms;
    transition.active = true;
    
    /* 更新场景状态 */
    scene_state.prev_scene = scene_state.current_scene;
    scene_state.current_scene = target_scene;
    scene_state.last_switch_time = k_uptime_get();
    scene_state.switch_count++;
    
    LOG_INF("Scene transition: %s -> %s", 
            scene_get_name(scene_state.prev_scene),
            scene_get_name(target_scene));
    
    return 0;
}

int scene_update_transition(uint32_t elapsed_ms,
                           uint16_t *color_temp, uint8_t *brightness)
{
    if (!transition.active) {
        *color_temp = transition.target_color_temp;
        *brightness = transition.target_brightness;
        return 1;
    }
    
    uint32_t now = k_uptime_get();
    uint32_t elapsed = now - transition.start_time;
    
    if (elapsed >= transition.duration_ms) {
        /* 过渡完成 */
        transition.active = false;
        *color_temp = transition.target_color_temp;
        *brightness = transition.target_brightness;
        return 1;
    }
    
    /* 线性插值 */
    float progress = (float)elapsed / transition.duration_ms;
    
    *color_temp = (uint16_t)(transition.current_color_temp +
                             (transition.target_color_temp - transition.current_color_temp) * progress);
    *brightness = (uint8_t)(transition.current_brightness +
                            (transition.target_brightness - transition.current_brightness) * progress);
    
    return 0;
}

scene_id_t scene_get_current(void)
{
    return scene_state.current_scene;
}

const char *scene_get_name(scene_id_t scene_id)
{
    if (scene_id >= 5) {
        return scene_names[5];
    }
    return scene_names[scene_id];
}

int scene_manual_switch(scene_id_t scene_id)
{
    if (scene_id >= 5) {
        return -EINVAL;
    }
    
    return scene_start_transition(scene_id, 0);
}

void scene_set_threshold(float confidence, uint32_t interval_ms)
{
    min_confidence = confidence;
    min_switch_interval = interval_ms;
}