# 最小任务单元清单 - Phase 4 仲裁机制

> 语音优先级仲裁与状态机实现

---

## T4.1 优先级仲裁器

### T4.1.1 仲裁器核心实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.1.1 |
| **预估工时** | 6h |
| **交付物** | `priority_arbiter.c/h` |

**核心逻辑**：
```c
arbiter_decision_t arbitrate(voice_cmd_t *voice, auto_decision_t *auto) {
    if (voice && voice->valid) return voice->decision;
    if (auto && auto->valid) return auto->decision;
    return default_decision;
}
```

**验收标准**：语音 > 自动 > 默认

---

### T4.1.2 状态机实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.1.2 |
| **预估工时** | 4h |
| **交付物** | `arbiter_state_machine.c` |

**状态定义**：
- `STATE_AUTO` - 自动控制模式
- `STATE_MANUAL` - 语音手动模式
- `STATE_TIMEOUT` - 超时恢复中

---

## T4.2 超时恢复机制

### T4.2.1 命令超时实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.1 |
| **预估工时** | 3h |

**交付物**：`command_timeout.c`

**验收标准**：30s 后自动恢复

### T4.2.2 NVS 状态持久化
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.2 |
| **预估工时** | 3h |

**交付物**：`state_storage.c`

**验收标准**：重启后恢复状态

---

## 里程碑

| 里程碑 | 预计完成 |
|--------|---------|
| M4.1 仲裁器 | 第3天 |
| M4.2 状态机 | 第5天 |
| M4.3 超时恢复 | 第7天 |

---

*创建日期：2026-04-27*