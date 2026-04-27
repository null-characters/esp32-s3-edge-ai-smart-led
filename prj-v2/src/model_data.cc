/**
 * @file model_data.cc
 * @brief 模型数据占位实现
 * 
 * TODO: 使用训练好的模型替换以下占位数据
 * 
 * 生成步骤：
 * 1. python train_sound_model.py  # 训练声音分类模型
 * 2. python train_radar_model.py  # 训练雷达分析模型
 * 3. python train_fusion_model.py # 训练融合模型
 * 4. xxd -i sound_model.tflite > model_data.cc
 * 5. xxd -i radar_model.tflite >> model_data.cc
 * 6. xxd -i fusion_model.tflite >> model_data.cc
 */

#include <stdint.h>

/* 声音分类模型占位数据 (实际大小 ~30KB) */
/* TODO: 替换为真实模型数据 */
const unsigned char g_sound_model_data[] = {
    /* TFLite FlatBuffer Header (占位) */
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    /* ... 实际模型数据 ... */
    0x00  /* 占位字节 */
};
const unsigned int g_sound_model_len = sizeof(g_sound_model_data);

/* 雷达分析模型占位数据 (实际大小 ~2KB) */
const unsigned char g_radar_model_data[] = {
    /* TFLite FlatBuffer Header (占位) */
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    /* ... 实际模型数据 ... */
    0x00  /* 占位字节 */
};
const unsigned int g_radar_model_len = sizeof(g_radar_model_data);

/* 融合决策模型占位数据 (实际大小 ~3KB) */
const unsigned char g_fusion_model_data[] = {
    /* TFLite FlatBuffer Header (占位) */
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    /* ... 实际模型数据 ... */
    0x00  /* 占位字节 */
};
const unsigned int g_fusion_model_len = sizeof(g_fusion_model_data);