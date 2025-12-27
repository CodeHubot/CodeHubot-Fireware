# ESP32-S3 DevKit Rain 板子配置说明

## 概述

ESP32-S3 DevKit Rain 是基于 ESP32-S3 DevKit 的扩展版本，新增了雨水传感器支持。

## 主要特性

- **传感器**：
  - DHT11 温湿度传感器（GPIO35）
  - DS18B20 防水温度传感器（GPIO39）
  - **雨水传感器（GPIO4）** ⭐ 新增

- **控制端口**：
  - 4个LED（GPIO42, 41, 37, 36）
  - 2个继电器（GPIO1, 2）
  - 2个舵机（GPIO48, 40）

## 使用方法

### 1. 切换板子类型

在 `firmware/aiot-esp32/main/main.c` 中修改板子引用：

**标准版：**
```c
#include "../boards/esp32-s3-devkit/bsp_esp32_s3_devkit.h"
#include "../boards/esp32-s3-devkit/board_config.h"
```

**Rain版本（含雨水传感器）：**
```c
#include "../boards/esp32-s3-devkit-rain/bsp_esp32_s3_devkit_rain.h"
#include "../boards/esp32-s3-devkit-rain/board_config.h"
```

### 2. 修改BSP注册调用

在 `main.c` 的 `app_main()` 函数中：

**标准版：**
```c
bsp_esp32_s3_devkit_register();
bsp_esp32_s3_devkit_print_config();
```

**Rain版本：**
```c
bsp_esp32_s3_devkit_rain_register();
bsp_esp32_s3_devkit_rain_print_config();
```

### 3. 添加雨水传感器支持

在 `main.c` 中添加：

```c
#include "rain_sensor.h"   // 雨水传感器驱动

// 全局变量
static bool g_rain_sensor_initialized = false;
static rain_sensor_data_t g_rain_sensor_data = {0};

// 初始化（在系统启动成功后）
rain_sensor_config_t rain_config = {
    .data_pin = RAIN_SENSOR_GPIO_PIN,
    .pull_up_enable = true,
    .debounce_ms = 50
};
esp_err_t rain_init_ret = rain_sensor_init(&rain_config);
if (rain_init_ret == ESP_OK) {
    g_rain_sensor_initialized = true;
}

// 读取和发送（在传感器读取循环中）
if (g_rain_sensor_initialized) {
    rain_sensor_read(&g_rain_sensor_data);
    // 发送到MQTT...
}
```

## BSP实现文件

由于BSP实现文件较大，Rain版本的BSP实现文件（`bsp_esp32_s3_devkit_rain.c`）可以基于标准版本复制并修改：

1. 复制 `bsp_esp32_s3_devkit.c` 为 `bsp_esp32_s3_devkit_rain.c`
2. 将所有函数名从 `bsp_esp32_s3_devkit_*` 改为 `bsp_esp32_s3_devkit_rain_*`
3. 在传感器初始化函数中添加雨水传感器初始化代码

## 产品配置

对应的产品编码：`ESP32-S3-Rain-01`

数据库中的产品配置应包含：
- DHT11_temperature
- DHT11_humidity  
- DS18B20_temperature
- **RAIN_SENSOR** ⭐ 新增

## 注意事项

1. GPIO4 用于雨水传感器，确保不与其它功能冲突
2. 雨水传感器需要上拉电阻（代码中已启用内部上拉）
3. 传感器读取包含50ms防抖处理
4. 数据上报频率：每10秒一次

