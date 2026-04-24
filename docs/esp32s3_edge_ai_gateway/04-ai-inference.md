# 04 - AI 推理方案与 PSRAM 内存规划

## 一、AI 推理框架选择

### TensorFlow Lite for Microcontrollers (TFLM)

**选择理由**：
- Zephyr 已有 TFLite Micro 的 west module 支持
- ESP32-S3 的 Xtensa LX7 支持 SIMD 矩阵运算指令，TFLM可利用
- 社区成熟，模型转换工具链完善

### 模型类型

会议室照明采用 **MLP（多层感知机）** 回归模型：

| 参数 | 值 |
|------|-----|
| 输入维度 | 7 (时间+天气+人员+太阳时间) |
| 隐藏层1 | 32神经元，ReLU激活 |
| 隐藏层2 | 16神经元，ReLU激活 |
| 输出层 | 2神经元 (color_temp, brightness)，线性激活 |
| 参数量 | ~1.2K |
| 模型大小 | ~4KB（INT8量化后） |
| 推理延迟 | <5ms (ESP32-S3 @ 240MHz) |

### 网络结构

```
输入层(7) → 全连接(32) → ReLU → 全连接(16) → ReLU → 输出层(2)
```

### 输入特征定义

| 特征名 | 维度 | 范围 | 计算方式 |
|--------|------|------|----------|
| hour_sin | 1 | [-1, 1] | sin(2π × hour / 24) |
| hour_cos | 1 | [-1, 1] | cos(2π × hour / 24) |
| sunset_proximity | 1 | [-1, 1] | 日落临近度（见下方计算） |
| sunrise_hour_norm | 1 | [0, 1] | sunrise_hour / 24 |
| sunset_hour_norm | 1 | [0, 1] | sunset_hour / 24 |
| weather | 1 | [0, 1] | 0=晴, 0.25=少云, 0.5=多云, 0.75=阴, 1=雨/雪 |
| presence | 1 | {0, 1} | 0=无人, 1=有人 (PIR传感器) |

### 日落临近度计算（核心逻辑）

```c
float calculate_sunset_proximity(float hour, float sunset_hour) {
    float hours_to_sunset = sunset_hour - hour;
    
    if (hours_to_sunset > 2.0f) {
        // 距离日落还有2小时以上
        return 0.0f;
    } else if (hours_to_sunset >= 0.0f) {
        // 日落前2小时内，线性增强
        // 例如：距离2小时→0，距离0小时(日落)→1
        return 1.0f - (hours_to_sunset / 2.0f);
    } else if (hours_to_sunset > -2.0f) {
        // 日落后2小时内，保持高亮
        return 1.0f;
    } else {
        // 深夜，逐渐降低
        return fmaxf(0.0f, 1.0f + (hours_to_sunset + 2.0f) / 2.0f);
    }
}
```

### 输出

| 输出 | 范围 | 说明 |
|------|------|------|
| color_temp | 2700~6500 | 色温，单位K |
| brightness | 0~100 | 光强百分比 |

---

## 二、训练与部署流程

### PC端训练

```python
import numpy as np
import tensorflow as tf

def generate_training_data(n_samples=10000):
    """
    基于领域知识生成训练数据：
    - 无人 → 关灯 (brightness=0)
    - 晴天白天 → 基础光强60%
    - 阴天白天 → 光强增强到80% (+25%)
    - 接近日落 → 光强逐渐增强（日落前2小时开始）
    - 深夜 → 限制最大亮度50%
    """
    X, Y = [], []
    
    for _ in range(n_samples):
        # 随机输入
        hour = np.random.uniform(6, 23)  # 6:00 - 23:00
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
            # 基础光强（按时段）
            if hour < 12:
                base_brightness, base_color_temp = 65, 4800  # 上午
            elif hour < sunset_hour - 1:
                base_brightness, base_color_temp = 60, 4200  # 下午
            else:
                base_brightness, base_color_temp = 55, 3500  # 傍晚
            
            # 天气补偿：阴天增强
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

# 训练模型
X, Y = generate_training_data(10000)

model = tf.keras.Sequential([
    tf.keras.layers.Dense(32, activation='relu', input_shape=(7,)),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(2)  # color_temp, brightness
])

model.compile(optimizer='adam', loss='mse')
model.fit(X, Y, epochs=150, batch_size=32, validation_split=0.2)

# 保存模型
model.save('lighting_model.h5')
```

