# 最小任务单元清单 - Phase 2 通信协议

> MQTT Broker + WebSocket 音频流 + JSON 控制协议

---

## T2.1 MQTT Broker

### T2.1.1 Mosquitto 配置
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.1 |
| **任务名称** | Mosquitto MQTT Broker 安装与配置 |
| **预估工时** | 2h |
| **前置任务** | Phase 1 完成 |

**执行步骤**：
```bash
# Docker 部署 Mosquitto
docker run -d \
  --name mosquitto \
  -p 1883:1883 \
  -p 9001:9001 \
  -v /opt/mosquitto/config:/mosquitto/config \
  -v /opt/mosquitto/data:/mosquitto/data \
  -v /opt/mosquitto/log:/mosquitto/log \
  eclipse-mosquitto
```

**配置文件** `/opt/mosquitto/config/mosquitto.conf`：
```
listener 1883
allow_anonymous true
max_connections -1

# WebSocket 支持
listener 9001
protocol websockets
```

**验收标准**：
- [ ] MQTT Broker 运行在 1883 端口
- [ ] WebSocket 端口 9001 正常
- [ ] 客户端连接成功

---

### T2.1.2 Topic 结构定义
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.2 |
| **任务名称** | MQTT Topic 结构设计与文档化 |
| **预估工时** | 2h |
| **前置任务** | T2.1.1 |

**Topic 结构**：
```
设备状态上报：
  node/{device_id}/radar/status     → {"presence": true, "distance": 120}
  node/{device_id}/sound/status     → {"class": "keyboard", "confidence": 0.92}
  node/{device_id}/light/status     → {"brightness": 70, "color_temp": 4000}
  node/{device_id}/system/heartbeat → {"uptime": 3600, "free_heap": 45000}

指令下发：
  cmd/{device_id}/light/control     → {"action": "set_brightness", "value": 80}
  cmd/{device_id}/audio/tts         → {"text": "好的", "emotion": "happy"}
  cmd/{device_id}/system/config     → {"volume": 80, "wake_sensitivity": 0.5}

唤醒选主：
  election/{room_id}/wakeup         → {"device_id": "A", "timestamp": 1234567890}
  election/{room_id}/vote           → {"device_id": "A", "energy": 0.85}
  election/{room_id}/result         → {"winner": "A", "devices": ["A", "B"]}
```

**验收标准**：
- [ ] Topic 结构文档化
- [ ] 命名规范一致

---

### T2.1.3 ACL 权限配置
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.3 |
| **任务名称** | MQTT ACL 访问控制配置 |
| **预估工时** | 1h |
| **前置任务** | T2.1.2 |

**ACL 配置**：
```
# /mosquitto/config/acl
user esp32_device
topic read cmd/+/+/+
topic write node/+/+/+
topic read election/+/+
topic write election/+/+

user edge_server
topic read node/+/+/+
topic write cmd/+/+/+
topic read election/+/+
topic write election/+/+
```

**验收标准**：
- [ ] ACL 规则生效
- [ ] 权限隔离正确

---

### T2.1.4 ESP32 MQTT 客户端
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.1.4 |
| **任务名称** | ESP32 MQTT 客户端实现 |
| **预估工时** | 4h |
| **前置任务** | T2.1.2 |

**文件清单**：
- `main/src/network/mqtt_client.c`
- `main/include/mqtt_client.h`

**接口定义**：
```c
esp_err_t mqtt_client_init(const mqtt_config_t *config);
esp_err_t mqtt_client_start(void);
esp_err_t mqtt_client_publish(const char *topic, const char *payload, int qos);
esp_err_t mqtt_client_subscribe(const char *topic, int qos);
void mqtt_client_set_callback(mqtt_event_callback_t callback);
```

**验收标准**：
- [ ] 连接 Broker 成功
- [ ] 订阅/发布正常
- [ ] 断线重连机制

---

## T2.2 WebSocket 音频流

### T2.2.1 WebSocket 端点集成到 Agent Backend
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.2.1 |
| **任务名称** | WebSocket 端点集成到 Agent Backend |
| **预估工时** | 3h |
| **前置任务** | Phase 1 完成 |

> **架构说明**：不另设独立的 WebSocket Server。Agent Backend (FastAPI) 直接承载 `/ws/audio/{device_id}` WebSocket 端点，减少一次内部 HTTP 中继，降低延迟与故障点。

**文件清单**：
- `agent_backend/app/main.py`（路由配置）

