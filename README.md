# ESP32-S3 Edge AI 智能照明网关

基于 ESP-IDF 的 ESP32-S3 边缘 AI 智能照明控制系统，集成多模态感知与离线语音交互，支持会议室场景的智能调光与设备管理。

> **架构巨变公告**：项目已从 Zephyr RTOS 迁移到 ESP-IDF，以集成 ESP-SR 语音识别框架，实现从"感知设备"到"交互设备"的本质跨越。详见 [架构巨变思考](docs/knowledge/from-perception-to-interaction.md)。

---

## 项目简介

本项目是一个完整的智能照明解决方案，结合 ESP32-S3 的 AI 推理能力和 ESP-IDF 的丰富生态，实现以下核心功能：

- **离线语音交互** — ESP-SR 框架集成，支持自定义唤醒词（"小白网关"）与 50+ 条命令词，无需云端
- **多模态 AI 推理** — 毫米波雷达 + MEMS 麦克风 + 环境特征的三模态融合，本地运行 INT8 量化模型 (~35KB)
- **双轨并行架构** — 轨道一（自动感知控制）+ 轨道二（语音命令控制），语音命令优先于自动决策
- **智能调光控制** — 基于环境感知的多层调光策略：规则引擎 → AI 推理 → 渐变控制
- **环境感知** — LD2410 毫米波雷达（距离/速度/能量）、INMP441 麦克风（声音分类）、SNTP 时间同步、天气 API
- **场景识别** — 自动识别会议高潮、专注工作、汇报演示、休息放松等 5+ 种场景
- **多协议通信** — Wi-Fi 联网 + UART 串口通信 + Bluetooth Mesh (规划中)
- **模块化架构** — 可扩展的组件设计，支持多种传感器和执行器

---

## 硬件平台

| 组件 | 规格 |
|------|------|
| **主控芯片** | ESP32-S3 (Xtensa LX7 双核 @ 240MHz) |
| **AI 加速器** | ESP32-S3 内置 Vector Instructions |
| **无线通信** | Wi-Fi 4 (802.11 b/g/n) + BLE 5.0 |
| **开发板** | ESP32-S3-DevKitC-1 |

### 外设支持

- I2C / SPI / UART 传感器接口
- PWM LED 驱动（支持多通道调光）
- USB OTG（固件升级与调试）

---

## 软件架构

### prj-v3（语音交互版本，ESP-IDF，开发中）

```
prj-v3/
├── main/                       # ESP-IDF 主入口
│   ├── CMakeLists.txt
│   └── src/
│       ├── app_main.c          # 应用入口
│       ├── voice/              # 语音交互模块 ⭐ 新增
│       │   ├── voice_assistant.c   # ESP-SR 集成
│       │   ├── wake_word.c         # 唤醒词处理（"小白网关"）
│       │   └── command_handler.c   # 命令词处理（50+ 条）
│       ├── arbitration/        # 双轨仲裁模块 ⭐ 新增
│       │   └── priority_arbiter.c  # 语音命令优先级仲裁
│       ├── multimodal/         # 多模态推理层（迁移自 prj-v2）
│       │   ├── multimodal_infer.c
│       │   └── tflm_manager.c
│       ├── audio/              # 音频处理层
│       │   ├── inmp441_driver.c
│       │   └── audio_pipeline.c    # ESP-ADF 集成
│       ├── radar/              # 雷达处理层
│       │   └── ld2410_driver.c
│       ├── control/            # 控制模块
│       │   ├── lighting.c
│       │   ├── rule_engine.c
│       │   └── fade_control.c
│       └── network/            # 网络模块
│           ├── wifi_manager.c
│           └── sntp_sync.c
├── components/                 # ESP-IDF 组件
│   ├── esp-sr/                 # ESP-SR 语音识别组件 ⭐
│   ├── esp-adf/                # ESP-ADF 音频开发框架 ⭐
│   └── tflite-micro/           # TFLM 组件
├── models/                     # AI 模型
│   ├── sound_classifier.tflite # 声音分类器 (~30KB)
│   ├── radar_analyzer.tflite   # 雷达分析器 (~2KB)
│   ├── fusion_model.tflite     # 融合决策器 (~3KB)
│   └── wake_word_model.bin     # 唤醒词模型 (ESP-SR)
└── CMakeLists.txt
```

