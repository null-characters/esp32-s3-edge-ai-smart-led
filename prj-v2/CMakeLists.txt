# SPDX-License-Identifier: Apache-2.0
# ESP32-S3 Edge AI Gateway V2 - Multimodal AI Build Configuration

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(esp32s3_edge_ai_gateway_v2)

# 头文件路径
target_include_directories(app PRIVATE 
    include
    include/radar
    include/audio
    include/multimodal
    include/scene
)

# 源文件
target_sources(app PRIVATE
    # 主程序
    src/main.c
    
    # Phase 1: 雷达接入模块
    src/radar/ld2410_driver.c
    
    # Phase 2: 声音分类模块
    src/audio/inmp441_driver.c
    src/audio/mfcc.c
    
    # Phase 3: 多模态推理
    src/multimodal/multimodal_infer.c
    
    # Phase 4: 场景智能
    src/scene/scene_recognition.c
    
    # 模型数据
    src/model_data.cc
    src/radar_model_data.cc
    
    # 基础驱动模块
    src/drivers/uart_driver.c
    src/drivers/lighting.c
    
    # 可选模块 (网络功能)
    src/drivers/wifi_manager.c
    src/drivers/sntp_time_sync.c
    src/drivers/http_client.c
    src/logic/weather_api.c
    src/logic/sunrise_sunset.c
    
    # 照明控制
    src/logic/fade_control.c
    src/logic/env_dimming.c
    src/logic/data_logger.c
)