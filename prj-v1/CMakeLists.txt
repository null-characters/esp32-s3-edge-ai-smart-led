# SPDX-License-Identifier: Apache-2.0
# ESP32-S3 Edge AI Gateway - Phase 3 Build Configuration

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(esp32s3_edge_ai_gateway)

# 头文件路径
target_include_directories(app PRIVATE include)

# 源文件
target_sources(app PRIVATE
	src/main.c
	# Phase 1: 基础硬件通信
	src/drivers/pir.c
	src/drivers/uart_driver.c
	src/drivers/lighting.c
	src/logic/presence_state.c
	src/logic/time_period.c
	src/logic/rule_engine.c
	src/logic/fade_control.c
	# Phase 2: WiFi数据服务
	src/drivers/wifi_manager.c
	src/drivers/sntp_time_sync.c
	src/drivers/http_client.c
	src/logic/weather_api.c
	src/logic/sunrise_sunset.c
	src/logic/env_dimming.c
	# Phase 3: AI推理系统
	src/logic/ai_infer.c
	src/logic/data_logger.c
)
