# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(prj-v3)

# SPIFFS 镜像生成 (包含 AI 模型)
# 模型文件: spiffs_data/models/*.tflite
spiffs_create_partition_image(spiffs spiffs_data FLASH_IN_PROJECT)
