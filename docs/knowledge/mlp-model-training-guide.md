# ESP32-S3 边缘 AI 模型训练指南

> 本文档记录智能照明网关项目的 MLP 模型训练完整流程，包括环境配置、数据生成、模型训练、INT8 量化和 C 数组导出。

---

## 目录

- [概述](#概述)
- [环境配置](#环境配置)
- [训练流水线](#训练流水线)
- [模型规格](#模型规格)
- [精度评估](#精度评估)
- [常见问题](#常见问题)

---

## 概述

### 项目背景

本项目使用 MLP（多层感知机）处理一维数值型传感器数据（PIR 状态、时间、天气），输出智能照明控制参数（色温、亮度）。

### 设计决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 网络结构 | MLP | 一维数值数据，无需 CNN/Transformer |
| 量化方式 | 训练后 INT8 量化 | 简单快捷，精度损失可接受 |
| 数据来源 | 合成数据 | MVP 快速启动，零成本 |

### 目标指标

| 指标 | 目标值 | 实际值 |
|------|--------|--------|
| 模型大小 | < 5 KB | 4.4 KB ✅ |
| 亮度 MAE | < 5% | 1.1% ✅ |
| 色温 MAE | < 50K | 27.8K ✅ |

---

## 环境配置

### 系统要求

- Python 3.12（推荐）
- TensorFlow 2.21+
- macOS / Linux / Windows

### 环境问题

**macOS ARM + Python 3.13 兼容性问题**：

TensorFlow 2.21 在 macOS ARM + Python 3.13 环境下会出现段错误（exit code 139）。解决方案是使用 Python 3.12 的 conda 环境。

### 安装步骤

```bash
# 方案一：使用 conda 创建 Python 3.12 环境（推荐）
conda create -n esp32-ai python=3.12 -y
conda activate esp32-ai
pip install tensorflow numpy pandas

# 方案二：直接安装（需要 Python 3.11-3.12）
cd prj/ai
pip install -r requirements.txt
```

### 依赖清单

```
# requirements.txt
tensorflow>=2.12.0
numpy>=1.24.0
pandas>=2.0.0
```

---

## 训练流水线

### 流程图

```
┌─────────────────────────────────────────────────────────────────┐
│                    AI 训练流水线                                  │
│                                                                 │
│  ① 数据生成                                                      │
│     generate_data.py                                            │
│     └─► 15K 合成样本 (PIR + 时间 + 天气 → 色温 + 亮度)            │
│                                                                 │
│  ② 模型训练                                                      │
│     train_model.py                                              │
│     └─► MLP: 7 → 32 → 16 → 2                                    │
│     └─► Adam 优化器, MSE 损失, Early Stopping                    │
│                                                                 │
│  ③ INT8 量化                                                    │
│     └─► Post-Training Quantization                              │
│     └─► Float32 → INT8 (体积缩小 4x)                             │
│                                                                 │
│  ④ 精度验证                                                      │
│     └─► 对比 Float32 vs INT8 误差                                │
│                                                                 │
│  ⑤ C 数组导出                                                    │
│     └─► model_data.h (ESP32-S3 直接使用)                         │
└─────────────────────────────────────────────────────────────────┘
```

### 步骤一：生成训练数据

```bash
cd prj/ai
python generate_data.py
```

**输出**：
- `training_data.csv` - CSV 格式（人类可读）
- `training_data.npz` - NumPy 格式（快速加载）

**数据规格**：

| 特征 (X) | 类型 | 范围 | 说明 |
|----------|------|------|------|
| hour_sin | float | [-1, 1] | 时间正弦编码 |
| hour_cos | float | [-1, 1] | 时间余弦编码 |
| sunset_proximity | float | [0, 1] | 日落临近度 |
| sunrise_hour_norm | float | [0, 1] | 日出时间归一化 |
| sunset_hour_norm | float | [0, 1] | 日落时间归一化 |
| weather | float | [0, 1] | 天气阴晴程度 |
| presence | float | {0, 1} | PIR 人体检测 |

| 标签 (Y) | 类型 | 范围 | 说明 |
|----------|------|------|------|
| color_temp | int | [2700, 6500] | 色温 (K) |
| brightness | int | [0, 100] | 亮度 (%) |

**数据生成规则**：

```python
# 核心规则逻辑
if presence == 0:
    brightness = 0      # 无人关灯
    color_temp = 2700   # 暖色待机
else:
    # 时段基础值
    if hour < 12:
        base_brightness = 65, base_color_temp = 4800  # 上午
    elif hour < sunset_hour - 1:
        base_brightness = 60, base_color_temp = 4200  # 下午
    else:
        base_brightness = 55, base_color_temp = 3500  # 傍晚

    # 天气补偿：阴天增强
    brightness += weather * 25

    # 日落补偿：越接近日落越亮
    brightness += sunset_proximity * 15

    # 深夜限制
    if hour >= 22:
        brightness = min(brightness, 50)
```

### 步骤二：训练模型

```bash
# 使用 conda 环境
conda activate esp32-ai
python train_model.py
```

**模型结构**：

```
┌─────────────────────────────────────────────────────────────────┐
│                    MLP 网络结构                                   │
├─────────────────┬───────────────┬───────────────────────────────┤
│ Layer           │ Output Shape  │ Params                        │
├─────────────────┼───────────────┼───────────────────────────────┤
│ hidden_1 (Dense)│ (None, 32)    │ 256  (7×32 + 32 bias)         │
│ hidden_2 (Dense)│ (None, 16)    │ 528  (32×16 + 16 bias)        │
│ output (Dense)  │ (None, 2)     │ 34   (16×2 + 2 bias)          │
├─────────────────┼───────────────┼───────────────────────────────┤
│ Total           │               │ 818 params (3.2 KB)           │
└─────────────────┴───────────────┴───────────────────────────────┘
```

**训练配置**：

| 参数 | 值 |
|------|-----|
| 优化器 | Adam (lr=0.001) |
| 损失函数 | MSE |
| 批大小 | 32 |
| 最大轮数 | 150 |
| 早停 | patience=15 |
| 学习率衰减 | factor=0.5, patience=5 |
| 验证集比例 | 20% |

**输出文件**：

| 文件 | 大小 | 说明 |
|------|------|------|
| `lighting_model.h5` | ~41 KB | Keras 原始模型 |
| `lighting_model_float.tflite` | ~5.3 KB | Float32 TFLite |
| `lighting_model_int8.tflite` | ~4.4 KB | INT8 量化模型 |
| `model_data.h` | 4472 bytes | C 数组头文件 |
| `normalization_params.txt` | - | 输出归一化参数 |

---

## 模型规格

### 文件大小对比

```
┌─────────────────────────────────────────────────────────────────┐
│                    模型体积变化                                   │
│                                                                 │
│  Keras H5         Float32 TFLite    INT8 TFLite                 │
│  ┌────────────┐   ┌────────────┐   ┌────────────┐              │
│  │   41 KB    │ ─►│   5.3 KB   │ ─►│   4.4 KB   │              │
│  └────────────┘   └────────────┘   └────────────┘              │
│       │                │                │                       │
│       │                │                └─► 量化后体积           │
│       │                │                    缩小为原来的 1/4     │
│       │                │                                       │
│       └────────────────┴─► TFLite 转换去除元数据                 │
│                                                                 │
│  目标: < 5 KB ───────────────────────────────► ✅ 达成           │
└─────────────────────────────────────────────────────────────────┘
```

### C 数组格式

```c
// model_data.h
const unsigned int g_lighting_model_data_len = 4472;
const alignas(16) unsigned char g_lighting_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, ...
};
```

### 归一化参数

```
# normalization_params.txt
y_mean_color_temp=3811.6
y_std_color_temp=523.4
y_mean_brightness=54.3
y_std_brightness=28.7
```

推理时需要反归一化：

```c
// ESP32 推理伪代码
float color_temp = output[0] * y_std_color_temp + y_mean_color_temp;
float brightness = output[1] * y_std_brightness + y_mean_brightness;
```

---

## 精度评估

### 量化前后对比

| 指标 | Float32 | INT8 | 量化损失 |
|------|---------|------|---------|
| 色温 MAE | 22.8K | 27.8K | 21.8% |
| 亮度 MAE | 1.1% | 1.1% | 4.6% |

### 精度分析

**亮度**：
- MAE 1.1%，量化损失 4.6% < 5%
- 人眼对亮度 5% 的变化几乎无感知
- ✅ 精度完全满足需求

**色温**：
- MAE 27.8K，量化损失 21.8%
- 人眼对色温 100K 以内的差异几乎无感知
- ✅ 精度满足需求（绝对误差远小于感知阈值）

### 量化风险评估

```
┌─────────────────────────────────────────────────────────────────┐
│                    量化风险评估                                   │
├──────────────────┬────────────────────┬──────────────────────────┤
│ 模型类型          │ 量化风险            │ 说明                     │
├──────────────────┼────────────────────┼──────────────────────────┤
│ 医疗影像 CNN      │ 高（可能不可接受）   │ 细微特征影响诊断          │
│ 语音识别 RNN      │ 中（需评估）        │ 频谱细节丢失              │
│ 智能照明 MLP ✓   │ 极低（可忽略）       │ 输出为连续控制量          │
└──────────────────┴────────────────────┴──────────────────────────┘
```

---

## 常见问题

### Q1: TensorFlow 导入时段错误 (exit code 139)

**原因**：TensorFlow 2.21 在 macOS ARM + Python 3.13 环境下不兼容。

**解决方案**：

```bash
# 使用 Python 3.12 conda 环境
conda create -n esp32-ai python=3.12 -y
conda activate esp32-ai
pip install tensorflow numpy pandas
```

### Q2: 如何重新训练模型？

```bash
cd prj/ai
conda activate esp32-ai

# 重新生成数据（可选）
python generate_data.py

# 训练
python train_model.py
```

### Q3: 如何调整模型精度？

1. **增加训练数据**：修改 `generate_data.py` 中的 `N_SAMPLES`
2. **调整网络结构**：修改 `train_model.py` 中的 `build_model()`
3. **使用混合量化**：对输出层保留 Float16（需修改量化配置）

### Q4: 如何验证模型在 ESP32 上的推理效果？

1. 编译固件，烧录到 ESP32-S3
2. 通过串口输入测试数据
3. 观察输出是否合理
4. 测量推理延迟（预期 < 1ms）

---

## 文件索引

```
prj/ai/
├── generate_data.py           # 数据生成脚本
├── train_model.py             # 训练脚本
├── requirements.txt           # Python 依赖
├── training_data.csv          # 训练数据 (CSV)
├── training_data.npz          # 训练数据 (NumPy)
├── lighting_model.h5          # Keras 模型
├── lighting_model_float.tflite # Float32 TFLite
├── lighting_model_int8.tflite # INT8 TFLite
└── normalization_params.txt   # 归一化参数

prj/include/
└── model_data.h               # C 数组头文件
```

---

## 参考资料

- [edge-ai-brainstorming-summary.md](edge-ai-brainstorming-summary.md) - 边缘 AI 架构脑暴纪要
- [edge-ai-architecture-deep-dive.md](edge-ai-architecture-deep-dive.md) - 边缘 AI 架构深度解析
- [TensorFlow Lite 量化指南](https://www.tensorflow.org/lite/performance/post_training_quantization)

---

*维护者：null-characters*
*创建日期：2026-04-25*
*版本：v1.0*
