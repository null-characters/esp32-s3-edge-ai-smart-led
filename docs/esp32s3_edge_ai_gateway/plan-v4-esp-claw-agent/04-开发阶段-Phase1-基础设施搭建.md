# 开发阶段 - Phase 1: 基础设施搭建

> POC 验证：ESP-Claw 集成 + 大模型连通 + 网络健壮性

---

## 阶段目标

1. 在 ESP-IDF 中集成 ESP-Claw 框架
2. 配置 Wi-Fi 自动联网与 HTTPS 通信
3. 接入云端大模型 API，实现基础的"闲聊"交互
4. 建立网络状态监控，为优雅降级做准备

---

## 技术风险关联

| 风险 | 本阶段应对 | 后续阶段 |
|------|-----------|---------|
| 内存压力 | sdkconfig 配置优化 | Phase 2 内存池 |
| 网络稳定性 | 网络状态监控 | Phase 4 优雅降级 |

---

## 任务清单

### 1.1 项目初始化

```
任务列表:
├── [ ] 创建 feature/agent-v4 分支
├── [ ] 复制 prj-v3 基础结构
├── [ ] 添加 ESP-Claw 组件依赖
├── [ ] 配置 sdkconfig
│   ├── PSRAM: Enable
│   ├── HTTPS: Enable
│   ├── WiFi: Enable
│   └── Lua: Enable (预留)
└── [ ] 验证编译通过
```

**sdkconfig 关键配置：**

```
# PSRAM 配置
CONFIG_ESP32C3_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_40M=y

# HTTPS 配置
CONFIG_ESP_HTTPS_ENABLE=y
CONFIG_ESP_TLS_MEM_ALLOC_MODE_PSRAM=y

# WiFi 配置
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32
```

### 1.2 网络层搭建

```
任务列表:
├── [ ] Wi-Fi STA 自动连接
│   ├── SSID/Password 配置
│   ├── 自动重连机制
│   └── 连接状态回调
├── [ ] HTTPS 客户端封装
│   ├── CA 证书配置
│   ├── 请求/响应封装
│   └── 超时处理
├── [ ] 大模型 API 封装
│   ├── DeepSeek API 对接
│   ├── 请求格式定义
│   └── 响应解析
└── [ ] 网络状态监控
    ├── Wi-Fi 连接状态
    ├── API 超时计数
    └── 状态变化事件
```

**网络层架构：**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          网络层架构                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │ Wi-Fi       │    │ HTTPS       │    │ API         │    │ 状态监控    │  │
│  │ Manager     │───►│ Client      │───►│ Wrapper     │───►│ Monitor     │  │
│  └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘  │
│         │                                                        │          │
│         │ 连接状态                                                │ 超时计数 │
│         │                                                        │          │
│         └────────────────────────────────────────────────────────┘          │
│                                        │                                    │
│                                        ▼                                    │
│                              ┌─────────────────┐                             │
│                              │ 降级触发器      │                             │
│                              │ (Phase 4 实现)  │                             │
│                              └─────────────────┘                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.3 ESP-Claw 集成

```
任务列表:
├── [ ] Agent 主循环实现
│   ├── 消息队列
│   ├── 事件循环
│   └── 任务调度
├── [ ] 五维记忆系统集成
│   ├── Profile 存储 (NVS)
│   ├── Preference 存储 (NVS)
│   ├── Fact 存储 (NVS)
│   ├── Rule 存储 (预留 Lua)
│   └── Context 缓存 (RAM)
└── [ ] 设备工具调用注册
    ├── set_lighting
    ├── get_sensor_data
    └── send_notify (占位)
```

**五维记忆存储策略：**

| 记忆类型 | 存储位置 | 容量限制 | 持久化 |
|---------|---------|---------|--------|
| Profile | NVS | <1KB | ✅ 重启保留 |
| Preference | NVS | <2KB | ✅ 重启保留 |
| Fact | NVS | <1KB | ✅ 重启保留 |
| Rule | PSRAM | <4KB/条 | ✅ 云端下发 |
| Context | RAM | <512B | ❌ 会话级 |

### 1.4 基础交互验证

```
任务列表:
├── [ ] 语音唤醒 → 录音
│   ├── ESP-SR 唤醒词
│   ├── I2S 录音启动
│   └── 录音缓冲管理
├── [ ] 录音上传（PCM 原始，Phase 2 优化）
│   ├── PCM 数据打包
│   └── HTTPS POST 上传
├── [ ] 大模型响应接收
│   ├── JSON 解析
│   └── 工具调用识别
└── [ ] 本地 TTS 播放（降级模式）
    ├── ESP-TTS 集成
    └── 固定短语播报
```

**基础交互流程（POC）：**

```
┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐
│ ESP-SR    │    │ I2S       │    │ HTTPS     │    │ DeepSeek  │
│ 唤醒      │───►│ 录音      │───►│ POST      │───►│ API       │
└───────────┘    └───────────┘    └───────────┘    └───────────┘
                                                      │
                                                      ▼
┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐
│ I2S       │    │ ESP-TTS   │    │ JSON      │◄──│ 响应      │
│ 播放      │◄───│ 合成      │◄───│ 解析      │    └───────────┘
└───────────┘    └───────────┘    └───────────┘
```

---

## 验收标准

| 检查项 | 标准 | 测试方法 |
|--------|------|---------|
| Wi-Fi 连接成功率 | >95% | 连续 100 次连接测试 |
| Wi-Fi 重连时间 | <5s | 断网后重连计时 |
| API 请求成功 | HTTPS 正常通信 | curl + 设备双验证 |
| 闲聊交互 | 能进行基础对话 | 10 轮对话测试 |
| 内存安全 | 内部 SRAM >50KB | heap_caps 检查 |

---

## 内存安全阈值

| 内存类型 | 安全阈值 | 检查方式 |
|---------|---------|---------|
| 内部 SRAM | > 50KB | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` |
| 外部 PSRAM | > 2MB | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` |

**内存检查代码：**

```c
void check_memory_health(void) {
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "Internal SRAM: %d KB", internal_free / 1024);
    ESP_LOGI(TAG, "PSRAM: %d KB", psram_free / 1024);
    
    assert(internal_free > 50 * 1024);  // > 50KB
    assert(psram_free > 2 * 1024 * 1024);  // > 2MB
}
```

---

## 极简链路验证

```
ESP-SR 唤醒 → 录音(5s) → 发送云端 STT → DeepSeek 处理 → 返回文本 → 本地 TTS

预期延迟: 8-12 秒（未优化）
目标延迟: <5 秒（Phase 2 优化后）
```

---

## 下阶段预告

Phase 2 将解决：
- **带宽黑洞**：PCM → OPUS 压缩，延迟削减 30%
- **TTS 雪崩**：云端流式 TTS，支持长文本

---

*维护者：null-characters*
*创建日期：2026-04-27*
*版本：v1.0*
