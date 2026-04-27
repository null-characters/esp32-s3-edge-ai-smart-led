# ESP32-S3 智能照明项目 — 编译指南

> 目标硬件: ESP32-S3 DevKitC
> Zephyr 版本: 4.2
> 工具链: Zephyr SDK (xtensa-espressif-esp32s3)

---

## 1. 环境准备

### 1.1 安装依赖

```bash
# macOS
brew install cmake ninja python3 dtc

# 安装 west (Zephyr 包管理器)
pip3 install west
```

### 1.2 初始化 Zephyr 工作区

```bash
# 克隆项目（如果尚未完成）
cd ~/
git clone <repo-url> git_prj

# 初始化 west 工作区
cd git_prj/zephyrproject
west init -l .  # 使用现有 west.yml
west update     # 拉取所有模块

# 导出 Zephyr SDK
west zephyr-export
```

### 1.3 安装 Zephyr SDK

```bash
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_macos-aarch64.tar.xz
tar xvf zephyr-sdk-0.16.8_macos-aarch64.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh -t xtensa-espressif-esp32s3
```

### 1.4 设置环境变量

```bash
# 添加到 ~/.zshrc
export ZEPHYR_BASE="$HOME/git_prj/zephyrproject/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk"

source ~/.zshrc
```

### 1.5 Python 依赖

```bash
pip3 install -r $ZEPHYR_BASE/scripts/requirements.txt
```

---

## 2. 获取 ESP32 二进制库 (Blobs)

ESP32 HAL 模块需要预编译的二进制库文件。`west update` 不会自动下载这些文件。

```bash
cd /Users/fengbing/git_prj/zephyrproject

# 方式一：使用 west blobs 命令（推荐）
west blobs fetch hal_espressif

# 方式二：手动下载脚本
python3 fetch_all_blobs_complete.py
```

涉及 6 个芯片型号的 blobs:
- `esp32/`, `esp32c2/`, `esp32c3/`, `esp32c6/`, `esp32s2/`, `esp32s3/`

每个目录包含 WiFi、蓝牙等预编译库，存放于:
```
modules/hal/espressif/zephyr/blobs/lib/<chip>/
```

---

## 3. 编译项目

### 3.1 标准编译

```bash
cd /Users/fengbing/git_prj/zephyrproject

# 首次编译（或完全重新编译）
west build -b esp32s3_devkitc/esp32s3/procpu prj -d build_prj

# 增量编译（修改代码后）
west build -d build_prj
```

### 3.2 完全清理重建

```bash
# 删除构建目录
rm -rf build_prj

# 或使用 --pristine 参数
west build -b esp32s3_devkitc/esp32s3/procpu prj -d build_prj --pristine
```

### 3.3 查看编译详情

```bash
# 查看编译错误
west build -d build_prj 2>&1 | grep "error:" | head -20

# 查看编译警告
west build -d build_prj 2>&1 | grep "warning:" | head -20

# 查看内存使用
west build -d build_prj -t rom_report
west build -d build_prj -t ram_report
```

---

## 4. 烧录

### 4.1 通过 west flash 烧录

```bash
# 标准烧录
west flash -d build_prj

# 指定串口
west flash -d build_prj --esp-device /dev/ttyUSB0

# macOS 上 ESP32-S3 通常显示为:
west flash -d build_prj --esp-device /dev/cu.usbmodem*
```

### 4.2 通过 esptool 烧录

```bash
pip3 install esptool

# 擦除 flash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem* erase_flash

# 烧录固件
esptool.py --chip esp32s3 --port /dev/cu.usbmodem* \
  --baud 460800 write_flash 0x0 build_prj/zephyr/zephyr.bin
```

### 4.3 串口监视

```bash
# 使用 minicom
minicom -D /dev/cu.usbmodem* -b 115200

# 使用 screen
screen /dev/cu.usbmodem* 115200
```

---

## 5. 项目配置

