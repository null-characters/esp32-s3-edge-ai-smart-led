# 06 - 会议室智能照明 MVP 方案（增强版）

## 场景概述

将 ESP32-S3 部署在会议室，实现基于多维度环境感知的智能照明控制：

- **输入维度**：时间、天气、太阳起落时间、人员存在
- **输出维度**：色温（2700K~6500K）、光强（0%~100%）

### 典型场景

| 场景 | 晴天光强 | 阴天光强 | 说明 |
|------|----------|----------|------|
| 上午会议 | 60% | 80% | 阴天需要补偿+20% |
| 下午3点 | 55% | 75% | 接近日落，开始增强 |
| 下午6点(日落前) | 70% | 85% | 日落临近，光强增强 |
| 晚上7点 | 65% | 80% | 日落后保持高亮 |

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         数据采集层                                       │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐         │
│  │    手机 App     │  │   天气 API      │  │  太阳起落 API   │         │
│  │  - 推送天气     │  │  - wttr.in      │  │  - 日出日落计算 │         │
│  │  - 推送位置     │  │  - 心知天气     │  │                 │         │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘         │
│           │                    │                    │                  │
│           └────────────────────┼────────────────────┘                  │
│                                │                                       │
│                                ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                    ESP32-S3 边缘AI网关                           │  │
│  │                                                                  │  │
│  │  ┌───────────┐  ┌───────────┐  ┌───────────┐                    │  │
│  │  │ BLE/WiFi  │  │ PIR传感器 │  │ RTC时钟   │                    │  │
│  │  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘                    │  │
│  │        │              │              │                          │  │
│  │        ▼              ▼              ▼                          │  │
│  │  ┌───────────────────────────────────────────────────────────┐  │  │
│  │  │               特征工程层（7维特征）                        │  │  │
│  │  │  hour_sin, hour_cos, sunset_proximity, sunrise_hour,     │  │  │
│  │  │  sunset_hour, weather, presence                          │  │  │
│  │  └─────────────────────────┬─────────────────────────────────┘  │  │
│  │                            │                                     │  │
│  │                            ▼                                     │  │
│  │  ┌───────────────────────────────────────────────────────────┐  │  │
│  │  │           TFLM推理引擎（MLP: 7→32→16→2）                  │  │  │
│  │  │              输出: [色温, 光强]                            │  │  │
│  │  └─────────────────────────┬─────────────────────────────────┘  │  │
│  │                            │                                     │  │
│  │                            ▼                                     │  │
│  │  ┌───────────────────────────────────────────────────────────┐  │  │
│  │  │              后处理 + 安全限制                             │  │  │
│  │  │  - 无人强制关灯 / 深夜限幅 / 渐变过渡                      │  │  │
│  │  └─────────────────────────┬─────────────────────────────────┘  │  │
│  └────────────────────────────┼────────────────────────────────────┘  │
│                               │                                        │
└───────────────────────────────┼────────────────────────────────────────┘
                                │ 串口/PWM
                                ▼
                       ┌────────────────┐
                       │    驱动板       │
                       │  色温+亮度调节  │
                       └────────────────┘
```

---

## 数据获取方案

### 方案A：手机App推送（推荐）

**数据格式**：
```json
{
  "timestamp": 1704067200,
  "location": {"lat": 39.9042, "lon": 116.4074},
  "weather": {"condition": "cloudy", "cloud_cover": 60},
  "sun": {"sunrise": "06:45", "sunset": "18:30"}
}
```

### 方案B：ESP32主动获取

```c
// 天气API: GET https://wttr.in/Beijing?format=%C+%t
// 日出日落API: GET https://api.sunrise-sunset.org/json?lat=39.9&lng=116.4
```

**更新频率**：
| 数据 | 频率 | 说明 |
|------|------|------|
| 天气 | 1小时 | 变化较慢 |
| 日落时间 | 每天1次 | 固定 |
| 人员传感器 | 500ms | 实时响应 |
| AI推理 | 30秒 | 平衡响应与功耗 |

---

## 特征工程

### 输入特征（7维）

```
┌─────────────────────────────────────────────────────────────┐
│ 时间特征 (3维)                                               │
├─────────────────────────────────────────────────────────────┤
│ hour_sin = sin(2π × hour / 24)                              │
│ hour_cos = cos(2π × hour / 24)                              │
│ sunset_proximity:                                           │
│   - 日落前2小时开始: 0 → 1                                    │
│   - 日落后2小时减弱: 1 → -1                                   │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ 太阳特征 (2维)                                               │
├─────────────────────────────────────────────────────────────┤
│ sunrise_hour → 日出时间 (0-24)                               │
│ sunset_hour  → 日落时间 (0-24)                               │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ 环境特征 (2维)                                               │
├─────────────────────────────────────────────────────────────┤
│ weather编码: 0=晴, 0.25=少云, 0.5=多云, 0.75=阴, 1=雨/雪     │
│ presence: 0=无人, 1=有人                                     │
└─────────────────────────────────────────────────────────────┘
```

### 特征计算

```c
// 时间特征
float hour = get_current_hour() + get_current_minute() / 60.0f;
float hour_sin = sinf(hour * 2.0f * PI / 24.0f);
float hour_cos = cosf(hour * 2.0f * PI / 24.0f);

