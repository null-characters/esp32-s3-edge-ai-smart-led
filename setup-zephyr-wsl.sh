#!/bin/bash
set -e

echo "=== Zephyr WSL2 环境配置脚本 ==="

# 更新系统
echo "[1/6] 更新系统..."
sudo apt update -qq

# 安装依赖
echo "[2/6] 安装基础依赖..."
sudo apt install -y -qq git cmake ninja-build gperf ccache \
    dfu-util device-tree-compiler wget python3 python3-pip \
    python3-venv python3-dev python3-setuptools xz-utils \
    file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 2>/dev/null

# 安装 west
echo "[3/6] 安装 west..."
pip3 install --user -q west

# 设置环境变量
echo "[4/6] 配置环境变量..."
if ! grep -q "ZEPHYR_BASE" ~/.bashrc; then
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
    echo 'export ZEPHYR_BASE=/mnt/e/zephyrproject/zephyr' >> ~/.bashrc
    echo 'export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0' >> ~/.bashrc
fi

export PATH="$HOME/.local/bin:$PATH"
export ZEPHYR_BASE=/mnt/e/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/mnt/e/zephyrproject/zephyr-sdk-0.17.0

# 注册 Zephyr
echo "[5/6] 注册 Zephyr..."
cd /mnt/e/zephyrproject/zephyr
west zephyr-export 2>/dev/null || true

# 安装 Python 依赖
echo "[6/6] 安装 Python 依赖..."
cd /mnt/e/zephyrproject
pip3 install --user -q -r zephyr/scripts/requirements.txt 2>/dev/null || true

echo "=== 配置完成！ ==="
echo "请运行: source ~/.bashrc"
