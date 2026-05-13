# 最小任务单元清单 - Phase 3 固件重构

> ESP32-S3 固件改造：剥离 MultiNet + WebSocket + JSON 处理 + 情感光效

---

## T3.1 ESP-SR 重构

### T3.1.1 保留 WakeNet 配置
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.1.1 |
| **任务名称** | WakeNet 唤醒词模块保留配置 |
| **预估工时** | 2h |
| **前置任务** | Phase 2 完成 |

**配置文件** `sdkconfig.defaults`：
```ini
# ESP-SR 配置 - 仅保留 WakeNet
CONFIG_ESP_SR_ENABLE=y
CONFIG_ESP_SR_WAKENET_ENABLE=y
CONFIG_ESP_SR_WAKENET_MODEL="wn_xiaobaitong"
CONFIG_ESP_SR_WAKENET_MODE=wakenet_only
CONFIG_ESP_SR_WAKENET_THRESHOLD=0.5

# MultiNet 禁用
CONFIG_ESP_SR_MULTINET_ENABLE=n
CONFIG_ESP_SR_RECOGNITION_ENABLE=n
```

**验收标准**：
- [ ] WakeNet 编译通过
- [ ] MultiNet 组件不参与编译
- [ ] 唤醒词识别率 > 95%

---

### T3.1.2 移除 MultiNet 组件
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.1.2 |
| **任务名称** | MultiNet 命令词组件移除 |
| **预估工时** | 2h |
| **前置任务** | T3.1.1 |

**执行步骤**：
```bash
# 1. 修改 CMakeLists.txt，移除 multinet 相关源文件
# 2. 修改 sdkconfig.defaults，禁用 CONFIG_ESP_SR_MULTINET_ENABLE
# 3. 移除 command_handler.c 中的 multinet 调用
# 4. 清理编译缓存并重新编译
idf.py fullclean
idf.py reconfigure
idf.py build
```

**验收标准**：
- [ ] 编译无 MultiNet 相关错误
- [ ] 固件大小减少 > 200KB
- [ ] 内存占用减少 > 100KB

---

### T3.1.3 唤醒事件回调重构
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.1.3 |
| **任务名称** | 唤醒词事件回调重构 |
| **预估工时** | 4h |
| **前置任务** | T3.1.2 |

**文件清单**：
- `main/src/voice/wake_word.c`
- `main/include/wake_word.h`

**核心逻辑**：
```c
// 唤醒后触发 WebSocket 连接
static void wake_word_callback(int wake_id, float confidence) {
    ESP_LOGI(TAG, "Wake word detected, id=%d, confidence=%.2f", wake_id, confidence);
    
    // 1. 播放提示音
    audio_player_play_tone(TONE_WAKE);
    
    // 2. 设置情感光效（thinking）
    emotion_led_set(EMOTION_THINKING);
    
    // 3. 建立 WebSocket 连接
    ws_client_connect();
    
    // 4. 开始音频上行
    audio_upstream_start();
}

void wake_word_task(void *arg) {
    esp_sr_wakenet_config_t wakenet_cfg = {
        .model_name = "wn_xiaobuitong",
        .threshold = 0.5,
        .callback = wake_word_callback,
    };
    
    esp_sr_wakenet_init(&wakenet_cfg);
    
    while (1) {
        // WakeNet 内部循环处理
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**验收标准**：
- [ ] 唤醒检测延迟 < 500ms
- [ ] 回调触发正确
- [ ] WebSocket 连接建立

---

### T3.1.4 VAD 触发逻辑修改
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.1.4 |
| **任务名称** | VAD 静音检测触发逻辑修改 |
| **预估工时** | 4h |
| **前置任务** | T3.1.3 |

**核心逻辑**：
```c
// VAD 检测到静音后发送音频到服务器
#define VAD_SILENCE_THRESHOLD_MS  1000  // 1s 静音判定

static int silence_duration_ms = 0;

