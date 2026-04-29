# Review Plan 06 - AI训练脚本与测试代码

## 概述
本计划审查 prj-v3 的 AI 模型训练脚本和测试代码，确保模型训练流程和测试覆盖的正确性。

## 审查范围

### 文件列表 - AI训练脚本
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `ai/train_all.py` | 统一训练入口 | ~150 |
| `ai/sound_classifier_train.py` | 声音分类模型训练 | ~400 |
| `ai/radar_analyzer_train.py` | 雷达特征编码模型训练 | ~350 |
| `ai/fusion_model_train.py` | 融合决策模型训练 | ~400 |
| `ai/test_models.py` | 模型测试脚本 | ~300 |

### 文件列表 - 主机测试
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `tests_host/test_host_main.c` | 主机测试入口 | ~100 |
| `tests_host/mock_esp.h` | ESP API模拟 | ~200 |
| `tests_host/mock_hardware.h` | 硬件模拟 | ~150 |

### 文件列表 - 嵌入式测试
| 目录 | 文件数 | 说明 |
|------|--------|------|
| `main/test/` | 11个.c文件 | 嵌入式单元测试 |

### 配置文件
- `ai/requirements.txt` - Python依赖
- `tests_host/CMakeLists.txt` - 测试构建配置
- `tests_host/README.md` - 测试说明

## Graphify 社区关联
- **Community 6**: 训练数据生成 (19 nodes, Cohesion: 0.11)
- **Community 8**: 消融实验 (20 nodes, Cohesion: 0.13)
- **Community 14**: 模型测试框架 (6 nodes, Cohesion: 0.12)
- **Community 15**: 雷达场景数据生成 (6 nodes, Cohesion: 0.17)
- **Community 17**: 雷达编码器训练 (10 nodes, Cohesion: 0.27)
- **Community 21**: 融合模型训练 (9 nodes, Cohesion: 0.31)
- **Community 22**: 声音分类器训练 (9 nodes, Cohesion: 0.31)
- **Community 24**: 雷达场景识别训练 (8 nodes, Cohesion: 0.36)
- **Community 28**: 统一训练入口 (5 nodes, Cohesion: 0.53)

## 审查重点

### 1. 训练脚本质量
- [ ] 代码结构是否清晰
- [ ] 是否遵循 Python 最佳实践
- [ ] 文档注释是否完整
- [ ] 错误处理是否充分

### 2. 模型架构设计
- [ ] 网络结构是否合理
- [ ] 参数量是否适合嵌入式
- [ ] 激活函数选择是否正确
- [ ] 损失函数是否合适

### 3. 训练数据处理
- [ ] 数据生成逻辑是否正确
- [ ] 数据增强是否合理
- [ ] 归一化处理是否一致
- [ ] 训练/验证集划分是否合理

### 4. 模型量化
- [ ] 量化方法是否正确
- [ ] 精度损失是否可接受
- [ ] 量化后模型验证

### 5. 测试覆盖
- [ ] 单元测试覆盖范围
- [ ] 边界条件测试
- [ ] 错误路径测试
- [ ] Mock 实现正确性

### 6. 测试框架
- [ ] 测试用例组织是否清晰
- [ ] 断言是否充分
- [ ] 测试报告是否详细

## 模型训练流程

```
train_all.py
├── train_sound_classifier()
│   ├── generate_synthetic_data()
│   ├── build_model()
│   ├── train_model()
│   └── quantize_model() → sound_classifier.tflite
├── train_radar_analyzer()
│   ├── generate_synthetic_data()
│   ├── build_model()
│   ├── train_model()
│   └── quantize_model() → radar_analyzer.tflite
└── train_fusion_model()
    ├── generate_synthetic_data()
    ├── build_model()
    ├── train_model()
    └── quantize_model() → fusion_model.tflite
```

## 审查输出
- [ ] Python代码质量评分
- [ ] 模型架构评估
- [ ] 测试覆盖率报告
- [ ] 改进建议

## 状态
- [ ] 待审查
- [ ] 审查中
- [ ] 已完成
