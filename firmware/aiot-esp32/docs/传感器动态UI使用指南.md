# 传感器动态UI使用指南

## 📋 概述

本文档说明如何使用动态传感器UI功能，让不同的ESP32-S3开发板在LCD上显示各自特定的传感器信息。

### ✨ 特性

- ✅ 根据板子配置自动生成传感器UI
- ✅ 支持最多4个传感器同时显示
- ✅ 每个板子可以定义自己的传感器列表
- ✅ 动态更新传感器数据
- ✅ 清晰的布局和颜色编码

---

## 🏗️ 架构设计

### 1. BSP层配置

每个板子在BSP文件中定义自己的传感器显示列表：

```c
// 传感器显示信息列表 - 用于LCD动态UI
static const bsp_sensor_display_info_t s_sensor_display_list[] = {
    {
        .name = "DHT11",        // 传感器名称
        .unit = "°C / %",       // 单位
        .gpio_pin = DHT11_GPIO_PIN
    },
    {
        .name = "DS18B20",
        .unit = "°C",
        .gpio_pin = DS18B20_GPIO_PIN
    }
};

// 板级信息中包含传感器列表
static const bsp_board_info_t s_board_info = {
    // ... 其他字段 ...
    .sensor_display_list = s_sensor_display_list,
    .sensor_display_count = sizeof(s_sensor_display_list) / sizeof(s_sensor_display_list[0])
};
```

### 2. 显示层接口

`simple_display` 组件提供以下接口：

```c
/**
 * @brief 初始化传感器UI
 * 根据板子配置动态创建传感器标签
 */
void simple_display_init_sensor_ui(simple_display_t *display, const struct bsp_board_info_t *board_info);

/**
 * @brief 更新传感器数值
 * @param sensor_index 传感器索引 (0-3)
 * @param value 数值字符串（如 "24.5°C / 60%"）
 */
void simple_display_update_sensor_value(simple_display_t *display, int sensor_index, const char *value);
```

---

## 🎨 UI布局

### 标准布局（240x240 LCD）

```
┌─────────────────────────────────┐
│ Y=10    ESP32-S3-DevKit         │  产品名称
│ Y=34    WiFi: Connected         │  WiFi状态
│ Y=58    MQTT: Connected         │  MQTT状态
│ Y=82    UUID: xxxx-xxxx...      │  设备UUID
│ Y=106   Uptime: 00:05:23        │  运行时间
├─────────────────────────────────┤
│ Y=140   DHT11:    24.5°C / 60%  │  ← 传感器1（动态）
│ Y=164   DS18B20:  23.8°C        │  ← 传感器2（动态）
│ Y=188   Rain:     无雨水        │  ← 传感器3（动态，Rain板）
│ Y=212   ...                     │  ← 传感器4（可选）
└─────────────────────────────────┘
```

### 颜色方案

