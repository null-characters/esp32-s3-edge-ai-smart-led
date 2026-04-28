# 最小任务单元清单 - Phase 1 框架迁移

> 可执行的原子任务列表，每个任务有明确的输入输出和验收标准

---

## T1.1 项目初始化

### T1.1.1 创建 ESP-IDF 项目
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.1 |
| **任务名称** | 创建 ESP-IDF 项目 |
| **预估工时** | 0.5h |
| **前置任务** | 无 |

**执行步骤**：
```bash
cd /Users/fengbing/git_prj/esp32-s3-edge-ai-smart-led/prj-v3
idf.py create-project smart_led_v3
cd smart_led_v3
idf.py set-target esp32s3
```

**验收标准**：
- [ ] `idf.py build` 编译成功
- [ ] 目录结构：main/, CMakeLists.txt, sdkconfig.defaults

---

### T1.1.2 配置目标芯片
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.2 |
| **任务名称** | 配置目标芯片 |
| **预估工时** | 0.5h |
| **前置任务** | T1.1.1 |

**执行步骤**：
```bash
idf.py menuconfig
# 配置项：
# - CONFIG_IDF_TARGET=esp32s3
# - CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
# - CONFIG_SPIRAM=y
# - CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
```

**验收标准**：
- [ ] `sdkconfig.defaults` 包含正确配置
- [ ] PSRAM 已启用

---

### T1.1.3 初始化 Git 仓库
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.3 |
| **任务名称** | 初始化 Git 仓库 |
| **预估工时** | 0.5h |
| **前置任务** | T1.1.1 |

**执行步骤**：
```bash
git init
echo "build/\nsdkconfig\n*.bin\n*.elf\n*.map" > .gitignore
git add . && git commit -m "feat: 初始化 ESP-IDF 项目"
```

**验收标准**：
- [ ] Git 仓库初始化成功
- [ ] `.gitignore` 配置正确

---

## T1.2 目录结构搭建

### T1.2.1 创建源码目录结构
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.1 |
| **任务名称** | 创建源码目录结构 |
| **预估工时** | 1h |
| **前置任务** | T1.1.1 |

**执行步骤**：
```bash
mkdir -p main/src/{voice,arbitration,multimodal,audio,radar,control,network,drivers}
mkdir -p main/include
mkdir -p components/{esp-sr,esp-adf,tflite-micro}
mkdir -p models
```

**验收标准**：
- [ ] 目录结构创建成功
- [ ] 每个模块目录包含 `.gitkeep`

---

### T1.2.2 配置 CMakeLists.txt
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.2 |
| **任务名称** | 配置 CMakeLists.txt |
| **预估工时** | 1h |
| **前置任务** | T1.2.1 |

**执行步骤**：
编辑 `main/CMakeLists.txt`：
```cmake
idf_component_register(
    SRCS "src/main.c"
         "src/drivers/led_pwm.c"
         "src/drivers/uart_driver.c"
         "src/drivers/i2s_driver.c"
    INCLUDE_DIRS "include"
                 "src/drivers"
)
```

**验收标准**：
- [ ] `idf.py build` 编译成功
- [ ] 源文件正确注册

---

## T1.3 基础外设驱动

### T1.3.1 LED PWM 驱动
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.1 |
| **任务名称** | LED PWM 驱动实现 |
| **预估工时** | 4h |
| **前置任务** | T1.2.2 |

**文件清单**：
- `main/src/drivers/led_pwm.c`
- `main/include/led_pwm.h`

**接口定义**：
```c
esp_err_t led_pwm_init(void);
esp_err_t led_set_brightness(uint8_t percent);
esp_err_t led_set_color_temp(uint16_t kelvin);
esp_err_t led_set_rgb(uint8_t r, uint8_t g, uint8_t b);
```

**验收标准**：
- [ ] PWM 输出频率 5kHz
- [ ] 亮度 0-100% 可调
- [ ] 色温 2700K-6500K 可调

---

### T1.3.2 UART 驱动
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.2 |
| **任务名称** | UART 驱动实现 |
| **预估工时** | 3h |
| **前置任务** | T1.2.2 |

**文件清单**：
- `main/src/drivers/uart_driver.c`
- `main/include/uart_driver.h`

**接口定义**：
```c
esp_err_t uart_init(uint32_t baudrate);
int uart_read_bytes(uint8_t *data, size_t len, uint32_t timeout_ms);
int uart_write_bytes(const uint8_t *data, size_t len);
```

**验收标准**：
- [ ] UART0 调试输出正常
- [ ] UART1 雷达通信正常
- [ ] 波特率 115200

---

### T1.3.3 I2S 驱动
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.3 |
| **任务名称** | I2S 驱动实现 |
| **预估工时** | 4h |
| **前置任务** | T1.2.2 |

**文件清单**：
- `main/src/drivers/i2s_driver.c`
- `main/include/i2s_driver.h`

**接口定义**：
```c
esp_err_t i2s_init(uint32_t sample_rate, uint8_t bits_per_sample);
esp_err_t i2s_read(int16_t *data, size_t samples, size_t *bytes_read);
esp_err_t i2s_deinit(void);
```

**验收标准**：
- [ ] 采样率 16kHz
- [ ] 16-bit 单声道
- [ ] DMA 缓冲正常

---

## T1.4 网络连接

### T1.4.1 Wi-Fi STA 模式
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.4.1 |
| **任务名称** | Wi-Fi STA 模式实现 |
| **预估工时** | 3h |
| **前置任务** | T1.2.2 |

**文件清单**：
- `main/src/network/wifi_sta.c`
- `main/include/wifi_sta.h`

**验收标准**：
- [ ] 连接指定 AP 成功
- [ ] 获取 IP 地址
- [ ] 断线重连机制

---

## 任务依赖图

```
T1.1.1 ──┬── T1.1.2 ──┐
         │            │
         └── T1.1.3   │
                      │
T1.2.1 ──── T1.2.2 ───┼── T1.3.1
                      │      │
                      │      ├── T1.3.2
                      │      │
                      │      └── T1.3.3
                      │
                      └── T1.4.1
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M1.1 项目骨架 | T1.1.1 - T1.1.3 | 第1天 |
| M1.2 目录结构 | T1.2.1 - T1.2.2 | 第2天 |
| M1.3 外设驱动 | T1.3.1 - T1.3.3 | 第5天 |
| M1.4 网络连接 | T1.4.1 | 第7天 |

---

*创建日期：2026-04-27*