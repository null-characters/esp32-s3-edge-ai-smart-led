# 最小任务单元清单 - Phase 4 仲裁机制

> 语音优先级仲裁与 TTL 租约状态机实现

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

## T4.2 TTL 租约状态机 ⭐ 防止语音锁死

### T4.2.1 TTL 租约计时器
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.1 |
| **预估工时** | 4h |
| **交付物** | `ttl_lease.c/h` |

**核心逻辑**：
```c
#define TTL_DEFAULT_MS (2 * 60 * 60 * 1000)  // 默认 2 小时

typedef struct {
    uint64_t start_time;
    uint64_t ttl_ms;
    bool active;
} ttl_lease_t;

bool ttl_lease_is_expired(ttl_lease_t *lease) {
    if (!lease->active) return false;
    return (esp_timer_get_time() - lease->start_time) >= lease->ttl_ms;
}
```

**验收标准**：默认 2 小时后自动过期

---

### T4.2.2 环境变化监听
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.2 |
| **预估工时** | 3h |
| **交付物** | `env_watcher.c` |

**触发条件**：
- 雷达检测人离开 > 10 分钟

**验收标准**：环境变化触发语音锁释放

---

### T4.2.3 自动释放逻辑
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.3 |
| **预估工时** | 3h |
| **交付物** | `lock_release.c` |

**释放条件**：
1. TTL 过期（时间到）
2. 环境状态变化（人离开 > 10 分钟）
3. 用户说"恢复自动"

---

### T4.2.4 状态机实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.2.4 |
| **预估工时** | 4h |
| **交付物** | `arbiter_state_machine.c` |

**状态定义**：
- `STATE_AUTO` - 自动控制模式
- `STATE_MANUAL` - 语音手动模式（启动 TTL 租约）

**状态转换图**：
```
           ┌──────────────┐
           │   AUTO_MODE   │ ←── 初始状态
           └───────┬──────┘
                   │ 语音命令
                   ↓
           ┌──────────────┐
           │  MANUAL_MODE │ ←── 启动 TTL 租约
           └───────┬──────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
 TTL 过期    环境状态变化   用户说"恢复自动"
    │              │              │
    └──────────────┴──────────────┘
                   │
                   ↓
           ┌──────────────┐
           │   AUTO_MODE   │
           └──────────────┘
```

---

## T4.3 统一事件队列

### T4.3.1 队列消费者实现
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.3.1 |
| **预估工时** | 3h |
| **交付物** | `Lighting_Controller.c` |

**核心逻辑**：
```c
void lighting_controller_task(void *arg) {
    Lighting_Event_t event;
    while (1) {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY)) {
            // 处理统一事件
            apply_lighting_event(&event);
        }
    }
}
```

---

### T4.3.2 NVS 状态持久化
| 属性 | 内容 |
|------|------|
| **任务ID** | T4.3.2 |
| **预估工时** | 3h |
| **交付物** | `state_storage.c` |

**验收标准**：重启后恢复状态

---

## 里程碑

| 里程碑 | 预计完成 |
|--------|---------|
| M4.1 仲裁器 | 第2天 |
| M4.2 TTL 租约 | 第4天 |
| M4.3 状态机 | 第5天 |
| M4.4 统一队列 | 第7天 |

---

*创建日期：2026-04-27*
*版本：v2.0*
*最后更新：2026-04-28*