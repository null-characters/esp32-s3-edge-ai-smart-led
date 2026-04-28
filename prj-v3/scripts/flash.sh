#!/bin/bash
# 一键烧录脚本 - ESP32-S3 Edge AI Smart LED
# 用法: ./flash.sh [PORT] [BAUD]

set -e

# 默认参数
PORT=${1:-/dev/ttyUSB0}
BAUD=${2:-921600}
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "========================================"
echo "ESP32-S3 Edge AI Smart LED 烧录工具"
echo "========================================"
echo "端口: $PORT"
echo "波特率: $BAUD"
echo "项目目录: $PROJECT_DIR"
echo ""

# 检查端口是否存在
if [ ! -e "$PORT" ]; then
    echo "错误: 端口 $PORT 不存在"
    echo "可用端口:"
    ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  无"
    exit 1
fi

# 检查是否有编译产物
BUILD_DIR="$PROJECT_DIR/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "编译产物不存在，开始编译..."
    cd "$PROJECT_DIR"
    idf.py build
fi

# 烧录固件
echo ""
echo ">>> 烧录固件..."
cd "$PROJECT_DIR"
idf.py -p "$PORT" -b "$BAUD" flash

# 等待设备重启
echo ""
echo ">>> 等待设备重启..."
sleep 3

# 启动监控
echo ""
echo ">>> 启动串口监控 (Ctrl+C 退出)..."
idf.py -p "$PORT" monitor

echo ""
echo "烧录完成！"