# 最小任务单元清单 - Phase 5 集成测试

> 端到端功能与性能验证

---

## T5.1 功能测试

### T5.1.1 唤醒词测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.1.1 |
| **任务名称** | WakeNet 唤醒词识别率测试 |
| **预估工时** | 4h |
| **前置任务** | Phase 4 完成 |

**测试方法**：
```bash
# 测试脚本
for i in {1..100}; do
    # 播放唤醒词音频
    play wake_word.wav
    
    # 检测是否唤醒
    timeout 5 grep -q "Wake word detected" /var/log/esp32.log
    
    if [ $? -eq 0 ]; then
        echo "Test $i: PASS" >> test_results.txt
    else
        echo "Test $i: FAIL" >> test_results.txt
    fi
    
    sleep 2
done

# 统计通过率
pass_count=$(grep -c "PASS" test_results.txt)
echo "Pass rate: ${pass_count}%"
```

**测试环境**：
- 安静环境（< 40dB）
- 正常环境（40-60dB）
- 嘈杂环境（> 60dB）

**验收标准**：
- [ ] 安静环境识别率 > 98%
- [ ] 正常环境识别率 > 95%
- [ ] 嘈杂环境识别率 > 85%

---

### T5.1.2 语音交互测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.1.2 |
| **任务名称** | 端到端语音交互测试 |
| **预估工时** | 6h |
| **前置任务** | T5.1.1 |

**测试用例**：
| 用例ID | 输入 | 期望输出 | Action |
|--------|------|---------|--------|
| TC001 | "小白天，把灯调亮一点" | "好的，已为您调亮" | brightness +20% |
| TC002 | "小白天，换成暖光" | "好的，已切换暖光" | color_temp 3000K |
| TC003 | "小白天，打开专注模式" | "专注模式已开启" | scene focus |
| TC004 | "小白天，现在几点了" | "现在是晚上 9 点" | 无 |
| TC005 | "小白天，关灯" | "好的，晚安" | turn_off |

**测试脚本**：
```python
import pytest
import requests
import json

class TestVoiceInteraction:
    @pytest.mark.parametrize("test_case", TEST_CASES)
    def test_voice_command(self, test_case):
        # 发送音频
        with open(test_case["audio_file"], "rb") as f:
            response = requests.post(
                "http://edge-server:8000/api/voice",
                files={"audio": f}
            )
        
        result = response.json()
        
        # 验证响应
        assert result["text"] == test_case["expected_text"]
        assert result["emotion"] in ["happy", "neutral", "apologetic"]
        assert result["actions"] == test_case["expected_actions"]
```

**验收标准**：
- [ ] 100 次测试通过率 > 90%
- [ ] 无崩溃、无内存泄漏

---

### T5.1.3 延迟测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.1.3 |
| **任务名称** | 端到端延迟测量 |
| **预估工时** | 3h |
| **前置任务** | T5.1.2 |

**测试方法**：
```python
import time
import pyaudio

def measure_latency():
    """测量从说话到 TTS 播放的延迟"""
    
    # 录音开始时间
    start_time = time.time()
    
    # 发送音频
    response = send_audio()
    
    # 收到 TTS 音频
    audio_data = response.content
    
    # 播放开始时间
    play_start = time.time()
    
    # 总延迟
    total_latency = play_start - start_time
    
    # TTFT（首字响应）
    ttft = response.headers.get("X-TTFT")
    
    return {
        "total_latency": total_latency,
        "ttft": float(ttft) if ttft else None
    }
```

**验收标准**：
- [ ] 端到端延迟 < 3s
- [ ] TTFT（首字响应） < 1.5s

---

## T5.2 性能测试

### T5.2.1 内存测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.2.1 |
| **任务名称** | ESP32 内存占用测试 |
| **预估工时** | 2h |
| **前置任务** | T5.1.2 |

**测试方法**：
```c
void memory_monitor_task(void *arg) {
    while (1) {
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Free PSRAM: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, "Min free heap: %lu bytes", esp_get_minimum_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

**验收标准**：
- [ ] 运行时峰值内存 < 1MB
- [ ] PSRAM 占用 < 500KB
- [ ] 无内存泄漏

---

### T5.2.2 固件大小测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.2.2 |
| **任务名称** | 固件大小与分区验证 |
| **预估工时** | 1h |
| **前置任务** | Phase 4 完成 |

**测试方法**：
```bash
# 编译固件
idf.py build

# 查看分区表
idf.py partition-table

