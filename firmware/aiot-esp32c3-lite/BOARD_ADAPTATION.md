# ESP32-C3开发板原理图适配指南

## 📋 原理图分析结果

根据提供的原理图，这是一块完整的ESP32-C3开发板，包含以下模块：

### 硬件配置

**区域1: USBC (USB接口)**
- USB Type-C接口
- USB转串口芯片（用于程序下载和调试）
- 支持VBUS供电

**区域2: LDO (电源管理)**
- 芯片: ME6211C3305G
- 输出: 3.3V
- 电流: 最大500mA
- 用途: USB 5V转3.3V供电

**区域3: OLED (显示屏) ⚠️**
- OLED显示屏（推测128x64或128x32）
- 接口: I2C (SDA/SCL)
- 额外: 2个LED指示灯 (NLED1, NLED2)
- 重要: **当前固件不支持OLED**

**区域4: KEYS & LEDS (按键和LED)**
- LED: DR12 (4.7K), DR8 (4.7K)
- 按键: 需要在原理图中确认具体GPIO

**区域5: ESP32-C3 (主控芯片)**
- 芯片: ESP32-C3FH4 (QFN32, 5×5mm)
- Flash: 4MB (板载)
- 晶振: 40MHz
- GPIO扩展接口

**区域6: IO (扩展接口)**
- JP1: 10针扩展口 (GPIO3-GPIO10)
- JP2: 8针扩展口 (VUSB, GND, RXD0, TXD0, GPIO2, GPIO3, GPIO0)

## 🔍 GPIO映射分析

### 需要从原理图确认的GPIO

根据ESP32-C3的QFN32封装和原理图，需要确认以下GPIO：

| 功能 | 可能GPIO | 需要确认 | 优先级 |
|------|---------|---------|--------|
| OLED SDA | GPIO1 或 GPIO6 | ⚠️ 必须确认 | 高 |
| OLED SCL | GPIO0 或 GPIO7 | ⚠️ 必须确认 | 高 |
| LED1 (NLED1) | GPIO8 | ⚠️ 需确认 | 中 |
| LED2 (NLED2) | GPIO? | ⚠️ 需确认 | 中 |
| Boot按键 | GPIO9 | ⚠️ 需确认 | 高 |
| Reset按键 | EN引脚 | ⚠️ 需确认 | 高 |

### ESP32-C3 QFN32引脚参考

```
ESP32-C3FH4 (QFN32 5x5mm) 引脚定义:
- GPIO0-10: 通用GPIO
- GPIO18-21: 通用GPIO
- TX0/RX0: UART0 (通常用于USB转串口)
- EN: 复位引脚
- VCC_3V3: 电源
- GND: 地
```

## 🔧 固件适配方案

### 方案A: 最小适配（不使用OLED）⭐ 推荐快速测试

**优点:**
- 无需修改代码
- 可以立即测试基本功能
- 适合验证硬件

**步骤:**

1. **确认GPIO映射**
   
   编辑 `main/board_config.h`:
   ```c
   // 根据原理图修改以下GPIO定义
   #define LED1_GPIO_PIN       ?  // 确认NLED1的GPIO
   #define LED2_GPIO_PIN       ?  // 确认NLED2的GPIO（如果有）
   #define RELAY1_GPIO_PIN     ?  // 如果板子有继电器接口
   #define DHT11_GPIO_PIN      ?  // 如果板子有传感器接口
   #define BOOT_BUTTON_GPIO    9  // 通常是GPIO9
   ```

2. **编译和烧录**
   ```bash
   cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite
   ./build.sh build
   ./build.sh flash
   ```

3. **功能测试**
   - WiFi配网
   - MQTT通信
   - LED控制
   - 基本功能验证

### 方案B: 完整适配（添加OLED支持）

**优点:**
- 充分利用硬件
- 可以显示状态信息
- 更好的用户体验

**缺点:**
- 需要添加OLED驱动代码
- 固件会增加~30-50KB
- 增加开发时间

**步骤:**

#### 1. 确认OLED连接

