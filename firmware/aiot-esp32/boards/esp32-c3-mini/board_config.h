/**
 * @file board_config.h
 * @brief ESP32-C3 Mini 开发板硬件配置
 * 
 * ESP32-C3是一款低功耗的RISC-V架构芯片，适合IoT应用
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// GPIO引脚定义 (ESP32-C3特有的GPIO)
#define GPIO_NUM_NC     (-1)
#define GPIO_NUM_0      0
#define GPIO_NUM_1      1
#define GPIO_NUM_2      2
#define GPIO_NUM_3      3
#define GPIO_NUM_4      4
#define GPIO_NUM_5      5
#define GPIO_NUM_6      6
#define GPIO_NUM_7      7
#define GPIO_NUM_8      8
#define GPIO_NUM_9      9
#define GPIO_NUM_10     10
#define GPIO_NUM_18     18
#define GPIO_NUM_19     19
#define GPIO_NUM_20     20
#define GPIO_NUM_21     21

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 板级基本信息 ====================
#define BOARD_NAME              "ESP32-C3-Mini"
#define CHIP_MODEL              "ESP32-C3"
#define BOARD_VERSION           "v1.0"
#define MANUFACTURER            "Espressif"

// ==================== 芯片特性 ====================
#define FLASH_SIZE_MB           4
#define PSRAM_SIZE_MB           0       // ESP32-C3没有PSRAM
#define HAS_WIFI                true
#define HAS_BLUETOOTH           true    // 支持Bluetooth 5.0 LE
#define HAS_ETHERNET            false

// ==================== 功能支持 ====================
#define SUPPORT_LED             true
#define SUPPORT_RELAY           true
#define SUPPORT_SERVO           true
#define SUPPORT_SENSOR          true
#define SUPPORT_BUTTON          true
#define SUPPORT_DISPLAY         false   // 简化配置，不支持显示屏
#define SUPPORT_AUDIO           false   // 简化配置，不支持音频

// ==================== LED配置 ====================
#define LED_COUNT               1
#define LED1_GPIO_PIN           GPIO_NUM_8      // 内置LED
#define LED1_ACTIVE_LEVEL       true
#define LED1_PWM_ENABLED        true
#define LED1_PWM_FREQUENCY      1000
#define LED1_PWM_RESOLUTION     8

// ==================== 继电器配置 ====================
#define RELAY_COUNT             1
#define RELAY1_GPIO_PIN         GPIO_NUM_2
#define RELAY1_ACTIVE_LEVEL     true
#define RELAY1_SWITCH_DELAY     10

// ==================== 舵机配置 ====================
#define SERVO_COUNT             2
// 舵机1 - 180度舵机
#define SERVO1_GPIO_PIN         GPIO_NUM_3
#define SERVO1_FREQUENCY        50
#define SERVO1_MIN_PULSE_US     500
#define SERVO1_MAX_PULSE_US     2500
#define SERVO1_MAX_ANGLE        180

// 舵机2 - 360度舵机
#define SERVO2_GPIO_PIN         GPIO_NUM_4
#define SERVO2_FREQUENCY        50
#define SERVO2_MIN_PULSE_US     1000
#define SERVO2_MAX_PULSE_US     2000
#define SERVO2_MAX_ANGLE        360

// ==================== 传感器配置 ====================
#define SENSOR_COUNT            2
#define DHT11_GPIO_PIN          GPIO_NUM_5      // DHT11温湿度传感器

// ==================== 按键配置 ====================
#define BUTTON_COUNT            2
#define BOOT_BUTTON_GPIO        GPIO_NUM_9      // BOOT按键
#define USER_BUTTON_GPIO        GPIO_NUM_6      // 用户按键

// ==================== 通信接口配置 ====================
// I2C配置
#define I2C_SDA_PIN             GPIO_NUM_1
#define I2C_SCL_PIN             GPIO_NUM_0
#define I2C_FREQUENCY           100000

// SPI配置
#define SPI_MOSI_PIN            GPIO_NUM_7
#define SPI_MISO_PIN            GPIO_NUM_10
#define SPI_SCLK_PIN            GPIO_NUM_18
#define SPI_CS_PIN              GPIO_NUM_19

// ==================== 系统配置 ====================
#define CPU_FREQUENCY_MHZ       160             // ESP32-C3最大160MHz
#define WATCHDOG_ENABLED        true
#define WATCHDOG_TIMEOUT_S      30

// ==================== 网络配置 ====================
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASSWORD_MAX_LEN   64
#define MQTT_BROKER_MAX_LEN     128
#define MQTT_TOPIC_MAX_LEN      64

#ifdef __cplusplus
}
#endif

#endif // BOARD_CONFIG_H