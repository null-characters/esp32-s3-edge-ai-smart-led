# Graph Report - .  (2026-04-29)

## Corpus Check
- Large corpus: 309 files · ~171,392 words. Semantic extraction will be expensive (many Claude tokens). Consider running on a subfolder, or use --no-semantic to run AST-only.

## Summary
- 889 nodes · 1526 edges · 32 communities detected
- Extraction: 72% EXTRACTED · 28% INFERRED · 0% AMBIGUOUS · INFERRED: 425 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]
- [[_COMMUNITY_Community 28|Community 28]]
- [[_COMMUNITY_Community 29|Community 29]]
- [[_COMMUNITY_Community 32|Community 32]]
- [[_COMMUNITY_Community 33|Community 33]]

## God Nodes (most connected - your core abstractions)
1. `xSemaphoreTake()` - 82 edges
2. `xSemaphoreGive()` - 81 edges
3. `main()` - 40 edges
4. `vSemaphoreDelete()` - 21 edges
5. `app_main()` - 16 edges
6. `xSemaphoreCreateMutex()` - 15 edges
7. `command_handler_process()` - 13 edges
8. `tts_speak()` - 12 edges
9. `data_logger_log_lighting()` - 12 edges
10. `ai_infer_thread_fn()` - 12 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `inmp441_init()`  [INFERRED]
  prj-v1/src/main.c → prj-v2/src/audio/inmp441_driver.c
- `main()` --calls--> `scene_init()`  [INFERRED]
  prj-v1/src/main.c → prj-v2/src/scene/scene_recognition.c
- `main()` --calls--> `mfcc_init()`  [INFERRED]
  prj-v1/src/main.c → prj-v2/src/audio/mfcc.c
- `ld2410_init()` --calls--> `xSemaphoreCreateMutex()`  [INFERRED]
  prj-v2/src/radar/ld2410_driver.c → prj-v3/tests_host/mock_esp.h
- `main()` --calls--> `ld2410_init()`  [INFERRED]
  prj-v1/src/main.c → prj-v2/src/radar/ld2410_driver.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.05
Nodes (90): check_timer_callback(), env_watcher_deinit(), env_watcher_get_absent_time_ms(), env_watcher_get_state(), env_watcher_set_callback(), env_watcher_stop(), env_watcher_update_radar(), get_time_us() (+82 more)

### Community 1 - "Community 1"
Cohesion: 0.03
Nodes (54): env_watcher_init(), audio_afe_deinit(), audio_afe_init(), audio_afe_reset(), audio_afe_set_callback(), audio_afe_set_wakenet_threshold(), audio_afe_start(), audio_afe_stop() (+46 more)

### Community 2 - "Community 2"
Cohesion: 0.05
Nodes (56): pir_get_status(), pir_init(), net_mgmt_event_handler(), set_state(), wifi_manager_connect(), wifi_manager_disconnect(), wifi_manager_get_ip(), wifi_manager_get_rssi() (+48 more)

### Community 3 - "Community 3"
Cohesion: 0.06
Nodes (30): apply_brightness(), apply_color_temp(), apply_scene(), lighting_controller_deinit(), lighting_controller_start(), lighting_controller_stop(), lighting_controller_submit_auto_event(), lighting_controller_submit_event() (+22 more)

### Community 4 - "Community 4"
Cohesion: 0.11
Nodes (33): sntp_time_get_hour(), sntp_time_get_local(), sntp_time_get_local_hour_f(), sntp_time_get_timestamp(), sntp_time_init(), sntp_time_is_synced(), sntp_time_start(), sntp_time_stop() (+25 more)

### Community 5 - "Community 5"
Cohesion: 0.09
Nodes (24): i2s_rx_callback(), inmp441_calc_rms(), inmp441_calc_zcr(), inmp441_init(), inmp441_read_frame(), inmp441_start(), inmp441_vad_detect(), init_hamming_window() (+16 more)

