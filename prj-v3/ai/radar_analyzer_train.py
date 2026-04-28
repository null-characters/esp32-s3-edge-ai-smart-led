"""
ESP32-S3 雷达特征编码器 - MLP模型训练与量化
prj-v3 多模态推理架构

模型结构: MLP (8 → 16 → 12 → 6)
训练: Adam优化器, MSE损失 (自编码器风格)
量化: INT8量化 (TFLite)
输出: radar_analyzer.tflite (~2KB)

功能: 将 8 维雷达特征编码为 6 维紧凑特征向量
供多模态融合层使用
"""

import os
import sys
import numpy as np

# 雷达特征维度
RADAR_INPUT_DIM = 8   # 输入: distance, velocity, energy, target_count, motion_amplitude, motion_period, distance_variance, energy_trend
RADAR_OUTPUT_DIM = 6  # 输出: 6 维紧凑特征

# ================================================================
# MLP 模型定义
# ================================================================
def build_model():
    """
    构建 MLP 特征编码模型
    
    输入: 8 维雷达特征
    输出: 6 维紧凑特征
    
    设计思路:
    - 自编码器风格的瓶颈层
    - 学习雷达特征的紧凑表示
    - 供融合层使用
    
    模型大小: ~2KB (INT8 量化后)
    """
    import tensorflow as tf
    
    model = tf.keras.Sequential([
        tf.keras.layers.Dense(16, activation='relu', input_shape=(RADAR_INPUT_DIM,),
                              name='encoder_1'),
        tf.keras.layers.Dense(12, activation='relu', name='encoder_2'),
        tf.keras.layers.Dense(RADAR_OUTPUT_DIM, activation='linear', name='bottleneck')
    ], name='radar_analyzer')
    
    model.summary()
    return model


