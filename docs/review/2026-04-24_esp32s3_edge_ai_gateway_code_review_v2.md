## ESP32-S3 Edge AI Gateway 二次代码审查报告（Review v2）

- **审查日期**: 2026-04-24
- **基线**: 已合入并推送至 `main` 的提交 `b72e18f`
- **审查目的**: 对第一次审查（v1）提出的 P0/P1 整改进行“关闭项确认”，并识别仍未修复或新引入的风险项，评估是否达到“可交付”标准

---

## 总体结论（更新后的 Go/No-Go）

**结论: Conditional Go（有条件可交付）**。  
第一次审查中的核心 P0（线程生命周期语义、fade 阻塞/并发、presence 并发、AI 量化健壮性、UART 可靠发送）已基本落地并显著降低系统不确定性。  
但仍存在若干**可交付前必须明确/修复**的点，主要集中在：

- **P0**: `weather_api.c` 仍使用 `strcasestr`（可移植性/构建一致性风险）
- **P1**: 时钟/小数小时计算仍用 `k_uptime_get()%3600000` 近似，存在整点跳变与长期漂移问题（影响日落临近度与 AI hour 特征）
- **P1**: 各 service 的 `stop()` 通过 `k_thread_abort()` 强杀线程，缺少“退出握手/资源释放”策略（影响将来可维护性与稳定停机）
- **P1**: 单测 `native_sim` 构建仍依赖环境侧 DTC（本次未在仓库内提供可复用的安装/检查脚本）

---

## v1 关键项关闭情况（对照）

- **P0-线程生命周期缺陷**: ✅ 已修复  
  - 从 `K_THREAD_DEFINE` 改为显式 `*_start()/stop()`（`wifi_manager` / `weather` / `sun` / `sntp` / `ai_infer`），`main.c` 中按 init 成功启动。

- **P0-跨线程共享状态无同步**: ✅ 已修复（关键路径）  
  - `fade_control` 增加 `k_mutex` 并由 timer 推进；`presence_state` 增加 `k_mutex`。

- **P0-时区重复偏移风险**: ✅ 主要路径已收敛  
  - `sntp_time_sync` 移除 `TZ` 环境变量策略；`sun_iso_to_hour` 对 API UTC 做一次 +8。  
  - 仍建议后续把“时区语义”写入 README/设计文档，避免后续开发者再次引入重复偏移。

- **P0-AI 量化边界与异常值保护**: ✅ 已修复  
  - 增加 `scale > 0` 校验、NaN/Inf sanitize、量化饱和 clamp。

- **P1-UART 可靠性（ACK/重试）**: ✅ 已修复  
  - `lighting_set_*` 统一走 `uart_send_with_retry()`。

- **P1-fade 阻塞式实现**: ✅ 已修复  
  - 改为非阻塞 timer 推进。

---

## 仍未修复/需明确的风险项

### P0（交付前必须修）

1. **`weather_api.c` 使用 `strcasestr`（非 ISO C，移植性风险）**
   - **位置**: `prj/src/logic/weather_api.c` `weather_encode()`
   - **风险**: 在不同 libc/newlib 配置下可能缺失或行为差异，导致构建不一致或运行时崩溃。
   - **建议**:
     - 用项目内实现替换：例如 `bool contains_case_insensitive(const char* haystack,const char* needle)`（只处理 ASCII 即可满足当前关键词表）。
     - 同步补单测：覆盖大小写/空串/未知关键词。

### P1（建议尽快修）

1. **“小时小数部分”的计算仍不严谨，存在跳变/漂移**
   - **位置**:
     - `sun_get_sunset_proximity()` 使用 `current_hour + (k_uptime_get()%3600000)/3600000.0`
     - `ai_infer_thread_fn()` 同样构造 `hour`（影响 `hour_sin/cos` 与深夜限幅边界）
   - **风险**:
     - 整点时刻会出现 hour 小数从接近 1.0 跳回 0.0 的突变。
     - 长时间运行后，uptime 与真实时间不同步会引入偏差（尤其 SNTP 同步失败/间歇时）。
   - **建议**:
     - 统一以 `clock_gettime(CLOCK_REALTIME,...)` 获取秒与纳秒，计算“本地小时 + 小数”。
     - 将该逻辑集中在 `sntp_time_sync` 提供一个 `sntp_time_get_local_hour_f()`（float hour）。

2. **`*_stop()` 使用 `k_thread_abort()` 强杀线程**
   - **风险**:
     - 强杀线程可能中断在锁/网络调用中，未来增加资源管理后风险更高。
   - **建议**:
     - 改为“退出标志 + join 等待”的软停机模型（必要时设置超时后再 abort）。

3. **单测/构建环境依赖未工程化（DTC 缺失）**
   - **现状**: `native_sim` 构建需要 DTC，本机环境未安装会失败；仓库未提供自动检查/安装指引（除非另有未提交的 `docs/setup-*.md`）。
   - **建议**:
     - 增加 `docs/setup-windows-build.md` 或在现有文档中加入 DTC 安装与验证步骤（并提供 `where dtc` 例子）。

---

## 可交付判定建议

- **若定位为“PoC 演示交付”**: 当前版本可交付（Conditional Go）。
- **若定位为“工程交付/可长期稳定运行”**: 必须先修复本报告 **P0: strcasestr 移植性**，并完成至少一项 P1（小时小数计算统一）后再给 Go。

---

## 涉及文件（v2 重点关注）

- `prj/src/logic/weather_api.c`
- `prj/src/logic/sunrise_sunset.c`
- `prj/src/logic/ai_infer.c`
- `prj/src/drivers/sntp_time_sync.c`

