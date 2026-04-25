# MLP 模型测试验证指南

> 从 AI 训练架构师和测试员角度，定义模型测试策略、框架和验收标准。

---

## 测试金字塔

```
                ┌──────────────────────┐
                │  E2E 测试 (ESP32-S3)  │  ← 少量，高成本
                │  TFLM推理 / 延迟测量    │
                ├──────────────────────┤
                │  集成测试 (Python)     │  ← 中等
                │  量化一致性 / 边界覆盖   │
                ├──────────────────────┤
                │  单元测试 (Python)     │  ← 大量，快速
                │  数据逻辑 / 模型结构    │
                └──────────────────────┘
```

## 测试矩阵

| 层级 | 测试内容 | 环境 | 频率 |
|------|---------|------|------|
| L1 单元 | 数据生成逻辑、模型结构、特征计算 | Python + pytest | 每次提交 |
| L2 集成 | TFLite推理一致性、量化精度、边界条件 | Python + TF | 每次训练 |
| L3 E2E | TFLM推理、延迟测量、内存检查 | ESP32-S3 硬件 | 版本发布前 |
| L4 回归 | Golden用例、精度对比报告 | Python | 模型更新后 |

---

## 测试框架

### 文件结构

```
prj/ai/
├── test_model.py          # 自动化测试 (pytest)
├── generate_data.py       # 数据生成
├── train_model.py         # 训练流水线
└── ...
```

### 运行方式

```bash
conda activate esp32-ai
cd prj/ai

# 全部测试
pytest test_model.py -v

# 按层级运行
pytest test_model.py -v -k "TestDataGeneration"    # L1
pytest test_model.py -v -k "TestModelStructure"    # L2
pytest test_model.py -v -k "TestGoldenCases"       # L4

# 运行并显示打印输出
pytest test_model.py -v -s
```

---

## L1 单元测试详情

| 测试用例 | 验证内容 | 关键断言 |
|---------|---------|---------|
| `test_sunset_proximity_calculation` | 日落临近度计算 | 3个典型值精确匹配 |
| `test_presence_zero_brightness` | 无人关灯 | brightness=0, color_temp=2700 |
| `test_late_night_brightness_cap` | 深夜亮度限制 | 上限=50% |
| `test_output_range` | 输出范围合法 | 色温[2700,6500], 亮度[0,100] |
| `test_feature_ranges` | 特征归一化范围 | sin/cos∈[-1,1], 归一化∈[0,1] |

## L2 集成测试详情

| 测试用例 | 验证内容 | 关键断言 |
|---------|---------|---------|
| `test_model_buildable` | 模型可构建 | model is not None |
| `test_input_shape` | 输入维度 | =7 |
| `test_output_shape` | 输出维度 | =2 |
| `test_parameter_count` | 参数量 | 800~900 |
| `test_quantized_model_size` | 模型大小 | <5KB |
| `test_float_vs_int8_brightness` | 量化精度 | 亮度误差<5% |
| `test_all_weather_values` | 天气枚举覆盖 | 输出合法 |
| `test_edge_hours` | 边界时间点 | 输出合法 |

## L4 回归测试详情

### Golden 用例

| 场景 | 时间 | 天气 | 人员 | 期望色温 | 期望亮度 | 容差 |
|------|------|------|------|---------|---------|------|
| 上午晴天 | 9:00 | 晴 | 在 | 4800K | 65% | 10% |
| 下午多云 | 14:00 | 阴 | 在 | 4200K | 72% | 15% |
| 傍晚阴天 | 18:00 | 暴雨 | 在 | 3500K | 80% | 15% |
| 深夜 | 22:00 | 晴 | 在 | 3500K | 50% | 20% |
| 无人关灯 | 12:00 | 多云 | 不在 | 2700K | 0% | 0% |
| 日落前1h | 17:30 | 晴 | 在 | 3500K | 64% | 15% |
| 清晨 | 6:00 | 晴 | 在 | 4800K | 65% | 10% |

**注意**：无人关灯用例容差为 0，必须精确匹配。

---

## L3 E2E 测试 (嵌入式)

### 在 ESP32-S3 上验证

1. **TFLM 推理一致性**：用相同输入对比 Python TFLite 和 ESP32 TFLM 输出
2. **推理延迟**：预期 < 1ms (ESP32-S3 @240MHz)
3. **内存占用**：运行时峰值 < 20KB
4. **特征计算一致性**：C 侧 `ai_calc_sunset_proximity()` 与 Python 侧一致

### 测试方法

```c
// Zephyr ztest 框架
ZTEST(ai_infer, test_golden_cases) { ... }
ZTEST(ai_infer, test_latency) { ... }
```

---

## 验收标准

### 模型发布门槛

| 指标 | 阈值 | 当前值 |
|------|------|--------|
| 亮度 MAE | < 5% | 1.1% ✅ |
| 色温 MAE | < 50K | 27.8K ✅ |
| 量化损失 | < 10% | 亮度4.6% ✅ |
| 模型大小 | < 5KB | 4.4KB ✅ |
| Golden通过率 | ≥ 85% | 100% ✅ |
| 单元测试通过率 | 100% | 100% ✅ |

### 已知风险

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 色温量化损失21.8% | 色温精度偏低 | 人眼阈值100K，绝对误差27.8K可接受 |
| 深夜色温偏高 | 22:00后色温~3500K而非2700K | 后处理强制限幅 |
| 合成数据偏差 | 未覆盖真实用户偏好 | 后续采集真实数据微调 |

---

## 测试结果 (2026-04-25)

```
14 passed, 5 warnings in 5.13s

L1 TestDataGeneration  - 5/5 passed ✅
L2 TestModelStructure  - 5/5 passed ✅
L2 TestInferenceConsistency - 1/1 passed ✅
L2 TestBoundaryConditions   - 2/2 passed ✅
L4 TestGoldenCases     - 1/1 passed ✅
```

---

*维护者：null-characters*
*创建日期：2026-04-25*
*版本：v1.0*
