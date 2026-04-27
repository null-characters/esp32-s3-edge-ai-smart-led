/*
 * Fade Control Module - 渐变控制
 * Phase 1: HW-020
 */

#ifndef FADE_CONTROL_H
#define FADE_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "lighting.h"

/* 默认渐变时间 */
#define FADE_DEFAULT_DURATION_MS  1000  /* 1秒 */
#define FADE_STEP_INTERVAL_MS     50    /* 每50ms更新一次 */

/**
 * @brief 初始化渐变控制模块
 * @return 0=成功
 */
int fade_control_init(void);

/**
 * @brief 平滑渐变到目标色温和光强
 * @param target_temp   目标色温 (2700-6500)
 * @param target_bright 目标光强 (0-100)
 * @param duration_ms   渐变时间 (毫秒)
 * @return 0=成功, 负值=失败
 */
int fade_to_target(uint16_t target_temp, uint8_t target_bright,
		   uint32_t duration_ms);

/**
 * @brief 立即设置 (无渐变)
 * @param temp   色温
 * @param bright 光强
 * @return 0=成功
 */
int fade_set_immediate(uint16_t temp, uint8_t bright);

/**
 * @brief 检查是否正在渐变中
 * @return true=正在渐变
 */
bool fade_is_in_progress(void);

/**
 * @brief 获取当前设置值
 * @param temp   输出当前色温
 * @param bright 输出当前光强
 */
void fade_get_current(uint16_t *temp, uint8_t *bright);

#endif /* FADE_CONTROL_H */
