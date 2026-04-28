"""
ESP32-S3 多模态融合决策器 - MLP模型训练与量化
prj-v3 多模态推理架构

模型结构: MLP (19 → 32 → 16 → 2)
训练: Adam优化器, MSE损失
量化: INT8量化 (TFLite)
输出: fusion_model.tflite (~3KB)

输入:
- 声音分类结果: 5 维 (softmax 概率)
- 雷达分析结果: 6 维 (紧凑特征)
- 环境特征: 8 维 (时间、天气等)

输出:
- 色温: 2700-6500K (归一化到 0-1)
- 亮度: 0-100% (归一化到 0-1)
"""

import os
import sys
import numpy as np

# 输入维度
SOUND_DIM = 5     # 声音分类概率
RADAR_DIM = 6     # 雷达紧凑特征
ENV_DIM = 8       # 环境特征
FUSION_INPUT_DIM = SOUND_DIM + RADAR_DIM + ENV_DIM  # 19 维

# 输出维度
FUSION_OUTPUT_DIM = 2  # 色温、亮度

# 输出范围
COLOR_TEMP_MIN = 2700
COLOR_TEMP_MAX = 6500
COLOR_TEMP_RANGE = COLOR_TEMP_MAX - COLOR_TEMP_MIN  # 3800K
BRIGHTNESS_MIN = 0
BRIGHTNESS_MAX = 100
BRIGHTNESS_RANGE = BRIGHTNESS_MAX - BRIGHTNESS_MIN  # 100%

# ================================================================
# MLP 模型定义
# ================================================================
def build_model():
    """
    构建 MLP 融合模型
    
    输入: 19 维多模态特征向量
    输出: 2 维控制决策 (色温、亮度)
    
    设计思路:
    - 声音概率 + 雷达特征 + 环境特征 → 融合决策
    - Sigmoid 输出保证 [0, 1] 范围
    - 反归一化得到真实值
    
    模型大小: ~3KB (INT8 量化后)
    """
    import tensorflow as tf
    
    model = tf.keras.Sequential([
        tf.keras.layers.Dense(32, activation='relu', input_shape=(FUSION_INPUT_DIM,),
                              name='fusion_1'),
        tf.keras.layers.Dense(16, activation='relu', name='fusion_2'),
        tf.keras.layers.Dense(FUSION_OUTPUT_DIM, activation='sigmoid', name='output')
    ], name='fusion_model')
    
    model.summary()
    return model


