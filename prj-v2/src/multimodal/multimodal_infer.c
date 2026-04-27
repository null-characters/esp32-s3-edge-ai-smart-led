/**
 * @file multimodal_infer.c
 * @brief 多模态融合推理实现
 * 
 * Phase 2: 声音分类模块
 * 任务 AU-019 ~ AU-025
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>
#include "multimodal_infer.h"

/* TensorFlow Lite Micro 头文件 */
#include <tflite/micro/micro_interpreter.h>
#include <tflite/micro/system_setup.h>
#include <tflite/schema/schema_generated.h>

LOG_MODULE_REGISTER(multimodal, LOG_LEVEL_INF);

/* Tensor Arena 大小 */
#define TENSOR_ARENA_SIZE  (64 * 1024)  /* 64KB */

/* 模型数据 (外部声明) */
extern const unsigned char g_sound_model_data[];
extern const unsigned int g_sound_model_len;
extern const unsigned char g_radar_model_data[];
extern const unsigned int g_radar_model_len;
extern const unsigned char g_fusion_model_data[];
extern const unsigned int g_fusion_model_len;

/* Tensor Arena */
static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

/* TFLM 解释器 */
static tflite::MicroInterpreter *sound_interpreter = nullptr;
static tflite::MicroInterpreter *radar_interpreter = nullptr;
static tflite::MicroInterpreter *fusion_interpreter = nullptr;

/* 张量指针 */
static float *sound_input = nullptr;
static float *sound_output = nullptr;
static float *radar_input = nullptr;
static float *radar_output = nullptr;
static float *fusion_input = nullptr;
static float *fusion_output = nullptr;

/* 推理统计 */
static uint32_t inference_count = 0;
static uint64_t total_inference_time = 0;

/* 配置 */
static multimodal_config_t config = {
    .sound_enabled = true,
    .radar_enabled = true,
    .fusion_enabled = true,
    .inference_interval_ms = 1000
};

/**
 * @brief 初始化单个模型
 */
static int init_model(const unsigned char *model_data, size_t model_len,
                      tflite::MicroInterpreter **interpreter,
                      float **input_ptr, float **output_ptr,
                      int input_size, int output_size)
{
    /* 加载模型 */
    const tflite::Model *model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        LOG_ERR("Model version mismatch");
        return -EINVAL;
    }
    
    /* 创建操作解析器 */
    static tflite::AllOpsResolver resolver;
    
    /* 创建解释器 */
    static tflite::ErrorReporter *error_reporter = nullptr;
    *interpreter = new tflite::MicroInterpreter(
        model, resolver, tensor_arena, TENSOR_ARENA_SIZE, error_reporter);
    
    /* 分配张量 */
    TfLiteStatus status = (*interpreter)->AllocateTensors();
    if (status != kTfLiteOk) {
        LOG_ERR("AllocateTensors failed");
        return -ENOMEM;
    }
    
    /* 获取输入输出指针 */
    *input_ptr = (*interpreter)->input(0)->data.f;
    *output_ptr = (*interpreter)->output(0)->data.f;
    
    LOG_INF("Model initialized, input_size=%d, output_size=%d", input_size, output_size);
    return 0;
}

int multimodal_init(void)
{
    /* 初始化 TFLite */
    tflite::InitializeTarget();
    
    /* 初始化声音分类模型 */
    if (config.sound_enabled) {
        int ret = init_model(g_sound_model_data, g_sound_model_len,
                             &sound_interpreter, &sound_input, &sound_output,
                             SOUND_MODEL_INPUT_SIZE, SOUND_MODEL_OUTPUT_SIZE);
        if (ret < 0) {
            LOG_ERR("Sound model init failed");
            return ret;
        }
    }
    
    /* 初始化雷达分析模型 */
    if (config.radar_enabled) {
        int ret = init_model(g_radar_model_data, g_radar_model_len,
                             &radar_interpreter, &radar_input, &radar_output,
                             RADAR_MODEL_INPUT_SIZE, RADAR_MODEL_OUTPUT_SIZE);
        if (ret < 0) {
            LOG_ERR("Radar model init failed");
            return ret;
        }
    }
    
    /* 初始化融合模型 */
    if (config.fusion_enabled) {
        int ret = init_model(g_fusion_model_data, g_fusion_model_len,
                             &fusion_interpreter, &fusion_input, &fusion_output,
                             FUSION_MODEL_INPUT_SIZE, FUSION_MODEL_OUTPUT_SIZE);
        if (ret < 0) {
            LOG_ERR("Fusion model init failed");
            return ret;
        }
    }
    
    LOG_INF("Multimodal inference engine initialized");
    return 0;
}

