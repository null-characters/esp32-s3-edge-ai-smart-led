"""
ESP32-S3 会议室智能照明 - 训练数据生成
Phase 3: AI-002

基于领域知识规则生成训练数据，让MLP模型学习这些规则。
规则来源: Phase 1 规则引擎 + Phase 2 环境感知调光算法
"""

import numpy as np
import csv
import os

# ================================================================
# 参数定义
# ================================================================
N_SAMPLES = 15000
SEED = 42

# 时段参数
MORNING_COLOR_TEMP = 4800
MORNING_BRIGHTNESS = 65
AFTERNOON_COLOR_TEMP = 4200
AFTERNOON_BRIGHTNESS = 60
EVENING_COLOR_TEMP = 3500
EVENING_BRIGHTNESS = 55

# 补偿参数
WEATHER_COMP_MAX = 25      # 天气补偿最大增加 25%
SUNSET_PROX_MAX = 15       # 日落临近最大增加 15%
LATE_NIGHT_MAX_BRIGHTNESS = 50  # 深夜最大亮度

# 范围
COLOR_TEMP_MIN = 2700
COLOR_TEMP_MAX = 6500
BRIGHTNESS_MIN = 0
BRIGHTNESS_MAX = 100


def calculate_sunset_proximity(hour: float, sunset_hour: float) -> float:
    """
    计算日落临近度 (与04-ai-inference.md中定义一致)
    """
    hours_to_sunset = sunset_hour - hour

    if hours_to_sunset > 2.0:
        return 0.0
    elif hours_to_sunset >= 0.0:
        return 1.0 - (hours_to_sunset / 2.0)
    elif hours_to_sunset > -2.0:
        return 1.0
    else:
        return max(0.0, 1.0 + (hours_to_sunset + 2.0) / 2.0)


def generate_sample(rng: np.random.Generator) -> dict:
    """
    生成单个训练样本
    """
    # 随机输入
    hour = rng.uniform(6, 23)  # 6:00 - 23:00 (工作时间)
    weather = rng.choice([0.0, 0.25, 0.5, 0.75, 1.0],
                         p=[0.30, 0.20, 0.25, 0.15, 0.10])
    presence = rng.choice([0.0, 1.0], p=[0.25, 0.75])
    sunrise_hour = rng.uniform(5.5, 7.0)   # 日出 5:30-7:00
    sunset_hour = rng.uniform(17.5, 19.5)   # 日落 17:30-19:30

    # 特征编码
    hour_sin = np.sin(hour * 2.0 * np.pi / 24.0)
    hour_cos = np.cos(hour * 2.0 * np.pi / 24.0)
    sunset_proximity = calculate_sunset_proximity(hour, sunset_hour)
    sunrise_hour_norm = sunrise_hour / 24.0
    sunset_hour_norm = sunset_hour / 24.0

    # 输出计算 (领域规则)
    if presence == 0.0:
        # 无人 → 关灯
        color_temp = 2700
        brightness = 0
    else:
        # 基础参数 (按时段)
        if hour < 12:
            base_brightness = MORNING_BRIGHTNESS
            base_color_temp = MORNING_COLOR_TEMP
        elif hour < sunset_hour - 1:
            base_brightness = AFTERNOON_BRIGHTNESS
            base_color_temp = AFTERNOON_COLOR_TEMP
        else:
            base_brightness = EVENING_BRIGHTNESS
            base_color_temp = EVENING_COLOR_TEMP

        # 天气补偿: 阴天增强
        weather_boost = weather * WEATHER_COMP_MAX

        # 日落补偿: 越接近日落越亮
        sunset_boost = sunset_proximity * SUNSET_PROX_MAX

        # 深夜限制
        max_brightness = LATE_NIGHT_MAX_BRIGHTNESS if hour >= 22 else BRIGHTNESS_MAX

        # 最终输出
        brightness = min(max_brightness,
                         base_brightness + weather_boost + sunset_boost)
        color_temp = base_color_temp

        # 添加少量噪声 (模拟真实场景的随机性)
        brightness += rng.normal(0, 1.5)
        color_temp += rng.normal(0, 30)

    # 限制范围
    color_temp = np.clip(color_temp, COLOR_TEMP_MIN, COLOR_TEMP_MAX)
    brightness = np.clip(brightness, BRIGHTNESS_MIN, BRIGHTNESS_MAX)

    return {
        'hour_sin': hour_sin,
        'hour_cos': hour_cos,
        'sunset_proximity': sunset_proximity,
        'sunrise_hour_norm': sunrise_hour_norm,
        'sunset_hour_norm': sunset_hour_norm,
        'weather': weather,
        'presence': presence,
        'color_temp': color_temp,
        'brightness': brightness
    }


