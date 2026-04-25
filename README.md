# ESP32-S3 Edge AI 智能照明网关

基于 Zephyr RTOS 的 ESP32-S3 边缘 AI 智能照明控制系统，支持会议室场景的智能调光与设备管理。

---

## 项目简介

本项目是一个完整的智能照明解决方案，结合 ESP32-S3 的 AI 推理能力和 Zephyr RTOS 的实时性能，实现以下核心功能：

- **边缘 AI 推理** — 本地运行 INT8 量化 MLP 模型 (4.4KB)，基于时间/天气/人体状态智能调光
- **智能调光控制** — 基于环境感知的多层调光策略：规则引擎 → AI 推理 → 渐变控制
- **环境感知** — PIR 人体检测、SNTP 时间同步、天气 API、日出日落计算
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

```
prj/
├── src/                        # 核心源代码
│   ├── main.c                  # 应用入口
│   ├── logic/                  # 业务逻辑层
│   │   ├── ai_infer.c          # AI 推理引擎 (TFLM Stub)
│   │   ├── rule_engine.c       # 规则引擎
│   │   ├── env_dimming.c       # 环境感知调光
│   │   ├── fade_control.c      # 渐变控制
│   │   ├── presence_state.c    # 人体存在状态机
│   │   ├── time_period.c       # 时段管理
│   │   ├── sunrise_sunset.c    # 日出日落计算
│   │   ├── weather_api.c       # 天气数据
│   │   └── data_logger.c       # 数据记录
│   └── drivers/                # 设备驱动层
│       ├── lighting.c          # LED 照明驱动
│       ├── pir.c               # PIR 传感器驱动
│       ├── wifi_manager.c      # Wi-Fi 管理
│       ├── sntp_time_sync.c    # SNTP 时间同步
│       ├── http_client.c       # HTTP 客户端
│       └── uart_driver.c       # UART 串口驱动
├── include/                    # 头文件
│   ├── ai_infer.h              # AI 推理接口
│   ├── model_data.h            # INT8 量化模型数据 (4472 bytes)
│   ├── rule_engine.h           # 规则引擎接口
│   └── ...                     # 其他模块头文件
├── ai/                         # AI 模型与训练脚本
│   ├── generate_data.py        # 合成训练数据生成 (15K样本)
│   ├── train_model.py          # MLP 训练 + INT8 量化 + C数组导出
│   ├── test_model.py           # 模型测试框架 (14个用例)
│   └── requirements.txt        # Python 依赖
├── boards/                     # 自定义板级配置
└── CMakeLists.txt              # 构建配置
```

---

## AI 模型

### 模型规格

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

```
合成数据生成 (15K) → MLP 训练 → INT8 量化 → 精度验证 → C数组导出
```

详见 [MLP 模型训练指南](docs/knowledge/mlp-model-training-guide.md) 和 [测试验证指南](docs/knowledge/mlp-model-testing-guide.md)

---

## 快速开始

### 环境要求

- **开发平台**: macOS (推荐) / Linux
- **工具链**: Zephyr SDK 0.17.0+
- **依赖管理**: west (Zephyr 元工具)

### 安装步骤

1. **克隆仓库**

```bash
cd ~/workspace
git clone https://github.com/null-characters/esp32-s3-edge-ai-smart-led.git zephyrproject
cd zephyrproject
```

2. **初始化 Zephyr 工作区**

```bash
west init -l
west update
```

3. **安装依赖**

```bash
pip3 install --user -r zephyr/scripts/requirements.txt
```

4. **配置环境变量**

```bash
export ZEPHYR_BASE="$HOME/workspace/zephyrproject/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk/zephyr-sdk-0.17.0"
```

详细环境搭建指南请参考 [docs/环境搭建报告.md](docs/环境搭建报告.md)

---

## 构建与烧录

### 编译项目

```bash
cd ~/workspace/zephyrproject
west build -b esp32s3_devkitc/esp32s3/procpu prj -d build
```

### 烧录固件

```bash
west flash -d build
```

### 监控串口输出

```bash
west espressif monitor -d build
```

---

## 测试

### 嵌入式单元测试 (Twister)

项目使用 Twister 测试框架，支持 `native_sim` 平台快速验证：

```bash
# 运行所有单元测试
cd $ZEPHYR_BASE
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim

# 运行特定测试
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim -s prj.unit.command_parser
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

### 知识库

| 文档 | 说明 |
|------|------|
| [边缘 AI 架构深度解析](docs/knowledge/edge-ai-architecture-deep-dive.md) | 边缘 AI 理论与技术选型 |
| [边缘 AI 脑暴纪要](docs/knowledge/edge-ai-brainstorming-summary.md) | 架构设计脑暴纪要 |
| [MLP 模型训练指南](docs/knowledge/mlp-model-training-guide.md) | 环境配置、数据生成、训练量化与导出 |
| [MLP 模型测试验证指南](docs/knowledge/mlp-model-testing-guide.md) | 测试策略、框架和验收标准 |

---

## 项目状态

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
| 数据记录 | 🚧 进行中 | 运行数据采集与回传 |
| 单元测试 | 🚧 进行中 | Twister 测试覆盖扩展中 |
| Bluetooth Mesh | ⏳ 规划中 | 设备组网与控制 |

---

## 技术栈

- **[Zephyr RTOS](https://zephyrproject.org/)** — 实时操作系统内核
- **[ESP-IDF HAL](https://docs.espressif.com/projects/esp-idf/)** — ESP32 硬件抽象层
- **[TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)** — 边缘 AI 推理框架
- **[TensorFlow](https://www.tensorflow.org/)** — 模型训练与量化
- **[Bluetooth Mesh](https://www.bluetooth.com/bluetooth-resources/?types=mesh)** — 设备组网协议

---

## 许可证

本项目采用 Apache-2.0 许可证，详见 [LICENSE](LICENSE) 文件。

---

## 贡献指南

欢迎提交 Issue 和 PR！请确保：

1. 代码符合 Zephyr 编码规范
2. 新增功能包含单元测试
3. AI 模型变更需运行 `pytest prj/ai/test_model.py` 验证
4. 提交前运行 `./scripts/twister` 验证嵌入式测试通过

---

**维护者**: null-characters  
**创建日期**: 2026-04-24  
**Zephyr 版本**: 4.2.0
