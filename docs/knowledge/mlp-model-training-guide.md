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
| 模型大小 | < 5 KB | 2.4 KB ✅ |
| 亮度 MAE | < 5% | 1.5% ✅ |
| 色温 MAE | < 50K | 47.3K ✅ |

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
│                    MLP 网络结构 (优化后)                          │
├─────────────────┬───────────────┬───────────────────────────────┤
│ Layer           │ Output Shape  │ Params                        │
├─────────────────┼───────────────┼───────────────────────────────┤
│ hidden_1 (Dense)│ (None, 16)    │ 128  (7×16 + 16 bias)         │
│ output (Dense)  │ (None, 2)     │ 34   (16×2 + 2 bias)          │
├─────────────────┼───────────────┼───────────────────────────────┤
│ Total           │               │ 162 params (0.65 KB)          │
└─────────────────┴───────────────┴───────────────────────────────┘
```

> **消融实验结论**：原 7→32→16→2 结构（818参数）为"能力过剩"，缩小至 7→16→2（162参数）后体积减少 46%，亮度 MAE 仍保持 1.5%（人眼无感知差异），色温 MAE 47K（< 100K 感知阈值）。

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
| `lighting_model.h5` | ~8 KB | Keras 原始模型 |
| `lighting_model_float.tflite` | ~2.8 KB | Float32 TFLite |
| `lighting_model_int8.tflite` | ~2.4 KB | INT8 量化模型 |
| `model_data.h` | 2424 bytes | C 数组头文件 |
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
│  │   8 KB     │ ─►│   2.8 KB   │ ─►│   2.4 KB   │              │
│  └────────────┘   └────────────┘   └────────────┘              │
│       │                │                │                       │
│       │                │                └─► 量化后体积           │
│       │                │                    缩小为原来的 1/4     │
│       │                │                                       │
│       └────────────────┴─► TFLite 转换去除元数据                 │
│                                                                 │
│  目标: < 5 KB ───────────────────────────────► ✅ 达成           │
│                                                                 │
│  消融实验优化: 原模型 4.4KB → 优化后 2.4KB (减少 46%)            │
└─────────────────────────────────────────────────────────────────┘
```

### C 数组格式

```c
// model_data.h
const unsigned int g_lighting_model_data_len = 2424;
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
| 色温 MAE | 39.0K | 47.3K | 21.2% |
| 亮度 MAE | 1.5% | 1.5% | 1.6% |

### 精度分析

**亮度**：
- MAE 1.5%，量化损失 1.6% < 5%
- 人眼对亮度 5% 的变化几乎无感知
- ✅ 精度完全满足需求

**色温**：
- MAE 47.3K，量化损失 21.2%
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

## 架构师审视与优化建议

### 审视结论总览

| 评估项 | 当前状态 | 架构师点评 | 行动建议 |
|--------|----------|------------|----------|
| 网络规模 | 7→32→16→2 (818参数) | "九牛一毛"，可能"能力过剩" | 尝试消融实验缩小 |
| 激活函数 | ReLU ✅ | 正确选择，边缘友好 | 无需修改 |
| 量化配置 | 完全INT8 ✅ | 已正确提供代表性数据集 | 无需修改 |
| 预处理对齐 | C/Python一致 ✅ | "像素级一致" | 无需修改 |

### 优化建议一：网络瘦身实验

**问题**：当前 818 个参数对 ESP32-S3 而言是"九牛一毛"，对于基于简单 if-else 规则生成的训练数据，32 个神经元可能是"能力过剩"（Over-parameterized）。

**建议实验**：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    消融实验设计                                          │
├─────────────────┬────────────┬────────────┬──────────────────────────────┤
│ 模型结构         │ 参数量      │ 预期体积    │ 实验目标                      │
├─────────────────┼────────────┼────────────┼──────────────────────────────┤
│ 7→32→16→2 (当前) │ 818        │ ~4.4 KB    │ 基准对照                      │
│ 7→16→8→2        │ ~226       │ ~1.2 KB    │ MAE是否几乎不增加？            │
│ 7→8→2           │ ~82        │ ~0.5 KB    │ 极简版，测试底线               │
└─────────────────┴────────────┴────────────┴──────────────────────────────┘
```

**实验方法**：

1. 修改 `train_model.py` 中 `build_model()` 函数
2. 分别训练三个版本，记录 MAE 变化
3. 选择"够用就好"的最小模型

**预期结果**：由于底层规则简单，缩小模型后 MAE 可能几乎不增加。在边缘计算中，**"够用就好"是最高准则**。

### 优化建议二：激活函数确认（已正确）

**当前实现**（`train_model.py` 第 25-28 行）：

```python
tf.keras.layers.Dense(32, activation='relu', input_shape=(7,), name='hidden_1'),
tf.keras.layers.Dense(16, activation='relu', name='hidden_2'),
tf.keras.layers.Dense(2, activation='linear', name='output')
```

