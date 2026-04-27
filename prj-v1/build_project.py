#!/usr/bin/env python3
"""
构建脚本 - 用于构建ESP32-S3 Edge AI Gateway项目
"""
import os
import sys
import subprocess

# 环境配置
ZEPHYR_BASE = r'E:\zephyrproject\zephyr'
ZEPHYR_SDK = r'E:\zephyrproject\zephyr-sdk-0.17.0'

def setup_env():
    """设置环境变量"""
    env = os.environ.copy()
    env['ZEPHYR_BASE'] = ZEPHYR_BASE
    env['ZEPHYR_SDK_INSTALL_DIR'] = ZEPHYR_SDK
    return env

def main():
    os.chdir(r'E:\zephyrproject\prj')
    env = setup_env()
    
    # 清理并构建
    if os.path.exists('build'):
        import shutil
        shutil.rmtree('build')
    
    cmd = [
        'west', 'build',
        '-b', 'esp32s3_devkitc/esp32s3/procpu',
        '-p'
    ]
    
    result = subprocess.run(cmd, env=env, capture_output=True, text=True)
    print("STDOUT:", result.stdout)
    print("STDERR:", result.stderr)
    print("Return code:", result.returncode)
    return result.returncode

if __name__ == '__main__':
    sys.exit(main())
