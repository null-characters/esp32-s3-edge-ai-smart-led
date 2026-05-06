/**
 * @file boost_debug_cmd.c
 * @brief Boost升压调试命令行接口
 * 
 * 提供串口命令控制Boost测试:
 * - boost_test 1 [duty]  : 阶段1固定PWM测试
 * - boost_test 2 [target_mv] [window_mv] : 阶段2滞环控制
 * - boost_test 3 : 阶段3 PID控制
 * - boost_stop : 停止测试
 * - boost_status : 查看状态
 * - boost_duty <percent> : 动态调整占空比
 */

#include "boost_debug_test.h"
#include "led_pwm.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BOOST_CMD";

/* ================================================================
 * 命令参数定义
 * ================================================================ */

/** boost_test 命令参数 */
static struct {
    struct arg_int *stage;
    struct arg_int *duty;
    struct arg_int *target;
    struct arg_int *window;
    struct arg_end *end;
} boost_test_args;

/** boost_duty 命令参数 */
static struct {
    struct arg_int *duty;
    struct arg_end *end;
} boost_duty_args;

/* ================================================================
 * 命令处理函数
 * ================================================================ */

static int cmd_boost_test(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&boost_test_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, boost_test_args.end, argv[0]);
        return 1;
    }
    
    int stage = boost_test_args.stage->ival[0];
    
    if (stage < 1 || stage > 3) {
        printf("错误: 阶段必须是 1, 2 或 3\n");
        return 1;
    }
    
    /* 初始化测试模块 */
    esp_err_t ret = boost_debug_test_init((boost_test_stage_t)stage);
    if (ret != ESP_OK) {
        printf("初始化失败: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    /* 根据阶段设置参数 */
    switch (stage) {
        case 1:
            if (boost_test_args.duty->count > 0) {
                int duty = boost_test_args.duty->ival[0];
                if (duty < 0 || duty > 100) {
                    printf("错误: 占空比必须在 0-100 范围\n");
                    return 1;
                }
                boost_debug_set_fixed_duty((uint8_t)duty);
            }
            printf("启动阶段1测试: 固定PWM占空比\n");
            printf("提示: 用万用表测量Boost输出电压验证电路\n");
            break;
            
        case 2:
            if (boost_test_args.target->count > 0) {
                int target = boost_test_args.target->ival[0];
                int window = boost_test_args.window->count > 0 ? 
                             boost_test_args.window->ival[0] : 500;
                boost_debug_set_hysteresis_target((uint16_t)target, (uint16_t)window);
            }
            printf("启动阶段2测试: 滞环控制\n");
            break;
            
        case 3:
            printf("启动阶段3测试: PID闭环控制\n");
            break;
    }
    
    /* 启动测试 */
    ret = boost_debug_test_start();
    if (ret != ESP_OK) {
        printf("启动失败: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    printf("测试已启动，观察串口日志输出\n");
    return 0;
}

static int cmd_boost_stop(int argc, char **argv)
{
    esp_err_t ret = boost_debug_test_stop();
    if (ret != ESP_OK) {
        printf("停止失败: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    printf("测试已停止\n");
    return 0;
}

static int cmd_boost_status(int argc, char **argv)
{
    bool running;
    boost_test_stage_t stage;
    uint16_t voltage;
    uint8_t duty;
    
    esp_err_t ret = boost_debug_test_get_status(&running, &stage, &voltage, &duty);
    if (ret != ESP_OK) {
        printf("获取状态失败: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    printf("\n");
    printf("=== Boost调试测试状态 ===\n");
    printf("运行状态: %s\n", running ? "运行中" : "已停止");
    printf("测试阶段: %d\n", stage);
    printf("当前电压: %d mV\n", voltage);
    printf("当前占空比: %d%%\n", duty);
    printf("========================\n");
    printf("\n");
    
    return 0;
}

static int cmd_boost_duty(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&boost_duty_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, boost_duty_args.end, argv[0]);
        return 1;
    }
    
    int duty = boost_duty_args.duty->ival[0];
    if (duty < 0 || duty > 100) {
        printf("错误: 占空比必须在 0-100 范围\n");
        return 1;
    }
    
    esp_err_t ret = boost_debug_set_fixed_duty((uint8_t)duty);
    if (ret != ESP_OK) {
        printf("设置失败: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    printf("占空比已设置为: %d%%\n", duty);
    return 0;
}

static int cmd_boost_voltage(int argc, char **argv)
{
    uint16_t voltage = boost_read_voltage();
    printf("Boost输出电压: %d mV (%.2f V)\n", voltage, voltage / 1000.0f);
    return 0;
}

static int cmd_boost_help(int argc, char **argv)
{
    printf("\n");
    printf("=== Boost升压调试命令 ===\n");
    printf("\n");
    printf("boost_test <stage> [options]\n");
    printf("  启动测试，stage: 1=固定PWM, 2=滞环控制, 3=PID控制\n");
    printf("  阶段1选项: duty=<0-100> 指定固定占空比\n");
    printf("  阶段2选项: target=<mV> window=<mV> 指定目标电压和滞环窗口\n");
    printf("  示例:\n");
    printf("    boost_test 1 --duty 50      # 50%%占空比固定PWM\n");
    printf("    boost_test 2 --target 24000 --window 500  # 24V目标±500mV\n");
    printf("    boost_test 3                # PID闭环控制\n");
    printf("\n");
    printf("boost_stop\n");
    printf("  停止当前测试\n");
    printf("\n");
    printf("boost_status\n");
    printf("  查看测试状态\n");
    printf("\n");
    printf("boost_duty <percent>\n");
    printf("  动态调整占空比 (仅阶段1有效)\n");
    printf("\n");
    printf("boost_voltage\n");
    printf("  读取当前Boost输出电压\n");
    printf("\n");
    printf("boost_help\n");
    printf("  显示此帮助信息\n");
    printf("\n");
    printf("=== 测试步骤建议 ===\n");
    printf("1. 阶段1: 用万用表测量Boost输出，验证PWM和电路正常\n");
    printf("2. 阶段2: 观察滞环控制能否稳定在目标电压\n");
    printf("3. 阶段3: 调整PID参数，优化响应速度和稳定性\n");
    printf("========================\n");
    printf("\n");
    return 0;
}

/* ================================================================
 * 命令注册
 * ================================================================ */

void register_boost_debug_commands(void)
{
    /* boost_test 命令 */
    boost_test_args.stage = arg_int1(NULL, NULL, "<stage>", "测试阶段 (1/2/3)");
    boost_test_args.duty = arg_int0(NULL, "duty", "<percent>", "固定占空比 (0-100)");
    boost_test_args.target = arg_int0(NULL, "target", "<mV>", "目标电压 (mV)");
    boost_test_args.window = arg_int0(NULL, "window", "<mV>", "滞环窗口 (mV)");
    boost_test_args.end = arg_end(4);
    
    const esp_console_cmd_t boost_test_cmd = {
        .command = "boost_test",
        .help = "启动Boost升压测试",
        .hint = NULL,
        .func = &cmd_boost_test,
        .argtable = &boost_test_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&boost_test_cmd));
    
    /* boost_stop 命令 */
    const esp_console_cmd_t boost_stop_cmd = {
        .command = "boost_stop",
        .help = "停止Boost测试",
        .func = &cmd_boost_stop,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&boost_stop_cmd));
    
    /* boost_status 命令 */
    const esp_console_cmd_t boost_status_cmd = {
        .command = "boost_status",
        .help = "查看Boost测试状态",
        .func = &cmd_boost_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&boost_status_cmd));
    
    /* boost_duty 命令 */
    boost_duty_args.duty = arg_int1(NULL, NULL, "<percent>", "占空比 (0-100)");
    boost_duty_args.end = arg_end(1);
    
    const esp_console_cmd_t boost_duty_cmd = {
        .command = "boost_duty",
        .help = "设置Boost占空比",
        .hint = NULL,
        .func = &cmd_boost_duty,
        .argtable = &boost_duty_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&boost_duty_cmd));
    
    /* boost_voltage 命令 */
    const esp_console_cmd_t boost_voltage_cmd = {
        .command = "boost_voltage",
        .help = "读取Boost输出电压",
        .func = &cmd_boost_voltage,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&boost_voltage_cmd));
    
    /* boost_help 命令 */
    const esp_console_cmd_t boost_help_cmd = {
        .command = "boost_help",
        .help = "显示Boost调试命令帮助",
        .func = &cmd_boost_help,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&boost_help_cmd));
    
    ESP_LOGI(TAG, "Boost调试命令已注册");
}
