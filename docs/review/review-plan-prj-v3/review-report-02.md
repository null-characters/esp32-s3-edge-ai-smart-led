# Code Review Report 02 - 音频处理与语音交互

**审查日期**: 2026-04-29  
**审查范围**: prj-v3/main/src/audio/ 和 prj-v3/main/src/voice/  
**审查人**: Code Reviewer Agent

---

## 综合评分

| 模块 | 评分 | 说明 |
|------|------|------|
| audio_afe.c | 8.0/10 | 良好的ESP-SR集成，内存管理合理 |
| audio_pipeline.c | 7.5/10 | 状态机设计清晰，缺少错误恢复 |
| mfcc.c | 7.0/10 | 算法正确，静态缓冲区占用大 |
| aec_pipeline.c | 8.0/10 | 良好的AEC实现，资源管理完善 |
| audio_router.c | 7.5/10 | 功能简洁，线程安全 |
| command_handler.c | 7.5/10 | 命令处理完整，场景预设清晰 |
| tts_engine.c | 8.0/10 | 良好的TTS封装，异步播放支持 |
| vad_detector.c | 7.5/10 | VAD实现完整，拖尾处理合理 |
| voice_commands.c | 7.5/10 | MultiNet封装良好 |
| wake_word.c | 7.0/10 | 功能完整，缺少线程安全保护 |

**总体评分: 7.6/10**

---

## 问题汇总

### Critical (严重问题) - 1个

#### 1. [Critical] vad_detector.c:345-347 - 访问私有结构体成员

```c
if (g_vad.vad_handle && g_vad.vad_handle->trigger) {
    duration_ms = g_vad.vad_handle->trigger->speech_len * VAD_FRAME_LENGTH_MS;
}
```

**问题**: 直接访问 ESP-SR VAD 内部结构体成员 `vad_handle->trigger->speech_len`，这违反了封装原则，且 ESP-SR API 变更时会导致编译失败。

**建议**: 使用 ESP-SR 提供的公开 API 获取语音时长，或联系乐鑫确认是否有官方接口。

---

### Major (主要问题) - 8个

#### 1. [Major] mfcc.c:23-24 - 静态缓冲区占用大量内存

```c
static float s_pre_emph_buf[MFCC_SAMPLE_RATE];                    /**< 预加重缓冲 64KB */
static float s_frames_buf[MFCC_NUM_FRAMES][MFCC_FRAME_SIZE];      /**< 分帧缓冲 51KB */
```

**问题**: 静态缓冲区占用约 115KB 内存（64KB + 51KB），对于嵌入式系统负担较大。且这些缓冲区始终占用内存，即使 MFCC 未使用。

**建议**: 
- 改为动态分配，在 `mfcc_init()` 时分配，`mfcc_deinit()` 时释放
- 或使用 PSRAM 存储这些大缓冲区

---

#### 2. [Major] mfcc.c:142-143 - 栈上分配大数组

```c
float spectrum[MFCC_FFT_SIZE];      // 2048 * 4 = 8KB
float power[MFCC_FFT_SIZE / 2 + 1]; // 1025 * 4 = 4KB
float mel_spectrum[MFCC_MEL_FILTERS]; // 较小
float mfcc[MFCC_NUM_COEFFS];        // 较小
```

**问题**: 每帧处理时在栈上分配约 12KB 数据，对于嵌入式系统可能导致栈溢出。

**建议**: 将这些缓冲区移至静态或动态分配的结构体中。

---

#### 3. [Major] wake_word.c - 缺少线程安全保护

```c
static struct {
    bool initialized;
    bool running;
    bool awake;
    float threshold;
    // ...
} wake_state = {0};

// wake_word_is_awake() 直接读取 wake_state.awake 无锁保护
bool wake_word_is_awake(void)
{
    if (!wake_state.initialized) {
        return false;
    }
    return wake_state.awake;  // 无锁访问
}
```

**问题**: `wake_state.awake` 等状态变量在多任务环境下无锁保护，可能导致数据竞争。

**建议**: 添加 mutex 保护或使用原子操作。

---

#### 4. [Major] audio_pipeline.c:122 - 超时时间硬编码

```c
if ((get_timestamp_ms() - g_pipe.last_wake_time_ms) > 5000) {
```

**问题**: 命令等待超时时间 5000ms 硬编码，无法动态调整。

**建议**: 将超时时间作为配置参数，支持运行时调整。

---

#### 5. [Major] audio_afe.c:154 - 静态变量在任务函数中

```c
static void audio_afe_task(void *arg)
{
    // ...
    static uint32_t loop_count = 0;  // 静态变量
    if (++loop_count % 1000 == 0) {
```

**问题**: 任务函数内的静态变量在多实例场景下会共享，且影响代码可读性。

**建议**: 将计数器移至状态结构体中。

---

#### 6. [Major] tts_engine.c:362-364 - 文本截断无警告