### prj-v2（多模态版本，Zephyr，已归档）

> **注意**：prj-v2 为 Zephyr 版本，已停止维护，代码保留供参考。新开发请使用 prj-v3。

```
prj-v2/
├── src/                        # 核心源代码
│   ├── main.c                  # 应用入口
│   ├── multimodal/             # 多模态推理层
│   │   └── multimodal_infer.c  # 三模态融合推理（声音+雷达+环境）
│   ├── audio/                  # 音频处理层
│   │   ├── inmp441_driver.c    # I2S 麦克风驱动
│   │   ├── mfcc.c              # MFCC 特征提取
│   │   └── vad.c                # VAD 语音活动检测
│   ├── radar/                  # 雷达处理层
│   │   └── ld2410_driver.c     # 毫米波雷达 UART 驱动
│   ├── logic/                  # 业务逻辑层
│   │   ├── rule_engine.c       # 规则引擎
│   │   ├── env_dimming.c       # 环境感知调光
│   │   └── fade_control.c      # 渐变控制
│   └── drivers/                # 设备驱动层
│       ├── lighting.c          # LED 照明驱动
│       ├── wifi_manager.c      # Wi-Fi 管理
│       └── sntp_time_sync.c    # SNTP 时间同步
├── include/                    # 头文件
│   ├── multimodal_infer.h      # 多模态推理接口
│   ├── inmp441_driver.h        # 麦克风接口（含 VAD）
│   ├── ld2410_driver.h         # 雷达接口
│   └── ...                     # 其他模块头文件
├── models/                     # AI 模型
│   ├── sound_classifier.tflite # 声音分类器 (~30KB)
│   ├── radar_analyzer.tflite   # 雷达分析器 (~2KB)
│   └── fusion_model.tflite     # 融合决策器 (~3KB)
├── tests/                      # 单元测试
│   └── test_multimodal.c       # 多模态推理测试
├── ai/                         # AI 模型训练脚本
│   └── *.py                    # 训练、量化、导出脚本
└── CMakeLists.txt              # 构建配置
```

### prj（经典版本，PIR + MLP，已归档）

> **注意**：prj 为 Zephyr 版本，已停止维护，代码保留供参考。新开发请使用 prj-v3。

```
prj/
├── src/                        # 核心源代码
│   ├── logic/                  # 业务逻辑层
│   │   ├── ai_infer.c          # AI 推理引擎 (TFLM)
│   │   ├── rule_engine.c       # 规则引擎
│   │   └── ...                 # 其他模块
│   └── drivers/                # 设备驱动层
│       └── ...                 # 驱动实现
├── ai/                         # AI 模型训练脚本
│   ├── generate_data.py        # 合成训练数据生成 (15K样本)
│   ├── train_model.py          # MLP 训练 + INT8 量化 + C数组导出
│   └── test_model.py           # 模型测试框架 (14个用例)
└── CMakeLists.txt              # 构建配置
```

---

## AI 模型

### 语音交互架构（prj-v3，ESP-IDF）

| 模块 | 框架 | 功能 | 大小 |
|------|------|------|------|
| **唤醒词检测** | WakeNet (ESP-SR) | "小白网关" 唤醒 | ~100 KB |
| **命令词识别** | MultiNet (ESP-SR) | 50+ 条命令词 | ~200 KB |
| **音频前端** | AFE (ESP-SR) | AEC/BSS/NS | 内置 |
| **声音分类器** | TFLM | 场景声音识别 | ~30 KB |
| **雷达分析器** | TFLM | 运动特征提取 | ~2 KB |
| **融合决策器** | TFLM | 最终控制决策 | ~3 KB |
| **总计** | - | - | **~335 KB** |

**支持的语音命令**：

