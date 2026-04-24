# WSL2 构建环境配置指南

## 概述
本文档记录如何在 WSL2 (Ubuntu) 中配置 Zephyr 构建环境，用于运行单元测试。

### 为什么需要 WSL2？
| 平台 | native_sim 支持 | 说明 |
|------|----------------|------|
| Windows 原生 | ❌ 不支持 | 缺少主机 GCC，设备树处理失败 |
| WSL2 Ubuntu | ✅ 完全支持 | 完整的 Linux 工具链 |
| Windows + ESP32 | ⚠️ 部分支持 | 可编译，但不能运行单元测试 |

**结论**: 要在 Windows 上运行 Zephyr 单元测试，必须使用 WSL2。

---

## 快速开始（复制粘贴执行）

打开 WSL2 Ubuntu 终端，执行以下命令：

```bash
# 1. 进入项目目录
cd /mnt/e/zephyrproject

# 2. 安装基础依赖（首次需要）
sudo apt update && sudo apt install -y python3-pip

# 3. 安装 west
pip3 install --user west

# 4. 设置环境变量
export PATH="$HOME/.local/bin:$PATH"
export ZEPHYR_BASE=/mnt/e/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0

# 5. 注册 Zephyr
cd $ZEPHYR_BASE && west zephyr-export

# 6. 运行测试
cd /mnt/e/zephyrproject/prj/tests/unit
rm -rf build
west build -b native_sim --pristine
west build -t run
```

---

## 1. WSL2 环境初始化

### 1.1 更新系统
```bash
sudo apt update && sudo apt upgrade -y
```

### 1.2 安装基础依赖
```bash
sudo apt install -y \
    git \
    cmake \
    ninja-build \
    gperf \
    ccache \
    dfu-util \
    device-tree-compiler \
    wget \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev \
    python3-setuptools \
    xz-utils \
    file \
    make \
    gcc \
    gcc-multilib \
    g++-multilib \
    libsdl2-dev \
    libmagic1
```

---

## 2. Python 环境配置

### 2.1 安装 west
```bash
pip3 install --user west
```

### 2.2 添加 pip bin 到 PATH
```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 2.3 验证安装
```bash
west --version
```

---

## 3. Zephyr SDK 配置

### 3.1 设置环境变量
编辑 `~/.bashrc`，添加以下内容：

```bash
# Zephyr 环境变量
export ZEPHYR_BASE=/mnt/e/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0
export PATH="$ZEPHYR_BASE/scripts:$PATH"
```

### 3.2 应用配置
```bash
source ~/.bashrc
```

### 3.3 注册 Zephyr 包
```bash
cd $ZEPHYR_BASE
west zephyr-export
```

---

## 4. 安装 Python 依赖

```bash
cd /mnt/e/zephyrproject
pip3 install --user -r zephyr/scripts/requirements.txt
```

---

## 5. 运行单元测试

### 方法 1：使用 west（单个测试）

```bash
cd /mnt/e/zephyrproject/prj/tests/unit
rm -rf build
west build -b native_sim --pristine
west build -t run
```

### 方法 2：使用 twister（批量测试，推荐）

Twister 是 Zephyr 的测试框架，可以批量运行所有测试并生成报告。

```bash
cd /mnt/e/zephyrproject/zephyr

# 列出所有可用测试
./scripts/twister --testsuite-root ../prj/tests/unit --list-tests

# 运行所有测试
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim

# 运行特定测试
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim -s prj.unit.command_parser
```

### 环境变量设置
运行前确保设置以下环境变量：
```bash
export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0
export ZEPHYR_SDK=/mnt/e/zephyrproject/zephyr-sdk-0.17.0
```

---

## 6. 常用命令速查

| 命令 | 说明 |
|------|------|
| `west build -b native_sim --pristine` | 清理并构建 native_sim 目标 |
| `west build -t run` | 运行测试 |
| `west build -t clean` | 清理构建目录 |
| `./build/zephyr/zephyr.exe` | 直接运行测试可执行文件 |

---

## 7. 故障排除

### 7.1 DNS 解析失败 / apt update 卡住
**现象**: `Temporary failure resolving 'archive.ubuntu.com'`

**解决**: 配置阿里 DNS
```bash
sudo bash -c 'echo "nameserver 223.5.5.5" > /etc/resolv.conf'
sudo bash -c 'echo "nameserver 114.114.114.114" >> /etc/resolv.conf'
```

切换阿里云镜像源（加速下载）：
```bash
sudo sed -i 's|http://archive.ubuntu.com/ubuntu/|https://mirrors.aliyun.com/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources
sudo sed -i 's|http://security.ubuntu.com/ubuntu/|https://mirrors.aliyun.com/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources
sudo apt update
```

### 7.2 构建错误：`bits/libc-header-start.h: No such file`
**原因**: 缺少多架构开发头文件

**解决**:
```bash
sudo apt install -y gcc-multilib g++-multilib linux-libc-dev
```

### 7.3 Windows 换行符问题
**现象**: `/usr/bin/env: 'python3\r': No such file or directory`

**解决**: 转换换行符
```bash
cd /mnt/e/zephyrproject
sed -i 's/\r$//' zephyr/scripts/twister
```

### 7.4 WSL 启动乱码
**现象**: `wsl: 纇Km0R localhost ...`

**原因**: WSL localhost 防火墙警告（字符编码问题）

**解决**: 在 Windows 用户目录创建 `.wslconfig` 文件：
```ini
[wsl2]
localhostForwarding=false
firewall=false
```
然后执行 `wsl --shutdown` 重启 WSL。

### 7.5 west: command not found
```bash
export PATH="$HOME/.local/bin:$PATH"
```

### 7.6 找不到 ZEPHYR_BASE
```bash
export ZEPHYR_BASE=/mnt/e/zephyrproject/zephyr
```

### 7.7 找不到 Zephyr SDK
```bash
export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0
export ZEPHYR_SDK=/mnt/e/zephyrproject/zephyr-sdk-0.17.0
```

### 7.8 权限问题
```bash
sudo chmod -R 755 /mnt/e/zephyrproject
```

---

## 8. 一键配置脚本

创建文件 `~/setup-zephyr-wsl.sh`：

```bash
#!/bin/bash
set -e

