/**
 * @file multimodal_infer.h
 * @brief 多模态融合推理头文件
 * 
 * 功能：
 * - 声音分类器推理
 * - 雷达分析器推理
 * - 多模态融合推理
 * - 推理结果输出
 */

#ifndef MULTIMODAL_INFER_H
#define MULTIMODAL_INFER_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include "radar/ld2410_driver.h"
#include "audio/mfcc.h"

/* 模型配置 */
#define SOUND_MODEL_INPUT_SIZE   (MFCC_NUM_COEFFS * MFCC_NUM_FRAMES)  /* 40 x 32 = 1280 */
#define SOUND_MODEL_OUTPUT_SIZE   5    /* 5 类场景 */
#define RADAR_MODEL_INPUT_SIZE    8    /* 8 维特征 */
#define RADAR_MODEL_OUTPUT_SIZE   6    /* 6 维输出 */
#define FUSION_MODEL_INPUT_SIZE   19   /* 5 + 6 + 8 */
#define FUSION_MODEL_OUTPUT_SIZE  2    /* 色温、亮度 */

/* 声音场景类别 */
typedef enum {
    SOUND_SILENCE = 0,      /* 寂静 */
    SOUND_SINGLE_VOICE,     /* 单人讲话 */
    SOUND_MULTI_VOICE,      /* 多人讨论 */
    SOUND_KEYBOARD,         /* 键盘敲击 */
    SOUND_OTHER_NOISE       /* 其他噪音 */
} sound_class_t;

/* 多模态输入 */
typedef struct {
    /* 声音特征 */
    float mfcc[SOUND_MODEL_INPUT_SIZE];
    
    /* 雷达特征 */
    radar_features_t radar;
    
    /* 环境特征 (8维) */
    float hour_sin;          /* 时间正弦编码 */
    float hour_cos;          /* 时间余弦编码 */
    float sunset_proximity;  /* 日落临近度 */
    float sunrise_hour_norm; /* 日出时间归一化 */
    float sunset_hour_norm;  /* 日落时间归一化 */
    float weather;           /* 天气编码 */
    float presence;          /* 人员存在 */
    float reserved;          /* 预留 */
} multimodal_input_t;

/* 多模态输出 */
typedef struct {
    /* 声音分类结果 */
    float sound_probs[SOUND_MODEL_OUTPUT_SIZE];
    sound_class_t sound_class;
    float sound_confidence;
    
    /* 雷达分析结果 */
    float radar_features[RADAR_MODEL_OUTPUT_SIZE];
    
    /* 融合决策结果 */
    uint16_t color_temp;     /* 色温 (2700K-6500K) */
    uint8_t brightness;      /* 亮度 (0-100%) */
    uint8_t scene_id;        /* 场景 ID */
    float scene_confidence;  /* 场景置信度 */
    
    /* 推理时间戳 */
    uint32_t timestamp;
    uint32_t inference_time_us;  /* 推理耗时 */
} multimodal_output_t;

/* 推理配置 */
typedef struct {
    bool sound_enabled;
    bool radar_enabled;
    bool fusion_enabled;
    int inference_interval_ms;  /* 推理间隔 */
} multimodal_config_t;

/**
 * @brief 初始化多模态推理引擎
 * @return 0 成功
 */
int multimodal_init(void);

/**
 * @brief 执行多模态推理
 * @param input 输入特征
 * @param output 输出结果
 * @return 0 成功
 */
int multimodal_infer(const multimodal_input_t *input, multimodal_output_t *output);

/**
 * @brief 声音分类器推理
 * @param mfcc MFCC 特征
 * @param probs 输出概率分布
 * @return 0 成功
 */
int sound_classifier_infer(const float *mfcc, float *probs);

/**
 * @brief 雷达分析器推理
 * @param features 雷达特征
 * @param output 输出特征
 * @return 0 成功
 */
int radar_analyzer_infer(const radar_features_t *features, float *output);

/**
 * @brief 融合层推理
 * @param sound_probs 声音概率
 * @param radar_features 雷达特征
 * @param env_features 环境特征
 * @param color_temp 输出色温
 * @param brightness 输出亮度
 * @return 0 成功
 */
int fusion_infer(const float *sound_probs, const float *radar_features,
                 const float *env_features, uint16_t *color_temp, uint8_t *brightness);

/**
 * @brief 设置推理配置
 * @param config 配置参数
 */
void multimodal_set_config(const multimodal_config_t *config);

/**
 * @brief 获取推理配置
 * @param config 输出配置
 */
void multimodal_get_config(multimodal_config_t *config);

/**
 * @brief 获取推理统计信息
 * @param avg_time_us 平均推理时间
 * @param total_count 总推理次数
 */
void multimodal_get_stats(uint32_t *avg_time_us, uint32_t *total_count);

/**
 * @brief 获取 Tensor Arena 信息（供测试验证）
 * @param sound_start 声音模型 Arena 起始地址
 * @param sound_end 声音模型 Arena 结束地址
 * @param radar_start 雷达模型 Arena 起始地址
 * @param radar_end 雷达模型 Arena 结束地址
 * @param fusion_start 融合模型 Arena 起始地址
 * @param fusion_end 融合模型 Arena 结束地址
 * @return 0 成功
 */
int multimodal_get_arena_info(uintptr_t *sound_start, uintptr_t *sound_end,
                               uintptr_t *radar_start, uintptr_t *radar_end,
                               uintptr_t *fusion_start, uintptr_t *fusion_end);

#endif /* MULTIMODAL_INFER_H */