#!/bin/bash
# 恢复出厂设置脚本 - 擦除所有配置
# 用法: ./erase_flash.sh [PORT]

set -e

PORT=${1:-/dev/ttyUSB0}
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "========================================"
echo "恢复出厂设置 - 擦除所有数据"
echo "========================================"
echo "警告: 此操作将清除所有用户配置！"
echo ""
read -p "确认继续? (y/N): " confirm
if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
    echo "取消操作"
    exit 0
fi

echo ""
echo ">>> 擦除 Flash..."
cd "$PROJECT_DIR"
idf.py -p "$PORT" erase-flash

echo ""
echo ">>> 重新烧录固件..."
idf.py -p "$PORT" flash

echo ""
echo "恢复出厂设置完成！设备将重启进入配网模式。"