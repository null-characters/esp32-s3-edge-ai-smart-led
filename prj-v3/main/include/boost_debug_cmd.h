/**
 * @file boost_debug_cmd.h
 * @brief Boost升压调试命令接口
 */

#ifndef BOOST_DEBUG_CMD_H
#define BOOST_DEBUG_CMD_H

/**
 * @brief 注册Boost调试命令
 * 
 * 注册以下串口命令:
 * - boost_test: 启动测试
 * - boost_stop: 停止测试
 * - boost_status: 查看状态
 * - boost_duty: 设置占空比
 * - boost_voltage: 读取电压
 * - boost_help: 显示帮助
 */
void register_boost_debug_commands(void);

#endif /* BOOST_DEBUG_CMD_H */
