/**
 * @file test_main.c
 * @brief 嵌入式测试入口点
 * 
 * 注册所有模块测试，运行 Unity 测试菜单
 */

#include <unity.h>

/* 声明测试组运行函数 (由 Unity 自动生成) */
/* MFCC 测试 */
extern void run_test_mfcc(void);

/* VAD 检测器测试 */
extern void run_test_vad_detector(void);

/* 唤醒词测试 */
extern void run_test_wake_word(void);

/* AEC 管道测试 */
extern void run_test_aec_pipeline(void);

/* 模型加载器测试 */
extern void run_test_model_loader(void);

/* LD2410 雷达测试 */
extern void run_test_ld2410(void);

/* 优先级仲裁器测试 */
extern void run_test_priority_arbiter(void);

/* 音频路由器测试 */
extern void run_test_audio_router(void);

/* 命令处理器测试 */
extern void run_test_command_handler(void);

/* TTS 引擎测试 */
extern void run_test_tts_engine(void);

/**
 * @brief 系统级集成测试
 */
void test_system_integration(void)
{
    /* 系统初始化顺序测试 */
    /* 1. NVS 初始化 */
    /* 2. SPIFFS 挂载 */
    /* 3. 驱动初始化 */
    /* 4. 模型加载 */
    /* 5. 音频流水线启动 */
    
    /* 验证各模块初始化不冲突 */
    TEST_ASSERT_EQUAL_UINT32(0, 0);  /* 占位，实际需要验证初始化状态 */
}

/**
 * @brief 性能基准测试
 */
void test_performance_benchmark(void)
{
    /* 推理延迟基准 */
    /* MFCC 提取: < 10ms */
    /* 声音分类: < 5ms */
    /* 雷达编码: < 1ms */
    /* 融合推理: < 1ms */
    
    /* 内存占用基准 */
    /* 模型总大小: < 50KB */
    /* 运行时内存: < 100KB */
    
    TEST_ASSERT_EQUAL_UINT32(0, 0);  /* 占位 */
}

/**
 * @brief 注册所有测试
 */
static void register_tests(void)
{
    /* 系统级测试 */
    RUN_TEST(test_system_integration);
    RUN_TEST(test_performance_benchmark);
}

/**
 * @brief 测试入口
 */
void app_main(void)
{
    unity_run_menu();
}
