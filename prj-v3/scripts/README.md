# ESP32-S3 Edge AI Smart LED 工具集

> 开发、调试、分析工具

---

## 工具列表

| 工具 | 用途 | 类型 |
|------|------|------|
| `flash.sh` | 一键烧录固件 | Shell |
| `erase_flash.sh` | 恢复出厂设置 | Shell |
| `log_analyzer.py` | 日志分析 | Python |
| `perf_analyzer.py` | 性能分析 | Python |

---

## 使用方法

### 一键烧录

```bash
# 默认端口 /dev/ttyUSB0
./tools/flash.sh

# 指定端口和波特率
./tools/flash.sh /dev/ttyUSB1 921600
```

### 恢复出厂设置

```bash
./tools/erase_flash.sh
```

### 日志分析

```bash
# 实时监控
python tools/log_analyzer.py --live

# 分析日志文件
python tools/log_analyzer.py -f logs/device.log

# 指定端口
python tools/log_analyzer.py -p /dev/ttyUSB0
```

### 性能分析

```bash
# 采集 60 秒
python tools/perf_analyzer.py -d 60

# 保存报告
python tools/perf_analyzer.py -d 60 -o report.json
```

---

## 依赖

### Python 依赖

```bash
pip install pyserial
```

### 系统依赖

- ESP-IDF v5.4+
- Python 3.8+
- Bash 4.0+

---

## 输出示例

### 日志分析报告

```
==================================================
日志分析报告
==================================================

[日志级别统计]
  E: 2 条
  W: 15 条
  I: 1250 条
  D: 890 条

[事件统计]
  唤醒次数: 25
  命令识别: 23
  TTS 播放: 20
  雷达检测: 1500
  AI 推理: 500

[内存使用]
  最小空闲: 245000 字节
  最大空闲: 512000 字节
  平均空闲: 380000 字节
==================================================
```

### 性能分析报告

```
==================================================
性能分析报告
==================================================

[内存使用]
  空闲内存: 200 - 450 KB (平均 320 KB)
  ✓ 内存使用正常

[CPU 使用率]
  范围: 15% - 75%
  平均: 45%
  ✓ CPU 使用正常

[响应延迟]
  范围: 150 - 850 ms
  平均: 320 ms
  ✓ 响应正常

所有指标达标 ✓
==================================================
```

---

*创建日期：2026-04-28*