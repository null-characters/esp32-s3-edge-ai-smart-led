"""
ESP32-S3 雷达场景识别 - 训练数据生成
Phase 1: RD-013

基于 LD2410 雷达特征生成合成训练数据，用于场景分类。

场景类别:
0 - 无人 (empty)
1 - 静坐呼吸 (breathing)
2 - 风扇摇头 (fan)
3 - 行走 (walking)
4 - 多人 (multiple_people)
5 - 进入/离开 (entering_leaving)

特征向量 (8维):
- distance_norm: 归一化距离 [0, 1]
- velocity_norm: 归一化速度 [-1, 1]
- energy_norm: 归一化能量 [0, 1]
- target_count: 目标数量
- motion_amplitude: 运动幅度
- motion_period: 运动周期 (Hz)
- distance_variance: 距离方差
- energy_trend: 能量趋势
"""

import numpy as np
import os

# ================================================================
# 参数定义
# ================================================================
N_SAMPLES_PER_CLASS = 3000
SEED = 42

# 场景定义
SCENES = {
    0: 'empty',           # 无人
    1: 'breathing',       # 静坐呼吸
    2: 'fan',             # 风扇摇头
    3: 'walking',         # 行走
    4: 'multiple_people', # 多人
    5: 'entering_leaving' # 进入/离开
}

# 归一化参数
DISTANCE_MAX = 8.0    # 最大距离 8m
VELOCITY_MAX = 5.0    # 最大速度 5m/s
ENERGY_MAX = 100.0    # 最大能量


def generate_empty_scene(rng: np.random.Generator) -> dict:
    """无人场景: 无目标，能量接近0"""
    return {
        'distance_norm': 0.0,
        'velocity_norm': 0.0,
        'energy_norm': rng.uniform(0.0, 0.05),
        'target_count': 0.0,
        'motion_amplitude': 0.0,
        'motion_period': 0.0,
        'distance_variance': 0.0,
        'energy_trend': 0.0
    }


def generate_breathing_scene(rng: np.random.Generator) -> dict:
    """静坐呼吸场景: 距离稳定，周期性微动 (0.2-0.3Hz)，能量低"""
    distance = rng.uniform(0.5, 3.0)  # 0.5-3m 距离
    return {
        'distance_norm': distance / DISTANCE_MAX,
        'velocity_norm': rng.uniform(-0.05, 0.05),  # 速度很小
        'energy_norm': rng.uniform(0.1, 0.3),       # 能量低
        'target_count': 1.0,
        'motion_amplitude': rng.uniform(0.0, 0.05),
        'motion_period': rng.uniform(0.2, 0.33),    # 呼吸频率 12-20次/分
        'distance_variance': rng.uniform(0.0, 0.02),
        'energy_trend': rng.uniform(-0.05, 0.05)
    }


def generate_fan_scene(rng: np.random.Generator) -> dict:
    """风扇场景: 固定位置，规律运动，中等能量"""
    distance = rng.uniform(1.0, 4.0)  # 风扇位置
    period = rng.choice([0.5, 0.67, 1.0])  # 风扇转速: 30/40/60 RPM
    return {
        'distance_norm': distance / DISTANCE_MAX,
        'velocity_norm': rng.uniform(0.1, 0.4),  # 规律速度
        'energy_norm': rng.uniform(0.3, 0.6),    # 中等能量
        'target_count': 1.0,
        'motion_amplitude': rng.uniform(0.1, 0.3),
        'motion_period': period,
        'distance_variance': rng.uniform(0.0, 0.05),  # 位置固定
        'energy_trend': rng.uniform(-0.02, 0.02)      # 能量稳定
    }


def generate_walking_scene(rng: np.random.Generator) -> dict:
    """行走场景: 速度变化大，能量高，距离变化"""
    return {
        'distance_norm': rng.uniform(0.2, 0.8),       # 距离变化
        'velocity_norm': rng.uniform(-0.6, 0.6),      # 速度变化大
        'energy_norm': rng.uniform(0.5, 0.9),         # 能量高
        'target_count': 1.0,
        'motion_amplitude': rng.uniform(0.3, 0.7),
        'motion_period': rng.uniform(0.8, 1.5),       # 步频
        'distance_variance': rng.uniform(0.1, 0.3),   # 距离变化大
        'energy_trend': rng.uniform(-0.1, 0.1)
    }


