/**
 * @file board_config.h
 * @brief ESP32-C3 自定义开发板配置 - 实际硬件适配版
 * 
 * 定义ESP32-C3开发板的硬件配置，基于实际原理图
 * 
 * GPIO引脚映射表 (实际硬件):
 * ┌─────────────┬──────────┬─────────────────────┐
 * │   功能      │  标识    │      GPIO引脚       │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ OLED SDA    │  I2C_SDA │       GPIO4         │
 * │ OLED SCL    │  I2C_SCL │       GPIO5         │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ LED1        │   LED1   │       GPIO19        │
 * │ LED2        │   LED2   │       GPIO18        │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ DHT11传感器 │   DHT11  │       GPIO11        │ (实际硬件为GPIO11，但ESP32-C3的GPIO11是Flash，可能有问题)
 * ├─────────────┼──────────┼─────────────────────┤
 * │ Boot按键    │  BOOT    │       GPIO9         │
 * │ Reset按键   │  RESET   │       EN(硬件复位)  │
 * └─────────────┴──────────┴─────────────────────┘
 * 
 * ⚠️ 注意：ESP32-C3的GPIO11连接到Flash，通常不建议使用
 * 如果DHT11连接到GPIO11无法工作，建议改为GPIO6或GPIO7
 * 
 * @author AIOT Team
 * @date 2025-12-27
 * @version 1.1 (硬件适配版)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 板级基本信息 ====================
#define BOARD_NAME          "ESP32-C3-Custom"
#define CHIP_MODEL          "ESP32-C3"
#define BOARD_VERSION       "v1.1-HW"
#define MANUFACTURER        "Custom"
#define PRODUCT_CODE        "ESP32-C3-OLED-01"
#define PRODUCT_ID          "ESP32-C3-OLED-01"

// Flash和RAM配置
#define FLASH_SIZE_MB       4
#define PSRAM_SIZE_MB       0       // ESP32-C3没有PSRAM
#define SRAM_SIZE_KB        400     // ESP32-C3内置400KB SRAM

// ==================== 功能支持标志 ====================
#define HAS_WIFI            1
#define HAS_BLUETOOTH       1       // BLE 5.0
#define HAS_ETHERNET        0
#define HAS_PSRAM           0
#define HAS_OTA             0       // ✅ 精简版不支持OTA
#define HAS_LVGL            0       // ✅ 精简版不支持LVGL
#define HAS_DISPLAY         1       // ✅ 支持OLED显示

// ==================== OLED显示屏配置 ====================
#define OLED_ENABLED        1
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_I2C_ADDRESS    0x3C    // SSD1306默认I2C地址
#define OLED_TYPE           0       // 0=SSD1306, 1=SSD1315

// I2C配置 (用于OLED)
#define I2C_ENABLED         1
#define I2C_SDA_PIN         4       // 实际硬件: GPIO4
#define I2C_SCL_PIN         5       // 实际硬件: GPIO5
#define I2C_FREQUENCY       400000  // 400kHz (快速模式)
#define I2C_PORT            0       // I2C端口0

// ==================== LED配置 ====================
#define LED_COUNT           2

// LED1配置 (GPIO19)
#define LED1_GPIO_PIN       19      // 实际硬件: GPIO19
#define LED1_ACTIVE_LEVEL   1       // 高电平有效
#define LED1_PWM_ENABLED    0       // 纯GPIO控制
#define LED_RED_PIN         19      // 别名
#define LED_RED             LED1_GPIO_PIN

// LED2配置 (GPIO18)
#define LED2_GPIO_PIN       18      // 实际硬件: GPIO18
#define LED2_ACTIVE_LEVEL   1       // 高电平有效
#define LED2_PWM_ENABLED    0       // 纯GPIO控制
#define LED_BLUE_PIN        18      // 别名
#define LED_BLUE            LED2_GPIO_PIN

// ==================== 继电器配置 ====================
// 注意：原理图没有继电器，这里保留配置但禁用
#define RELAY_COUNT         0
#define RELAY1_GPIO_PIN     (-1)    // 未连接
#define RELAY1_ACTIVE_LEVEL 1
#define RELAY1_SWITCH_DELAY 100

// ==================== 传感器配置 ====================
#define SENSOR_COUNT        1

// DHT11传感器配置 (GPIO6)
// ✅ GPIO6是完全可用的GPIO引脚
#define DHT11_GPIO_PIN      6       // 实际硬件: GPIO6 ✅
#define DHT11_ENABLED       1
#define DHT11_SENSOR_TYPE   0       // DHT11类型

// DS18B20 - 精简版不支持
#define DS18B20_ENABLED     0
#define DS18B20_GPIO_PIN    (-1)

// ==================== 按键配置 ====================
#define BUTTON_COUNT        2

// Boot按键 (GPIO9)
#define BOOT_BUTTON_GPIO    9       // 实际硬件: GPIO9
#define BOOT_BUTTON_ACTIVE  0       // 低电平有效（按下为0）

// Reset按键 (硬件复位)
#define RESET_BUTTON_EN     1       // 连接到EN引脚（硬件复位）

// 按键配置
#define BUTTON_LONG_PRESS_MS    3000    // 长按3秒
#define BUTTON_DEBOUNCE_MS      50      // 50ms防抖

// ==================== 系统配置 ====================
#define CPU_FREQUENCY_MHZ   160     // ESP32-C3最大160MHz
#define WATCHDOG_ENABLED    1
#define WATCHDOG_TIMEOUT_S  30      // 30秒

// ==================== 网络配置默认值 ====================
// 注意：这些是开发时的默认值，实际使用时通过配网获取
#define DEFAULT_WIFI_SSID   ""
#define DEFAULT_WIFI_PASS   ""
#define DEFAULT_CONFIG_SERVER "http://conf.aiot.powertechhub.com:8001"  // 配置服务器（带端口）
#define DEFAULT_MQTT_BROKER "conf.aiot.powertechhub.com"                // MQTT服务器（不带端口）
#define DEFAULT_MQTT_PORT   1883
#define DEFAULT_MQTT_USERNAME ""
#define DEFAULT_MQTT_PASSWORD ""

// ==================== 精简版特性 ====================
// 禁用的功能
#define SERVO_COUNT         0       // ✅ 不支持舵机
#define PWM_COUNT           0       // ✅ 不支持PWM高级控制
#define OTA_ENABLED         0       // ✅ 不支持OTA
#define LVGL_ENABLED        0       // ✅ 不支持LVGL

// 启用的功能
#define DISPLAY_ENABLED     1       // ✅ 支持OLED显示
#define DHT11_DRIVER_ENABLED 1      // ✅ 支持DHT11传感器

// 内存优化
#define MQTT_BUFFER_SIZE    512     // MQTT缓冲区
#define HTTP_BUFFER_SIZE    1024    // HTTP缓冲区
#define MAX_SENSOR_DATA     10      // 最多保存10条传感器数据

// ==================== 功耗管理 ====================
#define ENABLE_POWER_SAVE   1       // 启用省电模式
#define WIFI_PS_MODE        1       // WiFi省电模式 (WIFI_PS_MIN_MODEM)
#define CPU_FREQ_LIGHT      80      // 轻载时CPU频率 (80MHz)
#define CPU_FREQ_NORMAL     160     // 正常负载CPU频率 (160MHz)

// ==================== OLED显示配置 ====================
#define OLED_REFRESH_MS     1000    // OLED刷新间隔 (1秒)
#define OLED_FONT_SIZE      8       // 字体大小 (8x8像素)
#define OLED_MAX_LINES      8       // 最大显示行数 (64/8=8)
#define OLED_MAX_CHARS      16      // 每行最大字符数 (128/8=16)

// OLED显示内容配置
#define OLED_SHOW_LOGO      1       // 启动时显示Logo
#define OLED_SHOW_STATUS    1       // 显示WiFi/MQTT状态
#define OLED_SHOW_SENSOR    1       // 显示传感器数据
#define OLED_SHOW_IP        1       // 显示IP地址

// ==================== GPIO可用性检查宏 ====================
// ESP32-C3可用GPIO: 0-10, 18-21 (共22个)
// ⚠️ GPIO11-17保留给Flash使用，不可用
#define IS_VALID_GPIO(pin) \
    (((pin) >= 0 && (pin) <= 10) || ((pin) >= 18 && (pin) <= 21))

// GPIO功能检查
#define IS_FLASH_GPIO(pin) \
    ((pin) >= 11 && (pin) <= 17)  // Flash相关GPIO

// ==================== 调试配置 ====================
#define DEBUG_ENABLED       1
#define DEBUG_OLED          1       // OLED调试信息
#define DEBUG_DHT11         1       // DHT11调试信息
#define DEBUG_I2C           1       // I2C调试信息

// ==================== 硬件兼容性检查 ====================
#if DHT11_GPIO_PIN == 11
#error "GPIO11是Flash引脚，不能用于DHT11！请使用GPIO6、GPIO7或GPIO10"
#endif

#ifdef __cplusplus
}
#endif

#endif // BOARD_CONFIG_H
