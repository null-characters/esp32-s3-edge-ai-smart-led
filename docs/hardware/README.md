# 硬件模块资料索引

> 本文档索引项目使用的传感器模块技术资料，包含官方下载链接和关键参数摘要。

---

## 一、HLK-LD2410B 毫米波雷达模块

### 1.1 基本信息

| 项目 | 规格 |
|------|------|
| **型号** | HLK-LD2410B |
| **频率** | 24GHz |
| **检测类型** | 人体存在 + 运动检测 |
| **检测距离** | 0.1m ~ 8m（可调） |
| **接口** | UART (256000 bps) / GPIO |
| **供电** | 5V |
| **功耗** | ~0.5W |

### 1.2 官方资料下载

| 资料类型 | 下载链接 |
|----------|----------|
| **LD2410B 数据手册** | [Google Drive](https://drive.google.com/drive/folders/16zI-fium_BZeP08EyQke0rWp0BJTMvw3) |
| **LD2410 数据手册** | [Google Drive](https://drive.google.com/drive/folders/1lCQv3mfHJ3XKXzweeHPFnJ_8_D_EWEKk) |
| **LD2410C 数据手册** | [Google Drive](https://drive.google.com/drive/folders/1ypOlacBmmFXY6lDQ0f1hEJFmczNe-0WG) |
| **官方资料合集** | [Google Drive](https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-) |
| **海凌科官网** | [hlktech.net](https://www.hlktech.net) |
| **下载中心** | [下载页面](https://www.hlktech.net/index.php?id=download-center) |
| **技术论坛** | [ask.hlktech.net](https://ask.hlktech.net) |

### 1.3 通信协议

```c
// UART 配置
波特率: 256000 (默认，推荐硬件UART)
数据位: 8
停止位: 1
校验位: 无

// 帧格式
帧头: 0xFD 0xFC 0xFB 0xFA
帧尾: 0x04 0x03 0x02 0x01
```

### 1.4 输出数据

| 数据 | 说明 |
|------|------|
| `target_state` | 目标状态：0=无, 1=静止, 2=运动 |
| `distance` | 目标距离 (0-8m) |
| `energy` | 目标能量/信噪比 |
| `moving_distance` | 运动目标距离 |
| `still_distance` | 静止目标距离 |
| `g0~g8` | 9个距离门的能量值 |

### 1.5 距离门阈值（默认值）

| 门 | 运动阈值 | 静止阈值 | 距离范围 |
|----|----------|----------|----------|
| G0 | 50 | 0 | 0~0.75m |
| G1 | 50 | 0 | 0.75~1.5m |
| G2 | 40 | 40 | 1.5~2.25m |
| G3 | 30 | 40 | 2.25~3m |
| G4 | 20 | 30 | 3~3.75m |
| G5 | 15 | 30 | 3.75~4.5m |
| G6 | 15 | 20 | 4.5~5.25m |
| G7 | 15 | 20 | 5.25~6m |
| G8 | 15 | 20 | 6~8m |

### 1.6 ESPHome 配置示例

```yaml
ld2410:

uart:
  tx_pin: GPIOXX
  rx_pin: GPIOXX
  baud_rate: 256000

binary_sensor:
  - platform: ld2410
    has_target:
      name: "存在检测"
    has_moving_target:
      name: "运动目标"
    has_still_target:
      name: "静止目标"

sensor:
  - platform: ld2410
    moving_distance:
      name: "运动距离"
    still_distance:
      name: "静止距离"
    moving_energy:
      name: "运动能量"
    still_energy:
      name: "静止能量"
```

---

## 二、INMP441 MEMS 麦克风模块

### 2.1 基本信息

| 项目 | 规格 |
|------|------|
| **型号** | INMP441 |
| **类型** | MEMS 数字麦克风 |
| **接口** | I2S 数字输出 |
| **信噪比 (SNR)** | 61 dBA |
| **灵敏度** | -26 dB FS |
| **频率响应** | 60 Hz ~ 15 kHz |
| **工作电压** | 1.8V ~ 3.3V |
| **功耗** | ~1.4 mW |
| **封装** | 底部开孔 LGA (4.72×3.76mm) |

### 2.2 资料下载

| 资料类型 | 来源 | 备注 |
|----------|------|------|
| **官方数据手册** | [TDK InvenSense](https://invensense.tdk.com) | 需注册登录 |
| **分销商资料** | DigiKey / Mouser / LCSC | 搜索 "INMP441" |
| **SparkFun 模块** | [SparkFun](https://www.sparkfun.com) | 搜索 "INMP441" |

### 2.3 引脚定义

| 引脚 | 功能 | ESP32 连接 |
|------|------|-----------|
| **VDD** | 电源 | 3.3V |
| **GND** | 地 | GND |
| **SCK** | 位时钟 (BCLK) | GPIO26 |
| **WS** | 字选择 (LRCK) | GPIO25 |
| **SD** | 数据输出 | GPIO22 |
| **L/R** | 左右声道选择 | GND(左) / VDD(右) |

### 2.4 I2S 时序

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    I2S 信号时序                                          │
│                                                                         │
│  SCK (BCLK):  ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ...              │
│               └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘                   │
│                                                                         │
│  WS (LRCK):   ────────────────┐                       ┌───────────────  │
│                               └───────────────────────┘                │
│                               │← 左声道 →│← 右声道 →│                   │
│                                                                         │
│  SD (DATA):   ────┬───┬───┬───┬───┬───┬───┬───┬───┬─── ...              │
│                 D23 D22 D21 D20 ... D0   D23 D22 ...                    │
│                                                                         │
│  数据在 SCK 下降沿输出，24位数据，MSB优先                                 │
│  采样率: 8kHz ~ 51.2kHz                                                 │
│  SCK 频率: 通常为 64×采样率                                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.5 ESP32 I2S 驱动示例

```c
#include <driver/i2s.h>

#define I2S_WS      25
#define I2S_SCK     26
#define I2S_SD      22

void i2s_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = 16000,              // 16kHz
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };
    
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// 读取音频数据
int read_audio(int32_t *buffer, int samples)
{
    size_t bytes_read;
    i2s_read(I2S_NUM_0, buffer, samples * sizeof(int32_t), 
             &bytes_read, portMAX_DELAY);
    return bytes_read / sizeof(int32_t);
}
```

---

## 三、接线总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ESP32-S3 与传感器模块接线                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ESP32-S3                    LD2410B                                    │
│  ┌─────────┐               ┌─────────┐                                │
│  │ UART1_TX├──────────────►│ RX      │                                │
│  │ UART1_RX│◄──────────────┤ TX      │                                │
│  │ 5V      ├──────────────►│ VCC     │                                │
│  │ GND     ├──────────────►│ GND     │                                │
│  └─────────┘               └─────────┘                                │
│                                                                         │
│  ESP32-S3                    INMP441                                   │
│  ┌─────────┐               ┌─────────┐                                │
│  │ GPIO26  ├──────────────►│ SCK     │                                │
│  │ GPIO25  ├──────────────►│ WS      │                                │
│  │ GPIO22  │◄──────────────┤ SD      │                                │
│  │ 3.3V    ├──────────────►│ VDD     │                                │
│  │ GND     ├──────────────►│ GND     │                                │
│  │ GND     ├──────────────►│ L/R     │ (左声道)                        │
│  └─────────┘               └─────────┘                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 四、参考项目

### 4.1 LD2410 相关

| 项目 | 说明 |
|------|------|
| [ESPHome LD2410](https://esphome.io/components/sensor/ld2410.html) | ESPHome 官方集成 |
| [HLK 官方资料](https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-) | 数据手册合集 |

### 4.2 INMP441 相关

| 项目 | 说明 |
|------|------|
| [ESP-IDF I2S 示例](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s) | 官方 I2S 驱动示例 |
| [Arduino-ESP32 I2S](https://github.com/espressif/arduino-esp32) | Arduino I2S 库 |

---

## 五、本地保存的资料

### 5.1 文件列表

| 文件 | 说明 |
|------|------|
| `README.md` | 本文档（资料索引） |
| `ld2410_uart.h` | LD2410B 驱动头文件（API 定义、协议常量） |
| `inmp441_driver.h` | INMP441 驱动头文件（I2S 配置、MFCC 接口） |

### 5.2 使用说明

这些头文件为参考实现，实际集成时需要：

1. **LD2410B**：
   - 参考 `ld2410_uart.h` 中的帧格式和命令定义
   - 在 Zephyr 中实现 UART 驱动
   - 或直接使用 ESPHome 官方组件

2. **INMP441**：
   - 参考 `inmp441_driver.h` 中的 I2S 配置
   - 使用 ESP-IDF 或 Zephyr 的 I2S 驱动
   - MFCC 提取可使用 ESP-DSP 库

---

*维护者：null-characters*
*创建日期：2026-04-27*
*版本：v1.0*
