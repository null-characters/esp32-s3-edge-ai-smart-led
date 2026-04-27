# ESP32-S3 边缘AI网关 - 文档索引

> 本目录包含项目各版本的规划文档，按版本组织

---

## 版本演进

| 版本 | 代号 | 框架 | 核心能力 | 状态 |
|------|------|------|---------|------|
| v1 | PIR+MLP | Zephyr | 人体存在感知 + MLP 调光 | ✅ 已归档 |
| v2 | 多模态 | Zephyr | 雷达+麦克风 + 多模态融合 | ✅ 已归档 |
| v3 | 语音交互 | ESP-IDF | 多模态 + 离线语音命令 | 🚧 开发中 |

---

## 文档索引

### 设计文档

| 文档 | 说明 |
|------|------|
| [01-hardware-overview.md](01-hardware-overview.md) | MCU 硬件能力与板级配置 |
| [02-system-architecture.md](02-system-architecture.md) | 系统架构设计 |
| [03-communication-protocol.md](03-communication-protocol.md) | 通信协议设计（Modbus/BLE） |
| [04-ai-inference.md](04-ai-inference.md) | AI 推理方案与 PSRAM 内存规划 |
| [05-implementation-roadmap.md](05-implementation-roadmap.md) | 分阶段实施路线图 |
| [06-meeting-room-lighting-mvp.md](06-meeting-room-lighting-mvp.md) | 会议室智能照明 MVP 方案 |

### 规划文档（按版本）

| 目录 | 版本 | 说明 |
|------|------|------|
| [plan-v1-pir-mlp/](plan-v1-pir-mlp/) | v1 | PIR + MLP 经典版本规划 |
| [plan-v2-multimodal/](plan-v2-multimodal/) | v2 | 多模态版本规划 |
| [plan-v3-voice-interaction/](plan-v3-voice-interaction/) | v3 | 语音交互版本规划 ⭐ |

---

## v3 规划文档索引

| 文档 | 说明 |
|------|------|
| [01-语音交互升级总体规划.md](plan-v3-voice-interaction/01-语音交互升级总体规划.md) | 架构设计、阶段规划、命令设计 |
| [03-任务追踪表.md](plan-v3-voice-interaction/03-任务追踪表.md) | 开发任务进度追踪 |

---

## 应用场景

### 场景一：会议室智能照明（MVP 推荐）

多维度输入（时间、天气、日落、人员）驱动的智能调光系统，适合作为边缘AI入门项目。

- **安全约束低**：调光错误无安全风险，AI 可直接执行
- **决策频率高**：每天多次触发，数据积累快
- **特征维度多**：时间、天气、日落、人员存在，适合神经网络
- **用户感知强**：效果立即可见，便于验证迭代

详见 [06-meeting-room-lighting-mvp.md](06-meeting-room-lighting-mvp.md)

### 场景二：应急照明系统

安全优先级高，AI 仅作辅助建议，核心决策由硬规则保证。

- **安全约束高**：断电照明是安全功能，AI 不能绕过底线
- **决策频率低**：断电事件稀少，数据积累慢
- **AI 价值有限**：电池健康预测、自适应调光等辅助功能

---

*最后更新：2026-04-27*
