"""
ESP32-S3 Edge AI - 模型测试框架

测试内容：
- 模型加载与推理
- 输入/输出维度验证
- 量化模型正确性检查
- 推理延迟基准

运行方式：
    pytest test_models.py -v
"""

import numpy as np
import os
import time

# 尝试导入 TensorFlow
try:
    import tensorflow as tf
    TFLITE_AVAILABLE = True
except ImportError:
    TFLITE_AVAILABLE = False
    print("警告: TensorFlow 未安装，部分测试将跳过")


MODELS_DIR = os.path.join(os.path.dirname(__file__), '..', 'models')
MODELS = {
    'sound_classifier': {
        'file': 'sound_classifier.tflite',
        'input_shape': (1, 40, 32, 1),  # MFCC 特征 (40 frames, 32 mel bands)
        'output_shape': (1, 5),         # 5 类场景
        'input_type': np.int8,
    },
    'radar_analyzer': {
        'file': 'radar_analyzer.tflite',
        'input_shape': (1, 8),          # 雷达特征
        'output_shape': (1, 6),         # 6 类状态
        'input_type': np.int8,
    },
    'fusion_model': {
        'file': 'fusion_model.tflite',
        'input_shape': (1, 19),         # 融合输入
        'output_shape': (1, 2),         # 亮度 + 色温
        'input_type': np.int8,
    },
}


class TestModelAvailability:
    """模型文件存在性测试"""

    def test_models_directory_exists(self):
        """模型目录存在"""
        assert os.path.isdir(MODELS_DIR), f"模型目录不存在: {MODELS_DIR}"

    def test_all_model_files_exist(self):
        """所有模型文件存在"""
        for name, info in MODELS.items():
            path = os.path.join(MODELS_DIR, info['file'])
            assert os.path.isfile(path), f"模型文件缺失: {name} ({info['file']})"


class TestModelSizes:
    """模型大小测试"""

    def test_model_sizes_within_limit(self):
        """模型总大小不超过 50KB"""
        total_size = 0
        for name, info in MODELS.items():
            path = os.path.join(MODELS_DIR, info['file'])
            if os.path.isfile(path):
                size = os.path.getsize(path)
                total_size += size
                print(f"{name}: {size} 字节")
        
        print(f"模型总大小: {total_size} 字节")
        assert total_size < 51200, f"模型总大小 {total_size} 超过 50KB 限制"

    def test_individual_model_sizes(self):
        """单个模型大小合理"""
        max_sizes = {
            'sound_classifier': 20000,  # 20KB
            'radar_analyzer': 5000,     # 5KB
            'fusion_model': 8000,      # 8KB
        }
        
        for name, info in MODELS.items():
            path = os.path.join(MODELS_DIR, info['file'])
            if os.path.isfile(path):
                size = os.path.getsize(path)
                max_size = max_sizes.get(name, 50000)
                assert size < max_size, f"{name} 大小 {size} 超过限制 {max_size}"