**核心代码**：
```python
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI()

@app.websocket("/ws/audio/{device_id}")
async def audio_stream(websocket: WebSocket, device_id: str):
    await websocket.accept()
    
    opus_decoder = OpusDecoder(sample_rate=16000, channels=1)
    pcm_buffer = bytearray()
    
    try:
        while True:
            # 接收音频帧
            data = await websocket.receive_bytes()
            
            frame_type = data[0]      # Type
            codec_type = data[1]      # Codec (0x00=PCM, 0x01=Opus)
            frame_len  = (data[2] << 8) | data[3]
            flags      = data[4]
            
            payload = data[5:5+frame_len]
            
            if frame_type == 0x01:  # AUDIO_UPSTREAM
                # 解码（支持 PCM/Opus）
                if codec_type == 0x01:  # Opus
                    pcm = opus_decoder.decode(payload)
                else:
                    pcm = payload
                
                pcm_buffer.extend(pcm)
                
                # VAD 检测静音/EOS，送入 STT
                if (flags & 0x01) or (flags & 0x02):
                    text = await stt_service.transcribe(bytes(pcm_buffer))
                    pcm_buffer.clear()
                    
                    # 送入 LLM（含会话管理）
                    context = session_mgr.build_context(device_id)
                    response = await llm_service.chat(text, context)
                    session_mgr.add_message(device_id, "assistant", response["text"])
                    
                    # 送入 TTS
                    audio = await tts_service.synthesize(response["text"])
                    
                    # 下发 TTS 音频 + JSON
                    await websocket.send_bytes(bytes([0x02, 0x00]) + audio)
                    await websocket.send_json(response)
                    
    except WebSocketDisconnect:
        pass
```

**验收标准**：
- [ ] WebSocket 连接建立成功
- [ ] 音频流双向传输正常
- [ ] Opus 解码正确
- [ ] 连接保活机制

---

### T2.2.2 音频帧结构定义
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.2.2 |
| **任务名称** | WebSocket 音频帧协议定义 |
| **预估工时** | 2h |
| **前置任务** | T2.2.1 |

**帧结构**：
```
┌────────────────────────────────────────────────────────────┐
│                    WebSocket 音频帧                         │
├────────────────────────────────────────────────────────────┤
│  Header (4 bytes):                                         │
│  ┌────────┬────────┬────────┬────────┐                    │
│  │ Type   │ Seq    │ Length │ Flags  │                    │
│  │ 1byte  │ 1byte  │ 2bytes │ 1byte  │                    │
│  └────────┴────────┴────────┴────────┘                    │
│                                                            │
│  Payload: PCM 音频数据 (16kHz, 16-bit, mono)              │
│  建议每帧 320 samples (20ms)                               │
│                                                            │
│  Type 枚举:                                                │
│  0x01 = AUDIO_UPSTREAM   (ESP32 → Server)                 │
│  0x02 = AUDIO_DOWNSTREAM (Server → ESP32, TTS)            │
│  0x03 = CONTROL_CMD      (JSON 控制指令)                   │
│  0x04 = HEARTBEAT        (连接保活)                        │
└────────────────────────────────────────────────────────────┘
```

**验收标准**：
- [ ] 帧结构文档化
- [ ] 解析正确

---

### T2.2.3 ESP32 WebSocket 客户端
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.2.3 |
| **任务名称** | ESP32 WebSocket 客户端实现 |
| **预估工时** | 6h |
| **前置任务** | T2.2.2 |

**文件清单**：
- `main/src/network/ws_client.c`
- `main/include/ws_client.h`

**接口定义**：
```c
esp_err_t ws_client_init(const ws_config_t *config);
esp_err_t ws_client_connect(void);
esp_err_t ws_client_send_audio(const int16_t *samples, size_t count);
esp_err_t ws_client_close(void);
void ws_client_set_callback(ws_event_callback_t callback);
```

**验收标准**：
- [ ] 连接 WebSocket 服务端成功
- [ ] 音频帧发送正常
- [ ] TTS 音频接收正常

---

### T2.2.4 Opus 编解码层定义
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.2.4 |
| **任务名称** | WebSocket 帧内 Opus 编解码协议定义 |
| **预估工时** | 2h |
| **前置任务** | T2.2.2 |

**协议定义**：
```
WebSocket 帧 Header Codec 字段:
  0x00 = PCM（原始 16kHz 16-bit mono，调试用）
  0x01 = Opus（默认，生产环境使用）

编码参数:
  - 采样率: 16kHz
  - 声道: mono
  - 码率: 24kbps
  - 帧长: 20ms（对应 320 PCM samples → ~60 Opus bytes）
  - 复杂度: 0（ESP32-S3 低功耗模式）

带宽对比:
  PCM:  256 kbps → 16000×16bit=256000bps
  Opus:  24 kbps → ~1/10

ESP32-S3 支持情况:
  - ESP-ADF 包含 Opus 编码器组件
  - 专用硬件加速编码指令
  - 额外内存 ~8KB
```

