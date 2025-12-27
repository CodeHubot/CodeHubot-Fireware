/**
 * @file bsp_esp32_s3_devkit_rain_rain.c
 * @brief ESP32-S3 DevKit Rain 板级支持包实现（含雨水传感器）
 * 
 * @author AIOT Team
 * @date 2024
 */

#include "bsp_esp32_s3_devkit_rain.h"
#include "board_config.h"
#include "../../main/bsp/bsp_interface.h"
// #include "../../main/bluetooth/bt_provision.h"  // 临时禁用
#include "../../drivers/sensors/dht11.h"      // DHT11传感器驱动
// Rain板子不使用DS18B20，GPIO39用于雨水传感器
// #include "../../drivers/sensors/ds18b20.h"    // DS18B20传感器驱动（已禁用）
#include <stdio.h>
#include <string.h>

// ESP-IDF includes for hardware control
#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#else
// 模拟定义，用于非ESP-IDF环境编译
typedef int esp_err_t;
typedef int gpio_num_t;
typedef int gpio_mode_t;
typedef int gpio_pull_mode_t;
typedef int ledc_channel_t;
typedef int ledc_timer_t;
typedef int ledc_mode_t;
#define ESP_OK 0
#define GPIO_MODE_OUTPUT 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0
#define LEDC_LOW_SPEED_MODE 0
#endif

// 静态函数声明
static hal_err_t esp32_s3_devkit_rain_init_leds(void);
static hal_err_t esp32_s3_devkit_rain_init_buttons(void);
static hal_err_t esp32_s3_devkit_rain_init_relays(void);
static hal_err_t esp32_s3_devkit_rain_init_servos(void);
static hal_err_t esp32_s3_devkit_rain_led_control(uint8_t led_index, bool state);
static hal_err_t esp32_s3_devkit_rain_led_set_brightness(uint8_t led_index, uint8_t brightness);
static hal_err_t esp32_s3_devkit_rain_relay_control(uint8_t relay_index, bool state);
static hal_err_t esp32_s3_devkit_rain_servo_set_angle(uint8_t servo_index, uint16_t angle);

// 蓝牙配网功能函数声明 - 临时禁用
/*
static hal_err_t esp32_s3_devkit_rain_bt_provision_init(const bt_provision_config_t* config);
static hal_err_t esp32_s3_devkit_rain_bt_provision_deinit(void);
static hal_err_t esp32_s3_devkit_rain_bt_provision_start(void);
static hal_err_t esp32_s3_devkit_rain_bt_provision_stop(void);
*/

// ==================== 静态配置数据 ====================

// LED配置数组 - L1-L4都使用纯GPIO控制，不使用PWM
static hal_led_config_t s_led_configs[LED_COUNT] = {
    {
        .pin = LED1_GPIO_PIN,
        .active_level = LED1_ACTIVE_LEVEL,
        .pwm_enabled = LED1_PWM_ENABLED,  // false - 纯GPIO控制
        .pwm_frequency = LED1_PWM_FREQUENCY,
        .pwm_resolution = LED1_PWM_RESOLUTION
    },
    {
        .pin = LED2_GPIO_PIN,
        .active_level = LED2_ACTIVE_LEVEL,
        .pwm_enabled = LED2_PWM_ENABLED,  // false - 纯GPIO控制
        .pwm_frequency = LED2_PWM_FREQUENCY,
        .pwm_resolution = LED2_PWM_RESOLUTION
    },
    {
        .pin = LED3_GPIO_PIN,
        .active_level = LED3_ACTIVE_LEVEL,
        .pwm_enabled = LED3_PWM_ENABLED,  // false - 纯GPIO控制
        .pwm_frequency = LED3_PWM_FREQUENCY,
        .pwm_resolution = LED3_PWM_RESOLUTION
    },
    {
        .pin = LED4_GPIO_PIN,
        .active_level = LED4_ACTIVE_LEVEL,
        .pwm_enabled = LED4_PWM_ENABLED,  // false - 纯GPIO控制
        .pwm_frequency = LED4_PWM_FREQUENCY,
        .pwm_resolution = LED4_PWM_RESOLUTION
    }
};

// 继电器配置数组
static hal_relay_config_t s_relay_configs[RELAY_COUNT] = {
    {
        .pin = RELAY1_GPIO_PIN,
        .active_level = RELAY1_ACTIVE_LEVEL,
        .switch_delay_ms = RELAY1_SWITCH_DELAY
    },
    {
        .pin = RELAY2_GPIO_PIN,
        .active_level = RELAY2_ACTIVE_LEVEL,
        .switch_delay_ms = RELAY2_SWITCH_DELAY
    }
};