# ================================================================
# 数据生成
# ================================================================
def generate_synthetic_data(num_samples=5000):
    """
    生成合成雷达训练数据
    
    雷达特征 (LD2410):
    - distance: 目标距离 0-8m
    - velocity: 目标速度 -5~+5 m/s
    - energy: 目标能量/信噪比 0-100
    - target_count: 目标数量 0-8
    - motion_amplitude: 运动幅度 0-1
    - motion_period: 运动周期 0-10s (检测呼吸/风扇)
    - distance_variance: 距离方差 0-5
    - energy_trend: 能量趋势 -1~+1
    
    目标: 学习这些特征的紧凑编码
    """
    print(f"Generating {num_samples} synthetic radar samples...")
    
    X = []
    
    for _ in range(num_samples):
        features = np.zeros(RADAR_INPUT_DIM, dtype=np.float32)
        
        # 场景类型随机选择
        scene_type = np.random.randint(0, 6)
        
        if scene_type == 0:  # 无人
            features[0] = np.random.uniform(0, 0.5)      # distance: 近距离噪声
            features[1] = np.random.uniform(-0.5, 0.5)   # velocity: 微小
            features[2] = np.random.uniform(0, 10)       # energy: 低
            features[3] = 0                              # target_count: 0
            features[4] = np.random.uniform(0, 0.1)      # motion_amplitude: 极小
            features[5] = 0                              # motion_period: 无周期
            features[6] = np.random.uniform(0, 0.5)      # distance_variance: 小
            features[7] = 0                              # energy_trend: 无趋势
            
        elif scene_type == 1:  # 静坐呼吸
            features[0] = np.random.uniform(1, 3)        # distance: 中等距离
            features[1] = np.random.uniform(-0.2, 0.2)   # velocity: 微动
            features[2] = np.random.uniform(20, 40)      # energy: 中等
            features[3] = 1                              # target_count: 1
            features[4] = np.random.uniform(0.1, 0.3)    # motion_amplitude: 呼吸幅度
            features[5] = np.random.uniform(2, 5)        # motion_period: 呼吸周期
            features[6] = np.random.uniform(0, 0.3)      # distance_variance: 小
            features[7] = np.random.uniform(-0.1, 0.1)   # energy_trend: 稳定
            
        elif scene_type == 2:  # 风扇摇头
            features[0] = np.random.uniform(2, 5)        # distance: 中远距离
            features[1] = np.random.uniform(-2, 2)       # velocity: 周期变化
            features[2] = np.random.uniform(30, 60)      # energy: 中高
            features[3] = np.random.randint(1, 3)        # target_count: 1-2
            features[4] = np.random.uniform(0.3, 0.6)    # motion_amplitude: 摇头幅度
            features[5] = np.random.uniform(3, 8)        # motion_period: 风扇周期
            features[6] = np.random.uniform(0.5, 2)      # distance_variance: 中等
            features[7] = 0                              # energy_trend: 稳定
            
        elif scene_type == 3:  # 行走
            features[0] = np.random.uniform(0.5, 6)      # distance: 变化
            features[1] = np.random.uniform(-3, 3)       # velocity: 行走速度
            features[2] = np.random.uniform(40, 80)      # energy: 高
            features[3] = np.random.randint(1, 3)        # target_count: 1-2
            features[4] = np.random.uniform(0.5, 1.0)    # motion_amplitude: 大
            features[5] = np.random.uniform(0.5, 2)      # motion_period: 步频
            features[6] = np.random.uniform(1, 4)        # distance_variance: 大
            features[7] = np.random.uniform(-0.5, 0.5)   # energy_trend: 变化
            
        elif scene_type == 4:  # 多人
            features[0] = np.random.uniform(1, 5)        # distance: 分散
            features[1] = np.random.uniform(-2, 2)       # velocity: 多人混合
            features[2] = np.random.uniform(50, 100)     # energy: 高
            features[3] = np.random.randint(2, 5)        # target_count: 多人
            features[4] = np.random.uniform(0.2, 0.8)    # motion_amplitude: 混合
            features[5] = np.random.uniform(0, 5)        # motion_period: 无明显周期
            features[6] = np.random.uniform(2, 5)        # distance_variance: 大
            features[7] = np.random.uniform(-0.3, 0.3)   # energy_trend: 波动
            
        else:  # 进入/离开
            direction = np.random.choice([-1, 1])        # -1: 离开, 1: 进入
            features[0] = np.random.uniform(1, 6)        # distance
            features[1] = direction * np.random.uniform(1, 4)  # velocity: 方向性
            features[2] = np.random.uniform(30, 70)      # energy
            features[3] = 1                              # target_count: 1
            features[4] = np.random.uniform(0.4, 0.8)    # motion_amplitude
            features[5] = 0                              # motion_period: 无周期
            features[6] = np.random.uniform(0.5, 2)      # distance_variance
            features[7] = direction * np.random.uniform(0.3, 0.8)  # energy_trend: 趋势
        
        X.append(features)
    
    X = np.array(X)
    
    # 归一化到 [0, 1]
    X_norm = np.zeros_like(X)
    X_norm[:, 0] = X[:, 0] / 8.0           # distance: 0-8m
    X_norm[:, 1] = (X[:, 1] + 5) / 10.0    # velocity: -5~+5 → 0-1
    X_norm[:, 2] = X[:, 2] / 100.0         # energy: 0-100
    X_norm[:, 3] = X[:, 3] / 8.0           # target_count: 0-8
    X_norm[:, 4] = X[:, 4]                 # motion_amplitude: 0-1
    X_norm[:, 5] = X[:, 5] / 10.0          # motion_period: 0-10s
    X_norm[:, 6] = X[:, 6] / 5.0           # distance_variance: 0-5
    X_norm[:, 7] = (X[:, 7] + 1) / 2.0     # energy_trend: -1~+1 → 0-1
    
    print(f"Generated: X.shape={X_norm.shape}")
    return X_norm, X  # 返回归一化和原始数据


