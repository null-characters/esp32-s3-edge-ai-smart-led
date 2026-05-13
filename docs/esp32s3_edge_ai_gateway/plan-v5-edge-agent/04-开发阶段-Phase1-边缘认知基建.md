# 最小任务单元清单 - Phase 1 边缘认知基建

> X99 + Tesla P4 硬件组装与 AI 服务部署

---

## T1.1 硬件组装

### T1.1.1 主板+CPU+内存组装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.1 |
| **任务名称** | 主板+CPU+内存组装 |
| **预估工时** | 2h |
| **前置任务** | 硬件到货验收 |

**硬件清单**：
- X99 主板（原芯组）
- E5-2680v4 CPU
- DDR4 16G × 2 内存条

**执行步骤**：
```bash
# 1. 安装 CPU 到主板 LGA2011 插槽
# 2. 安装内存到 DDR4 插槽（建议插槽 1、3）
# 3. 连接 CPU 供电线（8pin）
# 4. 短接开机针脚测试启动
```

**验收标准**：
- [ ] 主板自检通过（蜂鸣器 1 短声）
- [ ] 内存识别 32GB
- [ ] BIOS 正常显示 CPU 信息

---

### T1.1.2 Tesla P4 显卡安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.2 |
| **任务名称** | Tesla P4 显卡安装 |
| **预估工时** | 2h |
| **前置任务** | T1.1.1 |

**硬件清单**：
- Tesla P4 8G 显卡
- P4 专用风扇
- HDMI 半高亮机卡

**执行步骤**：
```bash
# 1. 安装 Tesla P4 到 PCIe x16 插槽（插到底、扣上卡扣即可）
# 2. 安装 P4 专用涡轮风扇（接主板 4pin PWM 接口）
# 3. 安装 HDMI 半高亮机卡（用于 BIOS 初始化显示）
# 4. ⚠️ P4 无外接供电线，TDP 75W 完全由 PCIe 插槽供电
```

**验收标准**：
- [ ] 显卡风扇正常转动
- [ ] BIOS 识别显卡
- [ ] 显卡温度显示正常

---

### T1.1.3 NVMe SSD 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.3 |
| **任务名称** | 三星 981 1T NVMe SSD 安装 |
| **预估工时** | 1h |
| **前置任务** | T1.1.1 |

**验收标准**：
- [ ] BIOS 识别 NVMe 设备
- [ ] 容量显示 1TB

---

### T1.1.4 电源+散热+机箱组装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.4 |
| **任务名称** | 电源+散热+机箱整机组装 |
| **预估工时** | 2h |
| **前置任务** | T1.1.2, T1.1.3 |

**验收标准**：
- [ ] 整机正常启动
- [ ] 所有风扇正常转动
- [ ] 机箱前面板接线正常

---

### T1.1.5 BIOS 配置验证
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.1.5 |
| **任务名称** | BIOS 配置验证与优化 |
| **预估工时** | 1h |
| **前置任务** | T1.1.4 |

**BIOS 配置项**：
```
- Hyper-Threading: Enabled
- VT-d: Enabled
- Above 4G Decoding: Enabled
- PCIe Speed: Auto
- Fan Control: Auto
```

**验收标准**：
- [ ] CPU 温度 < 60°C（待机）
- [ ] GPU 温度 < 50°C（待机）
- [ ] 内存频率 2133MHz

---

## T1.2 系统部署

### T1.2.1 Ubuntu Server 22.04 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.1 |
| **任务名称** | Ubuntu Server 22.04 LTS 安装 |
| **预估工时** | 2h |
| **前置任务** | T1.1.5 |

**执行步骤**：
```bash
# 1. 制作启动 U 盘（Rufus / balenaEtcher）
# 2. 安装 Ubuntu Server 22.04
# 3. 分区方案：EFI 512MB, / 100GB, /home 剩余
# 4. 安装 OpenSSH Server
```

**验收标准**：
- [ ] 系统正常启动
- [ ] SSH 远程登录成功
- [ ] `lsb_release -a` 显示 22.04

---

### T1.2.2 Docker + Docker Compose 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.2 |
| **任务名称** | Docker 环境搭建 |
| **预估工时** | 2h |
| **前置任务** | T1.2.1 |

**执行步骤**：
```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
sudo apt install docker-compose-plugin
```

**验收标准**：
- [ ] `docker run hello-world` 成功
- [ ] Docker Compose v2 安装成功

---

### T1.2.3 NVIDIA Driver + CUDA 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.3 |
| **任务名称** | NVIDIA 驱动与 CUDA 安装 |
| **预估工时** | 4h |
| **前置任务** | T1.2.2 |

**执行步骤**：
```bash
sudo apt install nvidia-driver-535
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.0-1_all.deb
sudo dpkg -i cuda-keyring_1.0-1_all.deb
sudo apt update && sudo apt install cuda
nvidia-smi
```