def generate_training_data(n_samples: int = N_SAMPLES,
                           seed: int = SEED) -> tuple:
    """
    生成训练数据集
    Returns: (X, Y) numpy arrays
    """
    rng = np.random.default_rng(seed)

    X_list = []
    Y_list = []

    for _ in range(n_samples):
        sample = generate_sample(rng)
        X_list.append([
            sample['hour_sin'],
            sample['hour_cos'],
            sample['sunset_proximity'],
            sample['sunrise_hour_norm'],
            sample['sunset_hour_norm'],
            sample['weather'],
            sample['presence']
        ])
        Y_list.append([
            sample['color_temp'],
            sample['brightness']
        ])

    X = np.array(X_list, dtype=np.float32)
    Y = np.array(Y_list, dtype=np.float32)

    return X, Y


def save_to_csv(X: np.ndarray, Y: np.ndarray, filepath: str):
    """
    保存训练数据为CSV文件
    """
    feature_names = ['hour_sin', 'hour_cos', 'sunset_proximity',
                     'sunrise_hour_norm', 'sunset_hour_norm',
                     'weather', 'presence']
    output_names = ['color_temp', 'brightness']

    with open(filepath, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(feature_names + output_names)
        for i in range(len(X)):
            row = list(X[i]) + list(Y[i])
            writer.writerow(row)

    print(f"Saved {len(X)} samples to {filepath}")


def print_statistics(X: np.ndarray, Y: np.ndarray):
    """
    打印数据集统计信息
    """
    print("\n=== Training Data Statistics ===")
    print(f"Total samples: {len(X)}")

    feature_names = ['hour_sin', 'hour_cos', 'sunset_proximity',
                     'sunrise_hour_norm', 'sunset_hour_norm',
                     'weather', 'presence']

    print("\nInput features:")
    for i, name in enumerate(feature_names):
        print(f"  {name:20s}: min={X[:, i].min():.4f}, "
              f"max={X[:, i].max():.4f}, mean={X[:, i].mean():.4f}")

    print("\nOutput targets:")
    print(f"  color_temp  : min={Y[:, 0].min():.0f}, "
          f"max={Y[:, 0].max():.0f}, mean={Y[:, 0].mean():.1f}")
    print(f"  brightness  : min={Y[:, 1].min():.0f}, "
          f"max={Y[:, 1].max():.0f}, mean={Y[:, 1].mean():.1f}")

    # 检查无人样本比例
    presence_mask = X[:, 6] == 1.0
    print(f"\nPresence distribution:")
    print(f"  Present: {presence_mask.sum()} ({presence_mask.mean()*100:.1f}%)")
    print(f"  Absent: {(~presence_mask).sum()} ({(~presence_mask).mean()*100:.1f}%)")
    print(f"  Absent brightness=0: {(Y[~presence_mask, 1] == 0).sum()}")


if __name__ == '__main__':
    # 生成数据
    X, Y = generate_training_data()

    # 打印统计
    print_statistics(X, Y)

    # 保存CSV
    output_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(output_dir, 'training_data.csv')
    save_to_csv(X, Y, csv_path)

    # 保存numpy格式 (更快加载)
    np.savez(os.path.join(output_dir, 'training_data.npz'),
             X=X, Y=Y)
    print(f"Saved numpy format to training_data.npz")
