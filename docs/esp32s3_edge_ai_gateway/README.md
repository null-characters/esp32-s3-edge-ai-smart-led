# ESP32-S3 边缘AI网关 - 脑暴方案

> 状态：脑暴/想法阶段  
> 日期：2026-04-23  
> 硬件：ESP32-S3 N16R8 (WROOM-1 模组)  
> RTOS：Zephyr RTOS

## 文档索引

| 文档 | 说明 |
|------|------|
| [01-hardware-overview.md](01-hardware-overview.md) | MCU硬件能力与Zephyr板级配置 |
| [02-system-architecture.md](02-system-architecture.md) | 系统架构设计 |
| [03-communication-protocol.md](03-communication-protocol.md) | 通信协议设计（Modbus/BLE） |
| [04-ai-inference.md](04-ai-inference.md) | AI推理方案与PSRAM内存规划 |
| [05-implementation-roadmap.md](05-implementation-roadmap.md) | 分阶段实施路线图 |
| [06-meeting-room-lighting-mvp.md](06-meeting-room-lighting-mvp.md) | 会议室智能照明 MVP 方案 |

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
