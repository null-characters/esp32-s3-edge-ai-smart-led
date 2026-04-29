# Review Report 03 - 多模态AI推理与雷达感知

**审查日期**: 2026-04-29  
**审查人**: Code Reviewer  
**审查范围**: model_loader.c, tflm_allocator.c, ld2410_driver.c

---

## 一、总体评分

| 模块 | 评分 | 说明 |
|------|------|------|
| model_loader.c | **8.0/10** | 设计良好，但存在占位符逻辑风险 |
| tflm_allocator.c | **7.5/10** | 简洁但缺少 deinit 和错误恢复机制 |
| ld2410_driver.c | **7.8/10** | 功能完整，但数据流和线程安全有问题 |
| **总体评分** | **7.8/10** | 代码质量良好，关键问题需修复 |

---

## 二、问题汇总

| 严重程度 | 数量 | 说明 |
|----------|------|------|
| **Critical** | 0 | 无致命问题 |
| **Major** | 6 | 需要修复的重要问题 |
| **Minor** | 10 | 建议改进的小问题 |

---

## 三、详细问题分析

### 3.1 model_loader.c

#### Major 问题

**[M-01] 占位符模型可能导致推理崩溃**
```c:96:105:prj-v3/main/src/multimodal/model_loader.c
model->data = heap_caps_malloc(model->size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
if (!model->data) {
    ESP_LOGE(TAG, "分配模型内存失败: %zu 字节", model->size);
    return ESP_ERR_NO_MEM;
}
memset(model->data, 0, model->size);
model->is_loaded = true;
```
**问题**: 当模型文件不存在时，使用全零占位符数据，但 TFLM 解析全零数据会崩溃。
**建议**: 
- 返回错误码 `ESP_ERR_NOT_FOUND` 而非假装加载成功
- 或提供有效的默认模型嵌入 Flash

---

**[M-02] SPIFFS 挂载失败时 format_if_mount_failed=true 会丢失数据**
```c:72:prj-v3/main/src/multimodal/model_loader.c
.format_if_mount_failed = true,
```
**问题**: 如果挂载失败会格式化分区，可能丢失已存储的模型文件。
**建议**: 生产环境应设为 `false`，仅在开发调试时使用 `true`。

---

**[M-03] 模型大小预定义与实际文件不匹配**
```c:30:prj-v3/main/src/multimodal/model_loader.c
.size = 12 * 1024,  /* 12KB (CNN) */
```
```c:142:prj-v3/main/src/multimodal/model_loader.c
model->size = bytes_read;  /* 实际大小 */
```
**问题**: 预定义的 size 仅用于占位符，实际加载后会更新。但 API `model_loader_get_info()` 返回的结构体中 size 在未加载时是错误的预定义值。
**建议**: 在头文件注释说明 size 字段的含义变化，或添加 `actual_size` 字段。

---

#### Minor 问题

**[m-01] 缺少模型版本校验**
- 建议: 加载后校验模型版本/架构是否匹配预期

**[m-02] 无并发保护**
- 问题: 多任务同时调用 `model_loader_load()` 可能冲突
- 建议: 添加互斥锁保护 `g_models` 数组

**[m-03] 分块读取 chunk_size 硬编码**
```c:122:prj-v3/main/src/multimodal/model_loader.c
size_t chunk_size = 4096;
```
- 建议: 定义为宏常量或根据模型大小动态调整

---

### 3.2 tflm_allocator.c

#### Major 问题

**[M-04] 缺少 deinit 函数，PSRAM 内存无法释放**
```c:1:60:prj-v3/main/src/multimodal/tflm_allocator.c
// 没有 tflm_allocator_deinit() 函数
```
**问题**: PSRAM 分配的 Arena 内存无法释放，影响资源管理和重启场景。
**建议**: 添加 `tflm_allocator_deinit()` 函数释放 PSRAM 内存。

---

**[M-05] Arena 大小硬编码，缺少动态调整能力**
```c:6:prj-v3/main/src/multimodal/tflm_allocator.h
#define TFLM_ARENA_SIZE (100 * 1024)  // 100KB
```
**问题**: 
- 100KB 对三个模型可能不够（sound_classifier CNN 需要较大 Arena）
- 无法根据实际模型需求调整
**建议**: 
- 提供配置选项或运行时探测所需 Arena 大小
- 参考 TFLM 的 `MicroInterpreter::GetArenaUsedBytes()` 确定实际需求

---

#### Minor 问题

**[m-04] 无 Arena 状态查询**
- 建议: 添加 `tflm_get_arena_used()` 返回已使用内存

**[m-05] 无内存碎片检测**
- 建议: 添加 Arena 健康检查 API

---

### 3.3 ld2410_driver.c

#### Major 问题

**[M-06] data_ready 标志从未被设置为 true**
```c:30:prj-v3/main/src/radar/ld2410_driver.c
static volatile bool data_ready = false;
```
```c:205:207:prj-v3/main/src/radar/ld2410_driver.c
if (!data_ready) {
    return -EAGAIN;
}
```
**问题**: `data_ready` 在初始化后始终为 `false`，没有任何代码将其设为 `true`。`ld2410_read_raw()` 永远返回 `-EAGAIN`。
**建议**: 
- 添加 UART 接收中断回调设置 `data_ready = true`
- 或在 `ld2410_get_features()` 中直接轮询 UART

---

#### Minor 问题