- **传感器名称**：深蓝色 (#000080)
- **传感器数值**：黑色 (#000000)
- **状态信息**：黑色
- **UUID前缀**：深灰色

---

## 💻 使用示例

### 示例1：在main.c中初始化和更新

```c
#include "simple_display.h"
#include "bsp_interface.h"

// 全局变量
static simple_display_t *g_display = NULL;

void app_main(void) {
    // 1. 初始化LCD显示
    g_display = simple_display_init(/* LCD参数 */);
    
    // 2. 获取板级信息
    const bsp_board_info_t *board_info = bsp_get_board_info();
    
    // 3. 初始化传感器UI（根据板子配置动态创建标签）
    simple_display_init_sensor_ui(g_display, board_info);
    
    // 4. 在主循环或传感器任务中更新数据
    while (1) {
        // 读取DHT11 (传感器索引0)
        float temp = 24.5, hum = 60.0;
        char sensor0_text[32];
        snprintf(sensor0_text, sizeof(sensor0_text), "%.1f°C / %.0f%%", temp, hum);
        simple_display_update_sensor_value(g_display, 0, sensor0_text);
        
        // 读取DS18B20 (传感器索引1)
        float temp2 = 23.8;
        char sensor1_text[16];
        snprintf(sensor1_text, sizeof(sensor1_text), "%.1f°C", temp2);
        simple_display_update_sensor_value(g_display, 1, sensor1_text);
        
        vTaskDelay(pdMS_TO_TICKS(2000));  // 每2秒更新
    }
}
```

### 示例2：Rain板的传感器更新

```c
// ESP32-S3-Rain-01 板子有DHT11和雨水传感器
void update_rain_board_sensors(void) {
    // 传感器0: DHT11
    char dht_text[32];
    snprintf(dht_text, sizeof(dht_text), "%.1f°C / %.0f%%", 
             g_dht11_temp, g_dht11_hum);
    simple_display_update_sensor_value(g_display, 0, dht_text);
    
    // 传感器1: 雨水传感器
    const char *rain_status = g_is_raining ? "有雨水" : "无雨水";
    simple_display_update_sensor_value(g_display, 1, rain_status);
}
```

---

## 🔧 为新板子添加传感器UI

### 步骤1：在BSP中定义传感器列表

编辑 `firmware/aiot-esp32/boards/your-board/bsp_your_board.c`：

```c
// 定义传感器显示信息
static const bsp_sensor_display_info_t s_sensor_display_list[] = {
    {
        .name = "传感器1名称",
        .unit = "单位",
        .gpio_pin = GPIO_PIN_NUMBER
    },
    {
        .name = "传感器2名称",
        .unit = "单位",
        .gpio_pin = GPIO_PIN_NUMBER
    },
    // 最多4个传感器
};

// 在板级信息中添加
static const bsp_board_info_t s_board_info = {
    // ... 其他字段 ...
    .sensor_display_list = s_sensor_display_list,
    .sensor_display_count = sizeof(s_sensor_display_list) / sizeof(s_sensor_display_list[0])
};
```

### 步骤2：在main.c中初始化UI

```c
// 获取板级信息并初始化传感器UI
const bsp_board_info_t *board_info = bsp_get_board_info();
simple_display_init_sensor_ui(g_display, board_info);
```

### 步骤3：定期更新传感器数据

```c
// 创建FreeRTOS任务定期更新
void sensor_display_task(void *pvParameters) {
    const bsp_board_info_t *board_info = bsp_get_board_info();
    
    while (1) {
        for (int i = 0; i < board_info->sensor_display_count; i++) {
            // 读取传感器i的数据
            char value_text[32];
            // ... 读取和格式化数据 ...
            
            // 更新显示
            simple_display_update_sensor_value(g_display, i, value_text);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

## 📊 当前支持的板子

### ESP32-S3-Dev-01（标准版）

| 索引 | 传感器 | 显示内容 | 单位 |
|------|--------|----------|------|
| 0 | DHT11 | 温度/湿度 | °C / % |
| 1 | DS18B20 | 温度 | °C |

### ESP32-S3-Rain-01（雨水传感器版）

| 索引 | 传感器 | 显示内容 | 单位 |
|------|--------|----------|------|
| 0 | DHT11 | 温度/湿度 | °C / % |
| 1 | Rain | 雨水状态 | 状态 |

---

## ⚠️ 注意事项

1. **最多4个传感器**：`MAX_SENSOR_LABELS = 4`，超过的会被忽略

2. **Y坐标限制**：
   - 起始位置：Y=140
   - 行高：24px
   - 最后传感器：Y=212
   - LCD高度：240px（留28px底部空间）

3. **线程安全**：
   - 所有display函数内部已使用`lvgl_port_lock/unlock`
   - 可以从任意任务安全调用

4. **文本长度限制**：
   - 传感器名称：建议≤10个字符
   - 传感器数值：建议≤15个字符
   - 超长文本会被截断

5. **内存管理**：
   - 标签在`simple_display_init_sensor_ui`中创建
   - 在`simple_display_destroy`中自动释放
   - 重复调用`init_sensor_ui`会先清除旧标签

---

## 🔍 调试技巧

### 1. 查看传感器配置

```c
const bsp_board_info_t *board_info = bsp_get_board_info();
ESP_LOGI("MAIN", "板子: %s", board_info->board_name);
ESP_LOGI("MAIN", "传感器数量: %d", board_info->sensor_display_count);

for (int i = 0; i < board_info->sensor_display_count; i++) {
    const bsp_sensor_display_info_t *sensor = &board_info->sensor_display_list[i];
    ESP_LOGI("MAIN", "传感器%d: %s (GPIO%d) %s", 
             i, sensor->name, sensor->gpio_pin, sensor->unit);
}
```

### 2. 测试传感器UI

```c
// 初始化后立即测试
simple_display_init_sensor_ui(g_display, board_info);

// 显示测试数据
simple_display_update_sensor_value(g_display, 0, "TEST 1");
simple_display_update_sensor_value(g_display, 1, "TEST 2");
```

### 3. 日志输出

启用simple_display的日志：
```c
esp_log_level_set("SimpleDisplay", ESP_LOG_INFO);
```

---

## 📚 API参考

### bsp_sensor_display_info_t

```c
typedef struct {
    const char *name;      // 传感器名称，如"DHT11"
    const char *unit;      // 单位，如"°C / %"
    int gpio_pin;          // GPIO引脚号
} bsp_sensor_display_info_t;
```

### simple_display_init_sensor_ui()

- **功能**：根据板子配置创建传感器UI
- **参数**：
  - `display`: 显示句柄
  - `board_info`: 板级信息（包含传感器列表）
- **返回**：无
- **注意**：必须在LVGL初始化后调用

### simple_display_update_sensor_value()

- **功能**：更新传感器显示值
- **参数**：
  - `display`: 显示句柄
  - `sensor_index`: 传感器索引（0-3）
  - `value`: 数值字符串
- **返回**：无
- **线程安全**：是

---

## 🎯 最佳实践

1. **在启动完成后初始化UI**：
   ```c
   // 等待系统稳定
   vTaskDelay(pdMS_TO_TICKS(1000));
   simple_display_init_sensor_ui(g_display, board_info);
   ```

2. **合理的更新频率**：
   - DHT11：2-5秒
   - DS18B20：1-2秒
   - 雨水传感器：0.5-1秒

3. **格式化数值**：
   ```c
   // 好的格式
   "24.5°C / 60%"     // DHT11
   "23.8°C"           // DS18B20
   "有雨水"           // 雨水传感器（中文）
   "Rain"             // 雨水传感器（英文）
   
   // 避免过长
   "Temperature: 24.5°C, Humidity: 60%"  // ❌ 太长
   ```

4. **错误处理**：
   ```c
   if (sensor_read_error) {
       simple_display_update_sensor_value(g_display, i, "-- --");
   }
   ```

---

**更新日期**: 2025-11-15  
**维护者**: AIOT Admin System Team

