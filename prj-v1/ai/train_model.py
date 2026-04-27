"""
ESP32-S3 会议室智能照明 - MLP模型训练与量化
Phase 3: AI-003 ~ AI-007

模型结构: 7 → 16 → 2 (MLP回归)
训练: Adam优化器, MSE损失
量化: INT8量化 (TFLite)
输出: C数组头文件 (model_data.h)

P1 改造：
- 输出层使用 Sigmoid 激活函数，数学保证输出在 [0, 1]
- 数据已使用 Min-Max 归一化，无需 Z-Score
"""

import os
import sys
import numpy as np

# P1: Min-Max 归一化参数（与 generate_data.py 一致）
COLOR_TEMP_MIN = 2700
COLOR_TEMP_MAX = 6500
BRIGHTNESS_MIN = 0
BRIGHTNESS_MAX = 100
COLOR_TEMP_RANGE = COLOR_TEMP_MAX - COLOR_TEMP_MIN  # 3800K
BRIGHTNESS_RANGE = BRIGHTNESS_MAX - BRIGHTNESS_MIN  # 100%

# ================================================================
# AI-003: MLP模型定义
# ================================================================
def build_model():
    """
    构建MLP模型: 7→16→2

    P1 改造：输出层使用 Sigmoid 激活函数
    - 数学保证：输出永远在 (0, 1) 范围内
    - 无需 C 端范围裁剪，PWM 驱动永远不会收到越界指令
    - INT8 量化精度更高（小范围映射）

    消融实验结论：单隐藏层16神经元已足够
    - 参数量: 162
    - INT8体积: ~2.4KB
    """
    import tensorflow as tf

    model = tf.keras.Sequential([
        tf.keras.layers.Dense(16, activation='relu', input_shape=(7,),
                              name='hidden_1'),
        # P1: Sigmoid 输出，数学保证 [0, 1]
        tf.keras.layers.Dense(2, activation='sigmoid',
                              name='output')
    ], name='lighting_mlp')

    model.summary()
    return model


# ================================================================
# AI-004: 模型训练
# ================================================================
def train_model(model, X, Y, epochs=150, batch_size=32):
    """
    训练模型

    P1 改造：数据已使用 Min-Max 归一化，无需 Z-Score
    - Y 已经在 [0, 1] 范围内
    - Sigmoid 输出天然匹配目标范围
    """
    import tensorflow as tf

    # P1: 数据已归一化，直接训练
    # 编译
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss='mse',
        metrics=['mae']
    )

    # 训练
    history = model.fit(
        X, Y,
        epochs=epochs,
        batch_size=batch_size,
        validation_split=0.2,
        verbose=1,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(
                patience=15,
                restore_best_weights=True
            ),
            tf.keras.callbacks.ReduceLROnPlateau(
                factor=0.5,
                patience=5,
                min_lr=1e-6
            )
        ]
    )

    # 打印训练结果
    final_loss = history.history['val_loss'][-1]
    print(f"\nTraining complete. Final val_loss: {final_loss:.6f}")

    return model


# ================================================================
# AI-005: INT8量化
# ================================================================
def quantize_model(model, X):
    """
    INT8量化模型

    P1 改造：移除 y_mean, y_std 参数（不再需要）
    """
    import tensorflow as tf

    # 转换为TFLite (先做float32版本作为基准)
    converter_float = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_float = converter_float.convert()
    print(f"Float32 model size: {len(tflite_float)} bytes")

    # INT8量化
    def representative_dataset():
        for i in range(min(500, len(X))):
            yield [X[i:i+1].astype(np.float32)]

    converter_int8 = tf.lite.TFLiteConverter.from_keras_model(model)
    converter_int8.optimizations = [tf.lite.Optimize.DEFAULT]
    converter_int8.representative_dataset = representative_dataset
    converter_int8.target_spec.supported_ops = [
        tf.lite.OpsSet.TFLITE_BUILTINS_INT8
    ]
    converter_int8.inference_input_type = tf.int8
    converter_int8.inference_output_type = tf.int8

    tflite_int8 = converter_int8.convert()
    print(f"INT8 model size: {len(tflite_int8)} bytes")

    return tflite_float, tflite_int8


