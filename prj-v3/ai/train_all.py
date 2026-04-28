"""
ESP32-S3 多模态 AI 模型训练 - 统一入口
prj-v3 多模态推理架构

训练三个模型:
1. sound_classifier.tflite (~30KB) - CNN 声音场景分类
2. radar_analyzer.tflite (~2KB) - MLP 雷达特征编码
3. fusion_model.tflite (~3KB) - MLP 多模态融合决策

总计: ~35KB (INT8 量化后)

用法:
    python train_all.py              # 训练所有模型
    python train_all.py --sound      # 只训练声音分类器
    python train_all.py --radar      # 只训练雷达编码器
    python train_all.py --fusion     # 只训练融合模型
"""

import os
import sys
import argparse

def train_sound_classifier():
    """训练声音分类器"""
    print("\n" + "=" * 60)
    print("Training Sound Classifier...")
    print("=" * 60)
    
    from sound_classifier_train import main as sound_main
    sound_main()


def train_radar_analyzer():
    """训练雷达编码器"""
    print("\n" + "=" * 60)
    print("Training Radar Analyzer...")
    print("=" * 60)
    
    from radar_analyzer_train import main as radar_main
    radar_main()


def train_fusion_model():
    """训练融合模型"""
    print("\n" + "=" * 60)
    print("Training Fusion Model...")
    print("=" * 60)
    
    from fusion_model_train import main as fusion_main
    fusion_main()


def main():
    parser = argparse.ArgumentParser(description='Train multimodal AI models')
    parser.add_argument('--sound', action='store_true', help='Train sound classifier only')
    parser.add_argument('--radar', action='store_true', help='Train radar analyzer only')
    parser.add_argument('--fusion', action='store_true', help='Train fusion model only')
    args = parser.parse_args()
    
    # 切换到脚本目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    print("=" * 60)
    print("ESP32-S3 Multimodal AI Model Training")
    print("=" * 60)
    print(f"Working directory: {script_dir}")
    
    # 如果没有指定任何参数，训练所有模型
    train_all = not (args.sound or args.radar or args.fusion)
    
    try:
        if train_all or args.sound:
            train_sound_classifier()
        
        if train_all or args.radar:
            train_radar_analyzer()
        
        if train_all or args.fusion:
            train_fusion_model()
        
        print("\n" + "=" * 60)
        print("All models trained successfully!")
        print("=" * 60)
        
        # 打印模型文件信息
        print("\nGenerated model files:")
        model_files = [
            'sound_classifier.tflite',
            'radar_analyzer.tflite',
            'fusion_model.tflite'
        ]
        
        total_size = 0
        for f in model_files:
            path = os.path.join(script_dir, f)
            if os.path.exists(path):
                size = os.path.getsize(path)
                total_size += size
                print(f"  {f}: {size} bytes ({size/1024:.1f} KB)")
            else:
                print(f"  {f}: NOT FOUND")
        
        print(f"\nTotal model size: {total_size} bytes ({total_size/1024:.1f} KB)")
        
    except Exception as e:
        print(f"\nError: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
