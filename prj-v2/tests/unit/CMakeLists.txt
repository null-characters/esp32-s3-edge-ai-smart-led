# SPDX-License-Identifier: Apache-2.0
# Unit Test Build Configuration - Phase 3

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_tests)

# 头文件路径
include_directories(../../include)

# 源文件 - 只包含被测试的模块 (排除 main.c)
target_sources(app PRIVATE
	# Phase 1: 基础硬件通信 (使用 mock_uart 替代 uart_driver)
	../../src/drivers/lighting.c
	../../src/logic/presence_state.c
	../../src/logic/time_period.c
	../../src/logic/rule_engine.c
	../../src/logic/fade_control.c
	# Phase 2: WiFi数据服务
	../../src/logic/weather_api.c
	../../src/logic/sunrise_sunset.c
	../../src/logic/env_dimming.c
	# Phase 3: AI推理系统
	../../src/logic/ai_infer.c
	../../src/logic/data_logger.c
	# Mock files
	mock_uart.c
	mock_pir.c
	mock_wifi.c
	mock_http.c
	mock_sntp.c
	# Test files
	test_main.c
	test_uart_frame.c
	test_time_period.c
	test_rule_engine.c
	test_lighting_params.c
	test_presence_state.c
	test_fade_control.c
	test_uart_retry.c
	test_lighting_interface.c
	# Phase 2 tests
	test_weather_api.c
	test_sunrise_sunset.c
	test_env_dimming.c
	# Phase 3 tests
	test_ai_features.c
	test_ai_postprocess.c
	test_data_logger.c
)

# 添加测试特定定义
target_compile_definitions(app PRIVATE
	CONFIG_PIR_MOCK=1
	CONFIG_WIFI_MOCK=1
	CONFIG_HTTP_MOCK=1
	CONFIG_SNTP_MOCK=1
	CONFIG_TFLM_MOCK=1
)
