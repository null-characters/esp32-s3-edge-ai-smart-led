"""
ESP32-S3 声音场景分类器 - CNN模型训练与量化
prj-v3 多模态推理架构

模型结构: CNN (MFCC 40×32×1 → 5类)
训练: Adam优化器, Sparse Categorical Crossentropy
量化: INT8量化 (TFLite)
输出: sound_classifier.tflite (~30KB)

场景类别:
0 - 寂静 (silence)
1 - 单人讲话 (single_speech)
2 - 多人讨论 (multi_speech)
3 - 键盘敲击 (keyboard)
4 - 其他噪音 (other_noise)
"""

import os
import sys
import numpy as np

# MFCC 参数
MFCC_NUM = 40      # 40 个 MFCC 系数
MFCC_FRAMES = 32   # 32 帧
NUM_CLASSES = 5    # 5 类场景

SCENES = {
    0: 'silence',
    1: 'single_speech',
    2: 'multi_speech',
    3: 'keyboard',
    4: 'other_noise'
}

# ================================================================
# CNN 模型定义
# ================================================================
def build_model():
    """
    构建 CNN 分类模型 (轻量版)
    
    输入: MFCC (40, 32, 1) - 40 个 MFCC 系数，32 帧
    输出: 5 类场景概率 (Softmax)
    
    优化: 使用 GlobalAveragePooling2D 替代 Flatten
    模型大小: ~30KB (INT8 量化后)
    """
    import tensorflow as tf
    
    model = tf.keras.Sequential([
        # 输入: (40, 32, 1)
        tf.keras.layers.Conv2D(8, (3, 3), activation='relu', 
                              input_shape=(MFCC_NUM, MFCC_FRAMES, 1),
                              padding='same', name='conv1'),
        tf.keras.layers.MaxPooling2D((2, 2), name='pool1'),
        # -> (20, 16, 8)
        
        tf.keras.layers.Conv2D(16, (3, 3), activation='relu',
                              padding='same', name='conv2'),
        tf.keras.layers.MaxPooling2D((2, 2), name='pool2'),
        # -> (10, 8, 16)
        
        tf.keras.layers.Conv2D(32, (3, 3), activation='relu',
                              padding='same', name='conv3'),
        # -> (10, 8, 32)
        
        # GlobalAveragePooling: 输出 32 维 (远小于 Flatten 的 2560 维)
        tf.keras.layers.GlobalAveragePooling2D(name='gap'),
        # -> 32
        
        tf.keras.layers.Dense(NUM_CLASSES, activation='softmax', name='output')
    ], name='sound_classifier')
    
    model.summary()
    return model