### Community 6 - "Community 6"
Cohesion: 0.11
Nodes (19): calculate_sunset_proximity(), generate_sample(), generate_training_data(), generate_training_data_with_raw(), normalize_output(), print_statistics(), ESP32-S3 会议室智能照明 - 训练数据生成 Phase 3: AI-002  基于领域知识规则生成训练数据，让MLP模型学习这些规则。 规则来源: Ph, 生成训练数据集     Returns: (X, Y) numpy arrays (+11 more)

### Community 7 - "Community 7"
Cohesion: 0.11
Nodes (20): filter_value(), ld2410_detect_breathing(), ld2410_detect_fan(), ld2410_get_features(), ld2410_get_history_stats(), ld2410_init(), ld2410_read_raw(), ld2410_update_history() (+12 more)

### Community 8 - "Community 8"
Cohesion: 0.13
Nodes (20): build_small_model(), count_params(), main(), print_comparison_table(), ESP32-S3 智能照明 - MLP 消融实验 测试不同网络规模对精度的影响  实验目标：找到"够用就好"的最小模型, 构建指定隐藏层大小的MLP模型          Args:         hidden_sizes: 隐藏层大小列表，如 [16, 8] 表示 7→16→8, 运行消融实验      Args:         X: 输入特征         Y: 归一化输出标签 [0, 1]         Y_raw: 原始输出值, run_ablation_experiment() (+12 more)

### Community 9 - "Community 9"
Cohesion: 0.13
Nodes (24): mock_file_register(), mock_generate_audio(), mock_generate_mfcc(), mock_generate_radar(), mock_generate_radar_features(), mock_led_get_state(), mock_led_set_brightness(), mock_led_set_color_temp() (+16 more)

### Community 10 - "Community 10"
Cohesion: 0.13
Nodes (21): ws2812_clear(), ws2812_deinit(), ws2812_set_all(), ws2812_set_brightness(), ws2812_set_pixel(), ws2812_show(), apply_state_color(), calculate_breath_brightness() (+13 more)

### Community 11 - "Community 11"
Cohesion: 0.2
Nodes (17): http_client_init(), http_get(), http_response_cb(), http_response_cleanup(), http_response_init(), ascii_tolower(), contains_case_insensitive_ascii(), weather_api_init() (+9 more)

### Community 12 - "Community 12"
Cohesion: 0.2
Nodes (17): check_timeout(), create_default_decision(), get_timestamp_ms(), load_state_from_nvs(), priority_arbiter_decide(), priority_arbiter_deinit(), priority_arbiter_get_decision(), priority_arbiter_get_timeout_remaining() (+9 more)

### Community 13 - "Community 13"
Cohesion: 0.22
Nodes (13): lighting_get_status(), lighting_init(), lighting_set_both(), lighting_set_brightness(), lighting_set_color_temp(), calc_checksum(), frame_build(), frame_parse() (+5 more)

### Community 14 - "Community 14"
Cohesion: 0.12
Nodes (6): ESP32-S3 Edge AI - 模型测试框架  测试内容： - 模型加载与推理 - 输入/输出维度验证 - 量化模型正确性检查 - 推理延迟基准  运行方, TestInferenceLatency, TestModelAvailability, TestModelInference, TestModelSizes, TestQuantization

### Community 15 - "Community 15"
Cohesion: 0.17
Nodes (6): add_noise(), generate_breathing_scene(), generate_training_data(), ESP32-S3 雷达场景识别 - 训练数据生成 Phase 1: RD-013  基于 LD2410 雷达特征生成合成训练数据，用于场景分类。  场景类别:, 生成训练数据集     Returns: (X, Y) numpy arrays, 静坐呼吸场景: 距离稳定，周期性微动 (0.2-0.3Hz)，能量低