**验收标准**：
- [ ] `nvidia-smi` 显示 Tesla P4
- [ ] CUDA 版本 12.x
- [ ] 驱动版本 535+

---

### T1.2.4 网络配置（静态IP）
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.4 |
| **任务名称** | 静态 IP 网络配置 |
| **预估工时** | 1h |
| **前置任务** | T1.2.1 |

**验收标准**：
- [ ] 静态 IP 配置成功
- [ ] 网关可达
- [ ] DNS 解析正常

---

### T1.2.5 运维监控配置
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.2.5 |
| **任务名称** | Docker 自愈 + Prometheus 监控配置 |
| **预估工时** | 2h |
| **前置任务** | T1.2.2 |

**执行步骤**：
```bash
# 1. 所有 Docker 服务添加 restart: unless-stopped
# 2. Agent 添加 healthcheck（30s 间隔，3 次重试）
# 3. 部署 Node Exporter + Prometheus
docker run -d --name node_exporter --net=host prom/node-exporter
docker run -d --name prometheus -p 9090:9090 \
  -v ./prometheus.yml:/etc/prometheus/prometheus.yml prom/prometheus

# 4. 配置 GPU 温度监控 cron
(crontab -l 2>/dev/null; echo "*/5 * * * * nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader >> /var/log/gpu_temp.log") | crontab -
```

**验收标准**：
- [ ] Docker 崩溃后自动重启
- [ ] Prometheus 抓取指标正常
- [ ] GPU 温度日志正常

---

## T1.3 LLM 部署

### T1.3.1 Ollama 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.1 |
| **任务名称** | Ollama 服务安装 |
| **预估工时** | 2h |
| **前置任务** | T1.2.3 |

**执行步骤**：
```bash
curl -fsSL https://ollama.com/install.sh | sh
sudo systemctl enable ollama
sudo systemctl start ollama
```

**验收标准**：
- [ ] `ollama --version` 正常
- [ ] 服务运行在 11434 端口

---

### T1.3.2 Qwen2.5-7B-GGUF 模型下载
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.2 |
| **任务名称** | Qwen2.5-7B 量化模型下载 |
| **预估工时** | 4h |
| **前置任务** | T1.3.1 |

**执行步骤**：
```bash
ollama pull qwen2.5:7b
ollama run qwen2.5:7b "你好，请介绍一下你自己"
```

**验收标准**：
- [ ] 模型下载成功（约 4.5GB）
- [ ] 推理响应正常
- [ ] GPU 利用率 > 0%

---

### T1.3.3 模型推理性能测试
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.3 |
| **任务名称** | LLM 推理性能基准测试 |
| **预估工时** | 2h |
| **前置任务** | T1.3.2 |

**验收标准**：
- [ ] TTFT（首字响应） < 500ms
- [ ] 吞吐量 > 20 tokens/s
- [ ] GPU 显存占用 < 6GB

---

### T1.3.4 API 接口验证
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.3.4 |
| **任务名称** | Ollama RESTful API 验证 |
| **预估工时** | 1h |
| **前置任务** | T1.3.3 |

**验收标准**：
- [ ] `/api/tags` 返回模型列表
- [ ] `/api/generate` 推理正常
- [ ] `/api/chat` 对话正常

---

## T1.4 STT/TTS 部署

### T1.4.1 Faster-Whisper 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.4.1 |
| **任务名称** | Faster-Whisper STT 服务安装 |
| **预估工时** | 2h |
| **前置任务** | T1.2.3 |

**验收标准**：
- [ ] 服务运行在 8001 端口
- [ ] 中文识别准确率 > 90%

---

### T1.4.2 Piper TTS 安装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.4.2 |
| **任务名称** | Piper TTS 中文语音服务安装 |
| **预估工时** | 2h |
| **前置任务** | T1.2.2 |

**验收标准**：
- [ ] 中文语音合成正常
- [ ] 合成延迟 < 200ms
- [ ] 语音自然流畅

---

### T1.4.3 RESTful API 封装
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.4.3 |
| **任务名称** | STT/TTS RESTful API 封装 |
| **预估工时** | 4h |
| **前置任务** | T1.4.1, T1.4.2 |

**验收标准**：
- [ ] `/stt` 接口正常
- [ ] `/tts` 接口正常
- [ ] API 响应延迟 < 500ms

---

## T1.5 Agent Backend

> **架构说明**：Agent Backend (FastAPI) 直接承载 WebSocket 音频流接口（端口 :8080），不另设独立的 WebSocket 服务进程。FastAPI 原生支持 WebSocket upgrade，减少一次内部 HTTP 中继。

### T1.5.1 Agent Backend 项目搭建（含 WebSocket）
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.5.1 |
| **任务名称** | Agent Backend 项目搭建（含 WebSocket 端点） |
| **预估工时** | 3h |
| **前置任务** | T1.3.4, T1.4.3 |

