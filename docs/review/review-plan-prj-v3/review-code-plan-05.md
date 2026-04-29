# Review Plan 05 - 网络通信与主程序

## 概述
本计划审查 prj-v3 的网络通信模块和主程序入口，以及所有头文件的接口设计。

## 审查范围

### 文件列表 - 网络模块
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/network/wifi_sta.c` | WiFi站点模式管理 | ~300 |

### 文件列表 - 主程序
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/main.c` | 主程序入口 | ~250 |

### 头文件审查（接口设计）
| 文件路径 | 说明 |
|---------|------|
| `main/include/aec_pipeline.h` | AEC接口 |
| `main/include/audio_afe.h` | AFE接口 |
| `main/include/audio_pipeline.h` | 音频流水线接口 |
| `main/include/audio_router.h` | 音频路由接口 |
| `main/include/command_handler.h` | 命令处理接口 |
| `main/include/command_words.h` | 命令词定义 |
| `main/include/dac_driver.h` | DAC驱动接口 |
| `main/include/env_watcher.h` | 环境监视接口 |
| `main/include/i2s_driver.h` | I2S驱动接口 |
| `main/include/ld2410_driver.h` | 雷达驱动接口 |
| `main/include/led_pwm.h` | LED PWM接口 |
| `main/include/Lighting_Controller.h` | 照明控制接口 |
| `main/include/Lighting_Event.h` | 照明事件定义 |
| `main/include/mfcc.h` | MFCC接口 |
| `main/include/model_loader.h` | 模型加载接口 |
| `main/include/priority_arbiter.h` | 仲裁器接口 |
| `main/include/status_led.h` | 状态LED接口 |
| `main/include/ttl_lease.h` | TTL租约接口 |
| `main/include/tts_engine.h` | TTS引擎接口 |
| `main/include/uart_driver.h` | UART驱动接口 |
| `main/include/vad_detector.h` | VAD检测接口 |
| `main/include/voice_commands.h` | 语音命令接口 |
| `main/include/wake_word.h` | 唤醒词接口 |
| `main/include/wifi_sta.h` | WiFi接口 |
| `main/include/ws2812_driver.h` | WS2812驱动接口 |

## Graphify 社区关联
- **Community 2**: WiFi/网络相关 (56 nodes)
- **Community 19**: WiFi Manager (9 nodes, Cohesion: 0.33)

## 审查重点

### 1. WiFi 连接管理
- [ ] 连接/断开流程
- [ ] 重连机制
- [ ] 网络事件处理
- [ ] 省电策略

### 2. 主程序架构
- [ ] 初始化顺序是否合理
- [ ] 任务创建和调度
- [ ] 错误恢复机制
- [ ] 看门狗配置

### 3. 头文件接口设计
- [ ] API设计是否清晰
- [ ] 文档注释是否完整
- [ ] 参数校验是否充分
- [ ] 返回值语义是否明确

### 4. 模块依赖
- [ ] 头文件依赖关系是否合理
- [ ] 是否存在循环依赖
- [ ] 公共类型定义位置

### 5. 配置管理
- [ ] sdkconfig 配置是否合理
- [ ] 编译选项是否优化
- [ ] 分区表是否正确

## 主程序初始化流程
```
app_main()
├── nvs_flash_init()
├── wifi_manager_init()
├── uart_driver_init()
├── led_init()
├── ld2410_init()
├── audio_pipeline_init()
├── voice_interaction_init()
├── ai_infer_init()
├── env_watcher_init()
├── priority_arbiter_init()
├── lighting_controller_start()
└── status_led_start()
```

## 审查输出
- [ ] 代码质量评分
- [ ] 发现问题列表（按严重程度分类）
- [ ] 改进建议
- [ ] 接口一致性报告

## 状态
- [ ] 待审查
- [ ] 审查中
- [ ] 已完成
