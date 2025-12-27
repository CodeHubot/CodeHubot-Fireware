# AIOT ESP32-S3 DevKit Firmware
# ESP-IDF CMakeLists.txt for ESP32-S3 development board

cmake_minimum_required(VERSION 3.16)

# 设置项目名称和版本
set(PROJECT_VER "1.0.0")
set(PROJECT_NAME "aiot-esp32s3-firmware")

# 添加额外的组件目录 - 按照开发指南的分层架构
set(EXTRA_COMPONENT_DIRS 
    "drivers/sensors"
    "drivers/lcd" 
    "components/display"
    "components/ui"
)

# 包含ESP-IDF构建系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目
project(${PROJECT_NAME})