**项目结构**：
```
agent_backend/
├── app/
│   ├── main.py           # FastAPI 入口 + WebSocket route (/ws/audio/{device_id})
│   ├── config.py
│   ├── routers/
│   │   ├── voice.py      # REST: /stt, /tts
│   │   └── mqtt.py       # MQTT 消息处理
│   ├── services/
│   │   ├── llm.py        # Ollama 调用 + JSON Schema 校验
│   │   ├── stt.py        # Faster-Whisper 封装
│   │   ├── tts.py        # Piper TTS 封装
│   │   └── conversation.py  # 会话管理
│   └── models/
│       └── schemas.py    # Pydantic 模型
├── Dockerfile
├── requirements.txt
└── docker-compose.yml
```

**验收标准**：
- [ ] 项目结构创建成功
- [ ] FastAPI 服务启动成功，端口 :8080
- [ ] WebSocket `/ws/audio/{device_id}` 端点正常
- [ ] 健康检查接口正常

---

### T1.5.2 系统级 Prompt 设计
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.5.2 |
| **任务名称** | 家电小精灵人设 Prompt 设计 |
| **预估工时** | 4h |
| **前置任务** | T1.5.1 |

**验收标准**：
- [ ] Prompt 模板定义完整
- [ ] JSON 输出格式稳定
- [ ] 人设风格一致

---

### T1.5.3 Function Calling 模块
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.5.3 |
| **任务名称** | Function Calling JSON 指令生成模块 |
| **预估工时** | 6h |
| **前置任务** | T1.5.2 |

**验收标准**：
- [ ] JSON 输出格式正确
- [ ] 异常处理完善
- [ ] Action 解析正确

---

### T1.5.4 会话管理模块
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.5.4 |
| **任务名称** | ConversationManager 多轮会话管理 |
| **预估工时** | 4h |
| **前置任务** | T1.5.2 |

**核心逻辑**：
```python
class ConversationManager:
    """管理多轮对话历史，支持上下文关联"""
    
    def __init__(self, max_history: int = 10):
        self.max_history = max_history
        self.sessions = {}  # device_id → messages
    
    def get_or_create_session(self, device_id: str) -> List:
        if device_id not in self.sessions:
            self.sessions[device_id] = []
        return self.sessions[device_id]
    
    def add_message(self, device_id: str, role: str, content: str):
        session = self.get_or_create_session(device_id)
        session.append({"role": role, "content": content})
        if len(session) > self.max_history:
            session.pop(0)  # 保留最近 N 轮
    
    def build_messages(self, device_id: str, system_prompt: str) -> List:
        return [{"role": "system", "content": system_prompt}] + \
               self.get_or_create_session(device_id)
```

**验收标准**：
- [ ] 多轮对话上下文关联正确
- [ ] 历史长度不超过 N 轮
- [ ] 会话隔离（不同 device_id 互不影响）

---

### T1.5.5 管线测试脚本
| 属性 | 内容 |
|------|------|
| **任务ID** | T1.5.5 |
| **任务名称** | wav → STT → LLM → TTS 端到端管线测试 |
| **预估工时** | 4h |
| **前置任务** | T1.5.3, T1.5.4 |

**验收标准**：
- [ ] 管线延迟 < 3s
- [ ] 输出音频正常
- [ ] 多轮对话上下文传递正确

---

## 任务依赖图

```
T1.1.1 ─── T1.1.2 ─── T1.1.4 ─── T1.1.5
    │         │
T1.1.3 ──────┘
    
T1.1.5 ─── T1.2.1 ─── T1.2.2 ─── T1.2.3 ─── T1.2.4
              └── T1.2.5
                          │
         ┌────────────────┼────────────────┐
         ↓                ↓                ↓
    T1.3.1 ── T1.3.2 ── T1.3.3 ── T1.3.4     T1.4.1 ── T1.4.2 ── T1.4.3
         ↓                                    ↓
         └────────────────── T1.5.1 ── T1.5.2 ──── T1.5.3 ── T1.5.4 ── T1.5.5
```

---

## 里程碑

| 里程碑 | 完成任务 | 预计完成 |
|--------|---------|---------|
| M1.1 硬件组装 | T1.1.1 - T1.1.5 | 第1周 |
| M1.2 系统部署 + 监控 | T1.2.1 - T1.2.5 | 第1周 |
| M1.3 LLM 部署 + 测试 | T1.3.1 - T1.3.4 | 第1-2周 |
| M1.4 STT/TTS | T1.4.1 - T1.4.3 | 第1-2周 |
| M1.5 Agent Backend（含WS+会话） | T1.5.1 - T1.5.5 | 第2周 |

---

*创建日期：2026-05-13*