int sound_classifier_infer(const float *mfcc, float *probs)
{
    if (!sound_interpreter || !sound_input) {
        return -ENODEV;
    }
    
    /* 复制输入数据 */
    memcpy(sound_input, mfcc, SOUND_MODEL_INPUT_SIZE * sizeof(float));
    
    /* 执行推理 */
    TfLiteStatus status = sound_interpreter->Invoke();
    if (status != kTfLiteOk) {
        LOG_ERR("Sound inference failed");
        return -EIO;
    }
    
    /* 复制输出数据 */
    memcpy(probs, sound_output, SOUND_MODEL_OUTPUT_SIZE * sizeof(float));
    
    return 0;
}

int radar_analyzer_infer(const radar_features_t *features, float *output)
{
    if (!radar_interpreter || !radar_input) {
        return -ENODEV;
    }
    
    /* 组装输入向量 */
    radar_input[0] = features->distance_norm;
    radar_input[1] = features->velocity_norm;
    radar_input[2] = features->energy_norm;
    radar_input[3] = features->target_count;
    radar_input[4] = features->motion_amplitude;
    radar_input[5] = features->motion_period;
    radar_input[6] = features->distance_variance;
    radar_input[7] = features->energy_trend;
    
    /* 执行推理 */
    TfLiteStatus status = radar_interpreter->Invoke();
    if (status != kTfLiteOk) {
        LOG_ERR("Radar inference failed");
        return -EIO;
    }
    
    /* 复制输出数据 */
    memcpy(output, radar_output, RADAR_MODEL_OUTPUT_SIZE * sizeof(float));
    
    return 0;
}

int fusion_infer(const float *sound_probs, const float *radar_features,
                 const float *env_features, uint16_t *color_temp, uint8_t *brightness)
{
    if (!fusion_interpreter || !fusion_input) {
        return -ENODEV;
    }
    
    /* 组装融合层输入 */
    int idx = 0;
    memcpy(&fusion_input[idx], sound_probs, SOUND_MODEL_OUTPUT_SIZE * sizeof(float));
    idx += SOUND_MODEL_OUTPUT_SIZE;
    memcpy(&fusion_input[idx], radar_features, RADAR_MODEL_OUTPUT_SIZE * sizeof(float));
    idx += RADAR_MODEL_OUTPUT_SIZE;
    memcpy(&fusion_input[idx], env_features, 8 * sizeof(float));
    
    /* 执行推理 */
    TfLiteStatus status = fusion_interpreter->Invoke();
    if (status != kTfLiteOk) {
        LOG_ERR("Fusion inference failed");
        return -EIO;
    }
    
    /* 后处理输出 */
    float ct_norm = fusion_output[0];  /* [0, 1] */
    float br_norm = fusion_output[1];  /* [0, 1] */
    
    /* 反归一化 */
    *color_temp = (uint16_t)(ct_norm * 3800.0f + 2700.0f);  /* 2700K - 6500K */
    *brightness = (uint8_t)(br_norm * 100.0f);              /* 0% - 100% */
    
    return 0;
}

int multimodal_infer(const multimodal_input_t *input, multimodal_output_t *output)
{
    uint32_t start_time = k_cycle_get_32();
    
    /* 1. 声音分类 */
    if (config.sound_enabled) {
        sound_classifier_infer(input->mfcc, output->sound_probs);
        
        /* 找最大概率类别 */
        float max_prob = 0;
        int max_idx = 0;
        for (int i = 0; i < SOUND_MODEL_OUTPUT_SIZE; i++) {
            if (output->sound_probs[i] > max_prob) {
                max_prob = output->sound_probs[i];
                max_idx = i;
            }
        }
        output->sound_class = (sound_class_t)max_idx;
        output->sound_confidence = max_prob;
    }
    
    /* 2. 雷达分析 */
    if (config.radar_enabled) {
        radar_analyzer_infer(&input->radar, output->radar_features);
    }
    
    /* 3. 多模态融合 */
    if (config.fusion_enabled) {
        float env_features[8] = {
            input->hour_sin, input->hour_cos, input->sunset_proximity,
            input->sunrise_hour_norm, input->sunset_hour_norm,
            input->weather, input->presence, input->reserved
        };
        
        fusion_infer(output->sound_probs, output->radar_features,
                     env_features, &output->color_temp, &output->brightness);
    }
    
    /* 4. 场景识别 (基于规则映射) */
    /* TODO: 调用场景识别模块 */
    output->scene_id = 0;
    output->scene_confidence = 0.8f;
    
    /* 统计 */
    uint32_t end_time = k_cycle_get_32();
    output->timestamp = k_uptime_get();
    output->inference_time_us = k_cyc_to_us_floor32(end_time - start_time);
    
    inference_count++;
    total_inference_time += output->inference_time_us;
    
    return 0;
}

void multimodal_set_config(const multimodal_config_t *cfg)
{
    config = *cfg;
}

void multimodal_get_config(multimodal_config_t *cfg)
{
    *cfg = config;
}

void multimodal_get_stats(uint32_t *avg_time_us, uint32_t *total_count)
{
    if (inference_count > 0) {
        *avg_time_us = (uint32_t)(total_inference_time / inference_count);
    } else {
        *avg_time_us = 0;
    }
    *total_count = inference_count;
}