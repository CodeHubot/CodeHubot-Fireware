/**
 * @file board_config.h
 * @brief ESP32-S3 DevKit Rain 开发板配置（含雨水传感器）
 * 
 * 定义ESP32-S3 DevKit Rain版本的硬件配置，包括GPIO映射、外设配置等
 * Rain版本新增雨水传感器支持
 * 
 * GPIO引脚映射表:
 * ┌─────────────┬──────────┬─────────────────────┐
 * │   功能      │  标识    │      GPIO引脚       │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ LED1        │   L1     │       G42           │
 * │ LED2        │   L2     │       G41           │
 * │ LED3        │   L3     │       G37           │
 * │ LED4        │   L4     │       G36           │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ 继电器1     │  RELAY1  │       G01           │
 * │ 继电器2     │  RELAY2  │       G02           │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ DHT11传感器 │   S1     │       G35           │
 * │ DS18B20传感器│  S2     │       G39           │
 * │ 雨水传感器  │   S3     │       G04           │
 * ├─────────────┼──────────┼─────────────────────┤
 * │ PWM输出1    │   M1     │       G48           │ (原舵机1，已改为PWM)
 * │ 舵机2       │   M2     │       G40           │ (保留作为舵机)
 * │ PWM输出2    │   M2     │       G40           │ (也可作为PWM使用)
 * └─────────────┴──────────┴─────────────────────┘
 * 
 * 兼容ESP-IDF版本: 5.4
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// GPIO引脚定义 (使用数值而不是枚举，避免包含ESP-IDF头文件)
#define GPIO_NUM_NC     (-1)
#define GPIO_NUM_0      0
#define GPIO_NUM_1      1
#define GPIO_NUM_2      2
#define GPIO_NUM_4      4
#define GPIO_NUM_5      5
#define GPIO_NUM_6      6
#define GPIO_NUM_7      7
#define GPIO_NUM_8      8
#define GPIO_NUM_9      9
#define GPIO_NUM_10     10
#define GPIO_NUM_11     11
#define GPIO_NUM_12     12
#define GPIO_NUM_13     13
#define GPIO_NUM_21     21
#define GPIO_NUM_22     22
#define GPIO_NUM_35     35
#define GPIO_NUM_36     36
#define GPIO_NUM_37     37
#define GPIO_NUM_38     38
#define GPIO_NUM_39     39
#define GPIO_NUM_40     40
#define GPIO_NUM_41     41
#define GPIO_NUM_42     42
#define GPIO_NUM_43     43
#define GPIO_NUM_44     44
#define GPIO_NUM_48     48

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 板级基本信息 ====================
#define BOARD_NAME          "ESP32-S3-DevKit-Rain"
#define CHIP_MODEL          "ESP32-S3"
#define BOARD_VERSION       "v1.0"
#define MANUFACTURER        "Espressif"
#define FLASH_SIZE_MB       8
#define PSRAM_SIZE_MB       8

// ==================== 功能支持 ====================
#define HAS_WIFI            true
#define HAS_BLUETOOTH       true
#define HAS_ETHERNET        false

// ==================== LED配置 ====================
#define LED_COUNT           4

// LED1配置 (L1 - G42) - 纯GPIO控制，不使用PWM
#define LED1_GPIO_PIN       GPIO_NUM_42
#define LED1_ACTIVE_LEVEL   true    // 高电平有效
#define LED1_PWM_ENABLED    false   // 禁用PWM，使用纯GPIO控制
#define LED1_PWM_FREQUENCY  1000    // 1kHz (保留定义但不使用)
#define LED1_PWM_RESOLUTION 8       // 8位分辨率 (保留定义但不使用)

// LED2配置 (L2 - G41) - 纯GPIO控制，不使用PWM
#define LED2_GPIO_PIN       GPIO_NUM_41
#define LED2_ACTIVE_LEVEL   true
#define LED2_PWM_ENABLED    false   // 禁用PWM，使用纯GPIO控制
#define LED2_PWM_FREQUENCY  1000
#define LED2_PWM_RESOLUTION 8

// LED3配置 (L3 - G37) - 纯GPIO控制，不使用PWM
#define LED3_GPIO_PIN       GPIO_NUM_37
#define LED3_ACTIVE_LEVEL   true
#define LED3_PWM_ENABLED    false   // 禁用PWM，使用纯GPIO控制
#define LED3_PWM_FREQUENCY  1000
#define LED3_PWM_RESOLUTION 8

// LED4配置 (L4 - G36) - 纯GPIO控制，不使用PWM
#define LED4_GPIO_PIN       GPIO_NUM_36
#define LED4_ACTIVE_LEVEL   true
#define LED4_PWM_ENABLED    false   // 禁用PWM，使用纯GPIO控制
#define LED4_PWM_FREQUENCY  1000
#define LED4_PWM_RESOLUTION 8

// ==================== 继电器配置 ====================
#define RELAY_COUNT         2

// 继电器1配置 (G01)
#define RELAY1_GPIO_PIN     GPIO_NUM_1
#define RELAY1_ACTIVE_LEVEL true    // 高电平有效
#define RELAY1_SWITCH_DELAY 100     // 100ms开关延迟

// 继电器2配置 (G02)
#define RELAY2_GPIO_PIN     GPIO_NUM_2
#define RELAY2_ACTIVE_LEVEL true
#define RELAY2_SWITCH_DELAY 100

// ==================== 舵机配置 ====================
#define SERVO_COUNT         1  // M1已改为PWM，只保留M2作为舵机

// 舵机1配置 (M1 - G48) - 已改为PWM输出，不再作为舵机
// #define SERVO1_GPIO_PIN     GPIO_NUM_48  // 已改为PWM M1

// 舵机2配置 (M2 - G40) - 保留作为舵机
#define SERVO2_GPIO_PIN     GPIO_NUM_40
#define SERVO2_FREQUENCY    50
#define SERVO2_MIN_PULSE_US 500
#define SERVO2_MAX_PULSE_US 2500
#define SERVO2_MAX_ANGLE    180

// ==================== PWM配置 ====================
#define PWM_COUNT           2

// PWM M1配置 (原舵机1端口 - G48)
#define PWM_M1_GPIO_PIN     GPIO_NUM_48
#define PWM_M1_CHANNEL      1  // PWM通道1

// PWM M2配置 (原舵机2端口 - G40)
#define PWM_M2_GPIO_PIN     GPIO_NUM_40
#define PWM_M2_CHANNEL      2  // PWM通道2

// ==================== 传感器配置 ====================
#define SENSOR_COUNT        2  // DHT11 + 雨水传感器（雨水传感器使用GPIO39，原DS18B20管脚）

// DHT11传感器配置 (S1 - G35, 温湿度传感器)
#define DHT11_GPIO_PIN      GPIO_NUM_35
#define DHT11_SENSOR_TYPE   0  // 温度传感器

// DS18B20传感器配置 (S2 - G39) - 已禁用，此管脚用于雨水传感器
// #define DS18B20_GPIO_PIN    GPIO_NUM_39
// #define DS18B20_SENSOR_TYPE 1  // 温度传感器

// 雨水传感器配置 (S2 - G39, 数字传感器，使用原DS18B20管脚)
#define RAIN_SENSOR_GPIO_PIN    GPIO_NUM_39  // 使用GPIO39（原DS18B20管脚）
#define RAIN_SENSOR_TYPE        2  // 数字传感器

// ==================== 按键配置 ====================
#define BUTTON_COUNT        2

// Boot按键 (通常连接到GPIO0)
#define BOOT_BUTTON_GPIO    GPIO_NUM_0

// 用户按键
#define USER_BUTTON_GPIO    GPIO_NUM_7

// ==================== 显示屏配置 ====================
#define DISPLAY_TYPE        1       // LCD显示屏
#define DISPLAY_WIDTH       240
#define DISPLAY_HEIGHT      240
#define DISPLAY_COLOR_DEPTH 1       // 单色

// SPI接口配置
#define DISPLAY_SPI_MOSI    GPIO_NUM_20  // SDA
#define DISPLAY_SPI_SCLK    GPIO_NUM_19  // SCL
#define DISPLAY_SPI_CS      GPIO_NUM_45  // CS
#define DISPLAY_DC_PIN      GPIO_NUM_47  // DC
#define DISPLAY_RESET_PIN   GPIO_NUM_21  // RES
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_38  // BLK

// ==================== 音频配置 ====================
#define AUDIO_ENABLED       false   // 默认不启用音频

// I2S接口配置 (如果需要)
#define AUDIO_I2S_BCLK_PIN  GPIO_NUM_NC
#define AUDIO_I2S_WS_PIN    GPIO_NUM_NC
#define AUDIO_I2S_DATA_PIN  GPIO_NUM_NC
#define AUDIO_AMP_PIN       GPIO_NUM_NC
#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_CHANNELS      2

// ==================== 通信接口配置 ====================
// UART配置
#define UART_TX_PIN         GPIO_NUM_43
#define UART_RX_PIN         GPIO_NUM_44
#define UART_BAUD_RATE      115200

// I2C配置
#define I2C_SDA_PIN         GPIO_NUM_21
#define I2C_SCL_PIN         GPIO_NUM_22
#define I2C_FREQUENCY       100000  // 100kHz

// SPI配置
#define SPI_MOSI_PIN        GPIO_NUM_11
#define SPI_MISO_PIN        GPIO_NUM_13
#define SPI_SCLK_PIN        GPIO_NUM_12
#define SPI_CS_PIN          GPIO_NUM_10

// ==================== 系统配置 ====================
#define CPU_FREQUENCY_MHZ   240     // 240MHz
#define WATCHDOG_ENABLED    true
#define WATCHDOG_TIMEOUT_S  30      // 30秒

// ==================== 网络配置默认值 ====================
// 注意：这些是开发时的默认值，实际使用时通过配网获取
#define DEFAULT_WIFI_SSID   ""
#define DEFAULT_WIFI_PASS   ""
#define DEFAULT_MQTT_BROKER ""
#define DEFAULT_MQTT_PORT   1883
#define DEFAULT_MQTT_USERNAME ""
#define DEFAULT_MQTT_PASSWORD ""
#define DEFAULT_DEVICE_ID   "esp32s3_devkit_rain_001"

#ifdef __cplusplus
}
#endif

#endif // BOARD_CONFIG_H

