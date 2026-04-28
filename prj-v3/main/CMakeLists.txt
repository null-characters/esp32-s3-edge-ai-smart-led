idf_component_register(
    SRCS "src/main.c"
         # 驱动层
         "src/drivers/led_pwm.c"
         "src/drivers/uart_driver.c"
         "src/drivers/i2s_driver.c"
         "src/drivers/dac_driver.c"
         # 网络层
         "src/network/wifi_sta.c"
         # 多模态推理
         "src/multimodal/tflm_allocator.c"
         "src/multimodal/model_loader.c"
         # 雷达模块
         "src/radar/ld2410_driver.c"
         # 音频处理
         "src/audio/mfcc.c"
         "src/audio/audio_afe.c"
         "src/audio/audio_pipeline.c"
         # 仲裁机制
         "src/arbitration/priority_arbiter.c"
         "src/arbitration/ttl_lease.c"
         "src/arbitration/env_watcher.c"
         # 控制层
         "src/control/Lighting_Controller.c"
         # 语音交互
         "src/voice/wake_word.c"
         "src/voice/command_handler.c"
         "src/voice/voice_commands.c"
         "src/voice/vad_detector.c"
         "src/voice/audio_router.c"
         "src/voice/aec_pipeline.c"
         "src/voice/tts_engine.c"
         # 单元测试文件
         "test/test_main.c"
         "test/test_ld2410.c"
         "test/test_mfcc.c"
         "test/test_wake_word.c"
         "test/test_command_handler.c"
         "test/test_model_loader.c"
         "test/test_priority_arbiter.c"
    INCLUDE_DIRS "include"
                 "src/drivers"
                 "src/network"
                 "src/multimodal"
                 "src/radar"
                 "src/audio"
                 "src/arbitration"
                 "src/control"
                 "src/voice"
                 "test"
    REQUIRES unity esp_wifi esp_netif lwip nvs_flash esp_event esp_timer driver esp_driver_i2s esp_driver_uart esp_driver_ledc spiffs espressif__esp-sr
)
