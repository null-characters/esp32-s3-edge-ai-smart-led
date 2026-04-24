# 05 - 分阶段实施路线图

## 总览

```
Phase 1        Phase 2        Phase 3
基础硬件       WiFi数据       AI推理
(2周)          (2周)          (2周)
   │              │              │
   v              v              v
┌──────┐      ┌──────┐      ┌──────┐
│GPIO  │      │WiFi  │      │TFLM  │
│UART  │─────►│HTTP  │─────►│模型  │
│      │      │API   │      │推理  │
└──────┘      └──────┘      └──────┘
```

---

## Phase 1：基础硬件与通信（2周）

### 目标
ESP32-S3与驱动板通过UART建立通信，PIR传感器GPIO输入正常，能基础控制灯具。

### 任务清单

**硬件连接与配置**
- [ ] 连接PIR传感器到GPIO4（上拉输入）
- [ ] 连接UART2到驱动板（TX=GPIO17, RX=GPIO18）
- [ ] 创建 `boards/esp32s3_devkitc_procpu.overlay`
- [ ] 配置prj.conf启用UART、GPIO、PSRAM

**Zephyr工程初始化**
- [ ] 创建新项目目录结构
- [ ] 配置west workspace
- [ ] 拉取hal_espressif blobs
- [ ] 编写CMakeLists.txt

**UART驱动板通信**
- [ ] 实现UART初始化（115200bps）
- [ ] 实现帧封装/解析函数
- [ ] 实现命令发送（设置色温、光强）
- [ ] 实现响应接收与ACK/NAK处理
- [ ] 与驱动板联调：能控制灯具开关和调光

**PIR传感器驱动**
- [ ] GPIO4配置为上拉输入
- [ ] 实现GPIO中断回调
- [ ] 实现防抖处理（50ms消抖）
- [ ] 实现有人/无人状态机

**基础控制逻辑（规则驱动）**
- [ ] 有人→开灯（默认色温4000K，亮度70%）
- [ ] 无人5分钟→关灯
- [ ] 按时段调整（上午冷白，下午中性，晚上暖白）

### 验收标准

1. PIR传感器检测到有人，GPIO能正确读取状态
2. ESP32-S3通过UART发送命令，驱动板能正确执行
3. 灯具能根据有人/无人自动开关
4. 能通过串口日志查看当前状态

### 关键代码验证

```c
// UART命令测试
void test_uart_control(void) {
    // 设置色温4500K，亮度80%
    uart_send_command(CMD_SET_BOTH, 4500, 80);
    
    // 验证ACK响应
    if (wait_for_ack(500) == RESP_ACK) {
        printk("Command success\n");
    }
}

// PIR测试
void test_pir_sensor(void) {
    while (1) {
        bool presence = gpio_get_pir_status();
        printk("Presence: %s\n", presence ? "YES" : "NO");
        k_sleep(K_MSEC(1000));
    }
}
```

---

## Phase 2：WiFi数据获取（2周）

### 目标
ESP32-S3通过WiFi获取天气和日出日落数据，实现天气感知和日落渐变调光。

### 任务清单

**WiFi连接**
- [ ] 配置WiFi STA模式
- [ ] 实现WiFi连接管理（自动重连）
- [ ] 配置DHCP获取IP

**SNTP时间同步**
- [ ] 配置SNTP客户端
- [ ] 同步网络时间到RTC
- [ ] 处理时区转换（北京时间UTC+8）

**HTTP客户端**
- [ ] 配置HTTP client
- [ ] 实现DNS解析
- [ ] 实现HTTP GET请求

**天气API集成**
- [ ] 实现wttr.in API调用
- [ ] 解析天气状况文本
- [ ] 转换为weather编码（0-1）
- [ ] 实现1小时定时更新

**日出日落API集成**
- [ ] 实现sunrise-sunset.org API调用
- [ ] 解析JSON响应
- [ ] 提取日出/日落时间
- [ ] 实现每天定时更新

**数据缓存管理**
- [ ] 实现weather_cache结构
- [ ] 实现sun_cache结构
- [ ] 实现缓存过期检测

**天气感知调光**
- [ ] 阴天增强光强（晴天60%→阴天80%）
- [ ] 根据日落时间渐变调光

### 验收标准

1. ESP32-S3成功连接WiFi并获取IP
2. 时间同步成功，RTC显示正确本地时间
3. 成功获取天气数据，解析为0-1编码
4. 成功获取日出日落时间
5. 阴天自动增强光强，日落后自动调亮

### 关键代码验证