def generate_multiple_people_scene(rng: np.random.Generator) -> dict:
    """多人场景: 多目标，距离方差大，能量高"""
    n_people = rng.integers(2, 5)
    return {
        'distance_norm': rng.uniform(0.2, 0.7),
        'velocity_norm': rng.uniform(-0.3, 0.3),
        'energy_norm': rng.uniform(0.6, 1.0),         # 能量高
        'target_count': float(n_people),
        'motion_amplitude': rng.uniform(0.2, 0.5),
        'motion_period': rng.uniform(0.3, 0.8),       # 多人混合频率
        'distance_variance': rng.uniform(0.2, 0.5),   # 方差大
        'energy_trend': rng.uniform(-0.05, 0.05)
    }


def generate_entering_leaving_scene(rng: np.random.Generator) -> dict:
    """进入/离开场景: 能量趋势明显，距离变化"""
    is_entering = rng.random() > 0.5
    return {
        'distance_norm': rng.uniform(0.3, 0.9),
        'velocity_norm': rng.uniform(-0.8, 0.8),      # 速度大
        'energy_norm': rng.uniform(0.4, 0.8),
        'target_count': 1.0,
        'motion_amplitude': rng.uniform(0.4, 0.8),
        'motion_period': rng.uniform(0.5, 1.0),
        'distance_variance': rng.uniform(0.1, 0.3),
        'energy_trend': 0.3 if is_entering else -0.3  # 趋势明显
    }


SCENE_GENERATORS = {
    0: generate_empty_scene,
    1: generate_breathing_scene,
    2: generate_fan_scene,
    3: generate_walking_scene,
    4: generate_multiple_people_scene,
    5: generate_entering_leaving_scene
}


def add_noise(features: dict, rng: np.random.Generator, noise_level: float = 0.05) -> dict:
    """添加高斯噪声"""
    noisy = {}
    for key, value in features.items():
        noise = rng.normal(0, noise_level * abs(value + 0.1))
        noisy[key] = np.clip(value + noise, -1.0, 1.0) if 'norm' in key else max(0, value + noise)
    return noisy


def generate_training_data(n_samples_per_class: int = N_SAMPLES_PER_CLASS,
                           seed: int = SEED) -> tuple:
    """
    生成训练数据集
    Returns: (X, Y) numpy arrays
    """
    rng = np.random.default_rng(seed)

    X_list = []
    Y_list = []

    for scene_id, generator in SCENE_GENERATORS.items():
        for _ in range(n_samples_per_class):
            features = generator(rng)
            features = add_noise(features, rng)

            X_list.append([
                features['distance_norm'],
                features['velocity_norm'],
                features['energy_norm'],
                features['target_count'],
                features['motion_amplitude'],
                features['motion_period'],
                features['distance_variance'],
                features['energy_trend']
            ])
            Y_list.append(scene_id)

    X = np.array(X_list, dtype=np.float32)
    Y = np.array(Y_list, dtype=np.int32)

    # 打乱数据
    indices = rng.permutation(len(X))
    X = X[indices]
    Y = Y[indices]

    return X, Y


def print_statistics(X: np.ndarray, Y: np.ndarray):
    """打印数据集统计信息"""
    print("\n=== Radar Training Data Statistics ===")
    print(f"Total samples: {len(X)}")

    feature_names = ['distance_norm', 'velocity_norm', 'energy_norm',
                     'target_count', 'motion_amplitude', 'motion_period',
                     'distance_variance', 'energy_trend']

    print("\nInput features:")
    for i, name in enumerate(feature_names):
        print(f"  {name:20s}: min={X[:, i].min():.4f}, "
              f"max={X[:, i].max():.4f}, mean={X[:, i].mean():.4f}")

    print("\nClass distribution:")
    for scene_id, scene_name in SCENES.items():
        count = (Y == scene_id).sum()
        print(f"  {scene_id}: {scene_name:20s}: {count} ({count/len(Y)*100:.1f}%)")


if __name__ == '__main__':
    # 生成数据
    X, Y = generate_training_data()

    # 打印统计
    print_statistics(X, Y)

    # 保存
    output_dir = os.path.dirname(os.path.abspath(__file__))
    np.savez(os.path.join(output_dir, 'radar_training_data.npz'), X=X, Y=Y)
    print(f"\nSaved to radar_training_data.npz")
