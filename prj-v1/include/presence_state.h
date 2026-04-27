/*
 * Presence Detection State Machine
 * Phase 1: HW-017
 */

#ifndef PRESENCE_STATE_H
#define PRESENCE_STATE_H

#include <stdint.h>
#include <stdbool.h>

/* 人员检测状态 */
typedef enum {
	PRESENCE_YES     = 0,  /* 检测到人员 */
	PRESENCE_NO      = 1,  /* 未检测到人员 (在超时窗口内) */
	PRESENCE_TIMEOUT = 2   /* 人员离开超过5分钟, 应关灯 */
} presence_state_t;

/* 超时时间: 5分钟 = 300秒 = 300000毫秒 */
#define PRESENCE_TIMEOUT_MS  300000

/**
 * @brief 初始化人员状态机
 * @return 0=成功, 负值=失败
 */
int presence_state_init(void);

/**
 * @brief 更新人员状态机 (主循环中周期性调用)
 * @return 当前状态
 */
presence_state_t presence_state_update(void);

/**
 * @brief 获取当前人员状态
 * @return 当前状态枚举值
 */
presence_state_t presence_get_state(void);

/**
 * @brief 获取进入无人状态后经过的秒数
 * @return 秒数 (0 if currently PRESENCE_YES)
 */
uint32_t presence_get_absence_seconds(void);

/**
 * @brief 重置超时计时器 (用于手动场景)
 */
void presence_reset_timer(void);

#endif /* PRESENCE_STATE_H */
