# AI训练模块 - Phase 3

## 用途

PC端训练会议室智能照明的MLP神经网络模型，并转换为TFLite INT8格式供ESP32-S3部署。

## 模型结构

```
输入(7) → Dense(32, ReLU) → Dense(16, ReLU) → 输出(2, Linear)
```

- 输入: [hour_sin, hour_cos, sunset_proximity, sunrise_hour_norm, sunset_hour_norm, weather, presence]
- 输出: [color_temp, brightness]

## 使用方法

```bash
# 1. 安装依赖
pip install -r requirements.txt

# 2. 生成训练数据
python generate_data.py

# 3. 训练+量化+导出
python train_model.py
```

## 输出文件

| 文件 | 说明 |
|------|------|
| `training_data.csv` | 训练数据集 (CSV格式) |
| `training_data.npz` | 训练数据集 (NumPy格式) |
| `lighting_model.h5` | Keras模型 |
| `lighting_model_float.tflite` | Float32 TFLite模型 |
| `lighting_model_int8.tflite` | INT8量化TFLite模型 |
| `../include/model_data.h` | C数组头文件 (自动部署到include) |
| `normalization_params.txt` | 输出归一化参数 |