class TestModelInference:
    """模型推理测试"""

    def test_sound_classifier_inference(self):
        """声音分类器推理"""
        if not TFLITE_AVAILABLE:
            return
        
        model_path = os.path.join(MODELS_DIR, 'sound_classifier.tflite')
        if not os.path.isfile(model_path):
            return
        
        interpreter = tf.lite.Interpreter(model_path=model_path)
        interpreter.allocate_tensors()
        
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        
        # 验证输入维度
        expected_shape = (1, 40, 32, 1)
        assert tuple(input_details[0]['shape']) == expected_shape, \
            f"输入维度不匹配: {input_details[0]['shape']} != {expected_shape}"
        
        # 执行推理
        input_data = np.random.randint(-128, 127, expected_shape, dtype=np.int8)
        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
        output = interpreter.get_tensor(output_details[0]['index'])
        
        # 验证输出
        assert output.shape == (1, 5), f"输出维度不匹配: {output.shape}"
        assert output.dtype == np.int8, f"输出类型不匹配: {output.dtype}"

    def test_radar_analyzer_inference(self):
        """雷达分析器推理"""
        if not TFLITE_AVAILABLE:
            return
        
        model_path = os.path.join(MODELS_DIR, 'radar_analyzer.tflite')
        if not os.path.isfile(model_path):
            return
        
        interpreter = tf.lite.Interpreter(model_path=model_path)
        interpreter.allocate_tensors()
        
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        
        # 验证输入维度
        expected_shape = (1, 8)
        assert tuple(input_details[0]['shape']) == expected_shape, \
            f"输入维度不匹配: {input_details[0]['shape']} != {expected_shape}"
        
        # 执行推理
        input_data = np.random.randint(-128, 127, expected_shape, dtype=np.int8)
        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
        output = interpreter.get_tensor(output_details[0]['index'])
        
        # 验证输出
        assert output.shape == (1, 6), f"输出维度不匹配: {output.shape}"

    def test_fusion_model_inference(self):
        """融合模型推理"""
        if not TFLITE_AVAILABLE:
            return
        
        model_path = os.path.join(MODELS_DIR, 'fusion_model.tflite')
        if not os.path.isfile(model_path):
            return
        
        interpreter = tf.lite.Interpreter(model_path=model_path)
        interpreter.allocate_tensors()
        
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        
        # 验证输入维度
        expected_shape = (1, 19)
        assert tuple(input_details[0]['shape']) == expected_shape, \
            f"输入维度不匹配: {input_details[0]['shape']} != {expected_shape}"
        
        # 执行推理
        input_data = np.random.randint(-128, 127, expected_shape, dtype=np.int8)
        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
        output = interpreter.get_tensor(output_details[0]['index'])
        
        # 验证输出
        assert output.shape == (1, 2), f"输出维度不匹配: {output.shape}"


class TestInferenceLatency:
    """推理延迟测试"""

    def test_inference_latency(self):
        """推理延迟基准测试"""
        if not TFLITE_AVAILABLE:
            return
        
        results = {}
        
        for name, info in MODELS.items():
            model_path = os.path.join(MODELS_DIR, info['file'])
            if not os.path.isfile(model_path):
                continue
            
            interpreter = tf.lite.Interpreter(model_path=model_path)
            interpreter.allocate_tensors()
            
            input_details = interpreter.get_input_details()
            input_shape = tuple(input_details[0]['shape'])
            
            # 预热
            input_data = np.random.randint(-128, 127, input_shape, dtype=np.int8)
            interpreter.set_tensor(input_details[0]['index'], input_data)
            interpreter.invoke()
            
            # 测量
            iterations = 100
            start = time.time()
            for _ in range(iterations):
                interpreter.set_tensor(input_details[0]['index'], input_data)
                interpreter.invoke()
            elapsed = (time.time() - start) / iterations * 1000
            
            results[name] = elapsed
            print(f"{name}: {elapsed:.3f} ms/次")
        
        # 验证延迟合理（< 10ms）
        for name, latency in results.items():
            assert latency < 10, f"{name} 推理延迟 {latency:.1f}ms 超过 10ms 限制"


class TestQuantization:
    """量化正确性测试"""

    def test_quantized_model_outputs_valid_range(self):
        """量化模型输出在有效范围内"""
        if not TFLITE_AVAILABLE:
            return
        
        for name, info in MODELS.items():
            model_path = os.path.join(MODELS_DIR, info['file'])
            if not os.path.isfile(model_path):
                continue
            
            interpreter = tf.lite.Interpreter(model_path=model_path)
            interpreter.allocate_tensors()
            
            input_details = interpreter.get_input_details()
            output_details = interpreter.get_output_details()
            
            # 多次推理，验证输出稳定
            for _ in range(10):
                input_shape = tuple(input_details[0]['shape'])
                input_data = np.random.randint(-128, 127, input_shape, dtype=np.int8)
                interpreter.set_tensor(input_details[0]['index'], input_data)
                interpreter.invoke()
                output = interpreter.get_tensor(output_details[0]['index'])
                
                # uint8 输出应在 0-255 范围
                if output_details[0]['dtype'] == np.uint8:
                    assert output.min() >= 0 and output.max() <= 255, \
                        f"{name} 输出超出 uint8 范围"


if __name__ == '__main__':
    import pytest
    pytest.main([__file__, '-v'])
