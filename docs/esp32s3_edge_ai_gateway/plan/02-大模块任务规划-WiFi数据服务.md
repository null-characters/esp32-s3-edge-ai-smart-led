# 大模块任务规划 - WiFi数据服务模块

## 模块概述

| 属性 | 说明 |
|------|------|
| 模块名称 | WiFi数据服务模块 |
| 所属阶段 | Phase 2 |
| 计划周期 | 2周 |
| 核心目标 | 通过WiFi获取天气和日出日落数据，实现环境感知调光 |

## 功能边界

### 包含功能
- WiFi STA模式连接管理
- SNTP时间同步
- HTTP客户端（天气API、日出日落API）
- 数据缓存管理
- 天气感知和日落渐变调光逻辑

### 不包含功能
- 蓝牙BLE通信
- AI神经网络推理
- 用户偏好学习
- 手机App交互

## 依赖关系

```
基础硬件通信模块 (Phase 1)
         │
         ▼
WiFi数据服务模块 (Phase 2) ──► 使用UART控制灯具
         │                         (已具备)
         ▼
AI推理系统模块 (Phase 3)
```

## 任务分解清单

### 任务组1：WiFi连接管理

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-001 | WiFi Kconfig配置 | 0.5d | HW-004 | CONFIG_WIFI=y等使能 |
| WF-002 | WiFi初始化 | 0.5d | WF-001 | 调用net_if_init正确 |
| WF-003 | WiFi连接管理 | 1d | WF-002 | 连接指定SSID，获取IP |
| WF-004 | 自动重连机制 | 0.5d | WF-003 | 断网后自动重连 |
| WF-005 | 连接状态查询 | 0.5d | WF-003 | 查询WiFi连接状态API |

### 任务组2：时间同步服务

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-006 | SNTP配置 | 0.5d | WF-003 | CONFIG_SNTP=y使能 |
| WF-007 | SNTP初始化 | 0.5d | WF-006 | 配置NTP服务器地址 |
| WF-008 | 时间同步 | 0.5d | WF-007 | 成功同步网络时间 |
| WF-009 | 时区处理 | 0.5d | WF-008 | UTC+8北京时间转换 |
| WF-010 | RTC更新 | 0.5d | WF-009 | 同步时间到系统RTC |

### 任务组3：HTTP客户端

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-011 | HTTP配置 | 0.5d | WF-003 | CONFIG_HTTP_CLIENT=y |
| WF-012 | DNS解析 | 0.5d | WF-011 | 域名解析为IP地址 |
| WF-013 | HTTP GET实现 | 1d | WF-012 | 发送GET请求，接收响应 |
| WF-014 | 响应缓冲区 | 0.5d | WF-013 | 分配HTTP响应缓冲区 |
| WF-015 | 超时处理 | 0.5d | WF-013 | 5秒超时，错误处理 |

### 任务组4：天气API集成

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-016 | wttr.in API封装 | 0.5d | WF-013 | 构建GET请求URL |
| WF-017 | 天气数据解析 | 1d | WF-016 | 解析文本响应 |
| WF-018 | 天气编码转换 | 0.5d | WF-017 | 文本→0-1编码 |
| WF-019 | 天气缓存结构 | 0.5d | WF-018 | weather_cache结构体 |
| WF-020 | 定时更新任务 | 0.5d | WF-019 | 1小时定时更新 |

### 任务组5：日出日落API集成

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-021 | sunrise-sunset API封装 | 0.5d | WF-013 | 带经纬度参数请求 |
| WF-022 | JSON解析 | 1d | WF-021 | 解析JSON响应 |
| WF-023 | 时间提取转换 | 0.5d | WF-022 | ISO8601→小时数 |
| WF-024 | 太阳数据缓存 | 0.5d | WF-023 | sun_cache结构体 |
| WF-025 | 定时更新任务 | 0.5d | WF-024 | 每天定时更新 |

### 任务组6：环境感知调光

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-026 | 日落临近度计算 | 0.5d | WF-010,024 | calculate_sunset_proximity函数 |
| WF-027 | 天气补偿算法 | 0.5d | WF-018 | 阴天光强增强 |
| WF-028 | 日落渐变算法 | 0.5d | WF-026 | 日落前渐变增强 |
| WF-029 | 增强调光引擎 | 0.5d | WF-027,028 | 融合天气+日落逻辑 |
| WF-030 | 联调测试 | 1d | WF-029 | 阴天/日落场景验证 |

### 任务组7：集成与测试

| 任务ID | 任务名称 | 工时 | 依赖 | 验收标准 |
|--------|----------|------|------|----------|
| WF-031 | WiFi管理线程 | 0.5d | WF-004,010,020,025 | 独立WiFi管理任务 |
| WF-032 | 主循环集成 | 0.5d | WF-029,031 | 集成到main循环 |
| WF-033 | 离线处理 | 0.5d | WF-032 | WiFi断开使用默认配置 |
| WF-034 | 功能测试 | 1d | WF-033 | 各场景调光正确 |

## 关键数据结构

### 天气缓存
```c
struct weather_cache {
    char condition[32];       // 天气状况
    float weather_code;       // 0=晴, 0.25=少云, 0.5=多云, 0.75=阴, 1=雨
    time_t last_update;       // 上次更新时间
    uint32_t update_interval; // 更新间隔(秒)
};
```

### 太阳数据缓存
```c
struct sun_cache {
    float sunrise_hour;       // 日出时间(0-24)
    float sunset_hour;        // 日落时间(0-24)
    time_t last_update;
    uint32_t update_interval; // 86400秒(1天)
};
```

## 关键接口定义

### WiFi管理接口
```c
int wifi_init(void);
int wifi_connect(const char *ssid, const char *password);
bool wifi_is_connected(void);
int wifi_reconnect(void);
```

### 时间服务接口
```c
int sntp_init(void);
int sntp_sync(void);
time_t get_timestamp(void);
struct tm* get_local_time(void);
```

### HTTP接口
```c
int http_get(const char *url, char *response_buf, size_t buf_size);
int fetch_weather(const char *city, struct weather_cache *out);
int fetch_sunset_time(float lat, float lon, struct sun_cache *out);
```

### 调光增强接口
```c
float calculate_sunset_proximity(float hour, float sunset_hour);
uint8_t apply_weather_boost(uint8_t base_brightness, float weather);
uint8_t apply_sunset_boost(uint8_t brightness, float sunset_proximity);
```

## 里程碑

- **M3** (Week 3末): WiFi连接成功，能获取天气和日落时间
- **M4** (Week 4末): 阴天自动增强光强，日落渐变调光正常工作

## 与Phase 1的衔接

```
Phase 1输出: rule_get_lighting_params(presence, hour, &ct, &br)
                   ↓
Phase 2增强: enhanced_get_lighting_params(presence, hour, weather, 
                                          sunset_prox, &ct, &br)
                   ↓
         uart_send_set_both(ct, br)  // 使用Phase 1的UART接口
```
