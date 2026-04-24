# SPDX-License-Identifier: Apache-2.0
# Unit Test Build Configuration

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_tests)

# 头文件路径
include_directories(../../include)

# 源文件 - 只包含被测试的模块 (排除 main.c)
target_sources(app PRIVATE
	../../src/drivers/uart_driver.c
	../../src/drivers/lighting.c
	../../src/logic/presence_state.c
	../../src/logic/time_period.c
	../../src/logic/rule_engine.c
	../../src/logic/fade_control.c
	# Mock for PIR hardware dependency
	mock_pir.c
	# Test files
	test_main.c
	test_uart_frame.c
	test_time_period.c
	test_rule_engine.c
	test_lighting_params.c
)

# 添加测试特定定义
target_compile_definitions(app PRIVATE
	CONFIG_PIR_MOCK=1
)
