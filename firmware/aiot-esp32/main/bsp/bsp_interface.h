/**
 * @file bsp_interface.h
 * @brief 板级支持包(BSP)统一接口
 * 
 * 定义所有开发板必须实现的统一接口，确保应用层代码的可移植性
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BSP_INTERFACE_H
#define BSP_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_common.h"
#include "../ota/ota_manager.h"
#include "../mqtt/aiot_mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 传感器显示信息结构体 - 用于LCD动态UI显示
 */
typedef struct {
    const char *name;             ///< 传感器名称，如"DHT11温湿度"
    const char *unit;             ///< 单位，如"°C / %"
    int gpio_pin;                 ///< GPIO引脚号
} bsp_sensor_display_info_t;

/**
 * @brief 板级信息结构体
 */
typedef struct {
    const char *board_name;       ///< 开发板名称
    const char *chip_model;       ///< 芯片型号
    const char *board_version;    ///< 板级版本
    const char *manufacturer;     ///< 制造商
    uint32_t flash_size_mb;       ///< Flash大小(MB)
    uint32_t psram_size_mb;       ///< PSRAM大小(MB)
    bool has_wifi;                ///< 是否支持WiFi
    bool has_ethernet;            ///< 是否支持以太网
    
    // 传感器显示信息
    const bsp_sensor_display_info_t *sensor_display_list;  ///< 传感器显示列表
    int sensor_display_count;     ///< 传感器显示数量
} bsp_board_info_t;

/**
 * @brief 板级硬件配置结构体
 */
typedef struct {
    // LED配置
    uint8_t led_count;                    ///< LED数量
    hal_led_config_t *led_configs;       ///< LED配置数组
    
    // 继电器配置
    uint8_t relay_count;                  ///< 继电器数量
    hal_relay_config_t *relay_configs;   ///< 继电器配置数组
    
    // 舵机配置
    uint8_t servo_count;                  ///< 舵机数量
    hal_servo_config_t *servo_configs;   ///< 舵机配置数组
    
    // 传感器配置
    uint8_t sensor_count;                 ///< 传感器数量
    hal_sensor_type_t *sensor_types;     ///< 传感器类型数组
    
    // 显示屏配置
    hal_display_config_t display_config;  ///< 显示屏配置
    
    // 音频配置
    hal_audio_config_t audio_config;      ///< 音频配置
    
    // 按键配置
    uint8_t button_count;                 ///< 按键数量
    gpio_num_t *button_pins;              ///< 按键GPIO数组
    
    // 系统配置
    hal_system_config_t system_config;    ///< 系统配置
} bsp_hardware_config_t;

/**
 * @brief BSP初始化函数类型
 */
typedef hal_err_t (*bsp_init_func_t)(void);

/**
 * @brief BSP去初始化函数类型
 */
typedef hal_err_t (*bsp_deinit_func_t)(void);

/**
 * @brief BSP获取板级信息函数类型
 */
typedef const bsp_board_info_t* (*bsp_get_board_info_func_t)(void);

/**
 * @brief BSP获取硬件配置函数类型
 */
typedef const bsp_hardware_config_t* (*bsp_get_hardware_config_func_t)(void);

// 类型定义
// mqtt_config_t在aiot_mqtt_client.h中定义
// 注意：旧的ota_config_t已弃用，使用新的startup_manager代替

/**
 * @brief BSP MQTT客户端初始化函数类型
 */
typedef hal_err_t (*bsp_mqtt_init_func_t)(const mqtt_config_t* config);

/**
 * @brief BSP MQTT客户端去初始化函数类型
 */
typedef hal_err_t (*bsp_mqtt_deinit_func_t)(void);

/**
 * @brief BSP MQTT客户端连接函数类型
 */
typedef hal_err_t (*bsp_mqtt_connect_func_t)(void);

/**
 * @brief BSP MQTT客户端断开函数类型
 */
typedef hal_err_t (*bsp_mqtt_disconnect_func_t)(void);

/**
 * @brief BSP OTA管理器初始化函数类型
 */
typedef hal_err_t (*bsp_ota_init_func_t)(void);

/**
 * @brief BSP OTA管理器去初始化函数类型
 */
typedef hal_err_t (*bsp_ota_deinit_func_t)(void);

/**
 * @brief BSP OTA检查更新函数类型
 */
typedef hal_err_t (*bsp_ota_check_update_func_t)(const char* url);

/**
 * @brief BSP OTA开始更新函数类型 (已弃用 - 使用新的startup_manager)
 */
// typedef hal_err_t (*bsp_ota_start_update_func_t)(const ota_config_t* config);

/**
 * @brief BSP WiFi初始化函数类型
 */
typedef hal_err_t (*bsp_wifi_init_func_t)(void);

/**
 * @brief BSP WiFi去初始化函数类型
 */
typedef hal_err_t (*bsp_wifi_deinit_func_t)(void);

/**
 * @brief BSP WiFi连接函数类型
 */
typedef hal_err_t (*bsp_wifi_connect_func_t)(const char* ssid, const char* password);

/**
 * @brief BSP WiFi断开函数类型
 */
typedef hal_err_t (*bsp_wifi_disconnect_func_t)(void);

/**
 * @brief BSP传感器初始化函数类型
 */
typedef hal_err_t (*bsp_sensor_init_func_t)(void);

/**
 * @brief BSP传感器去初始化函数类型
 */
typedef hal_err_t (*bsp_sensor_deinit_func_t)(void);

/**
 * @brief BSP传感器读取函数类型
 */
typedef hal_err_t (*bsp_sensor_read_func_t)(uint8_t sensor_id, float* value);

/**
 * @brief BSP系统监控初始化函数类型
 */
typedef hal_err_t (*bsp_system_monitor_init_func_t)(void);

