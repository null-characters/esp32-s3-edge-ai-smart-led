# ESP32-S3 Edge AI 智能照明网关

基于 Zephyr RTOS 的 ESP32-S3 边缘 AI 智能照明控制系统，支持会议室场景的智能调光与设备管理。

---

## 项目简介

本项目是一个完整的智能照明解决方案，结合 ESP32-S3 的 AI 推理能力和 Zephyr RTOS 的实时性能，实现以下核心功能：

- **边缘 AI 推理** — 本地运行 TensorFlow Lite Micro 模型，实现人体检测与行为识别
- **智能调光控制** — 基于环境光线、人体存在状态的自适应照明调节
- **多协议通信** — 支持 Bluetooth Mesh、Wi-Fi、UART 等多种通信方式
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
├── src/                    # 核心源代码
│   ├── main.c              # 应用入口
│   ├── lighting/           # 照明控制模块
│   ├── ai/                 # AI 推理引擎
│   ├── comm/               # 通信协议栈
│   └── drivers/            # 设备驱动
├── include/                # 头文件
├── tests/unit/             # 单元测试
├── boards/                 # 自定义板级配置
├── ai/                     # AI 模型与训练脚本
└── CMakeLists.txt          # 构建配置
```

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

## 单元测试

项目使用 Twister 测试框架，支持 `native_sim` 平台快速验证：

```bash
# 运行所有单元测试
cd $ZEPHYR_BASE
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim

# 运行特定测试
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim -s prj.unit.command_parser
```

---

## 文档

| 文档 | 说明 |
|------|------|
| [环境搭建报告](docs/环境搭建报告.md) | macOS 开发环境完整搭建指南 |
| [硬件概述](docs/esp32s3_edge_ai_gateway/01-hardware-overview.md) | ESP32-S3 硬件规格与接口 |
| [系统架构](docs/esp32s3_edge_ai_gateway/02-system-architecture.md) | 软件架构设计 |
| [通信协议](docs/esp32s3_edge_ai_gateway/03-communication-protocol.md) | Bluetooth Mesh / Wi-Fi 协议栈 |
| [AI 推理](docs/esp32s3_edge_ai_gateway/04-ai-inference.md) | TensorFlow Lite Micro 集成 |
| [MVP 规划](docs/esp32s3_edge_ai_gateway/06-meeting-room-lighting-mvp.md) | 会议室照明 MVP 开发计划 |

---

## 项目状态

| 模块 | 状态 |
|------|------|
| 基础框架 | ✅ 已完成 |
| LED 驱动 | ✅ 已完成 |
| 单元测试 | 🚧 进行中 |
| AI 推理引擎 | ⏳ 规划中 |
| Bluetooth Mesh | ⏳ 规划中 |
| Wi-Fi 连接 | ⏳ 规划中 |

---

## 技术栈

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

1. 代码符合 Zephyr 编码规范
2. 新增功能包含单元测试
3. 提交前运行 `./scripts/twister` 验证测试通过

---

**维护者**: null-characters  
**创建日期**: 2026-04-24  
**Zephyr 版本**: 4.2.0