### 5.1 主配置文件

```
prj/prj.conf    — 主应用配置
```

### 5.2 关键配置项

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_WIFI` | y | 启用 WiFi |
| `CONFIG_NET_L2_ETHERNET` | y | 以太网 L2 |
| `CONFIG_NET_IPV4` | y | IPv4 协议栈 |
| `CONFIG_NET_TCP` | y | TCP 协议 |
| `CONFIG_NET_SOCKETS` | y | BSD Socket API |
| `CONFIG_DNS_RESOLVER` | y | DNS 解析 |
| `CONFIG_SNTP` | y | SNTP 时间同步 |
| `CONFIG_MBEDTLS` | y | TLS 加密 |
| `CONFIG_HTTP_CLIENT` | y | HTTP 客户端 |
| `CONFIG_HEAP_MEM_POOL_SIZE` | 65536 | 堆内存 |

### 5.3 自定义配置

如需修改配置（如 WiFi SSID/密码），建议创建 overlay 文件：

```bash
# 创建 overlay
cat > prj/boards/esp32s3_devkitc.overlay << 'EOF'
/* 自定义设备树配置 */
EOF
```

或使用 menuconfig:

```bash
west build -d build_prj -t menuconfig
```

---

## 6. 编译输出

编译成功后，固件文件位于:

```
build_prj/zephyr/
├── zephyr.bin       — 烧录用二进制文件
├── zephyr.elf       — 包含调试符号的 ELF 文件
├── zephyr.map       — 内存映射文件
└── .config          — 最终合并的配置
```

### 典型内存使用 (ESP32-S3)

| 区域 | 已用 | 总量 | 使用率 |
|------|------|------|--------|
| FLASH | ~576KB | 16MB | ~3.4% |
| IRAM | ~58KB | 336KB | ~17% |
| DRAM | ~235KB | 320KB | ~72% |
| PSRAM | ~1.4MB | 8MB | ~18% |

---

## 7. 常见问题排查

### Q1: `west: command not found`
```bash
pip3 install west
source ~/.zshrc
```

### Q2: 找不到 ESP32 blobs
```bash
west blobs fetch hal_espressif
```

### Q3: Python 模块缺失
```bash
pip3 install -r $ZEPHYR_BASE/scripts/requirements.txt
```

### Q4: 编译缓存损坏
```bash
rm -rf build_prj
west build -b esp32s3_devkitc/esp32s3/procpu prj -d build_prj
```

### Q5: 烧录时找不到设备
- 检查 USB 线是否支持数据传输
- 按住 Boot 键再点 Reset 进入下载模式
- macOS: 检查 `/dev/cu.usbmodem*` 是否存在

---

## 8. 目录结构

```
prj/
├── CMakeLists.txt          — 项目构建定义
├── prj.conf                — Zephyr 配置
├── README.md               — 项目说明
├── boards/                 — 板级配置
├── include/                — 公共头文件
│   ├── ai_infer.h
│   ├── data_logger.h
│   ├── env_dimming.h
│   ├── lighting.h
│   ├── model_data.h
│   ├── presence_state.h
│   ├── sntp_time_sync.h
│   ├── sunrise_sunset.h
│   ├── time_period.h
│   ├── weather_api.h
│   └── wifi_manager.h
├── src/
│   ├── main.c              — 主入口
│   ├── drivers/
│   │   ├── sntp_time_sync.c
│   │   └── wifi_manager.c
│   └── logic/
│       ├── ai_infer.c
│       ├── data_logger.c
│       ├── env_dimming.c
│       ├── lighting.c
│       ├── presence_state.c
│       ├── sunrise_sunset.c
│       ├── time_period.c
│       └── weather_api.c
├── tests/                  — 单元测试
├── ai/                     — AI 训练脚本
└── docs/                   — 文档
    ├── build_guide.md      — 本文件
    └── bugfix/             — 问题修复记录
```