// 舵机配置数组（Rain板子只有1个舵机，M1已改为PWM）
static hal_servo_config_t s_servo_configs[SERVO_COUNT] = {
    // M1已改为PWM，只保留M2作为舵机
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

// 显示屏配置
static hal_display_config_t s_display_config = {
    .type = DISPLAY_TYPE,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
    .color_depth = DISPLAY_COLOR_DEPTH,
    .reset_pin = DISPLAY_RESET_PIN,
    .dc_pin = DISPLAY_DC_PIN,
    .cs_pin = DISPLAY_SPI_CS,
    .backlight_pin = DISPLAY_BACKLIGHT_PIN,
    .backlight_active_level = true
};

// 音频配置
static hal_audio_config_t s_audio_config = {
    .i2s_bclk_pin = AUDIO_I2S_BCLK_PIN,
    .i2s_ws_pin = AUDIO_I2S_WS_PIN,
    .i2s_data_pin = AUDIO_I2S_DATA_PIN,
    .amplifier_pin = AUDIO_AMP_PIN,
    .sample_rate = AUDIO_SAMPLE_RATE,
    .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
    .channels = AUDIO_CHANNELS
};

// 系统配置
static hal_system_config_t s_system_config = {
    .cpu_frequency = CPU_FREQUENCY_MHZ,
    .flash_size = FLASH_SIZE_MB,
    .psram_size = PSRAM_SIZE_MB,
    .watchdog_enabled = WATCHDOG_ENABLED,
    .watchdog_timeout = WATCHDOG_TIMEOUT_S
};

// 传感器显示信息列表 - 用于LCD动态UI
static const bsp_sensor_display_info_t s_sensor_display_list[] = {
    {
        .name = "DHT11",
        .unit = "C / %",
        .gpio_pin = DHT11_GPIO_PIN
    },
    {
        .name = "Rain",
        .unit = "",
        .gpio_pin = RAIN_SENSOR_GPIO_PIN
    }
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
    .has_ethernet = HAS_ETHERNET,
    .sensor_display_list = s_sensor_display_list,
    .sensor_display_count = sizeof(s_sensor_display_list) / sizeof(s_sensor_display_list[0])
};

// 硬件配置 (使用函数返回，避免静态初始化问题)
static bsp_hardware_config_t s_hardware_config;

// ==================== BSP接口实现 ====================

static hal_err_t esp32_s3_devkit_rain_init(void)
{
    printf("BSP: Initializing ESP32-S3 DevKit...\n");
    
    // 1. 初始化LED GPIO
    hal_err_t ret = esp32_s3_devkit_rain_init_leds();
    if (ret != HAL_OK) {
        printf("BSP: Failed to initialize LEDs\n");
        return ret;
    }
    
    // 2. 初始化按键GPIO
    ret = esp32_s3_devkit_rain_init_buttons();
    if (ret != HAL_OK) {
        printf("BSP: Failed to initialize buttons\n");
        return ret;
    }
    
    // 3. 初始化继电器GPIO
    ret = esp32_s3_devkit_rain_init_relays();
    if (ret != HAL_OK) {
        printf("BSP: Failed to initialize relays\n");
        return ret;
    }
    
    // 4. 初始化舵机PWM
    ret = esp32_s3_devkit_rain_init_servos();
    if (ret != HAL_OK) {
        printf("BSP: Failed to initialize servos\n");
        return ret;
    }
    
    printf("BSP: ESP32-S3 DevKit initialized successfully\n");
    return HAL_OK;
}

static hal_err_t esp32_s3_devkit_rain_deinit(void)
{
    printf("BSP: Deinitializing ESP32-S3 DevKit...\n");
    
    // TODO: 实现具体的硬件去初始化
    
    printf("BSP: ESP32-S3 DevKit deinitialized\n");
    return HAL_OK;
}

static const bsp_board_info_t* esp32_s3_devkit_rain_get_board_info(void)
{
    return &s_board_info;
}

static const bsp_hardware_config_t* esp32_s3_devkit_rain_get_hardware_config(void)
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
    s_hardware_config.display_config = s_display_config;
    s_hardware_config.audio_config = s_audio_config;
    s_hardware_config.button_count = BUTTON_COUNT;
    s_hardware_config.button_pins = s_button_pins;
    s_hardware_config.system_config = s_system_config;
    
    return &s_hardware_config;
}