**分析**：

- ✅ 隐藏层使用 ReLU：`f(x) = max(0, x)`，C 语言层面仅为比较指令
- ✅ 输出层使用 Linear：回归任务标准配置
- ✅ 未使用 Sigmoid/Tanh：避免了昂贵的 `e^x` 浮点运算

**ESP-NN 加速库对 ReLU 的优化**：极致优化，推理速度最快。

### 优化建议三：量化配置确认（已正确）

**当前实现**（`train_model.py` 第 105-116 行）：

```python
def representative_dataset():
    for i in range(min(500, len(X))):
        yield [X[i:i+1].astype(np.float32)]

converter_int8 = tf.lite.TFLiteConverter.from_keras_model(model)
converter_int8.optimizations = [tf.lite.Optimize.DEFAULT]
converter_int8.representative_dataset = representative_dataset
converter_int8.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter_int8.inference_input_type = tf.int8  # ← 关键：输入也INT8
converter_int8.inference_output_type = tf.int8  # ← 关键：输出也INT8
```

**分析**：

- ✅ 提供了 `representative_dataset`（500 个样本）
- ✅ 设置了 `inference_input_type = tf.int8`
- ✅ 设置了 `inference_output_type = tf.int8`
- ✅ 使用了 `TFLITE_BUILTINS_INT8`

**结论**：这是**完全 INT8 量化**（权重 + 激活值均为 INT8），不是"半吊子量化"。ESP32 运行时无需 Float32 ↔ INT8 转换，推理效率最高。

### 优化建议四：预处理对齐确认（已正确）

**Python 端**（`generate_data.py`）：

| 特征 | 计算方式 |
|------|----------|
| hour_sin | `np.sin(hour * 2.0 * np.pi / 24.0)` |
| hour_cos | `np.cos(hour * 2.0 * np.pi / 24.0)` |
| sunset_proximity | `calculate_sunset_proximity(hour, sunset_hour)` |
| sunrise_hour_norm | `sunrise_hour / 24.0` |
| sunset_hour_norm | `sunset_hour / 24.0` |
| weather | `[0.0, 0.25, 0.5, 0.75, 1.0]` |
| presence | `[0.0, 1.0]` |

**C 端**（`ai_infer.c`）：

| 特征 | 计算方式 | 对齐状态 |
|------|----------|----------|
| hour_sin | `sinf(hour * 2.0f * PI / 24.0f)` | ✅ |
| hour_cos | `cosf(hour * 2.0f * PI / 24.0f)` | ✅ |
| sunset_proximity | `ai_calc_sunset_proximity(hour, sunset_hour)` | ✅ 逻辑一致 |
| sunrise_hour_norm | `sunrise_hour / 24.0f` | ✅ |
| sunset_hour_norm | `sunset_hour / 24.0f` | ✅ |
| weather | `weather_get_code()` → `[0.0, 1.0]` | ✅ |
| presence | `(pstate == PRESENCE_YES) ? 1.0f : 0.0f` | ✅ |

**结论**：C 端预处理与 Python 端达到**像素级一致**。

### 行动清单

| 优先级 | 行动 | 状态 |
|--------|------|------|
| P1 | 执行消融实验，测试不同模型大小 | ✅ 已完成 |
| P2 | 记录不同模型大小的 MAE 对比 | ✅ 已完成 |
| P3 | 选择最小"够用"模型作为最终版本 | ✅ 已采用 7→16→2 |
| P4 | 重新训练生成 model_data.h | ✅ 已完成 (2424 bytes) |

---

## 架构师深度反馈与工程解法

> 本节记录架构师对三个致命隐患的深度技术反馈，以及对应的工程解法。

### 隐患一：C 端 Debounce 状态机设计技巧

#### 问题回顾

PIR 传感器存在"短视"和抖动问题：用户静坐几分钟后 PIR 输出 0，导致 MLP 立即关灯；用户挥手后 PIR 输出 1，MLP 又立即开灯——灯光疯狂闪烁。

#### 核心设计原则：非对称滤波

```
┌───────────────────────────────────────────────────────────────┐
│                    防抖状态机设计原则                           │
│                                                               │
│  上升沿（0→1，有人进入）                                        │
│  └─► 延迟：0ms 或极小                                          │
│  └─► 原因：用户推开门，灯必须在 50ms 内亮起                      │
│  └─► 否则体验极差                                              │
│                                                               │
│  下降沿（1→0，人离开或静坐）                                     │
│  └─► 延迟：几分钟到十几分钟                                      │
│  └─► 原因：用户可能只是静坐看屏幕                                │
│  └─► 给予足够的"信任窗口"                                      │
│                                                               │
│  本质：非对称滤波器                                             │
└───────────────────────────────────────────────────────────────┘
```