# ================================================================
# AI-007: 模型验证
# ================================================================
def validate_model(tflite_float, tflite_int8, X, Y, Y_raw):
    """
    验证量化前后模型精度

    P1 改造：
    - Y 是归一化值 [0, 1]
    - Y_raw 是原始值（色温 K，亮度 %）
    - 需要反归一化计算真实误差
    """
    import tensorflow as tf

    def run_tflite_inference(tflite_model, input_data):
        interpreter = tf.lite.Interpreter(model_content=tflite_model)
        interpreter.allocate_tensors()

        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()

        input_scale, input_zero_point = input_details[0]['quantization']
        output_scale, output_zero_point = output_details[0]['quantization']

        results = []
        for i in range(len(input_data)):
            # 量化输入
            if input_details[0]['dtype'] == np.int8:
                quantized_input = input_data[i:i+1] / input_scale + input_zero_point
                quantized_input = np.clip(quantized_input, -128, 127).astype(np.int8)
                interpreter.set_tensor(input_details[0]['index'], quantized_input)
            else:
                interpreter.set_tensor(input_details[0]['index'],
                                       input_data[i:i+1].astype(np.float32))

            interpreter.invoke()

            output = interpreter.get_tensor(output_details[0]['index'])
            # 反量化输出
            if output_details[0]['dtype'] == np.int8:
                output = (output.astype(np.float32) - output_zero_point) * output_scale

            results.append(output[0])

        return np.array(results)

    # Float32模型推理
    print("\nValidating float32 model...")
    Y_pred_float = run_tflite_inference(tflite_float, X)

    # P1: 反归一化到原始单位
    Y_pred_float_raw = np.zeros_like(Y_pred_float)
    Y_pred_float_raw[:, 0] = Y_pred_float[:, 0] * COLOR_TEMP_RANGE + COLOR_TEMP_MIN
    Y_pred_float_raw[:, 1] = Y_pred_float[:, 1] * BRIGHTNESS_RANGE + BRIGHTNESS_MIN

    mae_float_ct = np.mean(np.abs(Y_pred_float_raw[:, 0] - Y_raw[:, 0]))
    mae_float_br = np.mean(np.abs(Y_pred_float_raw[:, 1] - Y_raw[:, 1]))
    print(f"  Float32 MAE - color_temp: {mae_float_ct:.1f}K, brightness: {mae_float_br:.1f}%")

    # INT8模型推理
    print("Validating INT8 model...")
    Y_pred_int8 = run_tflite_inference(tflite_int8, X)

    # P1: 反归一化到原始单位
    Y_pred_int8_raw = np.zeros_like(Y_pred_int8)
    Y_pred_int8_raw[:, 0] = Y_pred_int8[:, 0] * COLOR_TEMP_RANGE + COLOR_TEMP_MIN
    Y_pred_int8_raw[:, 1] = Y_pred_int8[:, 1] * BRIGHTNESS_RANGE + BRIGHTNESS_MIN

    mae_int8_ct = np.mean(np.abs(Y_pred_int8_raw[:, 0] - Y_raw[:, 0]))
    mae_int8_br = np.mean(np.abs(Y_pred_int8_raw[:, 1] - Y_raw[:, 1]))
    print(f"  INT8 MAE - color_temp: {mae_int8_ct:.1f}K, brightness: {mae_int8_br:.1f}%")

    # 精度损失
    ct_loss = abs(mae_int8_ct - mae_float_ct) / max(mae_float_ct, 1) * 100
    br_loss = abs(mae_int8_br - mae_float_br) / max(mae_float_br, 1) * 100
    print(f"  Precision loss - color_temp: {ct_loss:.2f}%, brightness: {br_loss:.2f}%")

    if ct_loss < 5 and br_loss < 5:
        print("  ✅ INT8 quantization precision loss < 5%")
    else:
        print("  ⚠️  INT8 quantization precision loss >= 5%, consider mixed quantization")

    # P1: 检查输出范围（Sigmoid 保证）
    print(f"\n  Output range check (Sigmoid guarantee):")
    print(f"    Float32 output: [{Y_pred_float.min():.4f}, {Y_pred_float.max():.4f}]")
    print(f"    INT8 output: [{Y_pred_int8.min():.4f}, {Y_pred_int8.max():.4f}]")
    if Y_pred_float.min() >= 0 and Y_pred_float.max() <= 1:
        print("    ✅ All outputs within [0, 1] - PWM safe!")
    else:
        print("    ⚠️ Output out of range!")

    return mae_float_ct, mae_float_br, mae_int8_ct, mae_int8_br


# ================================================================
# AI-006: C数组转换
# ================================================================
def convert_to_c_array(tflite_model, output_path):
    """
    将TFLite模型转为C数组头文件
    """
    # 生成C头文件
    header_guard = "LIGHTING_MODEL_DATA_H_"
    array_name = "g_lighting_model_data"
    array_len = "g_lighting_model_data_len"

    lines = []
    lines.append("/*")
    lines.append(" * Lighting MLP Model Data - INT8 Quantized")
    lines.append(" * Auto-generated by train_model.py")
    lines.append(" * DO NOT EDIT")
    lines.append(" */")
    lines.append("")
    lines.append(f"#ifndef {header_guard}")
    lines.append(f"#define {header_guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"const unsigned int {array_len} = {len(tflite_model)};")
    lines.append(f"const alignas(16) unsigned char {array_name}[] = {{")

    # 每行12个字节
    for i in range(0, len(tflite_model), 12):
        chunk = tflite_model[i:i+12]
        hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
        if i + 12 < len(tflite_model):
            lines.append(f'    {hex_values},')
        else:
            lines.append(f'    {hex_values}')

    lines.append("};")
    lines.append("")
    lines.append(f"#endif /* {header_guard} */")
    lines.append("")

    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f"C array saved to {output_path} ({len(tflite_model)} bytes)")