// 传感器控制函数
static hal_err_t esp32_s3_devkit_rain_sensor_init(void)
{
    printf("BSP: Initializing sensors (DHT11 for Rain board)...\n");
    
    hal_err_t result = HAL_OK;
    bool dht11_ok = false;
    
    // 初始化DHT11传感器
    dht11_config_t dht11_config = {
        .data_pin = DHT11_GPIO_PIN
    };
    
    esp_err_t dht11_ret = dht11_init_adapter(&dht11_config);
    if (dht11_ret == ESP_OK) {
        printf("BSP: ✅ DHT11 initialized successfully on GPIO%d\n", DHT11_GPIO_PIN);
        dht11_ok = true;
    } else {
        printf("BSP: ⚠️ DHT11 initialization failed (error: %d) - 将继续运行，DHT11数据不可用\n", dht11_ret);
        // 不设置result = HAL_ERROR，允许其他传感器继续工作
    }
    
    // Rain板子不使用DS18B20，GPIO39用于雨水传感器（在main.c中初始化）
    printf("BSP: ℹ️ Rain板子：DS18B20已禁用，GPIO39用于雨水传感器（在main.c中初始化）\n");
    
    // 只要DHT11成功，就返回HAL_OK
    if (dht11_ok) {
        printf("BSP: ✅ Sensor initialization completed - DHT11: OK\n");
        result = HAL_OK;
    } else {
        printf("BSP: ⚠️ DHT11 failed to initialize - 系统将继续运行，雨水传感器将在main.c中初始化\n");
        // 即使DHT11失败，也返回HAL_OK，不阻塞系统启动
        result = HAL_OK;
    }
    
    return result;
}

static hal_err_t esp32_s3_devkit_rain_sensor_deinit(void)
{
    printf("BSP: Deinitializing sensors...\n");
    // 传感器去初始化逻辑（如果需要）
    return HAL_OK;
}