需要从原理图确认：
- OLED使用的I2C地址（通常是0x3C或0x3D）
- SDA连接的GPIO
- SCL连接的GPIO
- 电源连接（VCC/GND）

#### 2. 添加OLED驱动

创建 `main/oled_driver.h`:
```c
#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include "driver/i2c.h"

// 根据实际原理图修改
#define OLED_I2C_ADDRESS    0x3C
#define OLED_SDA_GPIO       ?  // 需要确认
#define OLED_SCL_GPIO       ?  // 需要确认
#define OLED_WIDTH          128
#define OLED_HEIGHT         64  // 或32

// OLED初始化
esp_err_t oled_init(void);

// 显示文本
void oled_display_text(const char *text, int line);

// 清屏
void oled_clear(void);

// 显示状态信息
void oled_show_status(const char *wifi, const char *mqtt, float temp, float humi);

#endif
```

创建 `main/oled_driver.c`:
```c
#include "oled_driver.h"
#include "esp_log.h"

static const char *TAG = "OLED";

// I2C初始化
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA_GPIO,
        .scl_io_num = OLED_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  // 400kHz
    };
    
    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) return err;
    
    return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

// OLED初始化序列（SSD1306）
static const uint8_t init_cmds[] = {
    0xAE,  // Display OFF
    0xD5, 0x80,  // Set display clock
    0xA8, 0x3F,  // Set multiplex ratio
    0xD3, 0x00,  // Set display offset
    0x40,  // Set start line
    0x8D, 0x14,  // Enable charge pump
    0x20, 0x00,  // Set memory mode
    0xA1,  // Set segment remap
    0xC8,  // Set COM output scan direction
    0xDA, 0x12,  // Set COM pins
    0x81, 0xCF,  // Set contrast
    0xD9, 0xF1,  // Set pre-charge period
    0xDB, 0x40,  // Set VCOMH
    0xA4,  // Display all ON
    0xA6,  // Normal display
    0xAF,  // Display ON
};

esp_err_t oled_init(void) {
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed");
        return ret;
    }
    
    // 发送初始化命令
    for (int i = 0; i < sizeof(init_cmds); i++) {
        uint8_t cmd[] = {0x00, init_cmds[i]};
        i2c_master_write_to_device(I2C_NUM_0, OLED_I2C_ADDRESS, 
                                   cmd, 2, pdMS_TO_TICKS(100));
    }
    
    oled_clear();
    ESP_LOGI(TAG, "OLED initialized");
    return ESP_OK;
}

void oled_clear(void) {
    // 发送清屏命令
    uint8_t clear_data[OLED_WIDTH] = {0};
    for (int page = 0; page < OLED_HEIGHT / 8; page++) {
        uint8_t cmd[] = {0x00, 0xB0 + page, 0x00, 0x10};
        i2c_master_write_to_device(I2C_NUM_0, OLED_I2C_ADDRESS, 
                                   cmd, 4, pdMS_TO_TICKS(100));
        
        i2c_master_write_to_device(I2C_NUM_0, OLED_I2C_ADDRESS, 
                                   clear_data, OLED_WIDTH, pdMS_TO_TICKS(100));
    }
}

void oled_display_text(const char *text, int line) {
    // 简化实现，需要添加字库
    ESP_LOGI(TAG, "Display: %s at line %d", text, line);
    // TODO: 实现文本显示
}

void oled_show_status(const char *wifi, const char *mqtt, 
                     float temp, float humi) {
    char status[128];
    snprintf(status, sizeof(status), 
             "WiFi: %s\nMQTT: %s\nTemp: %.1fC\nHumi: %.1f%%",
             wifi, mqtt, temp, humi);
    ESP_LOGI(TAG, "Status:\n%s", status);
    // TODO: 在OLED上显示
}
```

#### 3. 修改main.c集成OLED

