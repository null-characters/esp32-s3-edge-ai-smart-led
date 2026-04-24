/*
 * AI Inference Engine - TFLM推理封装
 * Phase 3: AI-008 ~ AI-012, AI-017 ~ AI-020
 *
 * 功能: 模型加载、特征填充、推理执行、后处理
 */

#ifndef AI_INFER_H
#define AI_INFER_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * AI-013: 特征结构定义
 * ================================================================ */
#define AI_FEATURE_COUNT  7

typedef struct {
	float hour_sin;           /* [-1, 1] 时间正弦编码 */
	float hour_cos;           /* [-1, 1] 时间余弦编码 */
	float sunset_proximity;   /* [-1, 1] 日落临近度 */
	float sunrise_hour_norm;  /* [0, 1] 日出时间归一化 */
	float sunset_hour_norm;   /* [0, 1] 日落时间归一化 */
	float weather;            /* [0, 1] 天气编码 */
	float presence;           /* {0, 1} 人员存在 */
} ai_features_t;

/* 输出结构 */
typedef struct {
	uint16_t color_temp;      /* 2700-6500 K */
	uint8_t brightness;       /* 0-100 % */
} ai_output_t;

/* 推理状态 */
typedef enum {
	AI_STATUS_OK = 0,
	AI_STATUS_ERROR_INIT = -1,
	AI_STATUS_ERROR_MODEL = -2,
	AI_STATUS_ERROR_TENSOR = -3,
	AI_STATUS_ERROR_INVOKE = -4,
	AI_STATUS_ERROR_INPUT = -5,
} ai_status_t;

/* ================================================================
 * AI-008 ~ AI-012: 初始化与推理
 * ================================================================ */

/**
 * @brief 初始化AI推理引擎 (AI-008 ~ AI-010)
 * @return AI_STATUS_OK=成功
 */
ai_status_t ai_infer_init(void);

/**
 * @brief 检查AI引擎是否就绪
 * @return true=已初始化
 */
bool ai_infer_is_ready(void);

/**
 * @brief 执行推理 (AI-017)
 * @param features 输入特征
 * @param output   输出结果
 * @return AI_STATUS_OK=成功
 */
ai_status_t ai_infer_run(const ai_features_t *features, ai_output_t *output);

/**
 * @brief 获取推理延迟 (AI-028)
 * @return 上次推理耗时(微秒)
 */
uint32_t ai_infer_get_latency_us(void);

/* ================================================================
 * AI-014 ~ AI-016: 特征工程
 * ================================================================ */

/**
 * @brief 计算时间特征 (AI-014)
 * @param hour 当前小时 (0-23)
 * @param features 输出特征结构
 */
void ai_calc_time_features(float hour, ai_features_t *features);

/**
 * @brief 计算日落临近度 (AI-015)
 * @param hour 当前小时
 * @param sunset_hour 日落时间(小时)
 * @return 临近度值 [-1, 1]
 */
float ai_calc_sunset_proximity(float hour, float sunset_hour);

/**
 * @brief 组装完整特征向量 (AI-016)
 * @param hour 当前小时
 * @param weather 天气编码 (0-1)
 * @param presence 人员存在 (0/1)
 * @param sunrise_hour 日出时间
 * @param sunset_hour 日落时间
 * @param features 输出特征结构
 */
void ai_assemble_features(float hour, float weather, float presence,
			  float sunrise_hour, float sunset_hour,
			  ai_features_t *features);

/* ================================================================
 * AI-018: 后处理
 * ================================================================ */

/**
 * @brief 推理结果后处理
 * - 无人强制关灯
 * - 深夜限制亮度
 * - 范围限制
 * @param raw_output 原始推理输出
 * @param presence   人员状态
 * @param hour       当前小时
 * @param final_output 最终输出
 */
void ai_post_process(const ai_output_t *raw_output, float presence,
		     float hour, ai_output_t *final_output);

/* ================================================================
 * AI-020: 推理线程
 * ================================================================ */

/**
 * @brief 启动AI推理线程
 */
void ai_infer_thread_start(void);

/**
 * @brief 停止AI推理线程
 */
void ai_infer_thread_stop(void);

/* 推理周期 */
#define AI_INFERENCE_INTERVAL_MS  30000  /* 30秒 */

#endif /* AI_INFER_H */