static hal_err_t esp32_s3_devkit_rain_sensor_read(uint8_t sensor_id, float* value)
{
    if (value == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    
    // 这是一个占位函数，实际的传感器读取在main.c中通过DHT11/DS18B20驱动完成
    // 这里返回HAL_OK表示传感器模块可用
    return HAL_OK;
}

// BSP接口结构体
static const bsp_interface_t s_esp32_s3_devkit_rain_interface = {
    .init = esp32_s3_devkit_rain_init,
    .deinit = esp32_s3_devkit_rain_deinit,
    .get_board_info = esp32_s3_devkit_rain_get_board_info,
    .get_hw_config = esp32_s3_devkit_rain_get_hardware_config,
    
    // 传感器功能
    .sensor_init = esp32_s3_devkit_rain_sensor_init,
    .sensor_deinit = esp32_s3_devkit_rain_sensor_deinit,
    .sensor_read = esp32_s3_devkit_rain_sensor_read,
    
    // 蓝牙配网功能 - 临时禁用
    /*
    .bt_provision_init = esp32_s3_devkit_rain_bt_provision_init,
    .bt_provision_deinit = esp32_s3_devkit_rain_bt_provision_deinit,
    .bt_provision_start = esp32_s3_devkit_rain_bt_provision_start,
    .bt_provision_stop = esp32_s3_devkit_rain_bt_provision_stop
    */
};

// ==================== 公共接口 ====================

hal_err_t bsp_esp32_s3_devkit_rain_register(void)
{
    printf("BSP: Registering ESP32-S3 DevKit interface...\n");
    return bsp_register_interface(&s_esp32_s3_devkit_rain_interface);
}

void bsp_esp32_s3_devkit_rain_print_config(void)
{
    printf("=== ESP32-S3 DevKit Configuration ===\n");
    printf("Board: %s\n", BOARD_NAME);
    printf("Chip: %s\n", CHIP_MODEL);
    printf("Version: %s\n", BOARD_VERSION);
    printf("Flash: %d MB\n", FLASH_SIZE_MB);
    printf("PSRAM: %d MB\n", PSRAM_SIZE_MB);
    printf("LEDs: %d\n", LED_COUNT);
    printf("Relays: %d\n", RELAY_COUNT);
    printf("Servos: %d\n", SERVO_COUNT);
    printf("Sensors: %d\n", SENSOR_COUNT);
    printf("Buttons: %d\n", BUTTON_COUNT);
    printf("====================================\n");
}

// ==================== 硬件控制函数实现 ====================

/**
 * @brief 初始化LED GPIO和PWM
 */
static hal_err_t esp32_s3_devkit_rain_init_leds(void)
{
    printf("BSP: Initializing LEDs...\n");
    
#ifdef ESP_PLATFORM
    esp_err_t ret;
    
    // 初始化LED1 (GPIO48 - 板载LED)
    gpio_config_t led1_config = {
        .pin_bit_mask = (1ULL << LED1_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&led1_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure LED1 GPIO\n");
        return HAL_ERROR;
    }
    
    // 设置LED1初始状态为关闭
    gpio_set_level(LED1_GPIO_PIN, !LED1_ACTIVE_LEVEL);
    
    // 如果启用PWM，配置LEDC
    if (LED1_PWM_ENABLED) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_0,
            .duty_resolution = LED1_PWM_RESOLUTION,
            .freq_hz = LED1_PWM_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ret = ledc_timer_config(&ledc_timer);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED1 PWM timer\n");
            return HAL_ERROR;
        }
        
        ledc_channel_config_t ledc_channel = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .timer_sel = LEDC_TIMER_0,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = LED1_GPIO_PIN,
            .duty = 0,
            .hpoint = 0
        };
        ret = ledc_channel_config(&ledc_channel);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED1 PWM channel\n");
            return HAL_ERROR;
        }
    }
    
    // 初始化LED2 (GPIO41 - 外接LED)
    gpio_config_t led2_config = {
        .pin_bit_mask = (1ULL << LED2_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&led2_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure LED2 GPIO\n");
        return HAL_ERROR;
    }
    
    // 设置LED2初始状态为关闭
    gpio_set_level(LED2_GPIO_PIN, !LED2_ACTIVE_LEVEL);
    
    // 如果启用PWM，配置LEDC
    if (LED2_PWM_ENABLED) {
        ledc_timer_config_t ledc_timer2 = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_1,
            .duty_resolution = LED2_PWM_RESOLUTION,
            .freq_hz = LED2_PWM_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ret = ledc_timer_config(&ledc_timer2);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED2 PWM timer\n");
            return HAL_ERROR;
        }
        
        ledc_channel_config_t ledc_channel2 = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .timer_sel = LEDC_TIMER_1,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = LED2_GPIO_PIN,
            .duty = 0,
            .hpoint = 0
        };
        ret = ledc_channel_config(&ledc_channel2);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED2 PWM channel\n");
            return HAL_ERROR;
        }
    }
    
    // 初始化LED3 (GPIO37 - 外接LED)
    gpio_config_t led3_config = {
        .pin_bit_mask = (1ULL << LED3_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&led3_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure LED3 GPIO\n");
        return HAL_ERROR;
    }
    
    // 设置LED3初始状态为关闭
    gpio_set_level(LED3_GPIO_PIN, !LED3_ACTIVE_LEVEL);
    
    // 如果启用PWM，配置LEDC
    if (LED3_PWM_ENABLED) {
        ledc_timer_config_t ledc_timer3 = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_2,
            .duty_resolution = LED3_PWM_RESOLUTION,
            .freq_hz = LED3_PWM_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ret = ledc_timer_config(&ledc_timer3);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED3 PWM timer\n");
            return HAL_ERROR;
        }
        
        ledc_channel_config_t ledc_channel3 = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_2,
            .timer_sel = LEDC_TIMER_2,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = LED3_GPIO_PIN,
            .duty = 0,
            .hpoint = 0
        };
        ret = ledc_channel_config(&ledc_channel3);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED3 PWM channel\n");
            return HAL_ERROR;
        }
    }
    
    // 初始化LED4 (GPIO36 - 外接LED)
    gpio_config_t led4_config = {
        .pin_bit_mask = (1ULL << LED4_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&led4_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure LED4 GPIO\n");
        return HAL_ERROR;
    }
    
    // 设置LED4初始状态为关闭
    gpio_set_level(LED4_GPIO_PIN, !LED4_ACTIVE_LEVEL);
    
    // 如果启用PWM，配置LEDC
    if (LED4_PWM_ENABLED) {
        ledc_timer_config_t ledc_timer4 = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_3,
            .duty_resolution = LED4_PWM_RESOLUTION,
            .freq_hz = LED4_PWM_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ret = ledc_timer_config(&ledc_timer4);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED4 PWM timer\n");
            return HAL_ERROR;
        }
        
        ledc_channel_config_t ledc_channel4 = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_3,
            .timer_sel = LEDC_TIMER_3,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = LED4_GPIO_PIN,
            .duty = 0,
            .hpoint = 0
        };
        ret = ledc_channel_config(&ledc_channel4);
        if (ret != ESP_OK) {
            printf("BSP: Failed to configure LED4 PWM channel\n");
            return HAL_ERROR;
        }
    }
    