```c
tts_play_request_t request;
strncpy(request.text, text, sizeof(request.text) - 1);
request.text[sizeof(request.text) - 1] = '\0';
```

**问题**: 当输入文本超过 127 字符时被静默截断，用户无感知。

**建议**: 添加截断警告日志，或返回错误码让调用者处理。

---

#### 7. [Major] command_handler.c:475-478 - 返回未保护的状态指针

```c
const system_state_t* command_handler_get_state(void)
{
    return &g_state;
}
```

**问题**: 直接返回全局状态指针，调用者可能在无锁情况下访问/修改状态。

**建议**: 
- 改为拷贝状态结构体
- 或在文档中明确说明调用者需要加锁

---

#### 8. [Major] audio_afe.c:248 - strdup 内存泄漏风险

```c
if (config->wakenet_model) {
    g_state.afe_cfg->wakenet_model_name = strdup(config->wakenet_model);
}
```

**问题**: `strdup` 分配的内存需要在 deinit 时释放，当前代码有释放逻辑但错误处理路径可能遗漏。

**建议**: 确保所有错误路径都正确释放 strdup 分配的内存。

---

### Minor (次要问题) - 15个

#### 1. [Minor] audio_afe.c:203 - I2S 位深硬编码

```c
esp_err_t ret = i2s_init(config->sample_rate, 32); /* INMP441 使用 32-bit */
```

**建议**: 将位深作为配置参数，提高灵活性。

---

#### 2. [Minor] audio_pipeline.c - 缺少 deinit 时的状态重置

```c
void audio_pipeline_deinit(void)
{
    // ...
    g_pipe.initialized = false;
    // 其他状态字段未重置
}
```

**建议**: 使用 memset 清零整个结构体。

---

#### 3. [Minor] aec_pipeline.c:188 - 未使用的函数

```c
aec_get_mode_string(AEC_MODE_SR_LOW_COST)
```

**问题**: 如果该函数不存在或未声明，会导致编译错误。

**建议**: 确认 API 存在或移除调用。

---

#### 4. [Minor] mfcc.c:104-107 - FFT 初始化失败仅警告

```c
esp_err_t ret = dsps_fft2r_init_fc32(NULL, MFCC_FFT_SIZE);
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "ESP-DSP FFT init failed, using fallback");
    // 继续执行，但无 fallback 实现
}
```

**建议**: 实现 fallback 或返回错误。

---

#### 5. [Minor] vad_detector.c:176-177 - 去除 const 不安全

```c
int16_t *samples_ptr = (int16_t *)samples;
vad_state_t esp_vad_state = vad_process_with_trigger(g_vad.vad_handle, samples_ptr);
```

**问题**: 强制去除 const 可能导致意外修改。

**建议**: 确认 ESP-SR API 是否真的需要非 const 指针，或使用局部拷贝。

---

#### 6. [Minor] voice_commands.c:53-54 - 类型不匹配

```c
char *model_name = config->model_name ? (char *)config->model_name : "mn2_cn";
```

**问题**: 强制去除 const，可能导致意外修改。

**建议**: 使用 const char* 类型的变量。

---

#### 7. [Minor] wake_word.c:77 - 任务循环中无必要延时

```c
vTaskDelay(1);
```

**问题**: 每次循环延时 1 tick，可能影响实时性。

**建议**: 移除此延时，依赖 I2S 读取阻塞。

---

#### 8. [Minor] tts_engine.c:29 - 栈大小可能不足

```c
#define TTS_TASK_STACK_SIZE    4096
```

**问题**: TTS 播放任务栈大小 4KB，如果 TTS 库使用较多栈空间可能不足。

**建议**: 监控栈使用情况，必要时增大。

---

#### 9. [Minor] audio_router.c:68 - 默认路由目标

```c
g_router.current_target = AUDIO_ROUTE_TFLM;  /* 默认路由到 TFLM */
```

**问题**: 初始路由到 TFLM，但 VAD 状态为 SILENCE，逻辑上应该一致。

**建议**: 确认初始状态是否合理。

---

#### 10. [Minor] command_handler.c:21-52 - 场景预设表使用指定初始化器

```c
static const scene_preset_t g_scene_presets[] = {
    [CMD_SCENE_FOCUS - CMD_SCENE_BASE] = { ... },
    // ...
};
```

**问题**: 如果命令 ID 不连续，会导致数组中有未初始化的空洞。

**建议**: 添加运行时边界检查或改用 switch-case 映射。

---

#### 11. [Minor] audio_afe.c:388 - 任务栈大小硬编码

```c
BaseType_t ret = xTaskCreatePinnedToCore(
    audio_afe_task,
    "audio_afe",
    16 * 1024,  /* 16KB 栈 (ESP-SR AFE需要较大栈空间) */
```

**建议**: 将栈大小作为配置参数。

---

#### 12. [Minor] aec_pipeline.c:156 - 延迟帧数硬编码