```c
// 天气获取测试
void test_weather_fetch(void) {
    int ret = fetch_weather("Beijing");
    if (ret == 0) {
        printk("Weather: %s, Code: %.2f\n", 
               g_weather.condition, g_weather.weather_code);
    }
}

// 日落时间获取测试
void test_sun_fetch(void) {
    int ret = fetch_sunset_time(39.9042, 116.4074);
    if (ret == 0) {
        printk("Sunrise: %.2f, Sunset: %.2f\n", 
               g_sun.sunrise_hour, g_sun.sunset_hour);
    }
}
```

---

## Phase 3：AI推理集成（2周）

### 目标
集成TFLite Micro，用神经网络替代规则引擎，实现平滑连续的智能调光。

### 任务清单

**PC端模型训练**
- [ ] 编写训练数据生成脚本（generate_training_data.py）
- [ ] 定义MLP模型结构（7→32→16→2）
- [ ] 训练模型并验证
- [ ] INT8量化
- [ ] 转换为C数组（xxd -i）

**Zephyr TFLM集成**
- [ ] 在west.yml中添加tflite-micro module
- [ ] 更新west workspace
- [ ] 创建ai_inference.c
- [ ] 实现ai_init()加载模型
- [ ] 实现ai_infer()推理函数

**特征工程**
- [ ] 实现hour_sin/hour_cos计算
- [ ] 实现sunset_proximity计算
- [ ] 实现特征向量组装

**AI控制循环**
- [ ] 创建AI推理线程
- [ ] 实现30秒定时推理
- [ ] 集成后处理（无人关灯、深夜限幅）
- [ ] 实现渐变过渡

**数据记录**
- [ ] 实现照明事件记录
- [ ] 记录时间、色温、光强、天气、人员
- [ ] 存储到PSRAM环形缓冲区

**验证与调优**
- [ ] 对比规则vs AI输出
- [ ] 验证天气补偿效果
- [ ] 验证日落渐变效果
- [ ] 调优推理间隔

### 验收标准

1. TFLM成功加载模型，推理正常
2. AI输出连续平滑，无阶梯跳变
3. 阴天自动增强光强，日落渐变过渡
4. 无人强制关灯，深夜限制亮度
5. 照明事件正确记录

### 关键代码验证

```c
// AI推理测试
void test_ai_inference(void) {
    struct lighting_features features = {
        .hour_sin = 0.5f,
        .hour_cos = 0.866f,
        .sunset_proximity = 0.75f,
        .sunrise_hour_norm = 0.28f,
        .sunset_hour_norm = 0.77f,
        .weather = 0.5f,  // 多云
        .presence = 1.0f  // 有人
    };
    
    uint16_t color_temp;
    uint8_t brightness;
    ai_infer(&features, &color_temp, &brightness);
    
    printk("AI Output: ColorTemp=%dK, Brightness=%d%%\n",
           color_temp, brightness);
}
```

---

## 关键里程碑

| 里程碑 | 预计时间 | 验收标准 |
|--------|----------|----------|
| UART控制灯具 | 第1周末 | 通过串口命令能控制灯具色温/光强 |
| PIR自动开关灯 | 第2周末 | 有人自动开灯，无人5分钟关灯 |
| WiFi数据获取 | 第3周末 | 能获取天气和日落时间 |
| 天气感知调光 | 第4周末 | 阴天增强光强，日落渐变调光 |
| AI推理上线 | 第6周末 | 神经网络驱动调光，输出平滑 |

---

## 风险与应对

| 风险 | 影响 | 应对措施 |
|------|------|----------|
| UART通信不稳定 | 控制失败 | 添加ACK/NAK机制，超时重试 |
| WiFi连接失败 | 无法获取天气 | 使用默认配置，定期重连 |
| HTTP请求超时 | 数据缺失 | 设置超时5秒，失败使用缓存 |
| TFLM内存不足 | 推理失败 | 减小模型（32→16神经元） |
| INT8量化精度损失 | 预测不准 | 混合量化或增加训练数据 |
| PIR误触发 | 频繁开关灯 | 软件消抖+持续检测 |

---

## 文件结构规划

```
prj/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── esp32s3_devkitc_procpu.overlay
├── src/
│   ├── main.c
│   ├── wifi_manager.c/.h
│   ├── http_client.c/.h
│   ├── uart_driver.c/.h
│   ├── gpio_pir.c/.h
│   ├── ai_inference.c/.h
│   ├── lighting_control.c/.h
│   └── model/
│       └── lighting_model_data.cc
└── scripts/
    ├── generate_training_data.py
    └── train_model.py
```