#endif
    
    printf("BSP: LEDs initialized successfully\n");
    return HAL_OK;
}

/**
 * @brief 初始化按键GPIO
 */
static hal_err_t esp32_s3_devkit_rain_init_buttons(void)
{
    printf("BSP: Initializing buttons...\n");
    
#ifdef ESP_PLATFORM
    esp_err_t ret;
    
    // 初始化BOOT按键 (GPIO0)
    gpio_config_t boot_button_config = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&boot_button_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure BOOT button GPIO\n");
        return HAL_ERROR;
    }
    
    // 初始化用户按键 (GPIO7)
    gpio_config_t user_button_config = {
        .pin_bit_mask = (1ULL << USER_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&user_button_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure USER button GPIO\n");
        return HAL_ERROR;
    }
    
#endif
    
    printf("BSP: Buttons initialized successfully\n");
    return HAL_OK;
}

/**
 * @brief 初始化继电器GPIO
 */
static hal_err_t esp32_s3_devkit_rain_init_relays(void)
{
    printf("BSP: Initializing relays...\n");
    
#ifdef ESP_PLATFORM
    esp_err_t ret;
    
    // 初始化继电器1 (GPIO4)
    gpio_config_t relay1_config = {
        .pin_bit_mask = (1ULL << RELAY1_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&relay1_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure RELAY1 GPIO\n");
        return HAL_ERROR;
    }
    
    // 设置继电器1初始状态为关闭
    gpio_set_level(RELAY1_GPIO_PIN, !RELAY1_ACTIVE_LEVEL);
    
    // 初始化继电器2 (GPIO5)
    gpio_config_t relay2_config = {
        .pin_bit_mask = (1ULL << RELAY2_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&relay2_config);
    if (ret != ESP_OK) {
        printf("BSP: Failed to configure RELAY2 GPIO\n");
        return HAL_ERROR;
    }
    
    // 设置继电器2初始状态为关闭
    gpio_set_level(RELAY2_GPIO_PIN, !RELAY2_ACTIVE_LEVEL);
    
#endif
    
    printf("BSP: Relays initialized successfully\n");
    return HAL_OK;
}

/**
 * @brief 控制LED开关
 * @param led_index LED索引 (0=LED1, 1=LED2, 2=LED3, 3=LED4)
 * @param state LED状态 (true=开, false=关)
 */
static hal_err_t esp32_s3_devkit_rain_led_control(uint8_t led_index, bool state)
{
    if (led_index >= LED_COUNT) {
        return HAL_ERROR_INVALID_PARAM;
    }
    
#ifdef ESP_PLATFORM
    // 使用配置数组获取LED参数
    hal_led_config_t *led_config = &s_led_configs[led_index];
    gpio_num_t pin = led_config->pin;
    bool active_level = led_config->active_level;
    bool pwm_enabled = led_config->pwm_enabled;
    
    if (pwm_enabled) {
        // 使用PWM控制LED (M1-M2使用)
        uint32_t duty = state ? ((1 << led_config->pwm_resolution) - 1) : 0;
        if (!active_level) {
            duty = ((1 << led_config->pwm_resolution) - 1) - duty;
        }
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)led_index, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)led_index);
    } else {
        // 使用GPIO直接控制LED (L1-L4使用)
        int level = state ? active_level : !active_level;
        gpio_set_level(pin, level);
    }
    
    printf("BSP: LED%d %s (GPIO%d)\n", led_index + 1, state ? "ON" : "OFF", pin);
#else
    printf("BSP: LED%d %s (simulated)\n", led_index + 1, state ? "ON" : "OFF");
#endif
    
    return HAL_OK;
}

/**
 * @brief 设置LED亮度 (PWM模式)
 * @param led_index LED索引 (0=LED1, 1=LED2)
 * @param brightness 亮度值 (0-255)
 */
