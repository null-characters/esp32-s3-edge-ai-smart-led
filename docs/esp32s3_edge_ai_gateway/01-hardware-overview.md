# 01 - MCU 硬件能力与 Zephyr 板级配置

## ESP32-S3 N16R8 规格概览

| 特性 | 规格 |
|------|------|
| 内核 | Xtensa LX7 双核，最高 240MHz |
| 内部 SRAM | 512KB |
| 内部 ROM | 384KB |
| 外部 Flash | 16MB (N16) - Quad/Octal SPI |
| 外部 PSRAM | 8MB (R8) - Octal SPI with ECC |
| Wi-Fi | 802.11 b/g/n，2.4GHz |
| 蓝牙 | BLE 5.0 |
| USB | USB OTG (Full-speed) |
| GPIO | 45 个可编程 GPIO |
| ADC | 2 × 12-bit，20 通道 |
| UART | 3 路独立 UART |
| AI 加速 | 矩阵运算指令集，适合轻量级神经网络推理 |

## 会议室照明系统硬件连接

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32-S3 N16R8                         │
│                                                             │
│  ┌──────────────┐      ┌──────────────┐                    │
│  │    WiFi      │      │     BLE      │                    │
│  │  (STA模式)   │      │  (预留接口)  │                    │
│  └──────┬───────┘      └──────┬───────┘                    │
│         │                     │                            │
│         │  HTTP GET           │  未来扩展                  │
│         ▼                     ▼                            │
│  ┌─────────────────────────────────────────────────────┐  │
│  │                    网络层                            │  │
│  │  - 天气API (wttr.in / 心知天气)                      │  │
│  │  - 日出日落API (sunrise-sunset.org)                  │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌──────────────┐      ┌──────────────┐                    │
│  │   GPIO输入   │      │   UART2      │                    │
│  │              │      │              │                    │
│  │ GPIO_NUM_4   │      │ TX: GPIO_17  │                    │
│  │ (上拉输入)   │      │ RX: GPIO_18  │                    │
│  └──────┬───────┘      └──────┬───────┘                    │
│         │                     │                            │
└─────────┼─────────────────────┼────────────────────────────┘
          │                     │
          │                     │ UART串口协议
          │                     ▼
          │            ┌────────────────┐
          │            │    驱动板MCU    │
          │            │                │
          │            │ - 解析串口命令 │
          │            │ - 控制PWM输出  │
          │            │ - 驱动灯具     │
          │            └────────┬───────┘
          │                     │
          │                     ▼ PWM调光
          │            ┌────────────────┐
          │            │   可调光灯具    │
          │            │                │
          │            │ - 冷白光通道    │
          │            │ - 暖白光通道    │
          │            └────────────────┘
          │
          │ GPIO高低电平
          ▼
┌─────────────────┐
│  人体传感器PIR  │
│                 │
│ - 有人: 高电平  │
│ - 无人: 低电平  │
└─────────────────┘
```

### GPIO 定义

| GPIO | 功能 | 模式 | 说明 |
|------|------|------|------|
| GPIO4 | PIR传感器 | 输入上拉 | 有人=高电平，无人=低电平 |
| GPIO17 | UART2 TX | 输出 | 发送控制命令到驱动板 |
| GPIO18 | UART2 RX | 输入 | 接收驱动板状态（可选） |

### 传感器特性

**PIR人体传感器**：
- 检测距离：3-7米可调
- 延迟时间：2秒-10分钟可调（硬件电位器）
- 输出：高低电平（3.3V兼容）
- 建议：延迟时间调至最小，由软件处理持续检测

## 关键概念

### Blobs（闭源固件）

Wi-Fi 和蓝牙协议栈的固件是闭源二进制文件（blobs），需要通过 west 工具拉取：

```bash
west blobs fetch hal_espressif
```

没有这些 blobs，Wi-Fi 和 BLE 功能无法使用。

### PSRAM（伪静态 RAM）

- 外部扩展 RAM，通过 SPI/OPI 总线与 MCU 连接
- 容量大（8MB）但速度比内部 SRAM 慢
- 适合存放：AI模型权重、历史数据缓冲区、大块工作内存
- 不适合存放：实时中断处理、时间敏感的栈/堆

## Zephyr 板级配置

### 官方 Board YAML

使用官方板级配置：

```yaml
# esp32s3_devkitc/esp32s3/procpu
```

官方默认 DTS 引用的是 `esp32s3_w_room_n8.dtsi`（8MB Flash），**不匹配 N16R8**。

### 修正方案：Overlay 文件

在项目根目录创建 `boards/esp32s3_devkitc_procpu.overlay`：

```dts
/* 修正 Flash 为 16MB */
&flash0 {
    reg = <0x0 0x1000000>;  /* 16MB */
};

/* 启用 8MB PSRAM */
&psram0 {
    status = "okay";
    reg = <0x3c000000 0x800000>;  /* 8MB */
};

/* 配置 UART2 用于驱动板通信 */
&uart2 {
    status = "okay";
    current-speed = <115200>;
    pinctrl-0 = <&uart2_default>;
    pinctrl-names = "default";
};

/* 配置 GPIO4 用于PIR传感器 */
/ {
    pir_sensor: pir-sensor {
        compatible = "gpio-keys";
        pir_input: pir_input {
            gpios = <&gpio0 4 GPIO_ACTIVE_HIGH>;
            label = "PIR Sensor";
        };
    };
};
```

### prj.conf 关键配置

```ini
# 启用 PSRAM
CONFIG_ESP_SPIRAM=y
CONFIG_ESP_SPIRAM_SIZE=8388608
CONFIG_ESP_SPIRAM_USE_CAPS_ALLOC=y

# 启用 WiFi
CONFIG_WIFI=y
CONFIG_NET_L2_WIFI_MGMT=y
CONFIG_NET_DHCPV4=y

# 启用 HTTP Client（获取天气/日落）
CONFIG_HTTP_CLIENT=y
CONFIG_NET_SOCKETS=y

# 启用 BLE（预留接口，暂不实现App）
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="EdgeAI-Lighting"

# 启用 UART
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y

# 启用 JSON 解析（天气API返回）
CONFIG_JSON_LIBRARY=y

# 启用 CBOR（BLE通信用）
CONFIG_CBOR=y
```

### 构建命令

```bash
west build -b esp32s3_devkitc/esp32s3/procpu -p
west flash
west espressif monitor
```
