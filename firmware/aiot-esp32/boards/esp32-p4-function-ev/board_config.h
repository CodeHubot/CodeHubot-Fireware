/**
 * @file board_config.h
 * @brief ESP32-P4 Function EV Board Configuration
 * 
 * ESP32-P4是Espressif最新的高性能双核处理器，专为AI和高性能应用设计
 * 支持高分辨率显示、AI加速、高速USB等先进功能
 * 
 * 主要特性：
 * - 双核RISC-V处理器，主频高达400MHz
 * - 内置AI加速器(PIE)，支持神经网络推理
 * - 高达8MB PSRAM，支持大型AI模型
 * - 支持MIPI-DSI显示接口，可驱动高分辨率屏幕
 * - USB 2.0 OTG支持，可作为主机或设备
 * - 丰富的外设接口：I2C、SPI、UART、I2S等
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
// #include "../../../main/hal/hal_common.h"  // 在实际项目中包含HAL头文件

// 临时定义HAL类型，避免头文件依赖
typedef enum {
    HAL_LED_TYPE_SINGLE = 0,
    HAL_LED_TYPE_RGB,
    HAL_LED_TYPE_ADDRESSABLE
} hal_led_type_t;

typedef enum {
    HAL_RELAY_TYPE_NORMAL = 0,
    HAL_RELAY_TYPE_HIGH_POWER,
    HAL_RELAY_TYPE_SOLID_STATE
} hal_relay_type_t;

typedef enum {
    HAL_SERVO_TYPE_STANDARD = 0,
    HAL_SERVO_TYPE_HIGH_PRECISION,
    HAL_SERVO_TYPE_CONTINUOUS
} hal_servo_type_t;

typedef enum {
    HAL_DISPLAY_TYPE_NONE = 0,
    HAL_DISPLAY_TYPE_SPI_LCD,
    HAL_DISPLAY_TYPE_I2C_OLED,
    HAL_DISPLAY_TYPE_MIPI_DSI
} hal_display_type_t;

typedef enum {
    HAL_AUDIO_TYPE_NONE = 0,
    HAL_AUDIO_TYPE_PWM_BUZZER,
    HAL_AUDIO_TYPE_I2S_DAC,
    HAL_AUDIO_TYPE_I2S_CODEC
} hal_audio_type_t;

typedef enum {
    HAL_CAMERA_TYPE_NONE = 0,
    HAL_CAMERA_TYPE_PARALLEL,
    HAL_CAMERA_TYPE_MIPI_CSI
} hal_camera_type_t;

typedef enum {
    HAL_CAMERA_RES_VGA = 0,
    HAL_CAMERA_RES_720P,
    HAL_CAMERA_RES_1080P
} hal_camera_resolution_t;

typedef enum {
    HAL_AI_TYPE_NONE = 0,
    HAL_AI_TYPE_PIE,
    HAL_AI_TYPE_NPU
} hal_ai_type_t;

// ================================
// 板级基本信息
// ================================
#define BOARD_NAME              "ESP32-P4-Function-EV"
#define BOARD_VERSION           "v1.0"
#define CHIP_MODEL              "ESP32-P4"
#define CHIP_ARCHITECTURE       "RISC-V Dual Core"
#define MANUFACTURER            "Espressif"
#define FLASH_SIZE_MB           16
#define PSRAM_SIZE_MB           8

// ================================
// 芯片特性
// ================================
#define CHIP_FREQ_MHZ           400
#define CHIP_CORES              2
#define CHIP_HAS_WIFI           false  // ESP32-P4不支持WiFi
#define CHIP_HAS_BLUETOOTH      false  // ESP32-P4不支持蓝牙
#define CHIP_HAS_ETHERNET       true   // 支持以太网
#define CHIP_HAS_USB            true   // 支持USB 2.0 OTG
#define CHIP_HAS_AI_ACCELERATOR true   // 内置AI加速器(PIE)

// ================================
// 功能支持
// ================================
#define SUPPORT_LED             true
#define SUPPORT_RELAY           true
#define SUPPORT_SERVO           true
#define SUPPORT_SENSOR          true
#define SUPPORT_BUTTON          true
#define SUPPORT_DISPLAY         true   // 支持高分辨率显示
#define SUPPORT_AUDIO           true   // 支持高质量音频
#define SUPPORT_CAMERA          true   // 支持摄像头
#define SUPPORT_AI_INFERENCE    true   // 支持AI推理

// ================================
// LED配置 (RGB LED阵列)
// ================================
#define LED_COUNT               8
#define LED_TYPE                HAL_LED_TYPE_RGB

// RGB LED引脚定义
#define LED_RGB_RED_PIN         10
#define LED_RGB_GREEN_PIN       11
#define LED_RGB_BLUE_PIN        12
#define LED_RGB_POWER_PIN       13

// 状态LED
#define LED_STATUS_PIN          14
#define LED_POWER_PIN           15

// ================================
// 继电器配置 (高功率继电器)
// ================================
#define RELAY_COUNT             4
#define RELAY_TYPE              HAL_RELAY_TYPE_HIGH_POWER

#define RELAY_1_PIN             16
#define RELAY_2_PIN             17
#define RELAY_3_PIN             18
#define RELAY_4_PIN             19
#define RELAY_ENABLE_PIN        20

// ================================
// 舵机配置 (高精度舵机)
// ================================
#define SERVO_COUNT             6
#define SERVO_TYPE              HAL_SERVO_TYPE_HIGH_PRECISION

#define SERVO_1_PIN             21
#define SERVO_2_PIN             22
#define SERVO_3_PIN             23
#define SERVO_4_PIN             24
#define SERVO_5_PIN             25
#define SERVO_6_PIN             26
#define SERVO_POWER_PIN         27

// ================================
// 传感器配置 (多种传感器)
// ================================
#define SENSOR_COUNT            6

// 温湿度传感器 (DHT22)
#define SENSOR_DHT22_PIN        28
#define SENSOR_DHT22_POWER_PIN  29

// 光照传感器 (BH1750)
#define SENSOR_BH1750_SDA_PIN   30
#define SENSOR_BH1750_SCL_PIN   31

// 距离传感器 (HC-SR04)
#define SENSOR_HCSR04_TRIG_PIN  32
#define SENSOR_HCSR04_ECHO_PIN  33

// 加速度计 (MPU6050)
#define SENSOR_MPU6050_SDA_PIN  34
#define SENSOR_MPU6050_SCL_PIN  35
#define SENSOR_MPU6050_INT_PIN  36

// 气体传感器 (MQ-2)
#define SENSOR_MQ2_ANALOG_PIN   37
#define SENSOR_MQ2_DIGITAL_PIN  GPIO_NUM_NC  // 不使用数字引脚，避免与LCD背光冲突

// 压力传感器 (BMP280)
#define SENSOR_BMP280_SDA_PIN   39
#define SENSOR_BMP280_SCL_PIN   40

// ================================
// 按键配置 (多功能按键)
// ================================
#define BUTTON_COUNT            6

#define BUTTON_POWER_PIN        41
#define BUTTON_RESET_PIN        42
#define BUTTON_MODE_PIN         43
#define BUTTON_UP_PIN           44
#define BUTTON_DOWN_PIN         45
#define BUTTON_SELECT_PIN       46

// ================================
// 显示屏配置 (高分辨率MIPI-DSI)
// ================================
#define DISPLAY_TYPE            HAL_DISPLAY_TYPE_MIPI_DSI
#define DISPLAY_WIDTH           800
#define DISPLAY_HEIGHT          480
#define DISPLAY_COLOR_DEPTH     24  // 24位真彩色

// MIPI-DSI接口引脚
#define DISPLAY_DSI_CLK_PIN     47
#define DISPLAY_DSI_DATA0_PIN   48
#define DISPLAY_DSI_DATA1_PIN   49
#define DISPLAY_RESET_PIN       50
#define DISPLAY_BACKLIGHT_PIN   51
#define DISPLAY_POWER_PIN       52

// ================================
// 音频配置 (高质量音频)
// ================================
#define AUDIO_TYPE              HAL_AUDIO_TYPE_I2S_CODEC
#define AUDIO_SAMPLE_RATE       48000
#define AUDIO_BIT_DEPTH         24
#define AUDIO_CHANNELS          2  // 立体声

// I2S音频接口
#define AUDIO_I2S_BCK_PIN       53
#define AUDIO_I2S_WS_PIN        54
#define AUDIO_I2S_DATA_OUT_PIN  55
#define AUDIO_I2S_DATA_IN_PIN   56

// 音频控制
#define AUDIO_MUTE_PIN          57
#define AUDIO_POWER_PIN         58

// ================================
// 摄像头配置 (高分辨率摄像头)
// ================================
#define CAMERA_TYPE             HAL_CAMERA_TYPE_MIPI_CSI
#define CAMERA_RESOLUTION       HAL_CAMERA_RES_1080P
#define CAMERA_FPS              30

// MIPI-CSI接口引脚
#define CAMERA_CSI_CLK_PIN      59
#define CAMERA_CSI_DATA0_PIN    60
#define CAMERA_CSI_DATA1_PIN    61
#define CAMERA_RESET_PIN        62
#define CAMERA_POWER_PIN        63

// ================================
// 通信接口配置
// ================================

// I2C接口 (多个I2C总线)
#define I2C_COUNT               3

#define I2C0_SDA_PIN            30
#define I2C0_SCL_PIN            31
#define I2C0_FREQ_HZ            100000

#define I2C1_SDA_PIN            34
#define I2C1_SCL_PIN            35
#define I2C1_FREQ_HZ            400000

#define I2C2_SDA_PIN            39
#define I2C2_SCL_PIN            40
#define I2C2_FREQ_HZ            400000

// SPI接口 (高速SPI)
#define SPI_COUNT               2

#define SPI0_MISO_PIN           64
#define SPI0_MOSI_PIN           65
#define SPI0_CLK_PIN            66
#define SPI0_CS_PIN             67
#define SPI0_FREQ_HZ            40000000

#define SPI1_MISO_PIN           68
#define SPI1_MOSI_PIN           69
#define SPI1_CLK_PIN            70
#define SPI1_CS_PIN             71
#define SPI1_FREQ_HZ            20000000

// UART接口
#define UART_COUNT              3

#define UART0_TX_PIN            72
#define UART0_RX_PIN            73
#define UART0_BAUD_RATE         115200

#define UART1_TX_PIN            74
#define UART1_RX_PIN            75
#define UART1_BAUD_RATE         9600

#define UART2_TX_PIN            76
#define UART2_RX_PIN            77
#define UART2_BAUD_RATE         115200

// 以太网接口
#define ETH_MDC_PIN             78
#define ETH_MDIO_PIN            79
#define ETH_POWER_PIN           80

// USB接口
#define USB_DP_PIN              81
#define USB_DM_PIN              82
#define USB_POWER_PIN           83

// ================================
// AI加速器配置
// ================================
#define AI_ACCELERATOR_TYPE     HAL_AI_TYPE_PIE
#define AI_MODEL_MAX_SIZE_MB    4
#define AI_INFERENCE_FREQ_MHZ   200

// ================================
// 系统配置
// ================================
#define SYSTEM_CLOCK_FREQ_MHZ   400
#define SYSTEM_VOLTAGE_V        3.3f
#define SYSTEM_POWER_BUDGET_W   15.0f

// 电源管理
#define POWER_ENABLE_PIN        84
#define POWER_MONITOR_PIN       85
#define BATTERY_MONITOR_PIN     86

// 系统状态
#define STATUS_LED_PIN          14
#define WATCHDOG_TIMEOUT_MS     30000

// 调试接口
#define DEBUG_UART_TX_PIN       72
#define DEBUG_UART_RX_PIN       73
#define DEBUG_UART_BAUD         115200

#endif // BOARD_CONFIG_H