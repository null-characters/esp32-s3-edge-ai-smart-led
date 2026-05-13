# 最小任务单元清单 - Phase 4 多模态融合

> 雷达触发对话 + 声音场景联动 + 多设备唤醒选主

---

## T4.1 雷达触发对话

### T4.1.1 LD2410 停留检测
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.1.1 |
| **任务名称** | LD2410 雷达停留检测实现 |
| **预估工时** | 4h |
| **前置任务** | Phase 3 完成 |

**文件清单**：
- `main/src/sensors/presence_detector.c`
- `main/include/presence_detector.h`

**核心逻辑**：
```c
#define PRESENCE_THRESHOLD_CM  150   // 1.5m 内检测
#define PRESENCE_DURATION_MS   3000  // 持续 3s 判定停留

typedef struct {
    bool presence;
    float distance;
    int duration_ms;
} presence_state_t;

static presence_state_t presence_state = {0};

void ld2410_process(radar_data_t *radar) {
    if (radar->distance < PRESENCE_THRESHOLD_CM && radar->energy > ENERGY_THRESHOLD) {
        if (!presence_state.presence) {
            presence_state.presence = true;
            presence_state.duration_ms = 0;
        }
        
        presence_state.duration_ms += FRAME_INTERVAL_MS;
        presence_state.distance = radar->distance;
        
        // 持续时间超过阈值，触发停留事件
        if (presence_state.duration_ms >= PRESENCE_DURATION_MS) {
            event_group_set(PRESENCE_STAY_EVENT);
        }
    } else {
        presence_state.presence = false;
        presence_state.duration_ms = 0;
    }
}
```

**验收标准**：
- [ ] 距离检测精度 ±10cm
- [ ] 停留判定正确
- [ ] 误触发率 < 5%

---

### T4.1.2 MQTT 事件触发
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.1.2 |
| **任务名称** | 雷达事件 MQTT 上报 |
| **预估工时** | 2h |
| **前置任务** | T4.1.1 |

**核心逻辑**：
```c
void presence_event_handler(void *arg) {
    presence_state_t *state = (presence_state_t *)arg;
    
    // 构建状态 JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "presence", state->presence);
    cJSON_AddNumberToObject(root, "distance", state->distance);
    cJSON_AddNumberToObject(root, "duration", state->duration_ms / 1000);
    
    char *json_str = cJSON_PrintUnformatted(root);
    
    // 发布到 MQTT
    mqtt_client_publish("node/{device_id}/radar/status", json_str, 1);
    
    cJSON_Delete(root);
    free(json_str);
}
```

**验收标准**：
- [ ] 状态上报正常
- [ ] JSON 格式正确

---

### T4.1.3 主动问候 Prompt 设计
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.1.3 |
| **任务名称** | 主动问候场景 Prompt 设计 |
| **预估工时** | 4h |
| **前置任务** | T4.1.2 |

**Prompt 模板**：
```python
GREETING_PROMPT = """当前场景：
- 用户刚进入房间，距离设备 {distance}cm
- 时间：{time_context}

请生成一个简短友好的问候语（不超过 20 字），例如：
- "欢迎回来，需要开灯吗？"
- "晚上好，有什么可以帮您的？"

注意：
- 不要过于频繁打扰用户
- 根据时间段调整问候语风格
- 输出 JSON 格式"""
```

**验收标准**：
- [ ] Prompt 设计合理
- [ ] 问候语自然

---

### T4.1.4 问候频率控制
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.1.4 |
| **任务名称** | 问候频率冷却控制实现 |
| **预估工时** | 2h |
| **前置任务** | T4.1.3 |

**核心逻辑**：
```c
#define GREETING_COOLDOWN_MS  300000  // 5 分钟冷却

static int64_t last_greeting_time = 0;

bool can_greet(void) {
    int64_t now = esp_timer_get_time() / 1000;  // ms
    
    if (now - last_greeting_time >= GREETING_COOLDOWN_MS) {
        last_greeting_time = now;
        return true;
    }
    
    return false;
}
```

**验收标准**：
- [ ] 5 分钟内不重复问候
- [ ] 冷却时间可配置

---

## T4.2 声音场景联动

### T4.2.1 TFLM 声音分类迁移
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.1 |
| **任务名称** | TFLM 声音分类模型迁移 |
| **预估工时** | 4h |
| **前置任务** | Phase 3 完成 |

**文件清单**：
- `main/src/audio/sound_classifier.c`
- `main/include/sound_classifier.h`

**模型规格**：
- `sound_classifier.tflite` (~30KB)
- 分类：keyboard, speech, silence, other

**验收标准**：
- [ ] 模型加载成功
- [ ] 分类准确率 > 85%

---

### T4.2.2 场景状态上报
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.2 |
| **任务名称** | 声音场景 MQTT 上报 |
| **预估工时** | 2h |
| **前置任务** | T4.2.1 |

**核心逻辑**：
```c
void sound_classified_handler(const char *class, float confidence) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "class", class);
    cJSON_AddNumberToObject(root, "confidence", confidence);
    
    char *json_str = cJSON_PrintUnformatted(root);
    mqtt_client_publish("node/{device_id}/sound/status", json_str, 1);
    
    cJSON_Delete(root);
    free(json_str);
}
```

**验收标准**：
- [ ] 状态上报正常
- [ ] 置信度正确

---