**验收标准**：
- [ ] Opus 协议文档定义完整
- [ ] 带宽压降 256kbps → 24kbps

---

### T2.3.1 Function Calling JSON Schema 定义
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.3.1 |
| **任务名称** | Agent 命令 JSON Schema 定义 |
| **预估工时** | 4h |
| **前置任务** | T2.1.2 |

**JSON Schema**：
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "AgentCommand",
  "type": "object",
  "required": ["text", "emotion", "actions"],
  "properties": {
    "text": {
      "type": "string",
      "description": "TTS 播放文本"
    },
    "emotion": {
      "type": "string",
      "enum": ["happy", "thinking", "neutral", "apologetic", "excited"],
      "description": "情感标签，驱动光效"
    },
    "actions": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["type"],
        "properties": {
          "type": {
            "type": "string",
            "enum": ["set_brightness", "set_color_temp", "set_scene", "turn_off"]
          },
          "value": { "type": "integer" }
        }
      }
    },
    "context": {
      "type": "object",
      "properties": {
        "radar_state": { "type": "string" },
        "sound_state": { "type": "string" }
      }
    }
  }
}
```

**验收标准**：
- [ ] JSON Schema 定义完整
- [ ] 支持校验

---

### T2.3.2 情感标签枚举
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.3.2 |
| **任务名称** | 情感标签与光效映射定义 |
| **预估工时** | 2h |
| **前置任务** | T2.3.1 |

**情感映射表**：
```c
typedef enum {
    EMOTION_HAPPY,       // 青色呼吸 → 用户请求成功执行
    EMOTION_THINKING,    // 橙色流水 → LLM 正在处理
    EMOTION_NEUTRAL,     // 白色常亮 → 待机状态
    EMOTION_APOLOGETIC,  // 黄色慢闪 → 执行失败
    EMOTION_EXCITED,     // 彩色律动 → 主动问候
} emotion_type_t;

typedef struct {
    emotion_type_t type;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    animation_type_t animation;
} emotion_led_map_t;

static const emotion_led_map_t emotion_map[] = {
    {EMOTION_HAPPY,      0,   255, 255, ANIM_BREATHE},
    {EMOTION_THINKING,   255, 165, 0,   ANIM_FLOW},
    {EMOTION_NEUTRAL,    255, 255, 255, ANIM_STATIC},
    {EMOTION_APOLOGETIC, 255, 255, 0,   ANIM_BLINK},
    {EMOTION_EXCITED,    255, 0,   255, ANIM_PULSE},
};
```

**验收标准**：
- [ ] 情感枚举定义完整
- [ ] 光效映射正确

---

### T2.3.3 ESP32 JSON 解析器
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.3.3 |
| **任务名称** | ESP32 JSON 命令解析器实现 |
| **预估工时** | 4h |
| **前置任务** | T2.3.2 |

**文件清单**：
- `main/src/protocol/json_parser.c`
- `main/include/json_parser.h`

**接口定义**：
```c
typedef struct {
    char text[256];
    emotion_type_t emotion;
    int action_count;
    light_action_t actions[8];
} agent_command_t;

esp_err_t json_parse_command(const char *json_str, agent_command_t *cmd);
esp_err_t json_parse_action(const cJSON *action_obj, light_action_t *action);
```

**验收标准**：
- [ ] JSON 解析正确
- [ ] 内存安全
- [ ] 错误处理完善

---

### T2.3.4 JSON Schema 校验
| 属性 | 内容 |
|------|------|
| **任务ID** | T2.3.4 |
| **任务名称** | JSON 格式校验模块实现 |
| **预估工时** | 2h |
| **前置任务** | T2.3.3 |

**验收标准**：
- [ ] Schema 校验正确
- [ ] 格式错误自动重试

---

## 任务依赖图

```
T2.1.1 ─── T2.1.2 ─── T2.1.3 ─── T2.1.4
              │
              └──────────────────────────┐
                                         │
T2.2.1 ─── T2.2.2 ─── T2.2.3 ─── T2.2.4  │
              │                          │
              └──────────────────────────┼── T2.3.4
                                         │
T2.3.1 ─── T2.3.2 ─── T2.3.3 ────────────┘
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M2.1 MQTT Broker | T2.1.1 - T2.1.4 | 第1周 |
| M2.2 WebSocket | T2.2.1 - T2.2.4 | 第1周 |
| M2.3 JSON 协议 | T2.3.1 - T2.3.4 | 第1周 |

---

*创建日期：2026-05-13*