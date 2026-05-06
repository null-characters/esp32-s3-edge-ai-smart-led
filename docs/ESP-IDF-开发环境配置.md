# ESP-IDF 开发环境配置指南

> 本文档记录项目的ESP-IDF开发环境配置，供团队成员参考。

## 环境信息

| 项目 | 版本/路径 |
|------|-----------|
| ESP-IDF版本 | v6.1-dev-4182-g47faecc3e4 |
| IDF路径 | `/Users/fengbing/esp/esp-idf` |
| Python版本 | 3.13.9 |
| 工具链 | xtensa-esp-elf-gcc 15.2.0 |
| 目标芯片 | ESP32-S3 |

## 快速激活环境

在编译项目前，必须先激活ESP-IDF环境：

```bash
# 激活ESP-IDF环境
source ~/esp/esp-idf/export.sh

# 进入项目目录
cd /Users/fengbing/git_prj/esp32-s3-edge-ai-smart-led/prj-v3

# 编译项目
idf.py build
```

## Shell配置（可选）

为避免每次手动source，可在 `~/.zshrc` 中添加别名：

```bash
# 编辑 ~/.zshrc，添加以下内容
alias get_idf='source ~/esp/esp-idf/export.sh'
```

之后只需执行 `get_idf` 即可激活环境。

## 项目依赖组件

`prj-v3` 项目使用的ESP-IDF组件（CMakeLists.txt）：

```cmake
REQUIRES unity esp_wifi esp_netif lwip nvs_flash esp_event esp_timer 
         driver esp_adc esp_driver_i2s esp_driver_uart esp_driver_ledc 
         esp_driver_rmt spiffs espressif__esp-sr
```

**重要变更记录**：
- 2026-05-06: 添加 `esp_adc` 组件依赖，用于电源板电压ADC读取

## 常用命令

| 命令 | 说明 |
|------|------|
| `idf.py build` | 编译项目 |
| `idf.py flash` | 烧录到设备 |
| `idf.py monitor` | 串口监视器 |
| `idf.py flash monitor` | 编译+烧录+监视 |
| `idf.py set-target esp32s3` | 设置目标芯片 |
| `idf.py menuconfig` | 配置菜单 |
| `idf.py fullclean` | 完全清理 |
| `idf.py size` | 查看内存占用 |

## 环境验证

验证环境是否正确配置：

```bash
source ~/esp/esp-idf/export.sh
idf.py --version    # 应显示 ESP-IDF v6.1
echo $IDF_PATH      # 应显示 /Users/fengbing/esp/esp-idf
```

## 环境安装（新机器）

如果需要在新机器上安装ESP-IDF环境：

```bash
# 克隆ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git

# 安装工具链
cd ~/esp/esp-idf
./install.sh esp32s3

# 激活环境
source ~/esp/esp-idf/export.sh
```

## 故障排查

### 编译失败：找不到idf.py

**原因**：未激活ESP-IDF环境

**解决**：执行 `source ~/esp/esp-idf/export.sh`

### 编译失败：找不到esp_adc组件

**原因**：CMakeLists.txt缺少组件依赖

**解决**：在 `main/CMakeLists.txt` 的 `REQUIRES` 中添加 `esp_adc`

### ADC API错误：adc_oneshot_init_unit未定义

**原因**：ESP-IDF v6.x API变更

**解决**：使用 `adc_oneshot_new_unit()` 替代旧API

## 参考链接

- [ESP-IDF官方文档](https://docs.espressif.com/projects/esp-idf/)
- [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)
- [ESP32-S3技术手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)