# Review Plan 02 - 音频处理与语音交互

## 概述
本计划审查 prj-v3 的音频处理和语音交互模块，这是 v3 版本新增的核心功能。

## 审查范围

### 文件列表 - 音频处理
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/audio/audio_afe.c` | 音频前端处理(AFE) | ~400 |
| `main/src/audio/audio_pipeline.c` | 音频流水线管理 | ~350 |
| `main/src/audio/mfcc.c` | MFCC特征提取 | ~300 |

### 文件列表 - 语音交互
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/voice/aec_pipeline.c` | 声学回声消除 | ~250 |
| `main/src/voice/audio_router.c` | 音频路由管理 | ~200 |
| `main/src/voice/command_handler.c` | 命令处理器 | ~300 |
| `main/src/voice/tts_engine.c` | TTS语音合成引擎 | ~350 |
| `main/src/voice/vad_detector.c` | 语音活动检测 | ~200 |
| `main/src/voice/voice_commands.c` | 语音命令定义 | ~250 |
| `main/src/voice/wake_word.c` | 唤醒词检测 | ~300 |

### 相关头文件
- `main/include/audio_afe.h`
- `main/include/audio_pipeline.h`
- `main/include/mfcc.h`
- `main/include/aec_pipeline.h`
- `main/include/audio_router.h`
- `main/include/command_handler.h`
- `main/include/tts_engine.h`
- `main/include/vad_detector.h`
- `main/include/voice_commands.h`
- `main/include/wake_word.h`

## Graphify 社区关联
- **Community 1**: Audio AFE 相关 (54 nodes)
- **Community 5**: INMP441/MFCC 相关 (24 nodes)

## 审查重点

### 1. 音频处理正确性
- [ ] MFCC计算参数是否正确
- [ ] AFE处理流程是否完整
- [ ] 音频缓冲区管理是否安全

### 2. 语音交互流程
- [ ] 唤醒词检测灵敏度
- [ ] VAD检测阈值合理性
- [ ] 命令解析准确性
- [ ] TTS输出流畅性

### 3. 并发安全
- [ ] 音频流处理线程安全
- [ ] 回调函数重入风险
- [ ] 缓冲区竞争条件

### 4. 性能考虑
- [ ] 实时处理延迟
- [ ] 内存使用效率
- [ ] CPU占用率

### 5. ESP-SR 集成
- [ ] ESP-SR 库使用是否正确
- [ ] 模型加载流程
- [ ] 回调机制是否合理

## 审查输出
- [ ] 代码质量评分
- [ ] 发现问题列表（按严重程度分类）
- [ ] 改进建议
- [ ] 性能分析报告

## 状态
- [ ] 待审查
- [ ] 审查中
- [ ] 已完成
