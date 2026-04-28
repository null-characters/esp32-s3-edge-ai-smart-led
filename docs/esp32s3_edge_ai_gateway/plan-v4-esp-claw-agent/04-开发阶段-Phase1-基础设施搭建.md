# 开发阶段 - Phase 1: 基础设施搭建

> POC 验证：ESP-Claw 集成 + 大模型连通

---

## 阶段目标

- 在 ESP-IDF 中集成 ESP-Claw 框架
- 配置 Wi-Fi 自动联网与 HTTPS 通信
- 接入云端大模型 API，实现基础的"闲聊"交互

---

## 任务清单

### 1.1 项目初始化

- [ ] 从 v3 创建 feature/agent-v4 分支
- [ ] 添加 ESP-Claw 组件依赖
- [ ] 配置 sdkconfig（PSRAM、HTTPS）

### 1.2 网络层搭建

- [ ] 实现 Wi-Fi STA 自动连接
- [ ] 实现 HTTPS 客户端
- [ ] 封装大模型 API 调用

### 1.3 ESP-Claw 集成

- [ ] 实现 Agent 主循环
- [ ] 集成五维记忆系统
- [ ] 注册设备工具调用

### 1.4 基础交互验证

- [ ] 语音唤醒 → 录音上传
- [ ] 大模型响应 → TTS 播放

---

## 验收标准

| 检查项 | 标准 |
|--------|------|
| Wi-Fi 连接成功率 | > 95% |
| API 请求成功 | HTTPS 正常通信 |
| 闲聊交互 | 能进行基础对话 |

---

## 极简链路验证

```
ESP-SR 唤醒 → 录音 → 发送 ESP-Claw (云端 DeepSeek) → 云端下发控制指令 → 调整 LED
```

---

## 内存安全阈值

| 内存类型 | 安全阈值 | 检查方式 |
|---------|---------|---------|
| 内部 SRAM | > 50KB | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` |
| 外部 PSRAM | > 2MB | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` |

---

*创建日期：2026-04-28*