| 命令类型 | 示例 | 效果 |
|----------|------|------|
| **场景切换** | "打开专注模式" | 4000K, 60% |
| **亮度调节** | "调亮一点" | 亮度 +20% |
| **色温调节** | "换成暖光" | 色温 3000K |
| **模式切换** | "切换演示模式" | 主灯 30%, 黑板灯亮 |
| **查询状态** | "现在是什么模式" | TTS 回复 |
| **取消操作** | "恢复自动" | 回到自动控制 |

### 多模态架构（prj-v2，Zephyr）

| 模型 | 输入 | 输出 | 大小 (INT8) | 用途 |
|------|------|------|-------------|------|
| **声音分类器** | MFCC (40×32) | 5 类概率 | ~30 KB | 场景声音识别 |
| **雷达分析器** | 8 维特征 | 6 维编码 | ~2 KB | 运动特征提取 |
| **融合决策器** | 19 维向量 | 色温/亮度 | ~3 KB | 最终控制决策 |
| **总计** | - | - | **~35 KB** | - |

### 单模态架构（prj）

| 指标 | 值 |
|------|-----|
| 网络结构 | MLP: 7 → 32 → 16 → 2 |
| 输入特征 | hour_sin, hour_cos, sunset_proximity, sunrise_hour_norm, sunset_hour_norm, weather, presence |
| 输出 | color_temp (2700-6500K), brightness (0-100%) |
| 量化方式 | Post-Training INT8 |
| 模型大小 | 4.4 KB (< 5KB 目标 ✅) |
| 亮度 MAE | 1.1% (< 5% 目标 ✅) |
| 色温 MAE | 27.8K (< 50K 目标 ✅) |

### 训练流水线

**多模态版本**:
```
数据采集 → 声音/雷达特征提取 → 多模型训练 → INT8 量化 → 精度验证 → TFLite 导出
```

**经典版本**:
```
合成数据生成 (15K) → MLP 训练 → INT8 量化 → 精度验证 → C数组导出
```

详见：
- [多模态升级总体规划](docs/esp32s3_edge_ai_gateway/plan/01-多模态升级总体规划.md)
- [MLP 模型训练指南](docs/knowledge/mlp-model-training-guide.md)
- [MLP 模型测试验证指南](docs/knowledge/mlp-model-testing-guide.md)

---

## 快速开始

### 环境要求

- **开发平台**: macOS (推荐) / Linux / Windows
- **工具链**: ESP-IDF v5.4+
- **依赖管理**: ESP-IDF Component Manager

### 安装步骤

1. **克隆仓库**

```bash
cd ~/workspace
git clone https://github.com/null-characters/esp32-s3-edge-ai-smart-led.git
cd esp32-s3-edge-ai-smart-led
```

2. **安装 ESP-IDF**

```bash
# macOS / Linux
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

3. **配置环境变量**

```bash
# 添加到 ~/.zshrc 或 ~/.bashrc
alias get_idf='. ~/esp/esp-idf/export.sh'
```

详细环境搭建指南请参考 [docs/环境搭建报告.md](docs/环境搭建报告.md)

---

## 构建与烧录

### 编译项目

**语音交互版本（prj-v3，推荐）**:
```bash
cd ~/workspace/esp32-s3-edge-ai-smart-led/prj-v3
idf.py set-target esp32s3
idf.py build
```

**多模态版本（prj-v2，Zephyr，已归档）**:
```bash
cd ~/workspace/zephyrproject
west build -b esp32s3_devkitc/esp32s3/procpu prj-v2 -d build-v2
```

**经典版本（prj，Zephyr，已归档）**:
```bash
cd ~/workspace/zephyrproject
west build -b esp32s3_devkitc/esp32s3/procpu prj -d build
```

### 烧录固件

```bash
# 语音交互版本 (ESP-IDF)
cd prj-v3
idf.py -p /dev/ttyUSB0 flash

# 多模态版本 (Zephyr)
west flash -d build-v2

