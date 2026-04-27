"""
ESP32-S3 雷达场景识别 - MLP模型训练与量化
Phase 1: RD-014 ~ RD-017

模型结构: 8 → 16 → 12 → 6 (MLP分类)
训练: Adam优化器, Sparse Categorical Crossentropy
量化: INT8量化 (TFLite)
输出: C数组头文件 (radar_model.cc)

场景类别:
0 - 无人 (empty)
1 - 静坐呼吸 (breathing)
2 - 风扇摇头 (fan)
3 - 行走 (walking)
4 - 多人 (multiple_people)
5 - 进入/离开 (entering_leaving)
"""

import os
import sys
import numpy as np

SCENES = {
    0: 'empty',
    1: 'breathing',
    2: 'fan',
    3: 'walking',
    4: 'multiple_people',
    5: 'entering_leaving'
}

# ================================================================
# RD-014: MLP模型定义
# ================================================================
def build_model():
    """
    构建MLP分类模型: 8→16→12→6

    输入: 8维雷达特征向量
    输出: 6类场景概率 (Softmax)

    参数量: ~400
    INT8体积: ~2KB
    """
    import tensorflow as tf

    model = tf.keras.Sequential([
        tf.keras.layers.Dense(16, activation='relu', input_shape=(8,),
                              name='hidden_1'),
        tf.keras.layers.Dense(12, activation='relu', name='hidden_2'),
        tf.keras.layers.Dense(6, activation='softmax', name='output')
    ], name='radar_classifier')

    model.summary()
    return model


# ================================================================
# RD-015: 模型训练
# ================================================================
def train_model(model, X, Y, epochs=100, batch_size=32):
    """训练模型"""
    import tensorflow as tf

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )

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

    final_acc = history.history['val_accuracy'][-1]
    print(f"\nTraining complete. Final val_accuracy: {final_acc:.4f}")

    return model


# ================================================================
# RD-016: INT8量化
# ================================================================
def quantize_model(model, X):
    """INT8量化模型"""
    import tensorflow as tf

    # Float32 模型
    converter_float = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_float = converter_float.convert()
    print(f"Float32 model size: {len(tflite_float)} bytes")

    # INT8 量化
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
# 模型验证
# ================================================================
def validate_model(tflite_float, tflite_int8, X, Y):
    """验证量化前后模型精度"""
    import tensorflow as tf

    def run_tflite_inference(tflite_model, input_data):
        interpreter = tf.lite.Interpreter(model_content=tflite_model)
        interpreter.allocate_tensors()

        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()

        input_scale, input_zero_point = input_details[0]['quantization']
        output_scale, output_zero_point = output_details[0]['quantization']

        predictions = []
        for i in range(len(input_data)):
            if input_details[0]['dtype'] == np.int8:
                quantized_input = input_data[i:i+1] / input_scale + input_zero_point
                quantized_input = np.clip(quantized_input, -128, 127).astype(np.int8)
                interpreter.set_tensor(input_details[0]['index'], quantized_input)
            else:
                interpreter.set_tensor(input_details[0]['index'],
                                       input_data[i:i+1].astype(np.float32))

            interpreter.invoke()

            output = interpreter.get_tensor(output_details[0]['index'])
            if output_details[0]['dtype'] == np.int8:
                output = (output.astype(np.float32) - output_zero_point) * output_scale

            predictions.append(np.argmax(output[0]))

        return np.array(predictions)

    # Float32 推理
    print("\nValidating float32 model...")
    Y_pred_float = run_tflite_inference(tflite_float, X)
    acc_float = np.mean(Y_pred_float == Y)
    print(f"  Float32 accuracy: {acc_float:.4f}")

    # INT8 推理
    print("Validating INT8 model...")
    Y_pred_int8 = run_tflite_inference(tflite_int8, X)
    acc_int8 = np.mean(Y_pred_int8 == Y)
    print(f"  INT8 accuracy: {acc_int8:.4f}")

    # 精度损失
    loss = abs(acc_float - acc_int8) * 100
    print(f"  Accuracy drop: {loss:.2f}%")

    if loss < 5:
        print("  ✅ INT8 quantization accuracy drop < 5%")
    else:
        print("  ⚠️  INT8 quantization accuracy drop >= 5%")

    # 混淆矩阵
    print("\nConfusion Matrix (INT8):")
    for true_id in range(6):
        mask = Y == true_id
        if mask.sum() > 0:
            pred_dist = np.bincount(Y_pred_int8[mask], minlength=6)
            print(f"  {SCENES[true_id]:15s}: {pred_dist}")

    return acc_float, acc_int8


# ================================================================
# RD-017: C数组转换
# ================================================================
def convert_to_c_array(tflite_model, output_path):
    """将TFLite模型转为C数组源文件"""
    lines = []
    lines.append("/*")
    lines.append(" * Radar Scene Classifier Model Data - INT8 Quantized")
    lines.append(" * Auto-generated by radar_train_model.py")
    lines.append(" * DO NOT EDIT")
    lines.append(" *")
    lines.append(" * Input: 8-dim radar features (INT8)")
    lines.append(" * Output: 6-class scene probabilities (INT8)")
    lines.append(" *")
    lines.append(" * Scenes: 0=empty, 1=breathing, 2=fan, 3=walking,")
    lines.append(" *         4=multiple_people, 5=entering_leaving")
    lines.append(" */")
    lines.append("")
    lines.append('#include <stdint.h>')
    lines.append('')
    lines.append(f'const unsigned int g_radar_model_data_len = {len(tflite_model)};')
    lines.append(f'const alignas(16) unsigned char g_radar_model_data[] = {{')

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
    npz_path = os.path.join(output_dir, 'radar_training_data.npz')

    if not os.path.exists(npz_path):
        print("No training data found. Run radar_generate_data.py first.")
        sys.exit(1)

    print(f"Loading data from {npz_path}")
    data = np.load(npz_path)
    X = data['X']
    Y = data['Y']

    print(f"Data loaded: {X.shape[0]} samples, {X.shape[1]} features, {len(np.unique(Y))} classes")

    # RD-014: 构建模型
    model = build_model()

    # RD-015: 训练
    model = train_model(model, X, Y, epochs=100)

    # RD-016: 量化
    tflite_float, tflite_int8 = quantize_model(model, X)

    # 保存模型文件
    model_path = os.path.join(output_dir, 'radar_model.h5')
    model.save(model_path)
    print(f"Keras model saved to {model_path}")

    tflite_float_path = os.path.join(output_dir, 'radar_model_float.tflite')
    with open(tflite_float_path, 'wb') as f:
        f.write(tflite_float)
    print(f"Float TFLite saved to {tflite_float_path}")

    tflite_int8_path = os.path.join(output_dir, 'radar_model_int8.tflite')
    with open(tflite_int8_path, 'wb') as f:
        f.write(tflite_int8)
    print(f"INT8 TFLite saved to {tflite_int8_path}")

    # 验证
    validate_model(tflite_float, tflite_int8, X, Y)

    # RD-017: 生成C数组
    c_source_path = os.path.join(output_dir, '..', 'src', 'radar_model_data.cc')
    os.makedirs(os.path.dirname(c_source_path), exist_ok=True)
    convert_to_c_array(tflite_int8, c_source_path)

    print("\n=== Radar Model Training Complete ===")


if __name__ == '__main__':
    main()
