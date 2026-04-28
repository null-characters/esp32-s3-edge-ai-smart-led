# 最小任务单元清单 - Phase 2 语音集成

> ESP-SR 语音识别框架集成
> **拆分为 Phase 2a + Phase 2b，共 2 周**

---

## Phase 2a: 纯语音控制 LED（1 周）⭐ 快速验证核心风险

> 目标：跳过多模态，直接验证语音命令 → LED 响应的端到端流程

---

### T2a.1 ESP-SR 基础集成

#### T2a.1.1 添加 ESP-SR 依赖
| 属性 | 内容 |
|------|------|
| **任务ID** | T2a.1.1 |
| **任务名称** | 添加 ESP-SR 组件依赖 |
| **预估工时** | 2h |
| **前置任务** | Phase 1 完成 |

**执行步骤**：
```bash
cd prj-v3
idf.py add-dependency "espressif/esp-sr^1.5.0"
```

**验收标准**：
- [ ] `idf_component.yml` 包含 esp-sr 依赖
- [ ] `idf.py reconfigure` 成功

---

#### T2a.1.2 配置 WakeNet 模型
| 属性 | 内容 |
|------|------|
| **任务ID** | T2a.1.2 |
| **任务名称** | 配置唤醒词模型 |
| **预估工时** | 4h |
| **前置任务** | T2a.1.1 |

**配置项**：
```ini
# sdkconfig.defaults
CONFIG_ESP_SR_WAKENET_MODEL="wn_xiaobaitong"
CONFIG_ESP_SR_WAKENET_MODE=wakenet_and_recognizer
CONFIG_ESP_SR_WAKENET_THRESHOLD=0.5
```

**验收标准**：
- [ ] 唤醒词模型加载成功
- [ ] 模型大小 < 200KB

---

#### T2a.1.3 配置 MultiNet 基础命令
| 属性 | 内容 |
|------|------|
| **任务ID** | T2a.1.3 |
| **任务名称** | 配置基础命令词 |
| **预估工时** | 4h |
| **前置任务** | T2a.1.2 |

**基础命令配置**：
```c
// voice_led_control.h
typedef enum {
    CMD_BRIGHT_UP,      // "调亮一点"
    CMD_BRIGHT_DOWN,    // "调暗一点"
    CMD_COLOR_WARM,     // "换成暖光"
    CMD_COLOR_COOL,     // "换成冷光"
    CMD_SCENE_FOCUS,    // "打开专注模式"
    CMD_AUTO_RESUME,    // "恢复自动"
} voice_led_cmd_t;
```

---

### T2a.2 直接 LED 控制

#### T2a.2.1 语音命令 → PWM 直接映射
| 属性 | 内容 |
|------|------|
| **任务ID** | T2a.2.1 |
| **任务名称** | 语音命令直接控制 LED |
| **预估工时** | 4h |
| **前置任务** | T2a.1.3 |

**文件清单**：
- `main/src/voice/voice_led_control.c`
- `main/include/voice_led_control.h`

**核心逻辑**：
```c
void execute_voice_led_command(voice_led_cmd_t cmd) {
    switch (cmd) {
        case CMD_BRIGHT_UP:
            led_set_brightness(current_brightness + 20);
            break;
        case CMD_COLOR_WARM:
            led_set_color_temp(3000);
            break;
        // ... 直接 PWM 控制
    }
}
```

---

#### T2a.2.2 端到端验证
| 属性 | 内容 |
|------|------|
| **任务ID** | T2a.2.2 |
| **任务名称** | 语音命令 → LED 响应验证 |
| **预估工时** | 4h |
| **前置任务** | T2a.2.1 |

**验收标准**：
- [ ] 唤醒词识别正常
- [ ] 命令词识别正常
- [ ] LED 响应正确
- [ ] 端到端延迟 < 1s

---

## Phase 2b: 完整语音功能（1 周）

> 目标：添加 TTS 语音反馈和 AEC 回声消除

---

### T2b.1 DAC + 扬声器集成

#### T2b.1.1 MAX98357A 驱动实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T2b.1.1 |
| **任务名称** | DAC 驱动实现 |
| **预估工时** | 4h |

**文件清单**：
- `main/src/drivers/dac_driver.c`
- `main/include/dac_driver.h`

**接口定义**：
```c
esp_err_t dac_init(i2s_port_t i2s_port);  // 使用独立 I2S1
esp_err_t dac_write(int16_t *data, size_t samples);
```

> ⚠️ **重要**：DAC 和麦克风使用独立 I2S 通道（I2S0/I2S1），避免时钟冲突。

---

#### T2b.1.2 扬声器测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T2b.1.2 |
| **任务名称** | 扬声器音频输出验证 |
| **预估工时** | 2h |

**验收标准**：
- [ ] I2S1 输出正常
- [ ] 音频无杂音

---

### T2b.2 AEC 回声消除配置

#### T2b.2.1 ESP-ADF AEC 流水线配置
| 属性 | 内容 |
|------|------|
| **任务ID** | T2b.2.1 |
| **任务名称** | AEC 流水线实现 |
| **预估工时** | 6h |

**文件清单**：
- `main/src/voice/aec_pipeline.c`
- `main/include/aec_pipeline.h`

> ⚠️ **硬件闭环关键**：AEC 需要硬件提供一路 Reference（参考音频）信号，
> 严格按 ESP-ADF 示例配置，否则设备自身语音会被麦克风录入造成误识别。

---

#### T2b.2.2 AEC 效果验证
| 属性 | 内容 |
|------|------|
| **任务ID** | T2b.2.2 |
| **任务名称** | 回声消除测试 |
| **预估工时** | 4h |

**测试场景**：
- 设备播放 TTS 时唤醒词不应触发
- 设备播放 TTS 时命令词不应被误识别

---

### T2b.3 VAD 动态路由

#### T2b.3.1 VAD 检测实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T2b.3.1 |
| **任务名称** | VAD 检测模块 |
| **预估工时** | 4h |

**文件清单**：
- `main/src/voice/vad_detector.c`

**核心逻辑**：
```c
// VAD 动态路由：分时复用麦克风流
if (vad_detected()) {
    audio_route_to(ESP_SR);   // 有人声 → 语音识别
} else {
    audio_route_to(TFLM);     // 无人声 → 多模态推理
}
```

---

#### T2b.3.2 麦克风流动态路由
| 属性 | 内容 |
|------|------|
| **任务ID** | T2b.3.2 |
| **任务名称** | 音频路由器实现 |
| **预估工时** | 6h |

**文件清单**：
- `main/src/voice/audio_router.c`

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M2a.1 ESP-SR 基础 | T2a.1.1 - T2a.1.3 | 第1周前半 |
| M2a.2 端到端验证 | T2a.2.1 - T2a.2.2 | 第1周后半 |
| M2b.1 DAC 集成 | T2b.1.1 - T2b.1.2 | 第2周前半 |
| M2b.2 AEC 配置 | T2b.2.1 - T2b.2.2 | 第2周中 |
| M2b.3 VAD 路由 | T2b.3.1 - T2b.3.2 | 第2周后半 |

---

*创建日期：2026-04-27*
*版本：v2.0*
*最后更新：2026-04-28*