### Community 16 - "Community 16"
Cohesion: 0.35
Nodes (10): sntp_time_get_hour(), sntp_time_get_local(), sntp_time_get_local_hour_f(), sntp_time_get_timestamp(), sntp_time_init(), sntp_time_is_synced(), sntp_time_sync(), sntp_time_thread_fn() (+2 more)

### Community 17 - "Community 17"
Cohesion: 0.27
Nodes (10): build_model(), generate_synthetic_data(), main(), quantize_model(), ESP32-S3 雷达特征编码器 - MLP模型训练与量化 prj-v3 多模态推理架构  模型结构: MLP (8 → 16 → 12 → 6) 训练: Ad, 训练模型          使用自编码器风格训练:     - 输入: 8 维雷达特征     - 目标: 重建输入 (通过解码器)          但我们只, 构建 MLP 特征编码模型          输入: 8 维雷达特征     输出: 6 维紧凑特征          设计思路:     - 自编码器风格的瓶, 生成合成雷达训练数据          雷达特征 (LD2410):     - distance: 目标距离 0-8m     - velocity: 目标速 (+2 more)

### Community 18 - "Community 18"
Cohesion: 0.25
Nodes (7): presence_state_init(), mock_pir_reset(), mock_pir_set_presence(), pir_get_raw(), pir_get_status(), pir_init(), presence_state_setup()

### Community 19 - "Community 19"
Cohesion: 0.33
Nodes (9): wifi_manager_connect(), wifi_manager_disconnect(), wifi_manager_get_ip(), wifi_manager_get_rssi(), wifi_manager_get_state(), wifi_manager_init(), wifi_manager_is_connected(), wifi_manager_register_state_cb() (+1 more)

### Community 20 - "Community 20"
Cohesion: 0.25
Nodes (6): fusion_infer(), init_model(), multimodal_infer(), multimodal_init(), radar_analyzer_infer(), sound_classifier_infer()

### Community 21 - "Community 21"
Cohesion: 0.31
Nodes (9): build_model(), generate_synthetic_data(), main(), quantize_model(), ESP32-S3 多模态融合决策器 - MLP模型训练与量化 prj-v3 多模态推理架构  模型结构: MLP (19 → 32 → 16 → 2) 训练:, 构建 MLP 融合模型          输入: 19 维多模态特征向量     输出: 2 维控制决策 (色温、亮度)          设计思路:, 生成合成多模态训练数据          输入特征:     - 声音分类 (5维 softmax): 寂静、单人讲话、多人讨论、键盘、其他     - 雷达特, train_model() (+1 more)

### Community 22 - "Community 22"
Cohesion: 0.31
Nodes (9): build_model(), generate_synthetic_data(), main(), quantize_model(), ESP32-S3 声音场景分类器 - CNN模型训练与量化 prj-v3 多模态推理架构  模型结构: CNN (MFCC 40×32×1 → 5类) 训练:, 构建 CNN 分类模型 (轻量版)          输入: MFCC (40, 32, 1) - 40 个 MFCC 系数，32 帧     输出: 5 类场, 生成合成训练数据          由于真实 MFCC 数据需要音频采集，这里生成模拟数据：     - 寂静: 低能量，平坦频谱     - 单人讲话: 中能, train_model() (+1 more)

### Community 23 - "Community 23"
Cohesion: 0.53
Nodes (7): calc_checksum(), frame_build(), frame_parse(), uart_driver_init(), uart_receive_response(), uart_send_frame(), uart_send_with_retry()

