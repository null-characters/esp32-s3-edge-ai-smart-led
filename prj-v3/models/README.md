# prj-v3 AI 模型

本目录包含 prj-v3 多模态推理所需的 TensorFlow Lite 模型。

## 模型列表

| 模型 | 类型 | 输入 | 输出 | 大小 (INT8) | 用途 |
|------|------|------|------|-------------|------|
| `sound_classifier.tflite` | CNN | MFCC (40×32×1) | 5 类概率 | 12 KB | 声音场景分类 |
| `radar_analyzer.tflite` | MLP | 8 维特征 | 6 维编码 | 3.6 KB | 雷达特征编码 |
| `fusion_model.tflite` | MLP | 19 维融合 | 2 维决策 | 5 KB | 多模态融合决策 |
| **总计** | - | - | - | **~21 KB** | - |

## 模型详情

### sound_classifier.tflite

**功能**: 将环境音分类为 5 种场景

| 类别 | 名称 | 描述 |
|------|------|------|
| 0 | silence | 寂静 |
| 1 | single_speech | 单人讲话 |
| 2 | multi_speech | 多人讨论 |
| 3 | keyboard | 键盘敲击 |
| 4 | other_noise | 其他噪音 |

**模型结构**:
```
Conv2D(8, 3×3) → MaxPool(2×2) →
Conv2D(16, 3×3) → MaxPool(2×2) →
Conv2D(32, 3×3) → GlobalAvgPool →
Dense(5, softmax)
```

### radar_analyzer.tflite

**功能**: 将 8 维雷达特征编码为 6 维紧凑特征

**输入特征** (归一化到 [0, 1]):
- distance: 目标距离 0-8m
- velocity: 目标速度 -5~+5 m/s
- energy: 目标能量 0-100
- target_count: 目标数量 0-8
- motion_amplitude: 运动幅度 0-1
- motion_period: 运动周期 0-10s
- distance_variance: 距离方差 0-5
- energy_trend: 能量趋势 -1~+1

**模型结构**:
```
Dense(16, relu) → Dense(12, relu) → Dense(6, linear)
```

### fusion_model.tflite

**功能**: 融合声音、雷达、环境特征，输出控制决策

**输入** (19 维):
- 声音分类概率: 5 维
- 雷达编码特征: 6 维
- 环境特征: 8 维 (hour_sin, hour_cos, sunset_proximity, sunrise_norm, sunset_norm, weather, presence, day_type)

**输出** (2 维):
- 色温: 2700-6500K (输出值 × 3800 + 2700)
- 亮度: 0-100% (输出值 × 100)

**模型结构**:
```
Dense(32, relu) → Dense(16, relu) → Dense(2, sigmoid)
```

## 训练脚本

训练脚本位于 `../ai/` 目录:

```bash
# 安装依赖
pip install -r ../ai/requirements.txt

# 训练所有模型
python ../ai/train_all.py

# 单独训练
python ../ai/sound_classifier_train.py
python ../ai/radar_analyzer_train.py
python ../ai/fusion_model_train.py
```

## 使用示例

```c
#include "model_loader.h"

// 加载模型
model_handle_t *sound_model = model_load("sound_classifier.tflite");
model_handle_t *radar_model = model_load("radar_analyzer.tflite");
model_handle_t *fusion_model = model_load("fusion_model.tflite");

// 声音分类
float mfcc[40 * 32];  // MFCC 特征
float sound_probs[5];
model_invoke(sound_model, mfcc, sound_probs);

// 雷达编码
float radar_feat[8];
float radar_encoded[6];
model_invoke(radar_model, radar_feat, radar_encoded);

// 融合决策
float fusion_input[19];
float fusion_output[2];
model_invoke(fusion_model, fusion_input, fusion_output);

// 反归一化
uint16_t color_temp = (uint16_t)(fusion_output[0] * 3800 + 2700);
uint8_t brightness = (uint8_t)(fusion_output[1] * 100);
```

---

*生成日期: 2026-04-28*
*训练框架: TensorFlow 2.21 + TFLite INT8 量化*
