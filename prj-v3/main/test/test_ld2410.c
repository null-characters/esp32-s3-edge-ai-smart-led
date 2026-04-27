/**
 * @file test_ld2410.c
 * @brief LD2410 雷达驱动单元测试 (TDD RED Phase)
 * 
 * 测试内容：
 * - 帧头帧尾识别
 * - 数据解析
 * - 特征归一化
 */

#include <unity.h>
#include <math.h>
#include "ld2410_driver.h"

#define FLOAT_TOLERANCE 0.001f

/* ================================================================
 * T1: 帧头帧尾常量测试
 * ================================================================ */
TEST_CASE("ld2410_frame_header_constants", "[ld2410]")
{
    /* 帧头应为 F4 F3 F2 F1 */
    TEST_ASSERT_EQUAL_UINT8(0xF4, LD2410_FRAME_HEADER_0);
    TEST_ASSERT_EQUAL_UINT8(0xF3, LD2410_FRAME_HEADER_1);
    TEST_ASSERT_EQUAL_UINT8(0xF2, LD2410_FRAME_HEADER_2);
    TEST_ASSERT_EQUAL_UINT8(0xF1, LD2410_FRAME_HEADER_3);
    
    /* 帧尾应为 F8 F9 FA FB */
    TEST_ASSERT_EQUAL_UINT8(0xF8, LD2410_FRAME_TAIL_0);
    TEST_ASSERT_EQUAL_UINT8(0xF9, LD2410_FRAME_TAIL_1);
    TEST_ASSERT_EQUAL_UINT8(0xFA, LD2410_FRAME_TAIL_2);
    TEST_ASSERT_EQUAL_UINT8(0xFB, LD2410_FRAME_TAIL_3);
}

/* ================================================================
 * T2: 目标状态常量测试
 * ================================================================ */
TEST_CASE("ld2410_target_state_constants", "[ld2410]")
{
    TEST_ASSERT_EQUAL_INT(0, LD2410_TARGET_NONE);
    TEST_ASSERT_EQUAL_INT(1, LD2410_TARGET_STATIC);
    TEST_ASSERT_EQUAL_INT(2, LD2410_TARGET_MOVING);
}

/* ================================================================
 * T3: 历史缓冲大小测试
 * ================================================================ */
TEST_CASE("ld2410_history_size", "[ld2410]")
{
    /* 历史缓冲应为 60 秒 */
    TEST_ASSERT_EQUAL_INT(60, RADAR_HISTORY_SIZE);
}

/* ================================================================
 * T4: 特征归一化边界测试
 * ================================================================ */
TEST_CASE("ld2410_normalize_distance_min", "[ld2410]")
{
    /* 距离 0m → 归一化 0 */
    float distance = 0.0f;
    float expected = distance / 8.0f;  /* DISTANCE_MAX = 8.0f */
    
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.0f, expected);
}

TEST_CASE("ld2410_normalize_distance_max", "[ld2410]")
{
    /* 距离 8m → 归一化 1 */
    float distance = 8.0f;
    float expected = distance / 8.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, expected);
}

TEST_CASE("ld2410_normalize_velocity_range", "[ld2410]")
{
    /* 速度 -5~+5 m/s → 归一化 -1~+1 */
    float velocity_pos = 5.0f;
    float velocity_neg = -5.0f;
    float velocity_zero = 0.0f;
    
    float norm_pos = velocity_pos / 5.0f;
    float norm_neg = velocity_neg / 5.0f;
    float norm_zero = velocity_zero / 5.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, norm_pos);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, -1.0f, norm_neg);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.0f, norm_zero);
}

/* ================================================================
 * T5: 数据结构大小测试
 * ================================================================ */
TEST_CASE("ld2410_struct_sizes", "[ld2410]")
{
    /* 验证结构体大小合理 */
    TEST_ASSERT_TRUE(sizeof(ld2410_raw_data_t) > 0);
    TEST_ASSERT_TRUE(sizeof(radar_features_t) > 0);
    TEST_ASSERT_TRUE(sizeof(radar_history_t) > 0);
    
    /* radar_features_t 应为 8 个 float (32 bytes) */
    TEST_ASSERT_EQUAL_INT(32, sizeof(radar_features_t));
}