# ================================================================
# 主流程
# ================================================================
def main():
    import tensorflow as tf
    print(f"TensorFlow version: {tf.__version__}")

    output_dir = os.path.dirname(os.path.abspath(__file__))

    # 加载训练数据
    npz_path = os.path.join(output_dir, 'training_data.npz')
    csv_path = os.path.join(output_dir, 'training_data.csv')

    if os.path.exists(npz_path):
        print(f"Loading data from {npz_path}")
        data = np.load(npz_path)
        X = data['X']
        Y = data['Y']  # P1: 归一化值 [0, 1]
        Y_raw = data['Y_raw'] if 'Y_raw' in data else None  # P1: 原始值
    elif os.path.exists(csv_path):
        print(f"Loading data from {csv_path}")
        import pandas as pd
        df = pd.read_csv(csv_path)
        feature_cols = ['hour_sin', 'hour_cos', 'sunset_proximity',
                        'sunrise_hour_norm', 'sunset_hour_norm',
                        'weather', 'presence']
        X = df[feature_cols].values.astype(np.float32)
        # P1: 优先使用归一化列
        if 'color_temp_norm' in df.columns:
            Y = df[['color_temp_norm', 'brightness_norm']].values.astype(np.float32)
            Y_raw = df[['color_temp', 'brightness']].values.astype(np.float32)
        else:
            Y = df[['color_temp', 'brightness']].values.astype(np.float32)
            Y_raw = Y
    else:
        print("No training data found. Run generate_data.py first.")
        sys.exit(1)

    print(f"Data loaded: {X.shape[0]} samples, {X.shape[1]} features")

    # AI-003: 构建模型
    model = build_model()

    # AI-004: 训练
    model = train_model(model, X, Y, epochs=150)

    # AI-005: 量化
    tflite_float, tflite_int8 = quantize_model(model, X)

    # 保存模型文件
    model_path = os.path.join(output_dir, 'lighting_model.h5')
    model.save(model_path)
    print(f"Keras model saved to {model_path}")

    tflite_float_path = os.path.join(output_dir, 'lighting_model_float.tflite')
    with open(tflite_float_path, 'wb') as f:
        f.write(tflite_float)
    print(f"Float TFLite saved to {tflite_float_path}")

    tflite_int8_path = os.path.join(output_dir, 'lighting_model_int8.tflite')
    with open(tflite_int8_path, 'wb') as f:
        f.write(tflite_int8)
    print(f"INT8 TFLite saved to {tflite_int8_path}")

    # AI-007: 验证
    if Y_raw is not None:
        validate_model(tflite_float, tflite_int8, X, Y, Y_raw)
    else:
        print("Skipping validation (no raw values available)")

    # AI-006: 生成C数组
    c_header_path = os.path.join(output_dir, '..', 'include', 'model_data.h')
    os.makedirs(os.path.dirname(c_header_path), exist_ok=True)
    convert_to_c_array(tflite_int8, c_header_path)

    # P1: 保存 Min-Max 归一化参数 (C 端推理需要)
    norm_path = os.path.join(output_dir, 'normalization_params.txt')
    with open(norm_path, 'w') as f:
        f.write(f"# P1: Min-Max Normalization Parameters\n")
        f.write(f"# Output is already in [0, 1], denormalize to get real values\n")
        f.write(f"# color_temp = output[0] * COLOR_TEMP_RANGE + COLOR_TEMP_MIN\n")
        f.write(f"# brightness = output[1] * BRIGHTNESS_RANGE + BRIGHTNESS_MIN\n")
        f.write(f"\n")
        f.write(f"COLOR_TEMP_MIN={COLOR_TEMP_MIN}\n")
        f.write(f"COLOR_TEMP_MAX={COLOR_TEMP_MAX}\n")
        f.write(f"COLOR_TEMP_RANGE={COLOR_TEMP_RANGE}\n")
        f.write(f"BRIGHTNESS_MIN={BRIGHTNESS_MIN}\n")
        f.write(f"BRIGHTNESS_MAX={BRIGHTNESS_MAX}\n")
        f.write(f"BRIGHTNESS_RANGE={BRIGHTNESS_RANGE}\n")
    print(f"Normalization params saved to {norm_path}")

    print("\n=== Training Pipeline Complete ===")


if __name__ == '__main__':
    main()