static hal_err_t esp32_s3_devkit_rain_led_set_brightness(uint8_t led_index, uint8_t brightness)
{
    if (led_index >= LED_COUNT) {
        return HAL_ERROR_INVALID_PARAM;
    }
    
#ifdef ESP_PLATFORM
    if (led_index == 0 && LED1_PWM_ENABLED) {
        // 计算占空比 (8位分辨率: 0-255)
        uint32_t duty = (brightness * ((1 << LED1_PWM_RESOLUTION) - 1)) / 255;
        
        esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        if (ret != ESP_OK) {
            return HAL_ERROR;
        }
        
        ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        if (ret != ESP_OK) {
            return HAL_ERROR;
        }
        
        printf("BSP: LED%d brightness set to %d\n", led_index + 1, brightness);
    } else {
        // 非PWM模式，只能开关控制
        return esp32_s3_devkit_rain_led_control(led_index, brightness > 127);
    }
#else
    printf("BSP: LED%d brightness set to %d (simulated)\n", led_index + 1, brightness);
#endif
    
    return HAL_OK;
}

// ==================== 公共API函数 ====================

/**
 * @brief 控制LED1 (板载LED)
 * @param state LED状态 (true=开, false=关)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led1_control(bool state)
{
    return esp32_s3_devkit_rain_led_control(0, state);
}

/**
 * @brief 设置LED1亮度
 * @param brightness 亮度值 (0-255)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led1_set_brightness(uint8_t brightness)
{
    return esp32_s3_devkit_rain_led_set_brightness(0, brightness);
}

/**
 * @brief 控制LED2 (外接LED)
 * @param state LED状态 (true=开, false=关)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led2_control(bool state)
{
    return esp32_s3_devkit_rain_led_control(1, state);
}

/**
 * @brief 设置LED2亮度
 * @param brightness 亮度值 (0-255)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led2_set_brightness(uint8_t brightness)
{
    return esp32_s3_devkit_rain_led_set_brightness(1, brightness);
}

/**
 * @brief 控制LED3 (外接LED)
 * @param state LED状态 (true=开, false=关)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led3_control(bool state)
{
    return esp32_s3_devkit_rain_led_control(2, state);
}

/**
 * @brief 设置LED3亮度
 * @param brightness 亮度值 (0-255)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led3_set_brightness(uint8_t brightness)
{
    return esp32_s3_devkit_rain_led_set_brightness(2, brightness);
}

/**
 * @brief 控制LED4 (外接LED)
 * @param state LED状态 (true=开, false=关)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led4_control(bool state)
{
    return esp32_s3_devkit_rain_led_control(3, state);
}

/**
 * @brief 设置LED4亮度
 * @param brightness 亮度值 (0-255)
 */
hal_err_t bsp_esp32_s3_devkit_rain_led4_set_brightness(uint8_t brightness)
{
    return esp32_s3_devkit_rain_led_set_brightness(3, brightness);
}

// ==================== 继电器控制功能实现 ====================

/**
 * @brief 继电器控制函数
 * 
 * @param relay_index 继电器索引 (0-1)
 * @param state 继电器状态 (true=开启, false=关闭)
 * @return hal_err_t 操作结果
 */
static hal_err_t esp32_s3_devkit_rain_relay_control(uint8_t relay_index, bool state)
{
    if (relay_index >= RELAY_COUNT) {
        printf("BSP: Invalid relay index: %d\n", relay_index);
        return HAL_ERROR_INVALID_PARAM;
    }
    
    hal_relay_config_t *config = &s_relay_configs[relay_index];
    
    // 设置GPIO电平
    uint32_t level = state ? config->active_level : !config->active_level;
    
#ifdef ESP_PLATFORM
    esp_err_t ret = gpio_set_level(config->pin, level);
    if (ret != ESP_OK) {
        printf("BSP: Failed to set relay%d GPIO level\n", relay_index + 1);
        return HAL_ERROR;
    }
#else
    printf("BSP: Relay%d GPIO%d set to %s\n", 
           relay_index + 1, config->pin, level ? "HIGH" : "LOW");
#endif
    
    printf("BSP: Relay%d %s\n", relay_index + 1, state ? "ON" : "OFF");
    
    // 添加开关延迟
    if (config->switch_delay_ms > 0) {
#ifdef ESP_PLATFORM
        vTaskDelay(pdMS_TO_TICKS(config->switch_delay_ms));
#else
        // 非ESP-IDF环境的延迟模拟
        printf("BSP: Relay switch delay: %dms\n", config->switch_delay_ms);
#endif
    }
    
    return HAL_OK;
}

