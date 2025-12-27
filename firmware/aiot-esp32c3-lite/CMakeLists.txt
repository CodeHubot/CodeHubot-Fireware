# ESP32-C3 AIOT精简版固件 - CMake配置文件
# 最小依赖，无LVGL，无OTA复杂功能

cmake_minimum_required(VERSION 3.16)

# 项目信息
set(PROJECT_NAME "aiot-esp32c3-lite")
set(PROJECT_VER "1.0.0")

# 包含ESP-IDF的cmake项目配置
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目
project(${PROJECT_NAME})

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项 - 优化大小
add_compile_options(
    -Os                 # 优化大小
    -ffunction-sections # 分离函数到独立section
    -fdata-sections     # 分离数据到独立section
)

# 链接选项 - 移除未使用的section
add_link_options(
    -Wl,--gc-sections   # 垃圾回收未使用的section
)

# 项目编译定义
add_compile_definitions(
    AIOT_VERSION="${PROJECT_VER}"
    AIOT_BOARD="ESP32-C3-Lite"
    AIOT_NO_OTA=1
    AIOT_NO_LVGL=1
)

message(STATUS "================================================")
message(STATUS "  AIOT ESP32-C3 Lite Firmware")
message(STATUS "  Version: ${PROJECT_VER}")
message(STATUS "  Target: ESP32-C3")
message(STATUS "  Flash: 4MB")
message(STATUS "  Features: WiFi + MQTT + Sensor (No OTA/LVGL)")
message(STATUS "================================================")

