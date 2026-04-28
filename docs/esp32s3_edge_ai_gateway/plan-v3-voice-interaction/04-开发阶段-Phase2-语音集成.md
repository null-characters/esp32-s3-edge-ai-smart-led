# 最小任务单元清单 - Phase 2 语音集成

> ESP-SR 语音识别框架集成

---

## T2.1 ESP-SR 组件集成

### T2.1.1 添加 ESP-SR 依赖
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.1 |
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

### T2.1.2 配置 WakeNet 模型
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.2 |
| **任务名称** | 配置唤醒词模型 |
| **预估工时** | 4h |
| **前置任务** | T2.1.1 |

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

### T2.1.3 配置 MultiNet 模型
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.3 |
| **任务名称** | 配置命令词模型 |
| **预估工时** | 4h |
| **前置任务** | T2.1.2 |

**命令词配置**：
```c
// command_def.h
typedef enum {
    CMD_SCENE_FOCUS,      // "打开专注模式"
    CMD_SCENE_MEETING,    // "打开会议模式"
    CMD_SCENE_PRESENT,    // "打开演示模式"
    CMD_SCENE_RELAX,      // "打开休息模式"
    CMD_BRIGHT_UP,        // "调亮一点"
    CMD_BRIGHT_DOWN,      // "调暗一点"
    CMD_BRIGHT_MAX,       // "亮度调到最大"
    CMD_COLOR_WARM,       // "换成暖光"
    CMD_COLOR_COOL,       // "换成冷光"
    CMD_QUERY_MODE,       // "现在是什么模式"
    CMD_AUTO_RESUME,      // "恢复自动"
} voice_command_t;
```

**验收标准**：
- [ ] 命令词识别正常
- [ ] 支持中文命令

---

## T2.2 唤醒词模块

### T2.2.1 唤醒词检测实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.2.1 |
| **任务名称** | 唤醒词检测模块实现 |
| **预估工时** | 6h |
| **前置任务** | T2.1.3 |

**文件清单**：
- `main/src/voice/wake_word.c`
- `main/include/wake_word.h`

**核心逻辑**：
```c
void wake_word_task(void *arg) {
    while (1) {
        // 1. 从 I2S 读取音频
        i2s_read(audio_buffer, ...);

        // 2. 送入 WakeNet 检测
        int wake_id = wakenet_detect(audio_buffer);

        // 3. 唤醒后切换到命令识别模式
        if (wake_id >= 0) {
            xEventGroupSetBits(va_event_group, WAKE_WORD_DETECTED);
        }
    }
}
```

**验收标准**：
- [ ] 唤醒检测延迟 < 500ms
- [ ] 误唤醒率 < 1次/小时

---

### T2.2.2 灵敏度调优
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.2.2 |
| **任务名称** | 唤醒灵敏度参数调优 |
| **预估工时** | 4h |
| **前置任务** | T2.2.1 |

**测试场景**：
- 安静环境（< 40dB）
- 正常环境（40-60dB）
- 嘈杂环境（> 60dB）

**验收标准**：
- [ ] 安静环境识别率 > 98%
- [ ] 正常环境识别率 > 95%

---

## T2.3 命令词模块

### T2.3.1 命令处理器实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.3.1 |
| **任务名称** | 命令处理器核心实现 |
| **预估工时** | 8h |
| **前置任务** | T2.2.1 |

**文件清单**：
- `main/src/voice/command_handler.c`
- `main/include/command_handler.h`

**核心逻辑**：
```c
void command_handler_task(void *arg) {
    while (1) {
        // 等待唤醒事件
        xEventGroupWaitBits(va_event_group, WAKE_WORD_DETECTED, ...);

        // 进入命令识别模式（5s 超时）
        for (int i = 0; i < 50; i++) {
            int cmd_id = multinet_detect(audio_buffer);
            if (cmd_id >= 0) {
                execute_command(cmd_id);
                break;
            }
            vTaskDelay(100);
        }
    }
}
```

**验收标准**：
- [ ] 命令识别正确
- [ ] 5s 内无命令自动退出

---

### T2.3.2 场景命令实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.3.2 |
| **任务名称** | 场景切换命令实现 |
| **预估工时** | 4h |
| **前置任务** | T2.3.1 |

**场景定义**：
```c
static const scene_config_t scenes[] = {
    {"专注模式", 4000, 60},   // 冷光，中等亮度
    {"会议模式", 5000, 80},   // 自然光，高亮度
    {"演示模式", 3000, 30},   // 暖光，低亮度
    {"休息模式", 2700, 40},   // 暖光，低亮度
};
```

**验收标准**：
- [ ] 4 种场景切换正确
- [ ] 参数设置符合定义

---

## T2.4 语音助手核心

### T2.4.1 语音助手主循环
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.4.1 |
| **任务名称** | 语音助手主循环实现 |
| **预估工时** | 6h |
| **前置任务** | T2.3.2 |

**文件清单**：
- `main/src/voice/voice_assistant.c`
- `main/include/voice_assistant.h`

**状态机**：
```
IDLE → WAITING_WAKE → LISTENING_CMD → EXECUTING → IDLE
```

**验收标准**：
- [ ] 状态转换正确
- [ ] 超时机制有效

---

## 任务依赖图

```
T2.1.1 → T2.1.2 → T2.1.3
                      │
                      ↓
                 T2.2.1 → T2.2.2
                      │
                      ↓
                 T2.3.1 → T2.3.2
                      │
                      ↓
                 T2.4.1
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M2.1 ESP-SR 集成 | T2.1.1 - T2.1.3 | 第1周 |
| M2.2 唤醒词模块 | T2.2.1 - T2.2.2 | 第2周 |
| M2.3 命令词模块 | T2.3.1 - T2.3.2 | 第2周 |
| M2.4 语音助手 | T2.4.1 | 第3周 |

---

*创建日期：2026-04-27*