# 查看 .bin 文件大小
ls -lh build/*.bin

# 分析各模块大小
idf.py size-components
```

**验收标准**：
- [ ] Flash 占用 < 4MB
- [ ] 分区表配置正确

---

### T5.2.3 服务器性能测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.2.3 |
| **任务名称** | 边缘服务器性能测试 |
| **预估工时** | 2h |
| **前置任务** | T5.1.2 |

**测试方法**：
```bash
# GPU 利用率监控
watch -n 1 nvidia-smi

# 内存占用
docker stats

# 并发测试（locust）
locust -f locustfile.py --host http://edge-server:8000
```

**验收标准**：
- [ ] GPU 显存占用 < 6GB
- [ ] 支持 5 个并发会话
- [ ] 无 OOM 错误

---

## T5.3 稳定性测试

### T5.3.1 24小时运行测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.3.1 |
| **任务名称** | 24 小时连续运行测试 |
| **预估工时** | 8h |
| **前置任务** | T5.1.2 |

**测试方法**：
```python
import time
import schedule

def hourly_test():
    """每小时执行一次语音交互测试"""
    result = run_voice_test()
    log_result(result)

# 定时任务
schedule.every().hour.do(hourly_test)

# 主循环
start_time = time.time()
while time.time() - start_time < 24 * 3600:
    schedule.run_pending()
    time.sleep(60)

# 检查日志
check_for_errors("/var/log/esp32.log")
check_for_errors("/var/log/edge-server.log")
```

**验收标准**：
- [ ] 24 小时无崩溃
- [ ] 内存无泄漏趋势
- [ ] 所有定时测试通过

---

### T5.3.2 断线重连测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.3.2 |
| **任务名称** | 网络断线重连测试 |
| **预估工时** | 2h |
| **前置任务** | T5.3.1 |

**测试方法**：
```bash
# 模拟网络断开
sudo iptables -A INPUT -p tcp --dport 1883 -j DROP
sudo iptables -A INPUT -p tcp --dport 8080 -j DROP

# 等待 30s
sleep 30

# 恢复网络
sudo iptables -D INPUT -p tcp --dport 1883 -j DROP
sudo iptables -D INPUT -p tcp --dport 8080 -j DROP

# 检查重连状态
grep "Reconnected" /var/log/esp32.log
```

**验收标准**：
- [ ] 断线检测正确
- [ ] 重连成功
- [ ] 状态恢复正常

---

### T5.3.3 异常输入测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T5.3.3 |
| **任务名称** | 异常输入与边界测试 |
| **预估工时** | 2h |
| **前置任务** | T5.3.1 |

**测试用例**：
| 用例 | 输入 | 期望行为 |
|------|------|---------|
| 静音输入 | 无声音频 | 返回 "请说话" |
| 噪音输入 | 白噪声 | 返回 "没听清" |
| 超长输入 | 60s 音频 | 正常截断处理 |
| 无效 JSON | 损坏 JSON | 返回默认响应 |
| 空指令 | 无 Action | 仅播放 TTS |

**验收标准**：
- [ ] 无崩溃
- [ ] 返回合理响应

---

## T5.4 集成测试报告

### 测试报告模板

| 测试项 | 目标值 | 实测值 | 状态 |
|--------|--------|--------|------|
| 唤醒识别率（安静） | > 98% | - | ⏳ |
| 唤醒识别率（正常） | > 95% | - | ⏳ |
| 唤醒识别率（嘈杂） | > 85% | - | ⏳ |
| 语音交互成功率 | > 90% | - | ⏳ |
| 端到端延迟 | < 3s | - | ⏳ |
| TTFT 首字响应 | < 1.5s | - | ⏳ |
| 内存峰值 | < 1MB | - | ⏳ |
| Flash 占用 | < 4MB | - | ⏳ |
| GPU 显存 | < 6GB | - | ⏳ |
| 24h 稳定性 | 无崩溃 | - | ⏳ |
| 断线重连 | 自动恢复 | - | ⏳ |
| 异常输入 | 无崩溃 | - | ⏳ |

---

## 任务依赖图

```
T5.1.1 ─── T5.1.2 ─── T5.1.3
              │
              ├── T5.2.1 ─── T5.2.2 ─── T5.2.3
              │
              └── T5.3.1 ─── T5.3.2 ─── T5.3.3
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M5.1 功能测试 | T5.1.1 - T5.1.3 | 第1周 |
| M5.2 性能测试 | T5.2.1 - T5.2.3 | 第1周 |
| M5.3 稳定性测试 | T5.3.1 - T5.3.3 | 第1周 |

---

*创建日期：2026-05-13*