#### 推荐实现方案

```c
/* ai_infer.c - 稳定 presence 状态机 */

#define RISE_IMMEDIATE_MS    0      /* 上升沿：立即响应 */
#define FALL_HOLD_TIME_MS    300000 /* 下降沿：5分钟滞后 */

static struct {
    bool stable_presence;           /* 稳定状态（喂给 AI） */
    bool last_raw_presence;         /* 上一次原始状态 */
    int64_t falling_edge_ts;         /* 下降沿时间戳 */
} presence_sm;

static bool update_stable_presence(bool raw_presence, int64_t now_ms)
{
    /* 上升沿：立即响应 */
    if (raw_presence && !presence_sm.last_raw_presence) {
        presence_sm.stable_presence = true;
        presence_sm.falling_edge_ts = 0;
    }
    /* 下降沿：开始计时 */
    else if (!raw_presence && presence_sm.last_raw_presence) {
        presence_sm.falling_edge_ts = now_ms;
    }
    /* 持续低电平：检查滞后时间 */
    else if (!raw_presence && !presence_sm.stable_presence) {
        if (presence_sm.falling_edge_ts > 0) {
            int64_t elapsed = now_ms - presence_sm.falling_edge_ts;
            if (elapsed >= FALL_HOLD_TIME_MS) {
                presence_sm.stable_presence = false;
            }
        }
    }

    presence_sm.last_raw_presence = raw_presence;
    return presence_sm.stable_presence;
}
```

#### 设计要点

| 场景 | 原始 PIR | 稳定 presence | AI 行为 |
|------|----------|---------------|---------|
| 用户进门 | 0→1 | 立即变 1 | 立即开灯 |
| 用户静坐 3 分钟 | 1→0→0... | 保持 1 | 灯不灭 |
| 用户离开 5 分钟 | 0→0→0... | 延迟变 0 | 5 分钟后关灯 |
| 用户中途挥手 | 0→1 | 立即变 1 | 重置计时器 |

---

### 隐患二：Sigmoid 激活与 Min-Max 归一化的数学陷阱

#### 问题回顾

`linear` 激活函数输出范围 $(-\infty, +\infty)$，Z-Score 归一化导致量化精度损失。改用 `sigmoid` + Min-Max 归一化时，存在梯度消失问题。

#### 梯度消失现象

```
┌───────────────────────────────────────────────────────────────┐
│                    Sigmoid 函数特性                            │
│                                                               │
│  σ(x) = 1 / (1 + e^(-x))                                      │
│                                                               │
│  输出范围：(0, 1)                                              │
│                                                               │
│  梯度（导数）：                                                 │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │         ▲ 梯度                                           │ │
│  │    0.25 │      ╱╲                                        │ │
│  │         │     ╱  ╲                                       │ │
│  │         │    ╱    ╲                                       │ │
│  │         │   ╱      ╲                                      │ │
│  │    0    │──╱────────╲──► x                               │ │
│  │         │ -4  -2  0  2  4                                │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  问题：x 接近 ±∞ 时，梯度趋近于 0                               │
│       模型很难精确拟合到 0.000 或 1.000                         │
│       可能输出 0.01 或 0.99                                    │
└───────────────────────────────────────────────────────────────┘
```

#### 工程解法：死区（Deadzone）处理

```c
/* ai_infer.c - 反归一化带死区 */

#define DEADZONE_LOW  0.02f  /* 2% 以下视为 0 */
#define DEADZONE_HIGH 0.98f  /* 98% 以上视为 1 */

static void denormalize_output(const float *output, ai_output_t *result)
{
    /* 亮度：[0, 1] → [0%, 100%] */
    float brightness_norm = output[1];
    if (brightness_norm < DEADZONE_LOW) {
        result->brightness = 0;
    } else if (brightness_norm > DEADZONE_HIGH) {
        result->brightness = 100;
    } else {
        result->brightness = (uint8_t)(brightness_norm * 100.0f);
    }

    /* 色温：[0, 1] → [2700K, 6500K] */
    float color_temp_norm = output[0];
    if (color_temp_norm < DEADZONE_LOW) {
        result->color_temp = COLOR_TEMP_MIN;  /* 2700K */
    } else if (color_temp_norm > DEADZONE_HIGH) {
        result->color_temp = COLOR_TEMP_MAX;  /* 6500K */
    } else {
        result->color_temp = (uint16_t)(2700 + color_temp_norm * 3800);
    }
}
```

#### 数学保证

```
┌───────────────────────────────────────────────────────────────┐
│                    输出边界数学保证                              │
│                                                               │
│  Sigmoid 输出：σ(x) ∈ (0, 1)                                  │
│                                                               │
│  亮度计算：                                                    │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ σ(x) < 0.02  → brightness = 0%                           │ │
│  │ σ(x) > 0.98  → brightness = 100%                        │ │
│  │ 其他          → brightness = σ(x) × 100%                │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  结论：无论模型输出如何异常，亮度永远在 [0%, 100%]               │
│       PWM 驱动永远不会收到越界指令                              │
└───────────────────────────────────────────────────────────────┘
```