// 日落临近度（核心）
float hours_to_sunset = sunset_hour - hour;
float sunset_proximity;

if (hours_to_sunset > 2.0f) {
    sunset_proximity = 0.0f;  // 正常
} else if (hours_to_sunset >= 0.0f) {
    sunset_proximity = 1.0f - (hours_to_sunset / 2.0f);  // 渐变增强
} else if (hours_to_sunset > -2.0f) {
    sunset_proximity = 1.0f;  // 保持高亮
} else {
    sunset_proximity = max(0.0f, 1.0f + (hours_to_sunset + 2.0f) / 2.0f);
}

// 天气编码
float weather_encode(const char* condition) {
    if (strstr(condition, "晴") || strstr(condition, "Clear")) return 0.0f;
    else if (strstr(condition, "少云") || strstr(condition, "Partly")) return 0.25f;
    else if (strstr(condition, "多云") || strstr(condition, "Cloudy")) return 0.5f;
    else if (strstr(condition, "阴") || strstr(condition, "Overcast")) return 0.75f;
    else return 1.0f;
}
```

---

## 模型设计

### 网络结构

```
输入层(7) → 全连接(32) → ReLU → 全连接(16) → ReLU → 输出层(2)
```

| 参数 | 值 |
|------|-----|
| 参数量 | ~1.2K |
| 模型大小 | ~4KB（INT8量化后） |
| 推理延迟 | <5ms |

### 训练数据生成

```python
import numpy as np
import tensorflow as tf

def generate_training_data(n_samples=10000):
    X, Y = [], []
    
    for _ in range(n_samples):
        hour = np.random.uniform(6, 23)
        weather = np.random.choice([0, 0.25, 0.5, 0.75, 1.0], 
                                   p=[0.3, 0.2, 0.25, 0.15, 0.1])
        presence = np.random.choice([0, 1], p=[0.3, 0.7])
        sunrise_hour = np.random.uniform(5.5, 7.0)
        sunset_hour = np.random.uniform(17.5, 19.5)
        
        # 特征编码
        hour_sin = np.sin(hour * 2 * np.pi / 24)
        hour_cos = np.cos(hour * 2 * np.pi / 24)
        
        # 日落临近度
        hours_to_sunset = sunset_hour - hour
        if hours_to_sunset > 2:
            sunset_proximity = 0.0
        elif hours_to_sunset >= 0:
            sunset_proximity = 1.0 - (hours_to_sunset / 2)
        elif hours_to_sunset > -2:
            sunset_proximity = 1.0
        else:
            sunset_proximity = max(0, 1.0 + (hours_to_sunset + 2) / 2)
        
        X.append([hour_sin, hour_cos, sunset_proximity, 
                  sunrise_hour/24, sunset_hour/24, weather, presence])
        
        # 输出计算（领域规则）
        if presence == 0:
            color_temp, brightness = 4000, 0
        else:
            # 基础光强
            if hour < 12:
                base_brightness, base_color_temp = 65, 4800  # 上午
            elif hour < sunset_hour - 1:
                base_brightness, base_color_temp = 60, 4200  # 下午
            else:
                base_brightness, base_color_temp = 55, 3500  # 傍晚
            
            # 天气补偿：阴天增强20-25%
            weather_boost = weather * 25
            
            # 日落补偿：越接近日落越亮
            sunset_boost = sunset_proximity * 15
            
            # 深夜限制
            max_brightness = 50 if hour >= 22 else 100
            
            brightness = min(max_brightness, 
                           base_brightness + weather_boost + sunset_boost)
            color_temp = base_color_temp
            
        Y.append([color_temp, brightness])
    
    return np.array(X), np.array(Y)

# 训练
X, Y = generate_training_data(10000)

model = tf.keras.Sequential([
    tf.keras.layers.Dense(32, activation='relu', input_shape=(7,)),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(2)
])

model.compile(optimizer='adam', loss='mse')
model.fit(X, Y, epochs=150, batch_size=32, validation_split=0.2)

# 量化导出
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
with open('lighting_model.tflite', 'wb') as f:
    f.write(converter.convert())
```

---

## ESP32-S3 推理实现

```c
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "lighting_model_data.h"

static tflite::MicroInterpreter* interpreter;
static TfLiteTensor* input_tensor;
static TfLiteTensor* output_tensor;

void ai_init(void) {
    auto model = tflite::GetModel(g_lighting_model_data);
    static tflite::MicroErrorReporter error_reporter;
    static tflite::AllOpsResolver resolver;
    static uint8_t tensor_arena[8192];
    
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena), &error_reporter);
    interpreter = &static_interpreter;
    
    interpreter->AllocateTensors();
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);
}