/**
 * @brief 继电器1控制函数
 * 
 * @param state 继电器状态 (true=开启, false=关闭)
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_s3_devkit_rain_relay1_control(bool state)
{
    return esp32_s3_devkit_rain_relay_control(0, state);
}

/**
 * @brief 继电器2控制函数
 * 
 * @param state 继电器状态 (true=开启, false=关闭)
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_s3_devkit_rain_relay2_control(bool state)
{
    return esp32_s3_devkit_rain_relay_control(1, state);
}

/**
 * @brief 通用继电器控制函数
 * 
 * @param relay_index 继电器索引 (0-1)
 * @param state 继电器状态 (true=开启, false=关闭)
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_s3_devkit_rain_relay_control(uint8_t relay_index, bool state)
{
    return esp32_s3_devkit_rain_relay_control(relay_index, state);
}

// ==================== 舵机控制实现 ====================

/**
 * @brief 初始化舵机PWM
 */
static hal_err_t esp32_s3_devkit_rain_init_servos(void)
{
#ifdef ESP_PLATFORM
    printf("BSP: Initializing servos...\n");
    
    // 为每个舵机配置LEDC定时器和通道
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        hal_servo_config_t *config = &s_servo_configs[i];
        
        // 配置LEDC定时器（每个舵机使用独立的定时器）
        ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = (ledc_timer_t)(LEDC_TIMER_0 + i),  // 使用不同的定时器
            .duty_resolution = LEDC_TIMER_13_BIT,  // 13位分辨率
            .freq_hz = config->frequency,  // 50Hz
            .clk_cfg = LEDC_AUTO_CLK
        };
        
        esp_err_t ret = ledc_timer_config(&timer_config);
        if (ret != ESP_OK) {
            ESP_LOGE("BSP", "Failed to configure servo%d timer: %s", i + 1, esp_err_to_name(ret));
            return HAL_ERROR;
        }
        
        // 配置LEDC通道
        ledc_channel_config_t channel_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)(LEDC_CHANNEL_0 + i),  // 使用不同的通道
            .timer_sel = (ledc_timer_t)(LEDC_TIMER_0 + i),
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = config->pin,
            .duty = 0,
            .hpoint = 0
        };
        
        ret = ledc_channel_config(&channel_config);
        if (ret != ESP_OK) {
            ESP_LOGE("BSP", "Failed to configure servo%d channel: %s", i + 1, esp_err_to_name(ret));
            return HAL_ERROR;
        }
        
        // 设置初始状态为停止（对于360度连续旋转舵机，1500us为停止位置）
        // 对于180度定位舵机，也是中间位置（90度）
        // 计算中间脉宽：(min + max) / 2
        uint32_t neutral_pulse_us = (config->min_pulse_us + config->max_pulse_us) / 2;
        // 如果配置的脉宽范围是500-2500us，中间值是1500us（360度舵机停止位置）
        // 如果配置的脉宽范围是1000-2000us，中间值是1500us（360度舵机停止位置）
        uint32_t pulse_width_us = neutral_pulse_us;
        uint32_t duty = (uint32_t)((float)pulse_width_us / (1000000.0f / config->frequency) * ((1 << 13) - 1));
        
        ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(LEDC_CHANNEL_0 + i), duty);
        if (ret != ESP_OK) {
            ESP_LOGE("BSP", "Failed to set servo%d initial duty: %s", i + 1, esp_err_to_name(ret));
            return HAL_ERROR;
        }
        
        ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(LEDC_CHANNEL_0 + i));
        if (ret != ESP_OK) {
            ESP_LOGE("BSP", "Failed to update servo%d duty: %s", i + 1, esp_err_to_name(ret));
            return HAL_ERROR;
        }
        
        ESP_LOGI("BSP", "Servo%d initialized on GPIO%d (initial pulse: %lu us, stopped)", 
                 i + 1, config->pin, pulse_width_us);
    }
    
    printf("BSP: Servos initialized successfully\n");
#else
    printf("BSP: Initializing servos (simulation)...\n");
    printf("BSP: Servos initialized successfully\n");
#endif
    return HAL_OK;
}

/**
 * @brief 设置舵机角度
 */