在 `main/main.c` 中添加：
```c
#include "oled_driver.h"

// 在 app_main() 中初始化
void app_main(void) {
    // ... 现有代码 ...
    
    // 初始化OLED
    #ifdef ENABLE_OLED
    if (oled_init() == ESP_OK) {
        oled_display_text("AIOT C3 Lite", 0);
        oled_display_text("Initializing...", 1);
    }
    #endif
    
    // ... 现有代码 ...
}

// 在系统监控任务中更新OLED
static void system_monitor_task(void *pvParameters) {
    while (1) {
        // ... 现有代码 ...
        
        #ifdef ENABLE_OLED
        // 更新OLED显示
        oled_show_status(
            g_wifi_connected ? "Connected" : "Disconnected",
            g_mqtt_connected ? "Connected" : "Disconnected",
            g_sensor_data.temperature,
            g_sensor_data.humidity
        );
        #endif
        
        vTaskDelay(pdMS_TO_TICKS(SYSTEM_MONITOR_INTERVAL_MS));
    }
}
```

#### 4. 修改CMakeLists.txt

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c" "oled_driver.c"  # 添加oled_driver.c
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        mqtt
        driver
        esp_timer
        app_update
        esp_system
)

# 添加编译选项（如果需要OLED支持）
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    AIOT_NO_OTA=1
    # ENABLE_OLED=1  # 取消注释以启用OLED
)
```

#### 5. 固件大小估算

添加OLED支持后的固件大小：
```
基础固件:        ~400KB
OLED驱动:        ~30KB
字库(可选):      ~20KB
总计:           ~450KB / 3MB  (仍然很充裕)
```

## 📝 需要从原理图确认的信息清单

请提供或确认以下信息：

### 必需信息（高优先级）
- [ ] OLED的I2C地址 (通常是0x3C或0x3D)
- [ ] OLED_SDA连接的GPIO
- [ ] OLED_SCL连接的GPIO  
- [ ] LED1 (NLED1) 连接的GPIO
- [ ] LED2 (NLED2) 连接的GPIO
- [ ] Boot按键连接的GPIO
- [ ] OLED显示屏型号和分辨率 (128x64 或 128x32)

### 可选信息（中优先级）
- [ ] JP1扩展接口的详细定义
- [ ] JP2扩展接口的详细定义
- [ ] 是否有外接传感器接口（DHT11等）
- [ ] 是否有继电器或其他控制接口
- [ ] 其他未使用的GPIO功能

### 原理图补充
如果可以提供：
- [ ] OLED部分的详细原理图
- [ ] 按键部分的详细原理图
- [ ] LED部分的详细原理图

## 🚀 推荐的测试流程

### 第一阶段：基础功能测试（不使用OLED）
1. 编译基础固件
2. 烧录到板子
3. 测试USB连接和串口通信
4. 测试WiFi配网
5. 测试MQTT通信
6. 测试LED控制（如果GPIO正确）

### 第二阶段：OLED功能开发
1. 确认OLED的GPIO和I2C地址
2. 添加OLED驱动代码
3. 测试OLED初始化
4. 测试基本显示功能
5. 集成到主程序

### 第三阶段：完整功能测试
1. WiFi配网 + OLED提示
2. MQTT状态 + OLED显示
3. 传感器数据 + OLED显示
4. 按键交互 + OLED反馈
5. 长时间稳定性测试

## 💡 其他建议

### 1. 硬件测试工具
建议准备：
- USB转TTL串口线（调试用）
- 逻辑分析仪（I2C调试）
- 万用表（测量电压）

### 2. 软件调试
- 使用 `idf.py monitor` 查看日志
- 使用 `idf.py menuconfig` 配置选项
- 打开详细日志级别调试

### 3. 原理图改进建议
如果还在设计阶段：
- 添加更多的测试点
- 预留SWD调试接口
- 添加LED电源指示灯
- 考虑添加电源管理芯片

## 📞 下一步

请提供以下信息，我可以帮助：
1. ✅ 确认OLED的GPIO连接
2. ✅ 创建完整的板级配置文件
3. ✅ 生成适配后的固件
4. ✅ 提供详细的测试步骤

---

**文档版本**: v1.0  
**创建日期**: 2025-12-27  
**适用硬件**: ESP32-C3 自定义开发板

