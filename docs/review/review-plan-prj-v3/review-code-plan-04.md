# Review Plan 04 - 控制逻辑与仲裁系统

## 概述
本计划审查 prj-v3 的核心控制逻辑和优先级仲裁系统，这是多模态决策的中央处理单元。

## 审查范围

### 文件列表 - 仲裁系统
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/arbitration/priority_arbiter.c` | 优先级仲裁器 | ~400 |
| `main/src/arbitration/env_watcher.c` | 环境状态监视器 | ~350 |
| `main/src/arbitration/ttl_lease.c` | TTL租约管理 | ~200 |

### 文件列表 - 控制模块
| 文件路径 | 说明 | 预估行数 |
|---------|------|---------|
| `main/src/control/Lighting_Controller.c` | 照明控制器 | ~500 |
| `main/src/led/status_led.c` | 状态LED控制 | ~150 |

### 相关头文件
- `main/include/priority_arbiter.h`
- `main/include/env_watcher.h`
- `main/include/ttl_lease.h`
- `main/include/Lighting_Controller.h`
- `main/include/Lighting_Event.h`
- `main/include/status_led.h`

## Graphify 社区关联
- **Community 3**: Lighting控制 (30 nodes, Cohesion: 0.06)
- **Community 12**: Priority Arbiter (17 nodes, Cohesion: 0.2)
- **Community 0**: Env Watcher (90 nodes, Cohesion: 0.05)

## 审查重点

### 1. 优先级仲裁逻辑
- [ ] 优先级定义是否合理
- [ ] 冲突解决策略是否正确
- [ ] 超时机制是否完善
- [ ] 状态机设计是否完整

### 2. 环境状态管理
- [ ] 状态转换逻辑是否正确
- [ ] 雷达数据融合是否合理
- [ ] 异常状态处理

### 3. TTL租约机制
- [ ] 租约创建/续约/过期逻辑
- [ ] 资源释放是否及时
- [ ] 并发访问安全

### 4. 照明控制
- [ ] 控制算法正确性
- [ ] 平滑过渡效果
- [ ] 场景切换逻辑

### 5. 并发安全
- [ ] 临界区保护
- [ ] 死锁风险评估
- [ ] 信号量使用规范

## 核心数据结构

### Priority Arbiter
```c
// 决策优先级 (从高到低)
1. MANUAL_OVERRIDE    // 手动控制
2. VOICE_COMMAND      // 语音命令
3. AI_INFERENCE       // AI推理
4. SCHEDULE           // 定时计划
5. ENV_DEFAULT       // 环境默认
```

### Env Watcher States
```c
// 环境状态
- PRESENT      // 人在
- ABSENT       // 人离开
- UNKNOWN      // 未知
```

## 审查输出
- [ ] 代码质量评分
- [ ] 发现问题列表（按严重程度分类）
- [ ] 改进建议
- [ ] 状态机完整性报告

## 状态
- [ ] 待审查
- [ ] 审查中
- [ ] 已完成
