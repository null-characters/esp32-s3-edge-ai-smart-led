# ESP32-S3 ESP-SR/TFLM 最佳实践审查报告

**审查日期**: 2026-04-28  
**项目**: prj-v3 语音交互智能LED网关  
**参考文档**: ESP-IDF/ESP-SR 官方文档 (latest)

---

## 一、官方文档关键要点

### 1.1 ESP-SR 内存需求基准测试

| 模块 | 内部RAM | PSRAM | CPU占用率 |
|------|---------|-------|-----------|
| AFE (MR, LOW_COST) | 72.3 KB | 732.7 KB | Feed: 8.4%, Fetch: 15% |
| AFE (MMNR, LOW_COST) | 76.6 KB | 1173.9 KB | Feed: 36.6%, Fetch: 30% |
| WakeNet9 @ 2通道 | 16 KB | 324 KB | 3.0ms/帧 (32ms帧长) |
| WakeNet9l @ 2通道 | ~20 KB | ~420 KB | ~4ms/帧 (性能提升1.3倍) |
| MultiNet 7 | 18 KB | 2920 KB | 11ms/帧 |

> **关键结论**: ESP-SR 大量使用 PSRAM，内部 RAM 需求相对较小。

### 1.2 I2S DMA 配置最佳实践

官方建议防止数据丢失的配置：

```c
// 高采样率应用 (16kHz+)
dma_frame_num ≤ 4092 / (slot_num × data_bit_width / 8)  // 尽可能大
dma_desc_num > polling_cycle / interrupt_interval        // 大于两次读取��隔

// 示例: 16kHz, 32位, 单通道, 10ms轮询
dma_frame_num ≤ 511
dma_desc_num > 3
recv_buffer_size > 3 × 4092 = 12276 bytes
```

**必须实现溢出回调**：
```c
static IRAM_ATTR bool i2s_rx_overflow_callback(...) {
    // 处理RX队列溢出事件
    return false;
}
i2s_channel_register_event_callback(rx_handle, &cbs, NULL);
```

### 1.3 LEDC PWM 线程安全

官方明确指出以下 API **非线程安全**：
- `ledc_set_duty()` + `ledc_update_duty()` 组合 ❌
- `ledc_set_fade_with_time()` + `ledc_fade_start()` 组合 ❌

**线程安全替代 API** (ESP-IDF v5.1+)：
- `ledc_set_duty_and_update()` ✅
- `ledc_set_fade_time_and_start()` ✅
- `ledc_set_fade_step_and_start()` ✅

### 1.4 FreeRTOS 任务栈配置

ESP-IDF 使用自己的堆实现，默认分配到内部内存。

**将任务放入外部内存的方法**：
```c
// 方法一: 使用 CreateWithCaps API
xTaskCreateWithCaps(..., MALLOC_CAP_SPIRAM);

// 方法二: 静态分配
void* memory = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
xTaskCreateStatic(..., memory, ...);
```

---

## 二、代码审查发现的问题

### 2.1 已修复问题

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | `led_pwm.c` | PWM渐变非线程安全 | 使用 `ledc_set_fade_time_and_start()` |
| 2 | `audio_afe.c` | 缺少栈使用监控 | 添加 `uxTaskGetStackHighWaterMark()` |
| 3 | `i2s_driver.c` | 缺少溢出回调处理 | 添加 `i2s_rx_overflow_callback()` |
| 4 | `audio_afe.c` | 缺少内存状态打印 | 添加 `print_memory_status()` |

### 2.2 已符合最佳实践

| 项目 | 状态 | 说明 |
|------|------|------|
| PSRAM 音频缓冲区 | ✅ | 使用 `heap_caps_malloc(MALLOC_CAP_SPIRAM)` |
| 任务栈大小 16KB | ✅ | 符合 ESP-SR 要求 |
| LED PWM 线程安全 API | ✅ | 使用 `ledc_set_duty_and_update()` |
| I2S DMA 配置优化 | ✅ | 16×1024 配置 |
| WiFi 省电模式 | ✅ | `WIFI_PS_MIN_MODEM` |

---

## 三、遗留建议

### 3.1 可选优化

| 建议 | 说明 |
|------|------|
| 使用 `ledc_find_suitable_duty_resolution()` | 动态计算最佳分辨率 |
| 启用 `CONFIG_I2S_ISR_IRAM_SAFE` | Flash操作时保持I2S中断服务 |
| WiFi凭据移至NVS | 支持运行时配置 |

### 3.2 TFLM 配置建议

当前 TFLM Arena 100KB，对于多模态推理可能不足。建议：

```c
// 多模型独立 Arena
#define TFLM_ARENA_SIZE_SOUND   (50 * 1024)   // 声音分类器
#define TFLM_ARENA_SIZE_RADAR   (10 * 1024)   // 雷达分析器
#define TFLM_ARENA_SIZE_FUSION  (20 * 1024)   // 融合决策器
```

---

## 四、ESP-SR 模型选择建议

### 4.1 WakeNet 模型

| 模型 | 适用场景 | 内存需求 |
|------|----------|----------|
| `wn9_xiaobaitong` | 当前使用，标准版 | 16KB RAM + 324KB PSRAM |
| `wn9l_xiaobaitong` | 快速语速响应更好 | ~20KB RAM + ~420KB PSRAM |

### 4.2 MultiNet 模型

| 模型 | 识别率 (安静) | 识别率 (噪声) | PSRAM |
|------|---------------|---------------|-------|
| mn5_cn | 95.4% | 82.7% | 2310KB |
| mn6_cn | 96.8% | 85.5% | 4100KB |
| mn7_cn | 97.2% | 90.6% | 2920KB |

> **建议**: 使用 `mn7_cn` 以获得最佳噪声环境识别率。

---

## 五、参考链接

- ESP-SR 文档: https://docs.espressif.com/projects/esp-sr/
- ESP-SR GitHub: https://github.com/espressif/esp-sr
- ESP-IDF LEDC: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/ledc.html
- ESP-IDF I2S: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html
- ESP-TFLM: https://github.com/espressif/esp-tflite-micro

---

**审查完成** | 参考文档: ESP-IDF/ESP-SR Programming Guide (latest)