# 经典版本 (Zephyr)
west flash -d build
```

### 监控串口输出

```bash
# ESP-IDF
idf.py -p /dev/ttyUSB0 monitor

# Zephyr
west espressif monitor -d build
```

---

## 测试

### 嵌入式单元测试

**ESP-IDF 版本 (prj-v3)**:
```bash
cd prj-v3
idf.py test
```

**Zephyr 版本 (prj-v2 / prj)**:
```bash
# 运行所有单元测试（多模态版本）
cd $ZEPHYR_BASE
./scripts/twister --testsuite-root ../prj-v2/tests -p native_sim

# 运行所有单元测试（经典版本）
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim

# 运行特定测试
./scripts/twister --testsuite-root ../prj-v2/tests -p native_sim -s prj-v2.unit.multimodal
```

### AI 模型测试 (pytest)

```bash
# 创建 conda 环境 (首次)
conda create -n esp32-ai python=3.12 -y
conda activate esp32-ai
pip install tensorflow numpy pandas pytest

# 运行模型测试
cd prj/ai
pytest test_model.py -v

# 按层级运行
pytest test_model.py -v -k "TestDataGeneration"    # L1 数据生成
pytest test_model.py -v -k "TestModelStructure"    # L2 模型结构
pytest test_model.py -v -k "TestGoldenCases"       # L4 回归测试
```

---

## 文档

### 设计文档

| 文档 | 说明 |
|------|------|
| [环境搭建报告](docs/环境搭建报告.md) | macOS 开发环境完整搭建指南 |
| [硬件概述](docs/esp32s3_edge_ai_gateway/01-hardware-overview.md) | ESP32-S3 硬件规格与接口 |
| [系统架构](docs/esp32s3_edge_ai_gateway/02-system-architecture.md) | 软件架构设计 |
| [通信协议](docs/esp32s3_edge_ai_gateway/03-communication-protocol.md) | Bluetooth Mesh / Wi-Fi 协议栈 |
| [AI 推理](docs/esp32s3_edge_ai_gateway/04-ai-inference.md) | TensorFlow Lite Micro 集成 |
| [MVP 规划](docs/esp32s3_edge_ai_gateway/06-meeting-room-lighting-mvp.md) | 会议室照明 MVP 开发计划 |
| [多模态升级总体规划](docs/esp32s3_edge_ai_gateway/plan/01-多模态升级总体规划.md) | 雷达+麦克风多模态架构设计 |
| [任务追踪表](docs/esp32s3_edge_ai_gateway/plan/03-任务追踪表.md) | 开发任务进度追踪 |

### 知识库

| 文档 | 说明 |
|------|------|
| [语音识别方案评估与决策](docs/knowledge/voice-recognition-evaluation.md) | ESP-SR 与 Zephyr 兼容性调研，迁移决策 ⭐ |
| [架构巨变思考](docs/knowledge/from-perception-to-interaction.md) | 从"感知设备"到"交互设备"的本质跨越 ⭐ |
| [边缘 AI 架构深度解析](docs/knowledge/edge-ai-architecture-deep-dive.md) | 边缘 AI 理论与技术选型 |
| [边缘 AI 脑暴纪要](docs/knowledge/edge-ai-brainstorming-summary.md) | 架构设计脑暴纪要 |
| [多模态 AI 升级路线图](docs/knowledge/multimodal-ai-upgrade-roadmap.md) | 多模态升级技术路线 |
| [MLP 模型训练指南](docs/knowledge/mlp-model-training-guide.md) | 环境配置、数据生成、训练量化与导出 |
| [MLP 模型测试验证指南](docs/knowledge/mlp-model-testing-guide.md) | 测试策略、框架和验收标准 |

---

## 项目状态

### 语音交互版本 (prj-v3，ESP-IDF)

| 模块 | 状态 | 说明 |
|------|------|------|
| ESP-IDF 项目框架 | ⏳ 规划中 | 从 Zephyr 迁移 |
| ESP-SR 集成 | ⏳ 规划中 | WakeNet + MultiNet |
| 语音命令处理 | ⏳ 规划中 | 50+ 条命令词 |
| 双轨仲裁机制 | ⏳ 规划中 | 语音优先级仲裁 |
| 多模态推理迁移 | ⏳ 规划中 | TFLM 集成到 ESP-IDF |
| ESP-ADF 集成 | ⏳ 规划中 | 音频管道框架 |

### 多模态版本 (prj-v2，Zephyr，已归档)

| 模块 | 状态 | 说明 |
|------|------|------|
| 多模态推理框架 | ✅ 已完成 | 三模型独立 Arena 分配（384KB PSRAM） |
| LD2410 雷达驱动 | ✅ 已完成 | UART 驱动、数据解析、特征提取 |
| INMP441 麦克风驱动 | ✅ 已完成 | I2S 驱动、音频采集 |
| VAD 语音检测 | ✅ 已完成 | 能量阈值 + ZCR 组合检测 |
| 时序对齐 | ✅ 已完成 | 带时间戳的环形缓冲区 |
| 单元测试 | ✅ 已完成 | 5 个核心测试用例 |
| 声音分类器训练 | 🚧 进行中 | 数据采集与模型训练 |
| 融合模型训练 | 🚧 进行中 | 多模态融合网络 |
| 场景识别 | ⏳ 规划中 | 5+ 种场景自动识别 |

### 经典版本 (prj，Zephyr，已归档)

| 模块 | 状态 | 说明 |
|------|------|------|
| 基础框架 | ✅ 已完成 | Zephyr RTOS + ESP32-S3 启动 |
| LED 驱动 | ✅ 已完成 | PWM 多通道调光 |
| Wi-Fi 连接 | ✅ 已完成 | STA 模式 + SNTP 时间同步 |
| 规则引擎 | ✅ 已完成 | 时段/天气/人体规则调光 |
| 环境感知调光 | ✅ 已完成 | 日出日落 + 天气补偿 |
| AI 模型训练 | ✅ 已完成 | MLP 7→32→16→2, INT8 4.4KB |
| AI 模型测试 | ✅ 已完成 | 14 个自动化测试用例 |
| AI 推理引擎 | 🚧 进行中 | TFLM Stub → 完整 TFLM 集成 |
| Bluetooth Mesh | ⏳ 规划中 | 设备组网与控制 |

---

## 技术栈

### prj-v3 (ESP-IDF)

- **[ESP-IDF](https://docs.espressif.com/projects/esp-idf/)** — Espressif 官方 IoT 开发框架
- **[ESP-SR](https://github.com/espressif/esp-sr)** — 语音识别框架（WakeNet + MultiNet）
- **[ESP-ADF](https://github.com/espressif/esp-adf)** — 音频开发框架
- **[TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)** — 边缘 AI 推理框架
- **[TensorFlow](https://www.tensorflow.org/)** — 模型训练与量化

### prj-v2 / prj (Zephyr，已归档)

- **[Zephyr RTOS](https://zephyrproject.org/)** — 实时操作系统内核
- **[ESP-IDF HAL](https://docs.espressif.com/projects/esp-idf/)** — ESP32 硬件抽象层
- **[TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)** — 边缘 AI 推理框架
- **[Bluetooth Mesh](https://www.bluetooth.com/bluetooth-resources/?types=mesh)** — 设备组网协议

---

## 许可证

本项目采用 Apache-2.0 许可证，详见 [LICENSE](LICENSE) 文件。

---

## 贡献指南

欢迎提交 Issue 和 PR！请确保：

1. 代码符合 ESP-IDF 编码规范
2. 新增功能包含单元测试
3. AI 模型变更需运行 `pytest prj/ai/test_model.py` 或 `pytest prj-v2/ai/test_model.py` 验证
4. 提交前运行 `idf.py test` 验证嵌入式测试通过

---

**维护者**: null-characters
**创建日期**: 2026-04-24
**最后更新**: 2026-04-27
**ESP-IDF 版本**: v5.4+
**架构版本**: prj-v3 (语音交互版本)