echo "=== Zephyr WSL2 环境配置脚本 ==="

# 更新系统
echo "[1/6] 更新系统..."
sudo apt update

# 安装依赖
echo "[2/6] 安装基础依赖..."
sudo apt install -y git cmake ninja-build gperf ccache \
    dfu-util device-tree-compiler wget python3 python3-pip \
    python3-venv python3-dev python3-setuptools xz-utils \
    file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1

# 安装 west
echo "[3/6] 安装 west..."
pip3 install --user west

# 设置环境变量
echo "[4/6] 配置环境变量..."
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
echo 'export ZEPHYR_BASE=/mnt/e/zephyrproject/zephyr' >> ~/.bashrc
echo 'export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0' >> ~/.bashrc
source ~/.bashrc

# 注册 Zephyr
echo "[5/6] 注册 Zephyr..."
cd /mnt/e/zephyrproject/zephyr
west zephyr-export

# 安装 Python 依赖
echo "[6/6] 安装 Python 依赖..."
cd /mnt/e/zephyrproject
pip3 install --user -r zephyr/scripts/requirements.txt

echo "=== 配置完成！请重新打开终端使环境变量生效 ==="
```

运行脚本：
```bash
chmod +x ~/setup-zephyr-wsl.sh
~/setup-zephyr-wsl.sh
```

---

## 9. 测试验证

配置完成后，运行以下命令验证：

```bash
# 进入测试目录
cd /mnt/e/zephyrproject/prj/tests/unit

# 构建测试
west build -b native_sim --pristine

# 运行测试
west build -t run
```

预期输出应显示所有测试通过：
```
TEST SUITE: uart_frame_suite
TEST: test_calc_checksum_basic ... PASSED
TEST: test_frame_build_color_temp ... PASSED
...
```

---

## 10. 相关文档

- [Zephyr 官方文档](https://docs.zephyrproject.org/)
- [项目任务追踪表](../../plan/03-任务追踪表.md)
- [Phase1 最小任务单元](../../plan/04-最小任务单元-Phase1-01.md)

---

## 11. 推荐开发工作流

### Windows + WSL2 分工模式

| 工作 | 环境 | 工具 |
|------|------|------|
| **代码编写** | Windows | VS Code、IDE、编辑器 |
| **Git 操作** | Windows | Git Bash、VS Code Git |
| **编译构建** | WSL2 | west、twister、GCC |
| **单元测试** | WSL2 | twister、native_sim |
| **烧录固件** | Windows | esptool、ESP-IDF |

### 为什么这样分工？

1. **Windows 编码**: 享受更好的 IDE 支持、文件管理、图形界面
2. **WSL2 编译**: 
   - native_sim 只能在 Linux 上运行
   - 避免 Windows 路径、换行符、工具链问题
   - 与 Zephyr 官方 CI/CD 环境一致
3. **代码共享**: 通过 `/mnt/e/` 访问 Windows 磁盘，零拷贝共享

### 日常开发流程

```bash
# 1. 在 Windows 上用 VS Code 编写代码
# 2. 保存后自动同步到 WSL2

# 3. 在 WSL2 中编译测试
wsl -d Ubuntu
cd /mnt/e/zephyrproject/zephyr
./scripts/twister --testsuite-root ../prj/tests/unit -p native_sim

# 4. 在 Windows 上烧录到设备
# 使用 ESP-IDF 或 esptool
```

---

**创建日期**: 2026-04-24  
**最后更新**: 2026-04-24  
**维护者**: AI Assistant
