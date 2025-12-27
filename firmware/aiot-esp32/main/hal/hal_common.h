/**
 * @file hal_common.h
 * @brief 硬件抽象层通用定义
 * 
 * 提供统一的硬件接口，屏蔽不同芯片和开发板的差异
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#include <stdint.h>
#include <stdbool.h>

// 根据编译环境选择类型定义
#ifdef ESP_PLATFORM
// ESP-IDF环境，使用原生类型
#include <esp_err.h>
#include <hal/gpio_types.h>
#else
// 非ESP-IDF环境，使用模拟类型
typedef int esp_err_t;
typedef int gpio_num_t;
typedef int gpio_mode_t;
typedef int gpio_pull_mode_t;
typedef int gpio_int_type_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HAL错误代码定义
 */
typedef enum {
    HAL_OK = 0,                    ///< 操作成功
    HAL_ERROR = -1,                ///< 通用错误
    HAL_ERROR_INVALID_PARAM = -2,  ///< 无效参数
    HAL_ERROR_NOT_SUPPORTED = -3,  ///< 不支持的操作
    HAL_ERROR_TIMEOUT = -4,        ///< 操作超时
    HAL_ERROR_NO_MEMORY = -5,      ///< 内存不足
    HAL_ERROR_BUSY = -6,           ///< 设备忙
    HAL_ERROR_NOT_INITIALIZED = -7 ///< 未初始化
} hal_err_t;

/**
 * @brief GPIO配置结构体
 */
typedef struct {
    gpio_num_t pin;           ///< GPIO引脚号
    gpio_mode_t mode;         ///< GPIO模式
    gpio_pull_mode_t pull;    ///< 上下拉配置
    gpio_int_type_t intr;     ///< 中断类型
    bool output_invert;       ///< 输出反转
} hal_gpio_config_t;

/**
 * @brief LED配置结构体
 */
typedef struct {
    gpio_num_t pin;           ///< LED GPIO引脚
    bool active_level;        ///< 有效电平 (true=高电平有效, false=低电平有效)
    bool pwm_enabled;         ///< 是否启用PWM调光
    uint32_t pwm_frequency;   ///< PWM频率 (Hz)
    uint8_t pwm_resolution;   ///< PWM分辨率 (位数)
} hal_led_config_t;

/**
 * @brief 继电器配置结构体
 */
typedef struct {
    gpio_num_t pin;           ///< 继电器控制GPIO引脚
    bool active_level;        ///< 有效电平 (true=高电平有效, false=低电平有效)
    uint32_t switch_delay_ms; ///< 开关延迟时间 (毫秒)
} hal_relay_config_t;

/**
 * @brief 舵机配置结构体
 */
typedef struct {
    gpio_num_t pin;           ///< 舵机控制GPIO引脚
    uint32_t frequency;       ///< PWM频率 (Hz)
    uint32_t min_pulse_us;    ///< 最小脉宽 (微秒)
    uint32_t max_pulse_us;    ///< 最大脉宽 (微秒)
    uint16_t max_angle;       ///< 最大角度 (度)
} hal_servo_config_t;

/**
 * @brief 传感器类型枚举
 */
typedef enum {
    HAL_SENSOR_TYPE_TEMPERATURE,  ///< 温度传感器
    HAL_SENSOR_TYPE_HUMIDITY,     ///< 湿度传感器
    HAL_SENSOR_TYPE_PRESSURE,     ///< 压力传感器
    HAL_SENSOR_TYPE_LIGHT,        ///< 光照传感器
    HAL_SENSOR_TYPE_MOTION,       ///< 运动传感器
    HAL_SENSOR_TYPE_DISTANCE,     ///< 距离传感器
    HAL_SENSOR_TYPE_SOUND,        ///< 声音传感器
    HAL_SENSOR_TYPE_GAS,          ///< 气体传感器
    HAL_SENSOR_TYPE_CUSTOM        ///< 自定义传感器
} hal_sensor_type_t;

/**
 * @brief 传感器数据结构体
 */
typedef struct {
    hal_sensor_type_t type;   ///< 传感器类型
    float value;              ///< 传感器数值
    uint32_t timestamp;       ///< 时间戳
    bool valid;               ///< 数据有效性
} hal_sensor_data_t;

/**
 * @brief 显示屏类型枚举
 */
typedef enum {
    HAL_DISPLAY_TYPE_NONE,    ///< 无显示屏
    HAL_DISPLAY_TYPE_OLED,    ///< OLED显示屏
    HAL_DISPLAY_TYPE_LCD,     ///< LCD显示屏
    HAL_DISPLAY_TYPE_EINK,    ///< 电子墨水屏
    HAL_DISPLAY_TYPE_LED_MATRIX ///< LED矩阵
} hal_display_type_t;

/**
 * @brief 显示屏配置结构体
 */
typedef struct {
    hal_display_type_t type;  ///< 显示屏类型
    uint16_t width;           ///< 宽度 (像素)
    uint16_t height;          ///< 高度 (像素)
    uint8_t color_depth;      ///< 颜色深度 (位)
    gpio_num_t reset_pin;     ///< 复位引脚
    gpio_num_t dc_pin;        ///< 数据/命令引脚
    gpio_num_t cs_pin;        ///< 片选引脚
    gpio_num_t backlight_pin; ///< 背光引脚
    bool backlight_active_level; ///< 背光有效电平
} hal_display_config_t;

/**
 * @brief 音频配置结构体
 */
typedef struct {
    gpio_num_t i2s_bclk_pin;  ///< I2S位时钟引脚
    gpio_num_t i2s_ws_pin;    ///< I2S字选择引脚
    gpio_num_t i2s_data_pin;  ///< I2S数据引脚
    gpio_num_t amplifier_pin; ///< 功放使能引脚
    uint32_t sample_rate;     ///< 采样率
    uint8_t bits_per_sample;  ///< 每样本位数
    uint8_t channels;         ///< 声道数
} hal_audio_config_t;

/**
 * @brief 网络配置结构体
 */
typedef struct {
    char ssid[32];            ///< WiFi SSID
    char password[64];        ///< WiFi密码
    char mqtt_broker[128];    ///< MQTT代理地址
    uint16_t mqtt_port;       ///< MQTT端口
    char mqtt_username[32];   ///< MQTT用户名
    char mqtt_password[64];   ///< MQTT密码
    char device_id[32];       ///< 设备ID
} hal_network_config_t;

/**
 * @brief 系统配置结构体
 */
typedef struct {
    uint32_t cpu_frequency;   ///< CPU频率 (MHz)
    uint32_t flash_size;      ///< Flash大小 (MB)
    uint32_t psram_size;      ///< PSRAM大小 (MB)
    bool watchdog_enabled;    ///< 看门狗使能
    uint32_t watchdog_timeout; ///< 看门狗超时时间 (秒)
} hal_system_config_t;

#ifdef __cplusplus
}
#endif

#endif // HAL_COMMON_H