/**
 * @brief BSP系统监控获取状态函数类型
 */
typedef hal_err_t (*bsp_system_monitor_get_status_func_t)(void* status);

/**
 * @brief BSP接口结构体
 * 
 * 每个开发板都必须实现这些接口函数
 */
typedef struct {
    bsp_init_func_t init;                           ///< 初始化函数
    bsp_deinit_func_t deinit;                       ///< 去初始化函数
    bsp_get_board_info_func_t get_board_info;       ///< 获取板级信息
    bsp_get_hardware_config_func_t get_hw_config;   ///< 获取硬件配置
    
    // MQTT客户端相关函数
    bsp_mqtt_init_func_t mqtt_init;
    bsp_mqtt_deinit_func_t mqtt_deinit;
    bsp_mqtt_connect_func_t mqtt_connect;
    bsp_mqtt_disconnect_func_t mqtt_disconnect;
    
    // OTA管理器相关函数
    bsp_ota_init_func_t ota_init;
    bsp_ota_deinit_func_t ota_deinit;
    bsp_ota_check_update_func_t ota_check_update;
    // bsp_ota_start_update_func_t ota_start_update;  // 已弃用 - 使用新的startup_manager
    
    // WiFi相关函数
    bsp_wifi_init_func_t wifi_init;
    bsp_wifi_deinit_func_t wifi_deinit;
    bsp_wifi_connect_func_t wifi_connect;
    bsp_wifi_disconnect_func_t wifi_disconnect;
    
    // 传感器相关函数
    bsp_sensor_init_func_t sensor_init;
    bsp_sensor_deinit_func_t sensor_deinit;
    bsp_sensor_read_func_t sensor_read;
    
    // 系统监控相关函数
    bsp_system_monitor_init_func_t system_monitor_init;
    bsp_system_monitor_get_status_func_t system_monitor_get_status;
} bsp_interface_t;

/**
 * @brief 注册BSP接口
 * 
 * @param interface BSP接口结构体指针
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_register_interface(const bsp_interface_t *interface);

/**
 * @brief 获取当前BSP接口
 * 
 * @return const bsp_interface_t* BSP接口指针，失败返回NULL
 */
const bsp_interface_t* bsp_get_interface(void);

/**
 * @brief 初始化BSP
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_init(void);

/**
 * @brief 去初始化BSP
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_deinit(void);

/**
 * @brief 获取板级信息
 * 
 * @return const bsp_board_info_t* 板级信息指针，失败返回NULL
 */
const bsp_board_info_t* bsp_get_board_info(void);

/**
 * @brief 获取硬件配置
 * 
 * @return const bsp_hardware_config_t* 硬件配置指针，失败返回NULL
 */
const bsp_hardware_config_t* bsp_get_hardware_config(void);

/**
 * @brief 打印板级信息
 */
void bsp_print_board_info(void);

/**
 * @brief 验证硬件配置
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_validate_hardware_config(void);


/**
 * @brief 初始化MQTT客户端
 * @param config MQTT配置
 * @return hal_err_t 错误码
 */
hal_err_t bsp_mqtt_init(const mqtt_config_t* config);

/**
 * @brief 去初始化MQTT客户端
 * @return hal_err_t 错误码
 */
hal_err_t bsp_mqtt_deinit(void);

/**
 * @brief 连接MQTT服务器
 * @return hal_err_t 错误码
 */
hal_err_t bsp_mqtt_connect(void);

/**
 * @brief 断开MQTT连接
 * @return hal_err_t 错误码
 */
hal_err_t bsp_mqtt_disconnect(void);

/**
 * @brief 初始化OTA管理器
 * @return hal_err_t 错误码
 */
hal_err_t bsp_ota_init(void);

/**
 * @brief 去初始化OTA管理器
 * @return hal_err_t 错误码
 */
hal_err_t bsp_ota_deinit(void);

/**
 * @brief 检查OTA更新
 * @param url 更新服务器URL
 * @return hal_err_t 错误码
 */
hal_err_t bsp_ota_check_update(const char* url);

/**
 * @brief 开始OTA更新 (已弃用 - 使用新的startup_manager)
 * @param config OTA配置
 * @return hal_err_t 错误码
 */
// hal_err_t bsp_ota_start_update(const ota_config_t* config);

/**
 * @brief 初始化WiFi
 * @return hal_err_t 错误码
 */
hal_err_t bsp_wifi_init(void);

/**
 * @brief 去初始化WiFi
 * @return hal_err_t 错误码
 */
hal_err_t bsp_wifi_deinit(void);

/**
 * @brief 连接WiFi
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return hal_err_t 错误码
 */
hal_err_t bsp_wifi_connect(const char* ssid, const char* password);

/**
 * @brief 断开WiFi连接
 * @return hal_err_t 错误码
 */
hal_err_t bsp_wifi_disconnect(void);

/**
 * @brief 初始化传感器
 * @return hal_err_t 错误码
 */
hal_err_t bsp_sensor_init(void);

/**
 * @brief 去初始化传感器
 * @return hal_err_t 错误码
 */
hal_err_t bsp_sensor_deinit(void);

/**
 * @brief 读取传感器数据
 * @param sensor_id 传感器ID
 * @param value 传感器数值
 * @return hal_err_t 错误码
 */
hal_err_t bsp_sensor_read(uint8_t sensor_id, float* value);

/**
 * @brief 初始化系统监控
 * @return hal_err_t 错误码
 */
hal_err_t bsp_system_monitor_init(void);

/**
 * @brief 获取系统监控状态
 * @param status 状态信息
 * @return hal_err_t 错误码
 */
hal_err_t bsp_system_monitor_get_status(void* status);

#ifdef __cplusplus
}
#endif

#endif // BSP_INTERFACE_H