static hal_err_t esp32_s3_devkit_rain_servo_set_angle(uint8_t servo_index, uint16_t angle)
{
    if (servo_index >= SERVO_COUNT) {
        ESP_LOGE("BSP", "Invalid servo index: %d", servo_index);
        return HAL_ERROR_INVALID_PARAM;
    }
    
    hal_servo_config_t *config = &s_servo_configs[servo_index];
    
    // 限制角度范围
    if (angle > config->max_angle) {
        angle = config->max_angle;
    }
    
    // 计算脉宽（微秒）
    // 角度0度对应min_pulse_us，角度max_angle度对应max_pulse_us
    uint32_t pulse_width_us = config->min_pulse_us + 
                               (uint32_t)((float)angle / config->max_angle * 
                                         (config->max_pulse_us - config->min_pulse_us));
    
#ifdef ESP_PLATFORM
    // 计算占空比（13位分辨率）
    uint32_t period_us = 1000000 / config->frequency;  // 20ms (50Hz)
    uint32_t duty = (uint32_t)((float)pulse_width_us / period_us * ((1 << 13) - 1));
    
    ledc_channel_t channel = (ledc_channel_t)(LEDC_CHANNEL_0 + servo_index);
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE("BSP", "Failed to set servo%d duty: %s", servo_index + 1, esp_err_to_name(ret));
        return HAL_ERROR;
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (ret != ESP_OK) {
        ESP_LOGE("BSP", "Failed to update servo%d duty: %s", servo_index + 1, esp_err_to_name(ret));
        return HAL_ERROR;
    }
    
    ESP_LOGI("BSP", "Servo%d angle set to %d degrees (pulse: %lu us, duty: %lu)", 
             servo_index + 1, angle, pulse_width_us, duty);
#else
    printf("BSP: Servo%d angle set to %d degrees (pulse: %lu us) (simulation)\n", 
           servo_index + 1, angle, (unsigned long)pulse_width_us);
#endif
    
    return HAL_OK;
}

/**
 * @brief 设置舵机1角度
 */
hal_err_t bsp_esp32_s3_devkit_rain_servo1_set_angle(uint16_t angle)
{
    return esp32_s3_devkit_rain_servo_set_angle(0, angle);
}

/**
 * @brief 设置舵机2角度
 */
hal_err_t bsp_esp32_s3_devkit_rain_servo2_set_angle(uint16_t angle)
{
    return esp32_s3_devkit_rain_servo_set_angle(1, angle);
}

/**
 * @brief 通用舵机控制函数
 */
hal_err_t bsp_esp32_s3_devkit_rain_servo_set_angle(uint8_t servo_index, uint16_t angle)
{
    return esp32_s3_devkit_rain_servo_set_angle(servo_index, angle);
}

// ==================== 蓝牙配网功能实现 - 临时禁用 ====================

/*
static hal_err_t esp32_s3_devkit_rain_bt_provision_init(const bt_provision_config_t* config)
{
    if (config == NULL) {
        printf("BSP: Invalid Bluetooth provisioning config\n");
        return HAL_ERROR_INVALID_PARAM;
    }
    
    printf("BSP: Initializing Bluetooth provisioning for ESP32-S3 DevKit...\n");
    
    // 调用蓝牙配网模块的初始化函数
    bt_provision_err_t ret = bt_provision_init(config);
    if (ret != BT_PROVISION_ERR_OK) {
        printf("BSP: Failed to initialize Bluetooth provisioning: %s\n", 
               bt_provision_get_error_string(ret));
        return HAL_ERROR;
    }
    
    printf("BSP: Bluetooth provisioning initialized successfully\n");
    return HAL_OK;
}

static hal_err_t esp32_s3_devkit_rain_bt_provision_deinit(void)
{
    printf("BSP: Deinitializing Bluetooth provisioning...\n");
    
    bt_provision_err_t ret = bt_provision_deinit();
    if (ret != BT_PROVISION_ERR_OK) {
        printf("BSP: Failed to deinitialize Bluetooth provisioning: %s\n", 
               bt_provision_get_error_string(ret));
        return HAL_ERROR;
    }
    
    printf("BSP: Bluetooth provisioning deinitialized successfully\n");
    return HAL_OK;
}

static hal_err_t esp32_s3_devkit_rain_bt_provision_start(void)
{
    printf("BSP: Starting Bluetooth provisioning...\n");
    
    bt_provision_err_t ret = bt_provision_start();
    if (ret != BT_PROVISION_ERR_OK) {
        printf("BSP: Failed to start Bluetooth provisioning: %s\n", 
               bt_provision_get_error_string(ret));
        return HAL_ERROR;
    }
    
    printf("BSP: Bluetooth provisioning started successfully\n");
    return HAL_OK;
}

static hal_err_t esp32_s3_devkit_rain_bt_provision_stop(void)
{
    printf("BSP: Stopping Bluetooth provisioning...\n");
    
    bt_provision_err_t ret = bt_provision_stop();
    if (ret != BT_PROVISION_ERR_OK) {
        printf("BSP: Failed to stop Bluetooth provisioning: %s\n", 
               bt_provision_get_error_string(ret));
        return HAL_ERROR;
    }
    
    printf("BSP: Bluetooth provisioning stopped successfully\n");
    return HAL_OK;
}
*/