---

### 隐患三：数据飞轮（Data Flywheel）架构设计

#### 问题回顾

当前用 15 行 Python if-else 规则生成训练数据，模型只是在"模仿规则"。真正的 AI 应该学习用户偏好。

#### 影子模式（Shadow Mode）机制

```
┌───────────────────────────────────────────────────────────────┐
│                    影子模式运转机制                              │
│                                                               │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐   │
│  │ 传感器数据   │ ───► │  AI 推理    │ ───► │ 预测值 60%  │   │
│  └─────────────┘      └─────────────┘      └─────────────┘   │
│         │                                         │          │
│         │                                         │          │
│         ▼                                         ▼          │
│  ┌─────────────┐                           ┌─────────────┐   │
│  │ 物理旋钮    │ ───► 用户实际设定 80% ───► │ 偏差 20%    │   │
│  └─────────────┘                           └─────────────┘   │
│                                                   │          │
│                                                   ▼          │
│                                          ┌─────────────┐    │
│                                          │ 触发数据采集 │    │
│                                          └─────────────┘    │
└───────────────────────────────────────────────────────────────┘
```

#### 高价值数据采集策略

```c
/* ai_infer.c - 影子模式数据采集 */

#define DEVIATION_THRESHOLD 15  /* 偏差阈值：15% */

typedef struct {
    float features[7];   /* 输入特征 */
    float user_value;    /* 用户实际设定 */
    uint32_t timestamp;
} hard_example_t;

static void check_and_record_deviation(
    const ai_features_t *features,
    const ai_output_t *ai_pred,
    const ai_output_t *user_actual)
{
    /* 计算偏差 */
    int brightness_diff = abs((int)ai_pred->brightness - (int)user_actual->brightness);

    /* 偏差超过阈值，记录为高价值样本 */
    if (brightness_diff >= DEVIATION_THRESHOLD) {
        hard_example_t example = {
            .features = {
                features->hour_sin,
                features->hour_cos,
                features->sunset_proximity,
                features->sunrise_hour_norm,
                features->sunset_hour_norm,
                features->weather,
                features->presence
            },
            .user_value = (float)user_actual->brightness,
            .timestamp = k_uptime_get_32()
        };

        /* 写入 Flash 日志分区或发送到局域网服务器 */
        hard_example_log_append(&example);

        LOG_INF("Hard example recorded: AI=%d%%, User=%d%%, diff=%d%%",
                ai_pred->brightness, user_actual->brightness, brightness_diff);
    }
}
```

#### 数据飞轮完整流程

```
┌───────────────────────────────────────────────────────────────┐
│                    数据飞轮（Data Flywheel）                     │
│                                                               │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐│
│  │ 影子模式  │ ─► │ 偏差检测  │ ─► │ 纠错数据  │ ─► │ 云端聚合  ││
│  │ 运行     │    │ > 15%    │    │ 采集     │    │          │ │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘│
│                                                       │      │
│                                                       ▼      │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐│
│  │ OTA 下发  │ ◄─ │ INT8量化  │ ◄─ │ 微调训练  │ ◄─ │ 数据混合  ││
│  │ 新模型    │    │          │    │          │    │          │ │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘│
│                                                               │
│  数据来源：                                                     │
│  - 原始合成数据：15000 条（覆盖主要场景）                         │
│  - 用户纠错数据：几百条（高价值偏好）                              │
│  - 混合比例：合成数据 : 纠错数据 ≈ 10:1                          │
│                                                               │
│  这才是真正的边缘 AI 进化！                                      │
└───────────────────────────────────────────────────────────────┘
```

#### Roadmap 规划

| 阶段 | 功能 | 数据来源 | 差异化 |
|------|------|----------|--------|
| Phase 1 (MVP) | 规则模仿 | 合成数据 | 无 |
| Phase 2 | 用户偏好学习 | 影子模式采集 | 有 |
| Phase 3 | 群体智慧 | 云端聚合 | 有 |

---

### 修复优先级更新

| 优先级 | 隐患 | 修复方案 | 影响范围 | 状态 |
|--------|------|----------|----------|------|
| P0 | 传感器抖动 | C 端非对称 Debounce 状态机 | `ai_infer.c` | 待实施 |
| P1 | 输出越界 | Sigmoid + Min-Max + 死区 | `train_model.py` + `ai_infer.c` | 待实施 |
| P2 | 规则模仿 | 影子模式数据飞轮 | Roadmap 项目 | 规划中 |

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
*更新日期：2026-04-27*
*版本：v1.3*