# ================================================================
# 模型训练 (自编码器风格)
# ================================================================
def train_model(model, X, epochs=100, batch_size=32):
    """
    训练模型
    
    使用自编码器风格训练:
    - 输入: 8 维雷达特征
    - 目标: 重建输入 (通过解码器)
    
    但我们只保留编码器部分用于特征提取
    """
    import tensorflow as tf
    
    # 构建完整的自编码器用于训练
    autoencoder = tf.keras.Sequential([
        # 编码器 (即我们要的模型)
        tf.keras.layers.Dense(16, activation='relu', input_shape=(RADAR_INPUT_DIM,),
                              name='encoder_1'),
        tf.keras.layers.Dense(12, activation='relu', name='encoder_2'),
        tf.keras.layers.Dense(RADAR_OUTPUT_DIM, activation='linear', name='bottleneck'),
        
        # 解码器 (仅用于训练)
        tf.keras.layers.Dense(12, activation='relu', name='decoder_1'),
        tf.keras.layers.Dense(16, activation='relu', name='decoder_2'),
        tf.keras.layers.Dense(RADAR_INPUT_DIM, activation='sigmoid', name='output')
    ], name='radar_autoencoder')
    
    autoencoder.summary()
    
    autoencoder.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss='mse',
        metrics=['mae']
    )
    
    history = autoencoder.fit(
        X, X,  # 自编码器: 输入 = 目标
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
    
    final_loss = history.history['val_loss'][-1]
    print(f"\nTraining complete. Final val_loss: {final_loss:.6f}")
    
    # 提取编码器部分
    encoder = tf.keras.Sequential([
        autoencoder.get_layer('encoder_1'),
        autoencoder.get_layer('encoder_2'),
        autoencoder.get_layer('bottleneck')
    ])
    
    return encoder


# ================================================================
# INT8 量化
# ================================================================
def quantize_model(model, X):
    """INT8 量化模型"""
    import tensorflow as tf
    
    # Float32 模型
    converter_float = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_float = converter_float.convert()
    print(f"Float32 model size: {len(tflite_float)} bytes ({len(tflite_float)/1024:.2f} KB)")
    
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
    print(f"INT8 model size: {len(tflite_int8)} bytes ({len(tflite_int8)/1024:.2f} KB)")
    
    return tflite_float, tflite_int8


# ================================================================
# 模型验证
# ================================================================
def validate_model(tflite_float, tflite_int8, X):
    """验证量化前后模型输出一致性"""
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
            
            results.append(output[0])
        
        return np.array(results)
    
    # Float32 推理
    print("\nValidating float32 model...")
    Y_float = run_tflite_inference(tflite_float, X)
    print(f"  Output range: [{Y_float.min():.4f}, {Y_float.max():.4f}]")
    print(f"  Output mean: {Y_float.mean():.4f}")
    
    # INT8 推理
    print("Validating INT8 model...")
    Y_int8 = run_tflite_inference(tflite_int8, X)
    print(f"  Output range: [{Y_int8.min():.4f}, {Y_int8.max():.4f}]")
    print(f"  Output mean: {Y_int8.mean():.4f}")
    
    # MSE
    mse = np.mean((Y_float - Y_int8) ** 2)
    mae = np.mean(np.abs(Y_float - Y_int8))
    print(f"  MSE: {mse:.6f}")
    print(f"  MAE: {mae:.4f}")
    
    if mae < 0.1:
        print("  ✅ INT8 quantization MAE < 0.1")
    else:
        print("  ⚠️  INT8 quantization MAE >= 0.1")
    
    return mse, mae


# ================================================================
# 主流程
# ================================================================
def main():
    import tensorflow as tf
    print(f"TensorFlow version: {tf.__version__}")
    
    output_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 生成或加载数据
    npz_path = os.path.join(output_dir, 'radar_training_data.npz')
    
    if os.path.exists(npz_path):
        print(f"Loading data from {npz_path}")
        data = np.load(npz_path)
        X = data['X']
    else:
        X, X_raw = generate_synthetic_data(num_samples=5000)
        np.savez(npz_path, X=X, X_raw=X_raw)
        print(f"Data saved to {npz_path}")
    
    print(f"Data loaded: X.shape={X.shape}")
    
    # 构建模型
    model = build_model()
    
    # 训练 (自编码器风格)
    model = train_model(model, X, epochs=100)
    
    # 量化
    tflite_float, tflite_int8 = quantize_model(model, X)
    
    # 保存模型文件
    model_path = os.path.join(output_dir, 'radar_analyzer.h5')
    model.save(model_path)
    print(f"Keras model saved to {model_path}")
    
    tflite_float_path = os.path.join(output_dir, 'radar_analyzer_float.tflite')
    with open(tflite_float_path, 'wb') as f:
        f.write(tflite_float)
    print(f"Float TFLite saved to {tflite_float_path}")
    
    tflite_int8_path = os.path.join(output_dir, 'radar_analyzer.tflite')
    with open(tflite_int8_path, 'wb') as f:
        f.write(tflite_int8)
    print(f"INT8 TFLite saved to {tflite_int8_path}")
    
    # 验证
    validate_model(tflite_float, tflite_int8, X)
    
    print("\n=== Radar Analyzer Training Complete ===")


if __name__ == '__main__':
    main()
