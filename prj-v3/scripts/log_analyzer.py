#!/usr/bin/env python3
"""
日志分析工具 - ESP32-S3 Edge AI Smart LED

解析设备串口日志，提取关键信息并生成报告
"""

import argparse
import re
import sys
import time
from collections import defaultdict
from datetime import datetime

# 日志级别颜色
LOG_COLORS = {
    'E': '\033[91m',  # 红色 - Error
    'W': '\033[93m',  # 黄色 - Warning
    'I': '\033[92m',  # 绿色 - Info
    'D': '\033[94m',  # 蓝色 - Debug
    'V': '\033[95m',  # 紫色 - Verbose
}

RESET_COLOR = '\033[0m'

# 统计数据
stats = defaultdict(int)
events = []
memory_samples = []

# 日志解析正则
LOG_PATTERN = re.compile(r'^([EWIDV]) \((\d+)\) ([\w-]+): (.*)$')
MEMORY_PATTERN = re.compile(r'内存.*?(\d+).*?(\d+)')
WAKE_PATTERN = re.compile(r'唤醒.*?检测')
COMMAND_PATTERN = re.compile(r'命令.*?识别.*?id=(\d+)')
TTS_PATTERN = re.compile(r'TTS.*?播放')
RADAR_PATTERN = re.compile(r'雷达.*?状态.*?(\d+)')
AI_PATTERN = re.compile(r'推理.*?结果')


def parse_log_line(line):
    """解析单行日志"""
    match = LOG_PATTERN.match(line.strip())
    if match:
        level, timestamp, tag, message = match.groups()
        return {
            'level': level,
            'timestamp': int(timestamp),
            'tag': tag,
            'message': message,
            'raw': line.strip()
        }
    return None


def analyze_log(log_data):
    """分析日志数据"""
    for line in log_data.split('\n'):
        entry = parse_log_line(line)
        if not entry:
            continue
        
        # 统计日志级别
        stats[f'log_{entry["level"]}'] += 1
        stats[f'tag_{entry["tag"]}'] += 1
        
        # 检测关键事件
        msg = entry['message']
        
        # 唤醒事件
        if WAKE_PATTERN.search(msg):
            events.append({
                'time': entry['timestamp'],
                'type': 'wake',
                'detail': msg
            })
            stats['wake_count'] += 1
        
        # 命令识别
        cmd_match = COMMAND_PATTERN.search(msg)
        if cmd_match:
            events.append({
                'time': entry['timestamp'],
                'type': 'command',
                'id': int(cmd_match.group(1)),
                'detail': msg
            })
            stats['command_count'] += 1
        
        # TTS 播放
        if TTS_PATTERN.search(msg):
            events.append({
                'time': entry['timestamp'],
                'type': 'tts',
                'detail': msg
            })
            stats['tts_count'] += 1
        
        # 雷达状态
        radar_match = RADAR_PATTERN.search(msg)
        if radar_match:
            events.append({
                'time': entry['timestamp'],
                'type': 'radar',
                'state': int(radar_match.group(1)),
                'detail': msg
            })
            stats['radar_count'] += 1
        
        # AI 推理
        if AI_PATTERN.search(msg):
            events.append({
                'time': entry['timestamp'],
                'type': 'ai_infer',
                'detail': msg
            })
            stats['ai_count'] += 1
        
        # 内存数据
        mem_match = MEMORY_PATTERN.search(msg)
        if mem_match:
            memory_samples.append({
                'time': entry['timestamp'],
                'free': int(mem_match.group(1)),
                'total': int(mem_match.group(2))
            })


def print_report():
    """打印分析报告"""
    print("\n" + "=" * 50)
    print("日志分析报告")
    print("=" * 50)
    
    # 日志级别统计
    print("\n[日志级别统计]")
    for level in ['E', 'W', 'I', 'D', 'V']:
        count = stats.get(f'log_{level}', 0)
        color = LOG_COLORS.get(level, '')
        print(f"  {color}{level}{RESET_COLOR}: {count} 条")
    
    # 模块统计
    print("\n[模块活跃度]")
    modules = sorted([(k.replace('tag_', ''), v) for k, v in stats.items() if k.startswith('tag_')],
                     key=lambda x: x[1], reverse=True)
    for tag, count in modules[:10]:
        print(f"  {tag}: {count} 条")
    
    # 事件统计
    print("\n[事件统计]")
    print(f"  唤醒次数: {stats.get('wake_count', 0)}")
    print(f"  命令识别: {stats.get('command_count', 0)}")
    print(f"  TTS 播放: {stats.get('tts_count', 0)}")
    print(f"  雷达检测: {stats.get('radar_count', 0)}")
    print(f"  AI 推理: {stats.get('ai_count', 0)}")
    
    # 内存分析
    if memory_samples:
        print("\n[内存使用]")
        free_values = [s['free'] for s in memory_samples]
        print(f"  最小空闲: {min(free_values)} 字节")
        print(f"  最大空闲: {max(free_values)} 字节")
        print(f"  平均空闲: {sum(free_values) // len(free_values)} 字节")
    
    # 最近事件
    print("\n[最近 10 个事件]")
    for event in events[-10:]:
        time_str = datetime.fromtimestamp(event['time'] / 1000).strftime('%H:%M:%S')
        print(f"  [{time_str}] {event['type']}: {event['detail'][:50]}...")
    
    print("\n" + "=" * 50)


def colorize_output(line):
    """给日志行添加颜色"""
    entry = parse_log_line(line)
    if entry:
        color = LOG_COLORS.get(entry['level'], '')
        return f"{color}{line.strip()}{RESET_COLOR}"
    return line.strip()


def monitor_live(port, baud=115200):
    """实时监控串口"""
    import serial
    
    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"监控端口: {port} @ {baud} bps")
        print("按 Ctrl+C 退出\n")
        
        while True:
            line = ser.readline().decode('utf-8', errors='ignore')
            if line:
                print(colorize_output(line))
                
                # 实时统计
                entry = parse_log_line(line)
                if entry:
                    stats[f'log_{entry["level"]}'] += 1
                    
                    # 检测关键事件
                    if WAKE_PATTERN.search(entry['message']):
                        print(f"\n>>> [唤醒检测] {entry['message']}\n")
                    if COMMAND_PATTERN.search(entry['message']):
                        print(f"\n>>> [命令识别] {entry['message']}\n")
                
    except KeyboardInterrupt:
        print("\n\n监控结束")
        print_report()
    except Exception as e:
        print(f"错误: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description='ESP32-S3 日志分析工具')
    parser.add_argument('--port', '-p', default='/dev/ttyUSB0', help='串口端口')
    parser.add_argument('--baud', '-b', default=115200, help='波特率')
    parser.add_argument('--file', '-f', help='分析日志文件')
    parser.add_argument('--live', '-l', action='store_true', help='实时监控')
    
    args = parser.parse_args()
    
    if args.file:
        # 分析日志文件
        with open(args.file, 'r') as f:
            log_data = f.read()
        analyze_log(log_data)
        print_report()
    
    elif args.live:
        # 实时监控
        monitor_live(args.port, args.baud)
    
    else:
        # 默认实时监控
        monitor_live(args.port, args.baud)


if __name__ == '__main__':
    main()