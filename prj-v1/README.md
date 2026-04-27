# Zephyr ESP32-S3 工程文件夹

此文件夹用于存放用户的 Zephyr 应用程序工程。

## 目录结构建议

```
prj/
├── my_app_1/           # 应用 1
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c
├── my_app_2/           # 应用 2
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c
└── README.md           # 本文件
```

## 编译示例

```bash
# 进入 Zephyr 环境
cd E:\zephyrproject
setup_env.cmd

# 编译应用到 prj/my_app_1
cd prj/my_app_1
west build -b esp32s3_devkitc/esp32s3/procpu

# 烧录到设备
west flash
```

## 支持的开发板

- `esp32s3_devkitc/esp32s3/procpu` - ESP32-S3-DevKitC (主核)
- `esp32s3_devkitm/esp32s3/procpu` - ESP32-S3-DevKitM (主核)
- `esp32s3_eye/esp32s3/procpu` - ESP32-S3-EYE (主核)

## 参考资源

- [Zephyr ESP32 文档](https://docs.zephyrproject.org/latest/boards/espressif/index.html)
- [ESP32-S3 DevKitC](https://docs.zephyrproject.org/latest/boards/espressif/esp32s3_devkitc/doc/index.html)
