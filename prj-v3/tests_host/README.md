# 主机端单元测试

> 无需硬件的纯逻辑测试框架

---

## 概述

| 属性 | 内容 |
|------|------|
| **测试框架** | Unity |
| **运行环境** | Linux/macOS/Windows |
| **测试数量** | 14 个用例 |
| **依赖** | CMake 3.22+, GCC/Clang |

---

## 测试用例

| 测试组 | 测试项 | 说明 |
|--------|--------|------|
| 模型测试 | test_model_loader_init | 模型加载初始化 |
| 模型测试 | test_model_size_validation | 模型大小验证 (< 100KB) |
| 仲裁测试 | test_arbiter_voice_priority | 语音优先级验证 |
| 仲裁测试 | test_arbiter_ttl_logic | TTL 锁定逻辑 |
| LED测试 | test_led_brightness_range | 亮度范围 0-100 |
| LED测试 | test_led_color_temp_range | 色温范围 2700-6500K |
| LED测试 | test_led_power_control | 开关控制 |
| 场景测试 | test_scene_presets | 5 种场景预设 |
| 雷达测试 | test_radar_frame_generation | 雷达帧生成 |
| 雷达测试 | test_radar_features | 雷达特征提取 |
| 音频测试 | test_audio_generation | 音频数据生成 |
| 音频测试 | test_mfcc_generation | MFCC 特征生成 |
| 命令测试 | test_voice_commands | 语音命令处理 |
| 内存测试 | test_memory_limits | 内存限制验证 |

---

## 运行方法

### 本地运行

```bash
# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
cmake --build .

# 运行测试
ctest --verbose
# 或直接运行
./host_tests
```

### CI 运行

GitHub Actions 自动执行：
- 编译检查
- 模型验证
- 代码 lint
- 文档检查

---

## 模拟层

### mock_esp.h

模拟 ESP-IDF API：
- FreeRTOS 队列/互斥锁
- SPIFFS 文件系统
- NVS 存储
- GPIO/PWM

### mock_hardware.h

模拟硬件设备：
- 音频数据生成 (正弦波 + 噪声)
- MFCC 特征生成
- 雷达帧生成
- LED 状态管理
- 场景预设

---

## 扩展测试

添加新测试：

```c
// test_host_main.c

void test_new_feature(void) {
    // 测试逻辑
    TEST_ASSERT_TRUE(condition);
}

// main() 中添加
RUN_TEST(test_new_feature);
```

---

*创建日期：2026-04-28*