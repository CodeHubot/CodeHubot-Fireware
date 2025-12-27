/** 
 **************************************************************************************************** 
 * @file        rain_sensor.h 
 * @author      AIOT Project Team
 * @version     V1.0 
 * @date        2024-01-01 
 * @brief       雨水传感器驱动代码头文件
 * 
 * 雨水传感器是一个数字传感器，通过GPIO读取高低电平来判断是否有雨水
 * - 高电平(1): 无雨水
 * - 低电平(0): 有雨水
 **************************************************************************************************** 
 */ 

#ifndef __RAIN_SENSOR_H
#define __RAIN_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 雨水传感器数据结构
 */
typedef struct {
    bool is_raining;     // 是否下雨 (true=有雨, false=无雨)
    uint8_t level;       // 电平值 (0=低电平/有雨, 1=高电平/无雨)
    bool valid;          // 数据是否有效
} rain_sensor_data_t;

/**
 * @brief 雨水传感器配置结构
 */
typedef struct {
    gpio_num_t data_pin;    // 数据引脚 (GPIO4)
    bool pull_up_enable;    // 是否启用上拉电阻 (默认true)
    uint32_t debounce_ms;   // 防抖时间(毫秒) (默认50ms)
} rain_sensor_config_t;

/**
 * @brief 初始化雨水传感器
 * 
 * @param config 传感器配置
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t rain_sensor_init(const rain_sensor_config_t *config);

/**
 * @brief 读取雨水传感器数据
 * 
 * @param data 输出数据结构
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t rain_sensor_read(rain_sensor_data_t *data);

/**
 * @brief 检查是否有雨水
 * 
 * @return true 有雨水
 * @return false 无雨水
 */
bool rain_sensor_is_raining(void);

/**
 * @brief 获取当前电平值
 * 
 * @return uint8_t 0=低电平(有雨), 1=高电平(无雨)
 */
uint8_t rain_sensor_get_level(void);

/**
 * @brief 检查传感器是否已初始化
 * 
 * @return true 已初始化
 * @return false 未初始化
 */
bool rain_sensor_is_ready(void);

/**
 * @brief 去初始化雨水传感器
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t rain_sensor_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // __RAIN_SENSOR_H

