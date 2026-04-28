#!/usr/bin/env python3
"""
性能分析工具 - ESP32-S3 Edge AI Smart LED

收集并分析设备性能指标：内存、CPU、延迟
"""

import argparse
import serial
import time
import json
import re
from collections import defaultdict
from datetime import datetime

# 性能数据存储
perf_data = {
    'memory': [],
    'cpu': [],
    'latency': [],
    'timestamps': []
}

# 解析正则
MEMORY_PATTERN = re.compile(r'内存.*?(\d+).*?(\d+).*?(\d+)')
CPU_PATTERN = re.compile(r'CPU.*?(\d+)')
LATENCY_PATTERN = re.compile(r'延迟.*?(\d+)')


def parse_performance_line(line):
    """解析性能数据行"""
    # 内存数据
    mem_match = MEMORY_PATTERN.search(line)
    if mem_match:
        return {
            'type': 'memory',
            'free': int(mem_match.group(1)),
            'used': int(mem_match.group(2)),
            'total': int(mem_match.group(3))
        }
    
    # CPU 数据
    cpu_match = CPU_PATTERN.search(line)
    if cpu_match:
        return {
            'type': 'cpu',
            'usage': int(cpu_match.group(1))
        }
    
    # 延迟数据
    lat_match = LATENCY_PATTERN.search(line)
    if lat_match:
        return {
            'type': 'latency',
            'ms': int(lat_match.group(1))
        }
    
    return None


def collect_data(port, baud=115200, duration=60):
    """收集性能数据"""
    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"收集性能数据: {port} @ {baud}")
        print(f"持续时间: {duration} 秒\n")
        
        start_time = time.time()
        
        while time.time() - start_time < duration:
            line = ser.readline().decode('utf-8', errors='ignore')
            if not line:
                continue
            
            data = parse_performance_line(line)
            if data:
                timestamp = time.time() - start_time
                perf_data['timestamps'].append(timestamp)
                
                if data['type'] == 'memory':
                    perf_data['memory'].append(data)
                elif data['type'] == 'cpu':
                    perf_data['cpu'].append(data)
                elif data['type'] == 'latency':
                    perf_data['latency'].append(data)
                
                # 实时显示
                print(f"[{timestamp:.1f}s] {data}")
        
        ser.close()
        print("\n数据收集完成")
        
    except Exception as e:
        print(f"错误: {e}")
        raise


def analyze_data():
    """分析性能数据"""
    report = {
        'memory': {},
        'cpu': {},
        'latency': {}
    }
    
    # 内存分析
    if perf_data['memory']:
        free_values = [m['free'] for m in perf_data['memory']]
        used_values = [m['used'] for m in perf_data['memory']]
        
        report['memory'] = {
            'min_free': min(free_values),
            'max_free': max(free_values),
            'avg_free': sum(free_values) // len(free_values),
            'max_used': max(used_values),
            'avg_used': sum(used_values) // len(used_values)
        }
    
    # CPU 分析
    if perf_data['cpu']:
        usage_values = [c['usage'] for c in perf_data['cpu']]
        
        report['cpu'] = {
            'min': min(usage_values),
            'max': max(usage_values),
            'avg': sum(usage_values) // len(usage_values),
            'samples': len(usage_values)
        }
    
    # 延迟分析
    if perf_data['latency']:
        lat_values = [l['ms'] for l in perf_data['latency']]
        
        report['latency'] = {
            'min': min(lat_values),
            'max': max(lat_values),
            'avg': sum(lat_values) // len(lat_values),
            'samples': len(lat_values)
        }
    
    return report


def print_report(report):
    """打印性能报告"""
    print("\n" + "=" * 60)
    print("性能分析报告")
    print("=" * 60)
    
    # 内存报告
    if report['memory']:
        print("\n[内存使用]")
        m = report['memory']
        print(f"  空闲内存: {m['min_free']} - {m['max_free']} KB (平均 {m['avg_free']} KB)")
        print(f"  已用内存: 最大 {m['max_used']} KB (平均 {m['avg_used']} KB)")
        
        if m['avg_free'] < 100 * 1024:
            print("  ⚠️  警告: 内存使用率较高")
        else:
            print("  ✓ 内存使用正常")
    
    # CPU 报告
    if report['cpu']:
        print("\n[CPU 使用率]")
        c = report['cpu']
        print(f"  范围: {c['min']}% - {c['max']}%")
        print(f"  平均: {c['avg']}%")
        print(f"  样本数: {c['samples']}")
        
        if c['avg'] > 80:
            print("  ⚠️  警告: CPU 使用率过高")
        else:
            print("  ✓ CPU 使用正常")
    
    # 延迟报告
    if report['latency']:
        print("\n[响应延迟]")
        l = report['latency']
        print(f"  范围: {l['min']} - {l['max']} ms")
        print(f"  平均: {l['avg']} ms")
        print(f"  样本数: {l['samples']}")
        
        if l['avg'] > 1000:
            print("  ⚠️  警告: 响应延迟过高")
        else:
            print("  ✓ 响应正常")
    
    print("\n" + "=" * 60)
    
    # 验收检查
    print("\n[验收检查]")
    passed = True
    
    if report['memory']:
        if report['memory']['avg_free'] < 100 * 1024:
            print("  ✗ 内存占用: 未达标 (< 100KB 空闲)")
            passed = False
        else:
            print("  ✓ 内存占用: 达标")
    
    if report['cpu']:
        if report['cpu']['avg'] > 80:
            print("  ✗ CPU 利用率: 未达标 (> 80%)")
            passed = False
        else:
            print("  ✓ CPU 利用率: 达标")
    
    if report['latency']:
        if report['latency']['avg'] > 1000:
            print("  ✗ 响应延迟: 未达标 (> 1000ms)")
            passed = False
        else:
            print("  ✓ 响应延迟: 达标")
    
    print("\n" + ("所有指标达标 ✓" if passed else "部分指标未达标 ✗"))
    
    return passed


def save_report(report, output_file):
    """保存报告到文件"""
    with open(output_file, 'w') as f:
        json.dump({
            'timestamp': datetime.now().isoformat(),
            'report': report,
            'raw_data': {
                'memory': perf_data['memory'][-100:],  # 最近 100 条
                'cpu': perf_data['cpu'][-100:],
                'latency': perf_data['latency'][-100:]
            }
        }, f, indent=2, ensure_ascii=False)
    print(f"\n报告已保存: {output_file}")


def main():
    parser = argparse.ArgumentParser(description='ESP32-S3 性能分析工具')
    parser.add_argument('--port', '-p', default='/dev/ttyUSB0', help='串口端口')
    parser.add_argument('--baud', '-b', default=115200, help='波特率')
    parser.add_argument('--duration', '-d', type=int, default=60, help='采集时长(秒)')
    parser.add_argument('--output', '-o', help='输出文件')
    
    args = parser.parse_args()
    
    # 收集数据
    collect_data(args.port, args.baud, args.duration)
    
    # 分析数据
    report = analyze_data()
    
    # 打印报告
    passed = print_report(report)
    
    # 保存报告
    if args.output:
        save_report(report, args.output)
    
    return 0 if passed else 1


if __name__ == '__main__':
    exit(main())