void vad_process(int16_t *samples, size_t count) {
    float energy = calculate_energy(samples, count);
    
    if (energy < ENERGY_THRESHOLD) {
        silence_duration_ms += FRAME_DURATION_MS;
        
        if (silence_duration_ms >= VAD_SILENCE_THRESHOLD_MS) {
            // 静音超过阈值，发送音频到服务器
            audio_upstream_flush();
            silence_duration_ms = 0;
        }
    } else {
        silence_duration_ms = 0;
    }
}
```

**验收标准**：
- [ ] VAD 静音检测正确
- [ ] 音频发送时机正确

---

## T3.2 WebSocket 客户端

### T3.2.1 WebSocket 连接管理
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.2.1 |
| **任务名称** | WebSocket 连接管理模块实现 |
| **预估工时** | 4h |
| **前置任务** | T3.1.3 |

**文件清单**：
- `main/src/network/ws_client.c`
- `main/include/ws_client.h`

**状态机**：
```c
typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_STREAMING,
    WS_STATE_ERROR,
} ws_state_t;

static ws_state_t ws_state = WS_STATE_DISCONNECTED;

esp_err_t ws_client_connect(void) {
    if (ws_state != WS_STATE_DISCONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ws_state = WS_STATE_CONNECTING;
    
    esp_err_t ret = esp_websocket_client_open(ws_client);
    
    if (ret == ESP_OK) {
        ws_state = WS_STATE_CONNECTED;
        // 发送心跳
        ws_send_heartbeat();
    } else {
        ws_state = WS_STATE_ERROR;
    }
    
    return ret;
}
```

**验收标准**：
- [ ] 连接状态机正确
- [ ] 连接延迟 < 500ms

---

### T3.2.2 音频流上行传输
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.2.2 |
| **任务名称** | 音频流上行传输实现 |
| **预估工时** | 6h |
| **前置任务** | T3.2.1 |

**文件清单**：
- `main/src/audio/audio_upstream.c`
- `main/include/audio_upstream.h`

**核心逻辑**：
```c
#define FRAME_SIZE_SAMPLES  320   // 20ms @ 16kHz
#define FRAME_HEADER_SIZE   4
#define FRAME_PAYLOAD_SIZE  (FRAME_SIZE_SAMPLES * 2)  // 16-bit

static uint8_t frame_seq = 0;

void audio_upstream_send_frame(int16_t *samples, size_t count) {
    uint8_t frame[FRAME_HEADER_SIZE + FRAME_PAYLOAD_SIZE];
    
    // Header
    frame[0] = 0x01;  // AUDIO_UPSTREAM
    frame[1] = frame_seq++;
    frame[2] = (count * 2) >> 8;  // Length high
    frame[3] = (count * 2) & 0xFF;  // Length low
    
    // Payload
    memcpy(&frame[4], samples, count * 2);
    
    // 发送
    esp_websocket_client_send_bin(ws_client, frame, sizeof(frame), portMAX_DELAY);
}

void audio_upstream_task(void *arg) {
    int16_t samples[FRAME_SIZE_SAMPLES];
    
    while (ws_state == WS_STATE_STREAMING) {
        // 从 I2S 读取音频
        i2s_read(samples, FRAME_SIZE_SAMPLES);
        
        // 发送到服务器
        audio_upstream_send_frame(samples, FRAME_SIZE_SAMPLES);
    }
}
```

**验收标准**：
- [ ] 20ms/帧发送正常
- [ ] 无丢帧
- [ ] CPU 占用 < 20%

---

### T3.2.3 TTS 流下行接收
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.2.3 |
| **任务名称** | TTS 音频流下行接收与播放 |
| **预估工时** | 4h |
| **前置任务** | T3.2.2 |

**文件清单**：
- `main/src/audio/audio_downstream.c`
- `main/include/audio_downstream.h`

**核心逻辑**：
```c
void ws_data_handler(uint8_t *data, size_t len) {
    uint8_t frame_type = data[0];
    
    switch (frame_type) {
        case 0x02:  // AUDIO_DOWNSTREAM (TTS)
            // 写入 RingBuffer
            ring_buffer_write(&tts_buffer, &data[4], len - 4);
            
            // 触发播放
            if (!audio_player_is_playing()) {
                audio_player_start_from_buffer(&tts_buffer);
            }
            break;
            
        case 0x03:  // CONTROL_CMD (JSON)
            // 解析并执行
            json_parse_and_execute((char *)&data[4]);
            break;
            
        case 0x04:  // HEARTBEAT
            ws_send_heartbeat();
            break;
    }
}
```

**验收标准**：
- [ ] TTS 音频接收正常
- [ ] 播放流畅无断续
- [ ] JSON 解析正确

---

### T3.2.4 断线重连机制
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.2.4 |
| **任务名称** | WebSocket 断线重连机制实现 |
| **预估工时** | 2h |
| **前置任务** | T3.2.3 |

**核心逻辑**：
```c
#define RECONNECT_INTERVAL_MS  5000
#define MAX_RECONNECT_ATTEMPTS  5

static int reconnect_attempts = 0;

void ws_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            ws_state = WS_STATE_DISCONNECTED;
            
            // 尝试重连
            if (reconnect_attempts < MAX_RECONNECT_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RECONNECT_INTERVAL_MS));
                ws_client_connect();
                reconnect_attempts++;
            }
            break;
            
        case WEBSOCKET_EVENT_CONNECTED:
            reconnect_attempts = 0;
            ws_state = WS_STATE_CONNECTED;
            break;
    }
}
```

**验收标准**：
- [ ] 断线自动重连
- [ ] 重连成功后状态恢复

---

## T3.3 JSON 命令处理

### T3.3.1 JSON 解析模块
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.3.1 |
| **任务名称** | JSON 命令解析模块实现 |
| **预估工时** | 4h |
| **前置任务** | Phase 2 完成 |

**文件清单**：
- `main/src/protocol/json_parser.c`
- `main/include/json_parser.h`

**验收标准**：
- [ ] JSON 解析正确
- [ ] 内存安全

---

### T3.3.2 Action 执行器
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.3.2 |
| **任务名称** | 灯光 Action 执行器实现 |
| **预估工时** | 6h |
| **前置任务** | T3.3.1 |

**文件清单**：
- `main/src/control/action_executor.c`
- `main/include/action_executor.h`

**核心逻辑**：
```c
esp_err_t execute_action(const light_action_t *action) {
    switch (action->type) {
        case ACTION_SET_BRIGHTNESS:
            return led_pwm_set_brightness(action->value);
            
        case ACTION_SET_COLOR_TEMP:
            return led_pwm_set_color_temp(action->value);
            
        case ACTION_SET_SCENE:
            return scene_manager_apply(action->value);
            
        case ACTION_TURN_OFF:
            return led_pwm_set_brightness(0);
            
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

void execute_command(const agent_command_t *cmd) {
    // 1. 设置情感光效
    emotion_led_set(cmd->emotion);
    
    // 2. 执行所有 Action
    for (int i = 0; i < cmd->action_count; i++) {
        execute_action(&cmd->actions[i]);
    }
    
    // 3. 播放 TTS（音频已在播放中）
    // 无需额外操作
}
```

**验收标准**：
- [ ] Action 执行正确
- [ ] 多 Action 顺序执行

---

### T3.3.3 情感光效映射
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.3.3 |
| **任务名称** | 情感光效映射模块实现 |
| **预估工时** | 4h |
| **前置任务** | T3.3.2 |

**文件清单**：
- `main/src/led/emotion_led.c`
- `main/include/emotion_led.h`

**验收标准**：
- [ ] 情感光效正确
- [ ] 动画流畅

---

### T3.3.4 错误处理与重试
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.3.4 |
| **任务名称** | JSON 解析错误处理与重试 |
| **预估工时** | 2h |
| **前置任务** | T3.3.3 |

**验收标准**：
- [ ] 错误处理完善
- [ ] 重试机制有效

---

## T3.4 情感光效引擎

### T3.4.1 情感枚举定义
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.4.1 |
| **任务名称** | 情感类型与颜色枚举定义 |
| **预估工时** | 1h |
| **前置任务** | T3.3.2 |

**文件清单**：
- `main/include/emotion_def.h`

**验收标准**：
- [ ] 枚举定义完整

---

### T3.4.2 呼吸动画实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.4.2 |
| **任务名称** | 呼吸动画实现 |
| **预估工时** | 4h |
| **前置任务** | T3.4.1 |

**文件清单**：
- `main/src/led/anims/breathe.c`

**验收标准**：
- [ ] 2s 周期呼吸
- [ ] 平滑过渡

---

### T3.4.3 流水动画实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.4.3 |
| **任务名称** | 流水动画实现 |
| **预估工时** | 4h |
| **前置任务** | T3.4.1 |

**文件清单**：
- `main/src/led/anims/flow.c`

**验收标准**：
- [ ] 左右流动效果
- [ ] 速度可调

---

### T3.4.4 律动动画实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.4.4 |
| **任务名称** | 律动动画实现 |
| **预估工时** | 4h |
| **前置任务** | T3.4.1 |

**文件清单**：
- `main/src/led/anims/pulse.c`

**验收标准**：
- [ ] 快节奏律动
- [ ] 彩色效果

---

### T3.4.5 动画调度器
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.4.5 |
| **任务名称** | 动画调度器实现 |
| **预估工时** | 4h |
| **前置任务** | T3.4.2, T3.4.3, T3.4.4 |

**文件清单**：
- `main/src/led/animation_scheduler.c`

**验收标准**：
- [ ] 动画切换平滑
- [ ] 独立任务运行

---

## T3.5 MQTT 状态上报

### T3.5.1 MQTT 客户端初始化
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.5.1 |
| **任务名称** | MQTT 客户端初始化与连接 |
| **预估工时** | 2h |
| **前置任务** | Phase 2 完成 |

**验收标准**：
- [ ] MQTT 连接成功
- [ ] 订阅主题正确

---

### T3.5.2 雷达状态上报
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.5.2 |
| **任务名称** | 雷达状态 MQTT 上报 |
| **预估工时** | 2h |
| **前置任务** | T3.5.1 |

**验收标准**：
- [ ] 雷达数据上报正常
- [ ] JSON 格式正确

---

### T3.5.3 声音状态上报
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.5.3 |
| **任务名称** | 声音分类状态 MQTT 上报 |
| **预估工时** | 2h |
| **前置任务** | T3.5.1 |

**验收标准**：
- [ ] 声音分类上报正常

---

### T3.5.4 心跳机制
| 属性 | 内容 |
|------|------|
| **任务ID** | T3.5.4 |
| **任务名称** | MQTT 心跳机制实现 |
| **预估工时** | 2h |
| **前置任务** | T3.5.1 |

**验收标准**：
- [ ] 30s 心跳间隔
- [ ] 断线检测

---

## 任务依赖图

```
T3.1.1 ─── T3.1.2 ─── T3.1.3 ─── T3.1.4
                          │
                          ↓
T3.2.1 ─── T3.2.2 ─── T3.2.3 ─── T3.2.4
              │
              └──────────────────────────┐
                                         │
T3.3.1 ─── T3.3.2 ─── T3.3.3 ─── T3.3.4  │
                          │              │
                          ↓              │
T3.4.1 ─── T3.4.2 ─── T3.4.5             │
      │          │                       │
      ├── T3.4.3 ┘                       │
      │                                  │
      └── T3.4.4 ────────────────────────┘
                                         │
T3.5.1 ─── T3.5.2 ─── T3.5.3 ─── T3.5.4 ─┘
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M3.1 ESP-SR 重构 | T3.1.1 - T3.1.4 | 第1周 |
| M3.2 WebSocket | T3.2.1 - T3.2.4 | 第1周 |
| M3.3 JSON 处理 | T3.3.1 - T3.3.4 | 第2周 |
| M3.4 情感光效 | T3.4.1 - T3.4.5 | 第2周 |
| M3.5 MQTT 上报 | T3.5.1 - T3.5.4 | 第2周 |

---

*创建日期：2026-05-13*