```c
g_aec.ref_delay_frames = 2;  /* 默认 2 帧 (~64ms) 延迟补偿 */
```

**建议**: 将延迟帧数作为配置参数，支持运行时校准。

---

#### 13. [Minor] vad_detector.c:198 - 语音概率估算粗糙

```c
float voice_prob = (esp_vad_state == VAD_SPEECH) ? 0.8f : 0.1f;
```

**问题**: 语音概率简单估算，不够精确。

**建议**: 如果 ESP-SR 提供概率 API，使用实际概率值。

---

#### 14. [Minor] tts_engine.c:448 - 清空队列可能丢失数据

```c
xQueueReset(g_tts.play_queue);
```

**问题**: 停止时清空队列，可能导致已排队但未播放的内容丢失。

**建议**: 考虑等待队列播放完成或提供配置选项。

---

#### 15. [Minor] 所有文件 - 缺少单元测试

**建议**: 为各模块添加单元测试，特别是状态机和命令处理逻辑。

---

## 安全风险报告

### 1. 内存安全
| 风险 | 级别 | 位置 |
|------|------|------|
| 静态大缓冲区 | 高 | mfcc.c:23-24 |
| 栈上大数组 | 中 | mfcc.c:142-147 |
| 文本截断无警告 | 低 | tts_engine.c:362 |

### 2. 并发安全
| 风险 | 级别 | 位置 |
|------|------|------|
| 状态变量无锁保护 | 高 | wake_word.c:30-41 |
| 返回未保护指针 | 中 | command_handler.c:475 |
| 任务函数静态变量 | 低 | audio_afe.c:154 |

### 3. API 使用
| 风险 | 级别 | 位置 |
|------|------|------|
| 访问私有结构体成员 | 高 | vad_detector.c:345 |
| 去除 const | 低 | vad_detector.c:176, voice_commands.c:53 |

---

## 性能分析

### 1. 实时性评估

| 模块 | 预估延迟 | 说明 |
|------|---------|------|
| AFE 处理 | ~10ms | 基于 ESP-SR AFE 块大小 |
| VAD 检测 | ~30ms | 基于 VAD 帧长度 |
| MFCC 提取 | ~32ms | 32 帧 @ 10ms/帧 |
| 唤醒词检测 | ~10ms | WakeNet 延迟 |

**总延迟**: 约 50-80ms，满足实时语音交互需求。

### 2. 内存占用

| 模块 | 静态内存 | 动态内存 |
|------|---------|---------|
| MFCC | ~115KB | 0 |
| AFE | ~0 | ~200KB (ESP-SR) |
| AEC | ~0 | ~50KB |
| TTS | ~0 | ~100KB |
| **总计** | ~115KB | ~350KB |

### 3. CPU 占用

| 模块 | 预估 CPU | 核心绑定 |
|------|---------|---------|
| audio_afe_task | ~30% | Core 1 |
| wake_word_task | ~20% | Core 1 |
| tts_play_task | ~10% | 未绑定 |

---

## 改进建议

### 高优先级

1. **修复 vad_detector.c 访问私有成员问题**
   - 联系乐鑫确认官方 API
   - 或移除该功能

2. **优化 MFCC 内存使用**
   - 改为动态分配
   - 将大缓冲区移至 PSRAM

3. **为 wake_word.c 添加线程安全保护**

### 中优先级

4. **配置参数化**
   - 超时时间、栈大小、延迟帧数等

5. **添加截断警告**
   - TTS 文本截断时记录警告

6. **改进状态指针返回方式**
   - 使用拷贝或加锁保护

### 低优先级

7. **移除任务函数中的静态变量**
8. **添加单元测试**
9. **统一错误处理风格**

---

## ESP-SR 集成评估

### 优点
- 正确使用 WakeNet/MultiNet/VAD/AEC 模块
- 合理的任务优先级和核心绑定
- 完善的回调机制

### 改进空间
- 部分私有 API 访问需要确认兼容性
- 模型加载失败时的 fallback 机制

---

## 审查状态

- [x] 代码阅读完成
- [x] 问题识别完成
- [x] 报告生成完成
- [ ] 问题修复待定
- [ ] 回归测试待定

---

## 附录：文件统计

| 文件 | 行数 | 函数数 | 全局变量 |
|------|------|--------|---------|
| audio_afe.c | 576 | 15 | 2 |
| audio_pipeline.c | 318 | 9 | 1 |
| mfcc.c | 269 | 13 | 1 |
| aec_pipeline.c | 392 | 12 | 1 |
| audio_router.c | 188 | 8 | 1 |
| command_handler.c | 536 | 13 | 3 |
| tts_engine.c | 503 | 17 | 1 |
| vad_detector.c | 372 | 12 | 1 |
| voice_commands.c | 266 | 14 | 1 |
| wake_word.c | 251 | 10 | 2 |
| **总计** | **3671** | **123** | **14** |
