"""
ESP32-S3 智能照明 - MLP 消融实验
测试不同网络规模对精度的影响

实验目标：找到"够用就好"的最小模型
"""

import os
import sys
import numpy as np

# 确保能找到 train_model.py
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from train_model import (
    build_model,
    train_model,
    quantize_model,
    validate_model,
    convert_to_c_array
)


def build_small_model(hidden_sizes):
    """
    构建指定隐藏层大小的MLP模型
    
    Args:
        hidden_sizes: 隐藏层大小列表，如 [16, 8] 表示 7→16→8→2
    """
    import tensorflow as tf
    
    layers = []
    for i, size in enumerate(hidden_sizes):
        if i == 0:
            layers.append(tf.keras.layers.Dense(
                size, activation='relu', input_shape=(7,),
                name=f'hidden_{i+1}'
            ))
        else:
            layers.append(tf.keras.layers.Dense(
                size, activation='relu',
                name=f'hidden_{i+1}'
            ))
    
    layers.append(tf.keras.layers.Dense(2, activation='linear', name='output'))
    
    model = tf.keras.Sequential(layers, name=f'mlp_{"_".join(map(str, hidden_sizes))}')
    model.summary()
    return model


def count_params(model):
    """计算模型参数量"""
    return model.count_params()


def run_ablation_experiment(X, Y, configs):
    """
    运行消融实验
    
    Args:
        X, Y: 训练数据
        configs: 模型配置列表，如 [[32, 16], [16, 8], [8]]
    
    Returns:
        实验结果列表
    """
    results = []
    
    for config in configs:
        print(f"\n{'='*60}")
        print(f"实验: 7 → {' → '.join(map(str, config))} → 2")
        print(f"{'='*60}")
        
        # 构建模型
        model = build_small_model(config)
        n_params = count_params(model)
        
        # 训练
        model, y_mean, y_std = train_model(model, X, Y, epochs=150)
        
        # 量化
        tflite_float, tflite_int8 = quantize_model(model, X, y_mean, y_std)
        
        # 验证
        mae_float_ct, mae_float_br, mae_int8_ct, mae_int8_br = validate_model(
            tflite_float, tflite_int8, X, Y, y_mean, y_std
        )
        
        results.append({
            'config': config,
            'n_params': n_params,
            'model_size': len(tflite_int8),
            'mae_float_ct': mae_float_ct,
            'mae_float_br': mae_float_br,
            'mae_int8_ct': mae_int8_ct,
            'mae_int8_br': mae_int8_br,
        })
        
        print(f"\n结果摘要:")
        print(f"  参数量: {n_params}")
        print(f"  模型大小: {len(tflite_int8)} bytes")
        print(f"  Float32 MAE: 色温={mae_float_ct:.1f}K, 亮度={mae_float_br:.1f}%")
        print(f"  INT8 MAE: 色温={mae_int8_ct:.1f}K, 亮度={mae_int8_br:.1f}%")
    
    return results


def print_comparison_table(results):
    """打印对比表格"""
    print("\n" + "="*80)
    print("消融实验结果对比")
    print("="*80)
    
    # 表头
    print(f"{'模型结构':<20} {'参数量':>8} {'体积(B)':>10} {'Float32':>15} {'INT8':>15} {'亮度损失':>10}")
    print(f"{'':<20} {'':>8} {'':>10} {'色温/亮度':>15} {'色温/亮度':>15} {'(量化)':>10}")
    print("-"*80)
    
    # 数据行
    for r in results:
        config_str = f"7→{'→'.join(map(str, r['config']))}→2"
        float_str = f"{r['mae_float_ct']:.0f}K/{r['mae_float_br']:.1f}%"
        int8_str = f"{r['mae_int8_ct']:.0f}K/{r['mae_int8_br']:.1f}%"
        
        # 计算亮度损失百分比
        if r['mae_float_br'] > 0:
            br_loss = abs(r['mae_int8_br'] - r['mae_float_br']) / r['mae_float_br'] * 100
        else:
            br_loss = 0
        
        print(f"{config_str:<20} {r['n_params']:>8} {r['model_size']:>10} {float_str:>15} {int8_str:>15} {br_loss:>9.1f}%")
    
    print("="*80)
    
    # 推荐
    print("\n推荐分析:")
    baseline = results[0]  # 原始模型作为基准
    
    for r in results[1:]:
        # 检查亮度MAE是否增加超过10%
        br_increase = r['mae_int8_br'] - baseline['mae_int8_br']
        ct_increase = r['mae_int8_ct'] - baseline['mae_int8_ct']
        
        if br_increase <= 0.5 and ct_increase <= 5:
            print(f"  ✅ {r['config']}: 可替代原始模型，体积减少 {100*(1-r['model_size']/baseline['model_size']):.0f}%")
        else:
            print(f"  ⚠️  {r['config']}: 精度下降较多，不推荐")


def main():
    import tensorflow as tf
    print(f"TensorFlow version: {tf.__version__}")
    
    # 加载训练数据
    output_dir = os.path.dirname(os.path.abspath(__file__))
    npz_path = os.path.join(output_dir, 'training_data.npz')
    
    if not os.path.exists(npz_path):
        print("错误: 未找到训练数据，请先运行 generate_data.py")
        sys.exit(1)
    
    print(f"加载数据: {npz_path}")
    data = np.load(npz_path)
    X, Y = data['X'], data['Y']
    print(f"数据: {X.shape[0]} 样本, {X.shape[1]} 特征")
    
    # 定义实验配置
    configs = [
        [32, 16],  # 原始模型 (baseline)
        [16, 8],   # 缩小一半
        [8],       # 极简版
        [16],      # 单隐藏层
        [12, 6],   # 更小
    ]
    
    # 运行实验
    results = run_ablation_experiment(X, Y, configs)
    
    # 打印对比表
    print_comparison_table(results)
    
    # 保存结果
    results_path = os.path.join(output_dir, 'ablation_results.txt')
    with open(results_path, 'w') as f:
        f.write("消融实验结果\n")
        f.write("="*80 + "\n")
        for r in results:
            f.write(f"模型: 7→{'→'.join(map(str, r['config']))}→2\n")
            f.write(f"  参数量: {r['n_params']}\n")
            f.write(f"  模型大小: {r['model_size']} bytes\n")
            f.write(f"  Float32 MAE: 色温={r['mae_float_ct']:.1f}K, 亮度={r['mae_float_br']:.1f}%\n")
            f.write(f"  INT8 MAE: 色温={r['mae_int8_ct']:.1f}K, 亮度={r['mae_int8_br']:.1f}%\n")
            f.write("\n")
    
    print(f"\n结果已保存到: {results_path}")


if __name__ == '__main__':
    main()
