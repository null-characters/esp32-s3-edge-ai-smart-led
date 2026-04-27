/**
 * @file scene_recognition.h
 * @brief 场景智能识别头文件
 * 
 * 功能：
 * - 场景定义与映射
 * - 场景切换检测
 * - 平滑过渡控制
 * - 个性化配置
 */

#ifndef SCENE_RECOGNITION_H
#define SCENE_RECOGNITION_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "multimodal/multimodal_infer.h"

/* 场景定义 */
typedef enum {
    SCENE_MEETING = 0,      /* 会议高潮 */
    SCENE_FOCUS = 1,        /* 专注工作 */
    SCENE_PRESENTATION = 2, /* 汇报演示 */
    SCENE_RELAX = 3,        /* 休息放松 */
    SCENE_CLEANING = 4,     /* 打扫卫生 */
    SCENE_UNKNOWN = 255     /* 未知场景 */
} scene_id_t;

/* 场景配置 */
typedef struct {
    uint16_t color_temp;    /* 目标色温 */
    uint8_t brightness;     /* 目标亮度 */
    uint16_t fade_time_ms;  /* 渐变时间 */
} scene_config_t;

/* 场景特征映射 */
typedef struct {
    /* 雷达特征范围 */
    float distance_min, distance_max;
    float velocity_min, velocity_max;
    float energy_min, energy_max;
    int target_count_min, target_count_max;
    
    /* 声音特征 */
    sound_class_t sound_class;
    float sound_prob_min;
    
    /* 时间范围 */
    int hour_start, hour_end;
} scene_features_t;

/* 场景切换状态 */
typedef struct {
    scene_id_t current_scene;
    scene_id_t prev_scene;
    uint32_t last_switch_time;
    int switch_count;
    float confidence_history[10];
    int history_idx;
} scene_state_t;

/* 平滑过渡参数 */
typedef struct {
    uint16_t current_color_temp;
    uint8_t current_brightness;
    uint16_t target_color_temp;
    uint8_t target_brightness;
    uint32_t start_time;
    uint32_t duration_ms;
    bool active;
} scene_transition_t;

/**
 * @brief 初始化场景识别模块
 * @return 0 成功
 */
int scene_init(void);

/**
 * @brief 识别当前场景
 * @param output 多模态推理输出
 * @param scene_id 输出场景 ID
 * @param confidence 输出置信度
 * @return 0 成功
 */
int scene_recognize(const multimodal_output_t *output,
                    scene_id_t *scene_id, float *confidence);

/**
 * @brief 获取场景配置
 * @param scene_id 场景 ID
 * @param config 输出配置
 * @return 0 成功
 */
int scene_get_config(scene_id_t scene_id, scene_config_t *config);

/**
 * @brief 设置场景配置
 * @param scene_id 场景 ID
 * @param config 配置参数
 * @return 0 成功
 */
int scene_set_config(scene_id_t scene_id, const scene_config_t *config);

/**
 * @brief 检查场景切换
 * @param new_scene 新场景
 * @param confidence 置信度
 * @return true 允许切换
 */
bool scene_check_switch(scene_id_t new_scene, float confidence);

/**
 * @brief 更新场景过渡
 * @param elapsed_ms 经过时间
 * @param color_temp 输出色温
 * @param brightness 输出亮度
 * @return 0 成功, 1 过渡完成
 */
int scene_update_transition(uint32_t elapsed_ms,
                            uint16_t *color_temp, uint8_t *brightness);

/**
 * @brief 触发场景过渡
 * @param target_scene 目标场景
 * @param duration_ms 过渡时间
 * @return 0 成功
 */
int scene_start_transition(scene_id_t target_scene, uint32_t duration_ms);

/**
 * @brief 获取当前场景
 * @return 当前场景 ID
 */
scene_id_t scene_get_current(void);

/**
 * @brief 获取场景名称
 * @param scene_id 场景 ID
 * @return 场景名称字符串
 */
const char *scene_get_name(scene_id_t scene_id);

/**
 * @brief 手动切换场景
 * @param scene_id 目标场景
 * @return 0 成功
 */
int scene_manual_switch(scene_id_t scene_id);

/**
 * @brief 设置切换阈值
 * @param min_confidence 最小置信度
 * @param min_interval_ms 最小切换间隔
 */
void scene_set_threshold(float min_confidence, uint32_t min_interval_ms);

#endif /* SCENE_RECOGNITION_H */