### 模型转换与量化

```python
import tensorflow as tf

# 加载训练好的模型
model = tf.keras.models.load_model('lighting_model.h5')

# 转换为TFLite（INT8量化）
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]

# 提供代表性数据集用于量化校准
def representative_dataset():
    for _ in range(100):
        data = np.random.rand(1, 7).astype(np.float32)
        yield [data]

converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

# 保存
with open('lighting_model_int8.tflite', 'wb') as f:
    f.write(tflite_model)

print(f"Model size: {len(tflite_model)} bytes")

# 转换为C数组
# xxd -i lighting_model_int8.tflite > lighting_model_data.cc
```

### 部署流程

```
PC / Cloud 端:
  1. 构造训练数据 (领域知识)
  2. TensorFlow/Keras 训练模型
  3. INT8 量化
  4. 转换为 TFLite FlatBuffer (.tflite)
  5. 转换为 C 数组 (xxd -> .cc)

ESP32-S3 端:
  6. 模型数组编译进固件 (或存 Flash 分区)
  7. TFLM MicroInterpreter 加载模型
  8. 输入：7维特征 (时间+天气+人员+太阳时间)
  9. 推理 -> 输出 [色温, 光强]
  10. 后处理 -> UART控制驱动板
```

---

## 三、ESP32-S3 TFLM 实现

```c
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "lighting_model_data.h"  // xxd生成的模型数组

// 模型相关
static const tflite::Model* model;
static tflite::MicroInterpreter* interpreter;
static TfLiteTensor* input_tensor;
static TfLiteTensor* output_tensor;

// 张量工作区（必须放在内部SRAM）
static uint8_t tensor_arena[8192];  // 8KB工作内存

// 初始化
void ai_init(void) {
    model = tflite::GetModel(g_lighting_model_data);
    
    static tflite::MicroErrorReporter error_reporter;
    static tflite::AllOpsResolver resolver;
    
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena), &error_reporter);
    interpreter = &static_interpreter;
    
    interpreter->AllocateTensors();
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);
    
    printk("AI model loaded. Input size: %d, Output size: %d\n",
           input_tensor->bytes, output_tensor->bytes);
}

// 特征结构
struct lighting_features {
    float hour_sin;
    float hour_cos;
    float sunset_proximity;
    float sunrise_hour_norm;
    float sunset_hour_norm;
    float weather;
    float presence;
};

// 推理
void ai_infer(const struct lighting_features* features,
              uint16_t* out_color_temp, uint8_t* out_brightness) {
    // 填充输入张量
    input_tensor->data.f[0] = features->hour_sin;
    input_tensor->data.f[1] = features->hour_cos;
    input_tensor->data.f[2] = features->sunset_proximity;
    input_tensor->data.f[3] = features->sunrise_hour_norm;
    input_tensor->data.f[4] = features->sunset_hour_norm;
    input_tensor->data.f[5] = features->weather;
    input_tensor->data.f[6] = features->presence;
    
    // 执行推理
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        printk("AI inference failed!\n");
        return;
    }
    
    // 读取输出
    float color_temp = output_tensor->data.f[0];
    float brightness = output_tensor->data.f[1];
    
    // 后处理
    *out_color_temp = (uint16_t)clamp(color_temp, 2700.0f, 6500.0f);
    *out_brightness = (uint8_t)clamp(brightness, 0.0f, 100.0f);
}

// 完整推理流程
void ai_control_loop(void) {
    // 获取当前时间
    struct tm* tm_info = get_local_time();
    float hour = tm_info->tm_hour + tm_info->tm_min / 60.0f;
    
    // 获取缓存的天气和日落数据
    extern struct weather_cache g_weather;
    extern struct sun_cache g_sun;
    
    // 读取PIR传感器
    bool presence = gpio_get_pir_status();
    
    // 计算特征
    struct lighting_features features = {
        .hour_sin = sinf(hour * 2.0f * PI / 24.0f),
        .hour_cos = cosf(hour * 2.0f * PI / 24.0f),
        .sunset_proximity = calculate_sunset_proximity(hour, g_sun.sunset_hour),
        .sunrise_hour_norm = g_sun.sunrise_hour / 24.0f,
        .sunset_hour_norm = g_sun.sunset_hour / 24.0f,
        .weather = g_weather.weather_code,
        .presence = presence ? 1.0f : 0.0f
    };
    
    // AI推理
    uint16_t color_temp;
    uint8_t brightness;
    ai_infer(&features, &color_temp, &brightness);
    
    // 安全限制
    if (!presence) {
        brightness = 0;  // 无人强制关灯
    }
    if (tm_info->tm_hour >= 22) {
        brightness = MIN(brightness, 50);  // 深夜限制
    }
    
    // 渐变控制（避免跳变）
    smooth_transition(color_temp, brightness);
}
```