**[m-06] 滤波缓冲区初始化为零导致初始值偏差**
```c:189:191:prj-v3/main/src/radar/ld2410_driver.c
memset(distance_filter, 0, sizeof(distance_filter));
memset(velocity_filter, 0, sizeof(velocity_filter));
memset(energy_filter, 0, sizeof(energy_filter));
```
- 问题: 前 FILTER_WINDOW 次滤波结果偏低
- 建议: 使用第一次测量值填充整个缓冲区

**[m-07] filter_idx 全局共享导致多变量滤波冲突**
```c:49:prj-v3/main/src/radar/ld2410_driver.c
static int filter_idx = 0;
```
```c:93:94:prj-v3/main/src/radar/ld2410_driver.c
buffer[filter_idx] = new_value;
filter_idx = (filter_idx + 1) % FILTER_WINDOW;
```
- 问题: distance/velocity/energy 三个滤波器共用一个索引，导致写入错位
- 建议: 为每个滤波器维护独立索引

**[m-08] 历史缓冲区大小 60 秒可能过大**
```c:57:prj-v3/main/src/radar/ld2410_driver.h
#define RADAR_HISTORY_SIZE  60  /* 60秒历史数据 */
```
- 计算: `60 * sizeof(radar_features_t) ≈ 60 * 32 = 1920 字节`
- 建议: 根据实际需求评估，或改为环形缓冲动态分配

**[m-09] 运动周期硬编码**
```c:289:prj-v3/main/src/radar/ld2410_driver.c
*period = 0.2f;
```
- 问题: 没有实际计算运动周期
- 建议: 使用 FFT 或峰值检测计算真实周期

**[m-10] 呼吸/风扇检测阈值硬编码**
```c:314:319:prj-v3/main/src/radar/ld2410_driver.c
if (current_features.motion_period >= 0.1f && 
    current_features.motion_period <= 0.5f &&
    current_features.energy_norm < 0.3f &&
    fabsf(current_features.velocity_norm) < 0.2f) {
    return true;
}
```
- 建议: 将阈值定义为可配置参数或宏常量

---

## 四、架构与设计评估

### 4.1 TFLM 集成评估

| 项目 | 状态 | 说明 |
|------|------|------|
| 模型加载流程 | ✅ 正确 | SPIFFS 分块加载设计合理 |
| 内存分配策略 | ⚠️ 需改进 | Arena 大小固定，缺少动态探测 |
| 量化模型处理 | ✅ 支持 | INT8 量化模型注释明确 |
| 推理接口设计 | ⚠️ 缺失 | 未提供推理执行接口 |

### 4.2 内存管理评估

| 项目 | 状态 | 说明 |
|------|------|------|
| PSRAM 使用 | ✅ 正确 | 模型和 Arena 都使用 PSRAM |
| 内存碎片风险 | ✅ 低 | 静态分配为主 |
| 生命周期管理 | ⚠️ 不完整 | 缺少 deinit 函数 |
| 内存泄漏风险 | ✅ 低 | 模型卸载时正确释放 |

### 4.3 雷达数据处理评估

| 项目 | 状态 | 说明 |
|------|------|------|
| 数据解析正确性 | ✅ 正确 | 帧头查找和字段解析合理 |
| 特征提取算法 | ⚠️ 需改进 | 周期计算硬编码 |
| 数据滤波 | ⚠️ 有 bug | 滤波索引共享导致错位 |
| 噪声处理 | ✅ 基本完善 | 滑动平均滤波有效 |

---

## 五、安全风险评估

| 风险类型 | 风险等级 | 说明 |
|----------|----------|------|
| **内存安全** | 低 | 使用 heap_caps_malloc/free 正确 |
| **数据安全** | 低 | 无外部输入验证问题 |
| **崩溃风险** | 中 | 占位符模型会导致 TFLM 崩溃 |
| **资源耗尽** | 低 | 内存分配有错误处理 |

---

## 六、改进建议优先级

### 高优先级 (立即修复)

1. **修复 ld2410_driver.c 的 data_ready 标志** - 当前完全无法读取数据
2. **修复滤波索引共享问题** - 导致滤波结果错误
3. **占位符模型处理** - 返回错误而非假装成功

### 中优先级 (近期修复)

4. **添加 tflm_allocator_deinit()** - 完善资源生命周期
5. **SPIFFS format_if_mount_failed** - 生产环境设为 false
6. **Arena 大小配置化** - 支持动态调整

### 低优先级 (后续优化)

7. 模型版本校验
8. 运动周期真实计算
9. 滤波缓冲区初始化优化
10. 添加并发保护

---

## 七、代码质量统计

| 文件 | 行数 | 函数数 | 注释率 | 复杂度 |
|------|------|--------|--------|--------|
| model_loader.c | 289 | 10 | 25% | 低 |
| tflm_allocator.c | 60 | 4 | 20% | 低 |
| ld2410_driver.c | 335 | 10 | 15% | 中 |

---

## 八、审查结论

**整体评价**: 代码结构清晰，模块化设计良好。主要问题集中在：
1. `ld2410_driver.c` 的数据流设计缺陷（data_ready 未触发）
2. `tflm_allocator.c` 的生命周期不完整
3. `model_loader.c` 的占位符处理风险

**建议**: 先修复 Critical/Major 问题后再进行后续模块审查。雷达驱动需要重新设计数据接收机制（建议使用 UART 事件队列或 FreeRTOS 任务轮询）。

---

**下一步**: 审查完成后，建议进行单元测试验证：
- 模型加载/卸载流程测试
- 雷达数据解析测试（模拟帧数据）
- Arena 内存边界测试