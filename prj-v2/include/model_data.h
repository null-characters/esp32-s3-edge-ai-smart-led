/**
 * @file model_data.h
 * @brief 多模态模型数据占位文件
 * 
 * 实际模型数据需要通过训练生成：
 * 1. 训练声音分类模型 (sound_classifier)
 * 2. 训练雷达分析模型 (radar_analyzer)
 * 3. 训练融合决策模型 (fusion_model)
 * 4. 使用 xxd 转换为 C 数组
 */

#ifndef MODEL_DATA_H
#define MODEL_DATA_H

#include <stdint.h>
#include <stddef.h>

/* 
 * 声音分类模型 (Sound Classifier)
 * 输入: MFCC (40 x 32 = 1280 维)
 * 输出: 5 类场景概率
 * 大小: ~30KB (INT8 量化后)
 */
extern const unsigned char g_sound_model_data[];
extern const unsigned int g_sound_model_len;

/*
 * 雷达分析模型 (Radar Analyzer)
 * 输入: 8 维特征向量
 * 输出: 6 维特征向量
 * 大小: ~2KB (INT8 量化后)
 */
extern const unsigned char g_radar_model_data[];
extern const unsigned int g_radar_model_len;

/*
 * 融合决策模型 (Fusion Model)
 * 输入: 19 维 (5 + 6 + 8)
 * 输出: 2 维 (色温, 亮度)
 * 大小: ~3KB (INT8 量化后)
 */
extern const unsigned char g_fusion_model_data[];
extern const unsigned int g_fusion_model_len;

/* 模型版本信息 */
#define MODEL_VERSION_MAJOR  1
#define MODEL_VERSION_MINOR  0
#define MODEL_VERSION_PATCH  0

/* 模型校验和 (训练后生成) */
#define SOUND_MODEL_CHECKSUM  0x00000000
#define RADAR_MODEL_CHECKSUM  0x00000000
#define FUSION_MODEL_CHECKSUM 0x00000000

#endif /* MODEL_DATA_H */