# ================================================================
# 数据生成
# ================================================================
def generate_synthetic_data(num_samples=5000):
    """
    生成合成多模态训练数据
    
    输入特征:
    - 声音分类 (5维 softmax): 寂静、单人讲话、多人讨论、键盘、其他
    - 雷达特征 (6维): 紧凑编码
    - 环境特征 (8维): hour_sin, hour_cos, sunset_proximity, sunrise_norm, sunset_norm, weather, presence, day_type
    
    输出目标:
    - 色温 (2700-6500K)
    - 亮度 (0-100%)
    
    场景映射:
    - 会议高潮 → 6500K, 100%
    - 专注工作 → 4000K, 60%
    - 汇报演示 → 主灯 30%, 黑板灯亮 (简化为 4000K, 40%)
    - 休息放松 → 2700K, 30%
    - 打扫卫生 → 4000K, 50%
    """
    print(f"Generating {num_samples} synthetic fusion samples...")
    
    X = []
    Y = []
    Y_raw = []
    
    for _ in range(num_samples):
        # 随机场景
        scene_type = np.random.randint(0, 5)
        
        # 声音特征 (softmax 概率)
        sound_feat = np.zeros(SOUND_DIM, dtype=np.float32)
        
        # 雷达特征 (6维紧凑编码)
        radar_feat = np.random.randn(RADAR_DIM).astype(np.float32) * 0.5
        
        # 环境特征 (8维)
        env_feat = np.zeros(ENV_DIM, dtype=np.float32)
        hour = np.random.uniform(0, 24)
        env_feat[0] = np.sin(hour * np.pi / 12)  # hour_sin
        env_feat[1] = np.cos(hour * np.pi / 12)  # hour_cos
        env_feat[2] = np.random.uniform(0, 1)    # sunset_proximity
        env_feat[3] = np.random.uniform(0, 1)    # sunrise_norm
        env_feat[4] = np.random.uniform(0, 1)    # sunset_norm
        env_feat[5] = np.random.randint(0, 4) / 3.0  # weather (0-3)
        env_feat[6] = np.random.randint(0, 2)    # presence
        env_feat[7] = np.random.randint(0, 2)    # day_type
        
        # 根据场景设置特征和输出
        if scene_type == 0:  # 会议高潮
            # 多人讨论 + 多人雷达特征
            sound_feat[2] = 0.8  # 多人讨论
            sound_feat[0] = 0.1
            sound_feat[1] = 0.1
            
            radar_feat[0:3] = [0.6, 0.4, 0.8]  # 多人特征
            
            # 输出: 高色温高亮度
            color_temp_norm = 1.0  # 6500K
            brightness_norm = 1.0  # 100%
            
        elif scene_type == 1:  # 专注工作
            # 键盘声 + 单人雷达特征
            sound_feat[3] = 0.7  # 键盘
            sound_feat[0] = 0.2
            sound_feat[1] = 0.1
            
            radar_feat[0:3] = [0.3, 0.2, 0.4]  # 单人静坐
            
            # 输出: 暖光中等亮度
            color_temp_norm = 0.34  # 4000K / 3800 + 2700 = 0.34
            brightness_norm = 0.6   # 60%
            
        elif scene_type == 2:  # 汇报演示
            # 单人讲话 + 单人面向投影
            sound_feat[1] = 0.8  # 单人讲话
            sound_feat[0] = 0.1
            sound_feat[4] = 0.1
            
            radar_feat[0:3] = [0.4, 0.1, 0.5]  # 单人面向
            
            # 输出: 中等亮度
            color_temp_norm = 0.34  # 4000K
            brightness_norm = 0.4   # 40%
            
        elif scene_type == 3:  # 休息放松
            # 寂静 + 单人躺姿
            sound_feat[0] = 0.9  # 寂静
            sound_feat[4] = 0.1
            
            radar_feat[0:3] = [0.2, 0.1, 0.3]  # 单人微动
            
            # 输出: 暖光低亮度
            color_temp_norm = 0.0  # 2700K
            brightness_norm = 0.3  # 30%
            
        else:  # 打扫卫生
            # 其他噪音 + 不规则移动
            sound_feat[4] = 0.6  # 其他噪音
            sound_feat[0] = 0.3
            sound_feat[1] = 0.1
            
            radar_feat[0:3] = [0.5, 0.6, 0.6]  # 不规则移动
            
            # 输出: 中等亮度
            color_temp_norm = 0.34  # 4000K
            brightness_norm = 0.5   # 50%
        
        # 添加噪声
        sound_feat += np.random.randn(SOUND_DIM) * 0.05
        sound_feat = np.clip(sound_feat, 0, 1)
        sound_feat /= sound_feat.sum()  # 保持 softmax 性质
        
        radar_feat += np.random.randn(RADAR_DIM) * 0.1
        
        env_feat += np.random.randn(ENV_DIM) * 0.05
        env_feat = np.clip(env_feat, 0, 1)
        
        # 拼接输入
        input_feat = np.concatenate([sound_feat, radar_feat, env_feat])
        X.append(input_feat)
        
        # 输出 (归一化)
        Y.append([color_temp_norm, brightness_norm])
        
        # 原始值
        color_temp_raw = color_temp_norm * COLOR_TEMP_RANGE + COLOR_TEMP_MIN
        brightness_raw = brightness_norm * BRIGHTNESS_RANGE + BRIGHTNESS_MIN
        Y_raw.append([color_temp_raw, brightness_raw])
    
    X = np.array(X)
    Y = np.array(Y)
    Y_raw = np.array(Y_raw)
    
    print(f"Generated: X.shape={X.shape}, Y.shape={Y.shape}")
    return X, Y, Y_raw