### Community 24 - "Community 24"
Cohesion: 0.36
Nodes (8): build_model(), convert_to_c_array(), main(), quantize_model(), ESP32-S3 雷达场景识别 - MLP模型训练与量化 Phase 1: RD-014 ~ RD-017  模型结构: 8 → 16 → 12 → 6 (ML, 构建MLP分类模型: 8→16→12→6      输入: 8维雷达特征向量     输出: 6类场景概率 (Softmax)      参数量: ~400, train_model(), validate_model()

### Community 25 - "Community 25"
Cohesion: 0.43
Nodes (6): ai_infer_get_latency_us(), ai_infer_init(), ai_infer_is_ready(), ai_infer_run(), ai_infer_thread_start(), ai_infer_thread_stop()

### Community 26 - "Community 26"
Cohesion: 0.67
Nodes (6): analyze_log(), colorize_output(), main(), monitor_live(), parse_log_line(), print_report()

### Community 27 - "Community 27"
Cohesion: 0.52
Nodes (6): analyze_data(), collect_data(), main(), parse_performance_line(), print_report(), save_report()

### Community 28 - "Community 28"
Cohesion: 0.53
Nodes (5): main(), ESP32-S3 多模态 AI 模型训练 - 统一入口 prj-v3 多模态推理架构  训练三个模型: 1. sound_classifier.tflite (, train_fusion_model(), train_radar_analyzer(), train_sound_classifier()

### Community 29 - "Community 29"
Cohesion: 0.53
Nodes (4): http_client_init(), http_get(), http_response_cleanup(), http_response_init()

### Community 32 - "Community 32"
Cohesion: 1.0
Nodes (2): main(), setup_env()

### Community 33 - "Community 33"
Cohesion: 1.0
Nodes (2): main(), setup_env()

## Knowledge Gaps
- **29 isolated node(s):** `ESP32-S3 雷达特征编码器 - MLP模型训练与量化 prj-v3 多模态推理架构  模型结构: MLP (8 → 16 → 12 → 6) 训练: Ad`, `构建 MLP 特征编码模型          输入: 8 维雷达特征     输出: 6 维紧凑特征          设计思路:     - 自编码器风格的瓶`, `生成合成雷达训练数据          雷达特征 (LD2410):     - distance: 目标距离 0-8m     - velocity: 目标速`, `训练模型          使用自编码器风格训练:     - 输入: 8 维雷达特征     - 目标: 重建输入 (通过解码器)          但我们只`, `ESP32-S3 Edge AI - 模型测试框架  测试内容： - 模型加载与推理 - 输入/输出维度验证 - 量化模型正确性检查 - 推理延迟基准  运行方` (+24 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 32`** (3 nodes): `main()`, `build_project.py`, `setup_env()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 33`** (3 nodes): `main()`, `build_project.py`, `setup_env()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Community 2` to `Community 4`, `Community 5`, `Community 7`, `Community 11`, `Community 13`, `Community 18`, `Community 20`?**
  _High betweenness centrality (0.236) - this node is a cross-community bridge._
- **Why does `ld2410_init()` connect `Community 7` to `Community 1`, `Community 2`?**
  _High betweenness centrality (0.132) - this node is a cross-community bridge._
- **Why does `xSemaphoreCreateMutex()` connect `Community 1` to `Community 0`, `Community 12`, `Community 7`?**
  _High betweenness centrality (0.115) - this node is a cross-community bridge._
- **Are the 81 inferred relationships involving `xSemaphoreTake()` (e.g. with `dac_deinit()` and `dac_write()`) actually correct?**
  _`xSemaphoreTake()` has 81 INFERRED edges - model-reasoned connections that need verification._
- **Are the 80 inferred relationships involving `xSemaphoreGive()` (e.g. with `dac_write()` and `dac_play_blocking()`) actually correct?**
  _`xSemaphoreGive()` has 80 INFERRED edges - model-reasoned connections that need verification._
- **Are the 38 inferred relationships involving `main()` (e.g. with `ld2410_init()` and `inmp441_init()`) actually correct?**
  _`main()` has 38 INFERRED edges - model-reasoned connections that need verification._
- **Are the 20 inferred relationships involving `vSemaphoreDelete()` (e.g. with `dac_init()` and `dac_deinit()`) actually correct?**
  _`vSemaphoreDelete()` has 20 INFERRED edges - model-reasoned connections that need verification._