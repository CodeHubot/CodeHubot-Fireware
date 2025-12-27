/**
 * @file bsp_esp32_c3_mini.c
 * @brief ESP32-C3 Mini 板级支持包实现
 * 
 * @author AIOT Team
 * @date 2024
 */

#include "bsp_esp32_c3_mini.h"
#include "board_config.h"
#include "../../main/bsp/bsp_interface.h"
#include <stdio.h>
#include <string.h>

// ==================== 静态配置数据 ====================

// LED配置数组
static hal_led_config_t s_led_configs[LED_COUNT] = {
    {
        .pin = LED1_GPIO_PIN,
        .active_level = LED1_ACTIVE_LEVEL,
        .pwm_enabled = LED1_PWM_ENABLED,
        .pwm_frequency = LED1_PWM_FREQUENCY,
        .pwm_resolution = LED1_PWM_RESOLUTION
    }
};

// 继电器配置数组
static hal_relay_config_t s_relay_configs[RELAY_COUNT] = {
    {
        .pin = RELAY1_GPIO_PIN,
        .active_level = RELAY1_ACTIVE_LEVEL,
        .switch_delay_ms = RELAY1_SWITCH_DELAY
    }
};

// 舵机配置数组
static hal_servo_config_t s_servo_configs[SERVO_COUNT] = {
    {
        .pin = SERVO1_GPIO_PIN,
        .frequency = SERVO1_FREQUENCY,
        .min_pulse_us = SERVO1_MIN_PULSE_US,
        .max_pulse_us = SERVO1_MAX_PULSE_US,
        .max_angle = SERVO1_MAX_ANGLE
    },
    {
        .pin = SERVO2_GPIO_PIN,
        .frequency = SERVO2_FREQUENCY,
        .min_pulse_us = SERVO2_MIN_PULSE_US,
        .max_pulse_us = SERVO2_MAX_PULSE_US,
        .max_angle = SERVO2_MAX_ANGLE
    }
};

// 传感器类型数组
static hal_sensor_type_t s_sensor_types[SENSOR_COUNT] = {
    HAL_SENSOR_TYPE_TEMPERATURE,  // DHT11温度
    HAL_SENSOR_TYPE_HUMIDITY      // DHT11湿度
};

// 按键GPIO数组
static gpio_num_t s_button_pins[BUTTON_COUNT] = {
    BOOT_BUTTON_GPIO,
    USER_BUTTON_GPIO
};

// 系统配置
static hal_system_config_t s_system_config = {
    .cpu_frequency = CPU_FREQUENCY_MHZ,
    .flash_size = FLASH_SIZE_MB,
    .psram_size = PSRAM_SIZE_MB,
    .watchdog_enabled = WATCHDOG_ENABLED,
    .watchdog_timeout = WATCHDOG_TIMEOUT_S
};

// 板级信息
static const bsp_board_info_t s_board_info = {
    .board_name = BOARD_NAME,
    .chip_model = CHIP_MODEL,
    .board_version = BOARD_VERSION,
    .manufacturer = MANUFACTURER,
    .flash_size_mb = FLASH_SIZE_MB,
    .psram_size_mb = PSRAM_SIZE_MB,
    .has_wifi = HAS_WIFI,
    .has_ethernet = HAS_ETHERNET
};

// 硬件配置 (使用函数返回，避免静态初始化问题)
static bsp_hardware_config_t s_hardware_config;

// ==================== BSP接口实现 ====================

static hal_err_t esp32_c3_mini_init(void)
{
    printf("BSP: Initializing ESP32-C3 Mini...\n");
    
    // TODO: 实现具体的硬件初始化
    // 1. 初始化GPIO
    // 2. 初始化PWM
    // 3. 初始化I2C/SPI等通信接口
    // 4. 初始化传感器
    // 注意：ESP32-C3没有显示屏和音频支持
    
    printf("BSP: ESP32-C3 Mini initialized successfully\n");
    return HAL_OK;
}

static hal_err_t esp32_c3_mini_deinit(void)
{
    printf("BSP: Deinitializing ESP32-C3 Mini...\n");
    
    // TODO: 实现具体的硬件去初始化
    
    printf("BSP: ESP32-C3 Mini deinitialized\n");
    return HAL_OK;
}

static const bsp_board_info_t* esp32_c3_mini_get_board_info(void)
{
    return &s_board_info;
}

static const bsp_hardware_config_t* esp32_c3_mini_get_hardware_config(void)
{
    // 动态初始化硬件配置
    s_hardware_config.led_count = LED_COUNT;
    s_hardware_config.led_configs = s_led_configs;
    s_hardware_config.relay_count = RELAY_COUNT;
    s_hardware_config.relay_configs = s_relay_configs;
    s_hardware_config.servo_count = SERVO_COUNT;
    s_hardware_config.servo_configs = s_servo_configs;
    s_hardware_config.sensor_count = SENSOR_COUNT;
    s_hardware_config.sensor_types = s_sensor_types;
    s_hardware_config.button_count = BUTTON_COUNT;
    s_hardware_config.button_pins = s_button_pins;
    s_hardware_config.system_config = s_system_config;
    
    // ESP32-C3不支持显示屏和音频，设置为NULL
    memset(&s_hardware_config.display_config, 0, sizeof(hal_display_config_t));
    memset(&s_hardware_config.audio_config, 0, sizeof(hal_audio_config_t));
    
    return &s_hardware_config;
}

// BSP接口结构体
static const bsp_interface_t s_esp32_c3_mini_interface = {
    .init = esp32_c3_mini_init,
    .deinit = esp32_c3_mini_deinit,
    .get_board_info = esp32_c3_mini_get_board_info,
    .get_hw_config = esp32_c3_mini_get_hardware_config
};

// ==================== 公共接口 ====================

hal_err_t bsp_esp32_c3_mini_register(void)
{
    printf("BSP: Registering ESP32-C3 Mini interface...\n");
    return bsp_register_interface(&s_esp32_c3_mini_interface);
}

void bsp_esp32_c3_mini_print_config(void)
{
    printf("=== ESP32-C3 Mini Configuration ===\n");
    printf("Board: %s\n", BOARD_NAME);
    printf("Chip: %s (RISC-V)\n", CHIP_MODEL);
    printf("Version: %s\n", BOARD_VERSION);
    printf("LEDs: %d\n", LED_COUNT);
    printf("Relays: %d\n", RELAY_COUNT);
    printf("Servos: %d\n", SERVO_COUNT);
    printf("Sensors: %d\n", SENSOR_COUNT);
    printf("Buttons: %d\n", BUTTON_COUNT);
    printf("Display: Not supported\n");
    printf("Audio: Not supported\n");
    printf("PSRAM: Not available\n");
    printf("===================================\n");
}