### T4.2.3 LLM Prompt 上下文注入
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.3 |
| **任务名称** | LLM Prompt 上下文注入实现 |
| **预估工时** | 4h |
| **前置任务** | T4.2.2 |

**核心逻辑**：
```python
def build_context_prompt(radar_state, sound_state, time_context):
    """构建带上下文的系统 Prompt"""
    
    context_map = {
        "keyboard": "用户正在专注工作，可能需要专注模式照明",
        "speech": "用户正在说话，可能需要语音交互",
        "silence": "环境安静，用户可能在休息",
    }
    
    return SYSTEM_PROMPT.format(
        radar_state=f"有人{'近距离' if radar_state['distance'] < 100 else '远距离'}停留",
        sound_state=context_map.get(sound_state["class"], "未知场景"),
        time_context=time_context,
    )
```

**验收标准**：
- [ ] 上下文注入正确
- [ ] Prompt 语义合理

---

### T4.2.4 专注模式自动触发
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.4 |
| **任务名称** | 专注模式自动触发实现 |
| **预估工时** | 4h |
| **前置任务** | T4.2.3 |

**核心逻辑**：
```python
def auto_scene_decision(radar_state, sound_state):
    """根据场景自动决策照明模式"""
    
    # 键盘声 + 近距离停留 → 专注模式
    if sound_state["class"] == "keyboard" and radar_state["distance"] < 100:
        return {
            "scene": "focus",
            "brightness": 70,
            "color_temp": 4000,
            "reason": "检测到用户正在专注工作"
        }
    
    # 静音 + 近距离停留 → 休息模式
    if sound_state["class"] == "silence" and radar_state["distance"] < 100:
        return {
            "scene": "relax",
            "brightness": 40,
            "color_temp": 2700,
            "reason": "检测到用户正在休息"
        }
    
    return None  # 不自动触发
```

**验收标准**：
- [ ] 专注模式触发正确
- [ ] 用户可关闭自动触发

---

## T4.3 多设备唤醒选主

### T4.3.1 音频能量上报
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.3.1 |
| **任务名称** | 音频能量计算与上报 |
| **预估工时** | 2h |
| **前置任务** | Phase 3 完成 |

**核心逻辑**：
```c
float calculate_audio_energy(int16_t *samples, size_t count) {
    int64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += (int64_t)samples[i] * samples[i];
    }
    return sqrtf((float)sum / count) / 32768.0f;  // 归一化到 [0, 1]
}

void wake_energy_report(float energy) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", DEVICE_ID);
    cJSON_AddNumberToObject(root, "energy", energy);
    
    char *json_str = cJSON_PrintUnformatted(root);
    mqtt_client_publish("election/{room_id}/vote", json_str, 1);
    
    cJSON_Delete(root);
    free(json_str);
}
```

**验收标准**：
- [ ] 能量计算正确
- [ ] 上报延迟 < 100ms

---

### T4.3.2 选主算法实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.3.2 |
| **任务名称** | 多设备唤醒选主算法实现 |
| **预估工时** | 4h |
| **前置任务** | T4.3.1 |

**核心逻辑**：
```python
import asyncio
from collections import defaultdict

class WakeElection:
    def __init__(self, room_id: str):
        self.room_id = room_id
        self.votes = {}
        self.election_timeout = 0.5  # 500ms 收集窗口
        
    async def collect_votes(self):
        """收集所有设备的投票"""
        await asyncio.sleep(self.election_timeout)
        
        if not self.votes:
            return None
        
        # 能量最高的设备胜出
        winner = max(self.votes.items(), key=lambda x: x[1]["energy"])
        
        # 广播结果
        result = {
            "winner": winner[0],
            "devices": list(self.votes.keys()),
            "energy": winner[1]["energy"]
        }
        
        mqtt_publish(f"election/{self.room_id}/result", result)
        return winner[0]
    
    def on_vote(self, device_id: str, energy: float):
        """收到投票"""
        self.votes[device_id] = {"energy": energy}
```

**验收标准**：
- [ ] 选主延迟 < 500ms
- [ ] 能量高者胜出

---

### T4.3.3 选主结果同步
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.3.3 |
| **任务名称** | 选主结果同步与状态恢复 |
| **预估工时** | 2h |
| **前置任务** | T4.3.2 |

**核心逻辑**：
```c
void election_result_handler(const char *winner_id) {
    if (strcmp(winner_id, DEVICE_ID) == 0) {
        // 本设备胜出，建立 WebSocket 连接
        ESP_LOGI(TAG, "Election won, establishing WebSocket connection");
        ws_client_connect();
    } else {
        // 其他设备胜出，进入待机状态
        ESP_LOGI(TAG, "Election lost, entering standby");
        ws_client_disconnect();
    }
}
```

**验收标准**：
- [ ] 胜出设备正常工作
- [ ] 其他设备待机

---

## 任务依赖图

```
T4.1.1 ─── T4.1.2 ─── T4.1.3 ─── T4.1.4

T4.2.1 ─── T4.2.2 ─── T4.2.3 ─── T4.2.4

T4.3.1 ─── T4.3.2 ─── T4.3.3
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M4.1 雷达触发 | T4.1.1 - T4.1.4 | 第1周 |
| M4.2 声音联动 | T4.2.1 - T4.2.4 | 第1周 |
| M4.3 多设备选主 | T4.3.1 - T4.3.3 | 第1周 |

---

*创建日期：2026-05-13*