# ================================================================
# 模型训练
# ================================================================
def train_model(model, X, Y, epochs=150, batch_size=32):
    """训练模型"""
    import tensorflow as tf
    
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss='mse',
        metrics=['mae']
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
    
    final_loss = history.history['val_loss'][-1]
    print(f"\nTraining complete. Final val_loss: {final_loss:.6f}")
    
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
def validate_model(tflite_float, tflite_int8, X, Y, Y_raw):
    """验证量化前后模型精度"""
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
    Y_pred_float = run_tflite_inference(tflite_float, X)
    
    # 反归一化
    Y_pred_float_raw = np.zeros_like(Y_pred_float)
    Y_pred_float_raw[:, 0] = Y_pred_float[:, 0] * COLOR_TEMP_RANGE + COLOR_TEMP_MIN
    Y_pred_float_raw[:, 1] = Y_pred_float[:, 1] * BRIGHTNESS_RANGE + BRIGHTNESS_MIN
    
    mae_float_ct = np.mean(np.abs(Y_pred_float_raw[:, 0] - Y_raw[:, 0]))
    mae_float_br = np.mean(np.abs(Y_pred_float_raw[:, 1] - Y_raw[:, 1]))
    print(f"  Float32 MAE - color_temp: {mae_float_ct:.1f}K, brightness: {mae_float_br:.1f}%")
    
    # INT8 推理
    print("Validating INT8 model...")
    Y_pred_int8 = run_tflite_inference(tflite_int8, X)
    
    # 反归一化
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
        print("  ⚠️  INT8 quantization precision loss >= 5%")
    
    # 输出范围检查
    print(f"\n  Output range check (Sigmoid guarantee):")
    print(f"    Float32 output: [{Y_pred_float.min():.4f}, {Y_pred_float.max():.4f}]")
    print(f"    INT8 output: [{Y_pred_int8.min():.4f}, {Y_pred_int8.max():.4f}]")
    
    return mae_float_ct, mae_float_br, mae_int8_ct, mae_int8_br


# ================================================================
# 主流程
# ================================================================
def main():
    import tensorflow as tf
    print(f"TensorFlow version: {tf.__version__}")
    
    output_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 生成或加载数据
    npz_path = os.path.join(output_dir, 'fusion_training_data.npz')
    
    if os.path.exists(npz_path):
        print(f"Loading data from {npz_path}")
        data = np.load(npz_path)
        X = data['X']
        Y = data['Y']
        Y_raw = data['Y_raw'] if 'Y_raw' in data else None
    else:
        X, Y, Y_raw = generate_synthetic_data(num_samples=5000)
        np.savez(npz_path, X=X, Y=Y, Y_raw=Y_raw)
        print(f"Data saved to {npz_path}")
    
    print(f"Data loaded: X.shape={X.shape}, Y.shape={Y.shape}")
    
    # 构建模型
    model = build_model()
    
    # 训练
    model = train_model(model, X, Y, epochs=150)
    
    # 量化
    tflite_float, tflite_int8 = quantize_model(model, X)
    
    # 保存模型文件
    model_path = os.path.join(output_dir, 'fusion_model.h5')
    model.save(model_path)
    print(f"Keras model saved to {model_path}")
    
    tflite_float_path = os.path.join(output_dir, 'fusion_model_float.tflite')
    with open(tflite_float_path, 'wb') as f:
        f.write(tflite_float)
    print(f"Float TFLite saved to {tflite_float_path}")
    
    tflite_int8_path = os.path.join(output_dir, 'fusion_model.tflite')
    with open(tflite_int8_path, 'wb') as f:
        f.write(tflite_int8)
    print(f"INT8 TFLite saved to {tflite_int8_path}")
    
    # 验证
    if Y_raw is not None:
        validate_model(tflite_float, tflite_int8, X, Y, Y_raw)
    
    print("\n=== Fusion Model Training Complete ===")


if __name__ == '__main__':
    main()