# ================================================================
# 数据生成
# ================================================================
def generate_synthetic_data(num_samples=5000):
    """
    生成合成训练数据
    
    由于真实 MFCC 数据需要音频采集，这里生成模拟数据：
    - 寂静: 低能量，平坦频谱
    - 单人讲话: 中能量，人声频段 (300-3400Hz)
    - 多人讨论: 高能量，多人声混合
    - 键盘敲击: 高频脉冲，周期性
    - 其他噪音: 随机频谱
    """
    print(f"Generating {num_samples} synthetic MFCC samples...")
    
    X = []
    Y = []
    
    samples_per_class = num_samples // NUM_CLASSES
    
    for class_id in range(NUM_CLASSES):
        for _ in range(samples_per_class):
            # 生成基础 MFCC 帧
            mfcc = np.random.randn(MFCC_NUM, MFCC_FRAMES).astype(np.float32) * 0.1
            
            if class_id == 0:  # 寂静
                # 低能量，平坦
                mfcc *= 0.2
                mfcc[0, :] += 0.1  # 低频分量
                
            elif class_id == 1:  # 单人讲话
                # 人声频段 (MFCC 1-10 对应 300-3400Hz)
                mfcc[1:10, :] += 0.5 + np.random.randn(9, MFCC_FRAMES) * 0.2
                mfcc[0, :] += 0.3  # 基频能量
                
            elif class_id == 2:  # 多人讨论
                # 高能量，多人声混合
                mfcc[1:15, :] += 0.8 + np.random.randn(14, MFCC_FRAMES) * 0.3
                mfcc[0, :] += 0.5
                
            elif class_id == 3:  # 键盘敲击
                # 高频脉冲 (MFCC 20-35)
                mfcc[20:35, :] += 0.6 + np.random.randn(15, MFCC_FRAMES) * 0.2
                # 周期性脉冲
                for f in range(0, MFCC_FRAMES, 4):
                    mfcc[:, f] += 0.3
                    
            else:  # 其他噪音
                # 随机频谱
                mfcc += np.random.randn(MFCC_NUM, MFCC_FRAMES) * 0.4
            
            X.append(mfcc)
            Y.append(class_id)
    
    X = np.array(X).reshape(-1, MFCC_NUM, MFCC_FRAMES, 1)
    Y = np.array(Y)
    
    # 打乱
    indices = np.random.permutation(len(X))
    X = X[indices]
    Y = Y[indices]
    
    print(f"Generated: X.shape={X.shape}, Y.shape={Y.shape}")
    return X, Y


# ================================================================
# 模型训练
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
# INT8 量化
# ================================================================
def quantize_model(model, X):
    """INT8 量化模型"""
    import tensorflow as tf
    
    # Float32 模型
    converter_float = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_float = converter_float.convert()
    print(f"Float32 model size: {len(tflite_float)} bytes ({len(tflite_float)/1024:.1f} KB)")
    
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
    print(f"INT8 model size: {len(tflite_int8)} bytes ({len(tflite_int8)/1024:.1f} KB)")
    
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
    for true_id in range(NUM_CLASSES):
        mask = Y == true_id
        if mask.sum() > 0:
            pred_dist = np.bincount(Y_pred_int8[mask], minlength=NUM_CLASSES)
            print(f"  {SCENES[true_id]:15s}: {pred_dist}")
    
    return acc_float, acc_int8


# ================================================================
# 主流程
# ================================================================
def main():
    import tensorflow as tf
    print(f"TensorFlow version: {tf.__version__}")
    
    output_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 生成或加载数据
    npz_path = os.path.join(output_dir, 'sound_training_data.npz')
    
    if os.path.exists(npz_path):
        print(f"Loading data from {npz_path}")
        data = np.load(npz_path)
        X = data['X']
        Y = data['Y']
    else:
        X, Y = generate_synthetic_data(num_samples=5000)
        np.savez(npz_path, X=X, Y=Y)
        print(f"Data saved to {npz_path}")
    
    print(f"Data loaded: X.shape={X.shape}, Y.shape={Y.shape}")
    
    # 构建模型
    model = build_model()
    
    # 训练
    model = train_model(model, X, Y, epochs=100)
    
    # 量化
    tflite_float, tflite_int8 = quantize_model(model, X)
    
    # 保存模型文件
    model_path = os.path.join(output_dir, 'sound_classifier.h5')
    model.save(model_path)
    print(f"Keras model saved to {model_path}")
    
    tflite_float_path = os.path.join(output_dir, 'sound_classifier_float.tflite')
    with open(tflite_float_path, 'wb') as f:
        f.write(tflite_float)
    print(f"Float TFLite saved to {tflite_float_path}")
    
    tflite_int8_path = os.path.join(output_dir, 'sound_classifier.tflite')
    with open(tflite_int8_path, 'wb') as f:
        f.write(tflite_int8)
    print(f"INT8 TFLite saved to {tflite_int8_path}")
    
    # 验证
    validate_model(tflite_float, tflite_int8, X, Y)
    
    print("\n=== Sound Classifier Training Complete ===")


if __name__ == '__main__':
    main()