---

## 四、内存规划

### PSRAM 8MB 布局

```
PSRAM 8MB 布局
+--------------------------------------+ 0x3C000000
|  模型权重区 (~1MB)                    |
|  - TFLite模型: ~4KB                  |
|  - 预留扩展: ~100KB                  |
|  - 对齐填充                          |
+--------------------------------------+ ~0x3C100000
|  历史数据记录区 (~2MB)                |
|  - 照明事件记录: 10K条 x 32B = 320KB |
|    每条: [timestamp][color_temp]     |
|          [brightness][weather]       |
|          [presence][source]          |
|  - 预留学习数据: ~1.5MB              |
+--------------------------------------+ ~0x3C300000
|  工作内存区 (~1MB)                    |
|  - HTTP响应缓冲: ~4KB                |
|  - JSON解析缓冲: ~2KB                |
|  - UART缓冲: ~1KB                    |
|  - 其他临时分配                      |
+--------------------------------------+ ~0x3C400000
|  预留区 (~4MB)                        |
|  - OTA固件缓冲                       |
|  - 未来扩展                          |
+--------------------------------------+ 0x3C800000
```

### 内部 SRAM 512KB 分配

| 用途 | 大小 | 说明 |
|------|------|------|
| Zephyr内核 | ~64KB | 系统、驱动、中断 |
| 线程栈 | ~16KB | 5个线程合计 |
| TFLM推理激活区 | ~8KB | tensor_arena（必须放SRAM） |
| 特征/输出缓冲 | ~1KB | 模型输入输出缓冲 |
| 堆内存 | ~100KB | 动态分配、WiFi/BLE缓冲 |
| 预留 | ~323KB | 安全余量 |

> **关键**：TFLM的激活张量（arena）必须放在内部SRAM，因为推理过程中频繁随机访问，PSRAM延迟不可接受。模型权重可以放Flash或PSRAM，因为推理时是顺序读取。

---

## 五、AI推理线程设计

```c
#define AI_INFERENCE_INTERVAL_MS 30000  // 30秒

void ai_inference_thread(void *p1, void *p2, void *p3) {
    while (1) {
        // 检查是否有人（有人时才推理）
        if (gpio_get_pir_status()) {
            // 执行AI控制
            ai_control_loop();
        }
        
        // 等待下次推理
        k_sleep(K_MSEC(AI_INFERENCE_INTERVAL_MS));
    }
}

K_THREAD_DEFINE(ai_thread, 8192, ai_inference_thread, 
                NULL, NULL, NULL, 5, 0, 0);
```