void ai_infer_lighting(float hour_sin, float hour_cos, float sunset_proximity,
                       float sunrise_hour_norm, float sunset_hour_norm,
                       float weather, float presence,
                       uint16_t* out_color_temp, uint8_t* out_brightness) {
    // 填充输入
    input_tensor->data.f[0] = hour_sin;
    input_tensor->data.f[1] = hour_cos;
    input_tensor->data.f[2] = sunset_proximity;
    input_tensor->data.f[3] = sunrise_hour_norm;
    input_tensor->data.f[4] = sunset_hour_norm;
    input_tensor->data.f[5] = weather;
    input_tensor->data.f[6] = presence;
    
    // 推理
    interpreter->Invoke();
    
    // 读取输出
    float color_temp = output_tensor->data.f[0];
    float brightness = output_tensor->data.f[1];
    
    // 后处理
    if (presence < 0.5f) {
        brightness = 0;  // 无人强制关灯
    }
    if (get_current_hour() >= 22) {
        brightness = min(brightness, 50.0f);  // 深夜限制
    }
    
    *out_color_temp = (uint16_t)clamp(color_temp, 2700.0f, 6500.0f);
    *out_brightness = (uint8_t)clamp(brightness, 0.0f, 100.0f);
}
```

---

## 决策优先级

```
1. 传感器无人 > 5分钟  → 强制关灯（安全约束，不可覆盖）
2. 用户手动调整        → 立即执行，记录到学习队列
3. AI预测开灯          → 预开到规则值
4. AI学习偏好          → 微调（±20%范围内）
5. 规则引擎            → 默认行为
```

---

## MVP分阶段实施

### Phase 1：规则驱动（1周）

```c
void lighting_control_loop(void) {
    bool presence = read_presence_sensor();
    float hour = get_current_hour();
    float weather = get_weather();  // 0-1
    float sunset_hour = get_sunset_hour();
    
    if (!presence) {
        if (no_presence_duration > 5 * 60 * 1000) {
            set_light(0, 0);
        }
        return;
    }
    
    // 基础光强
    int base = (hour < 12) ? 65 : (hour < sunset_hour - 1) ? 60 : 55;
    
    // 天气补偿：阴天+20%
    int weather_boost = (int)(weather * 25);
    
    // 日落补偿：日落前2小时开始增强
    float hours_to_sunset = sunset_hour - hour;
    int sunset_boost = (hours_to_sunset < 2 && hours_to_sunset > -2) ? 15 : 0;
    
    int brightness = min(100, base + weather_boost + sunset_boost);
    int color_temp = (hour < 12) ? 4800 : (hour < sunset_hour) ? 4200 : 3500;
    
    set_light(color_temp, brightness);
}
```

### Phase 2：数据采集 + WiFi/App（2周）

```c
struct lighting_event {
    uint32_t timestamp;
    uint8_t  event_type;      // 0=人进入, 1=人离开, 2=手动调光
    uint16_t color_temp;
    uint8_t  brightness;
    float    weather;
    float    sunset_proximity;
};

// BLE接收App数据
void ble_receive_weather_data(const uint8_t* data, uint16_t len) {
    // 解析JSON，更新全局天气/日落时间
}

// WiFi主动获取
void wifi_fetch_data(void) {
    fetch_weather();      // 每小时
    fetch_sunset_time();  // 每天
}
```

### Phase 3：AI推理集成（2周）

```c
void ai_control_loop(void) {
    // 获取所有特征
    float hour = get_current_hour();
    float features[7] = {
        sinf(hour * 2 * PI / 24),
        cosf(hour * 2 * PI / 24),
        calculate_sunset_proximity(),
        get_sunrise_hour() / 24.0f,
        get_sunset_hour() / 24.0f,
        get_weather(),
        (float)read_presence_sensor()
    };
    
    // AI推理
    uint16_t color_temp;
    uint8_t brightness;
    ai_infer_lighting(features[0], features[1], features[2], features[3],
                      features[4], features[5], features[6],
                      &color_temp, &brightness);
    
    // 渐变控制，避免跳变
    smooth_transition(color_temp, brightness);
}
```

---

## 模型迭代路线

```
阶段1: 规则拟合模型（当前）
       训练数据 = 领域专家定义的规则
       模型学习 = 规则的平滑插值

阶段2: 用户反馈学习
       记录用户手动调整事件
       用户调亮 → 模型学习"此场景需更亮"
       在线微调或定期重训练

阶段3: 完全数据驱动
       积累足够真实数据后
       用用户行为数据重新训练
       发现人类未发现的规律
```

---

## 价值总结

| 维度 | 传统规则 | 边缘AI |
|------|----------|--------|
| 多维度输入 | 大量if-else，难以维护 | 神经网络天然支持 |
| 天气补偿 | 硬编码阈值 | 平滑渐变过渡 |
| 日落适应 | 阶梯式跳变 | 连续渐变增强 |
| 可扩展性 | 加特征要改代码 | 加输入维度即可 |
| 个性化 | 不支持 | 可学习用户偏好 |

**核心价值**：天气(晴/阴) + 太阳起落时间 + 人员感知 → AI推理 → 自适应光强/色温调节


