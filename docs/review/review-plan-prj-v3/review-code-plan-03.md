# Review Plan 03 - 多模态AI推理与雷达感知

## 概述
本计划审查 prj-v3 的核心 AI 推理模块和雷达感知模块，这是多模态融合架构的关键部分。

## 审查范围

### 文件列表 - 多模态AI
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/multimodal/model_loader.c` | TFLite模型加载器 | ~400 |
| `main/src/multimodal/tflm_allocator.c` | TFLM内存分配器 | ~200 |

### 文件列表 - 雷达感知
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/radar/ld2410_driver.c` | LD2410毫米波雷达驱动 | ~350 |

### 相关头文件
- `main/include/model_loader.h`
- `main/src/multimodal/tflm_allocator.h`
- `main/include/ld2410_driver.h`

### AI模型文件
- `ai/sound_classifier.tflite` - 声音场景分类模型
- `ai/radar_analyzer.tflite` - 雷达特征编码模型
- `ai/fusion_model.tflite` - 多模态融合决策模型

## Graphify 社区关联
- **Community 20**: 多模态推理 (6 nodes, Cohesion: 0.25)
- **Community 7**: LD2410雷达相关 (20 nodes, Cohesion: 0.11)

## 审查重点

### 1. TFLite Micro 集成
- [ ] 模型加载流程是否正确
- [ ] 内存分配策略是否合理
- [ ] 量化模型处理是否正确
- [ ] 推理接口设计是否规范

### 2. 内存管理
- [ ] TFLM Arena 内存是否足够
- [ ] 是否存在内存碎片风险
- [ ] 模型加载/卸载生命周期

### 3. 雷达数据处理
- [ ] LD2410 数据解析正确性
- [ ] 特征提取算法合理性
- [ ] 数据滤波和噪声处理

### 4. 推理性能
- [ ] 推理延迟是否满足实时要求
- [ ] 多模型调度策略
- [ ] CPU/内存资源占用

### 5. 错误处理
- [ ] 模型加载失败处理
- [ ] 推理异常恢复
- [ ] 传感器故障处理

## 模型架构验证

### sound_classifier (CNN)
- 输入: MFCC (40, 32, 1)
- 输出: 5类场景概率
- 参数量: ~10KB

### radar_analyzer (MLP)
- 输入: 8维雷达特征
- 输出: 6维紧凑特征
- 参数量: ~400

### fusion_model (MLP)
- 输入: 19维多模态特征
- 输出: 2维控制决策 (色温、亮度)
- 参数量: ~600

## 审查输出
- [ ] 代码质量评分
- [ ] 发现问题列表（按严重程度分类）
- [ ] 改进建议
- [ ] AI推理性能报告

## 状态
- [ ] 待审查
- [ ] 审查中
- [ ] 已完成
