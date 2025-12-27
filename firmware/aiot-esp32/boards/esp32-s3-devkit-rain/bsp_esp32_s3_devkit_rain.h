/**
 * @file bsp_esp32_s3_devkit_rain.h
 * @brief ESP32-S3 DevKit Rain 板级支持包接口（含雨水传感器）
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BSP_ESP32_S3_DEVKIT_RAIN_H
#define BSP_ESP32_S3_DEVKIT_RAIN_H

#include "../../main/hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册ESP32-S3 DevKit Rain BSP接口
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_s3_devkit_rain_register(void);

/**
 * @brief 打印ESP32-S3 DevKit Rain配置信息
 */
void bsp_esp32_s3_devkit_rain_print_config(void);

// LED控制API（与原版相同）
hal_err_t bsp_esp32_s3_devkit_rain_led1_control(bool state);
hal_err_t bsp_esp32_s3_devkit_rain_led1_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_rain_led2_control(bool state);
hal_err_t bsp_esp32_s3_devkit_rain_led2_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_rain_led3_control(bool state);
hal_err_t bsp_esp32_s3_devkit_rain_led3_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_rain_led4_control(bool state);
hal_err_t bsp_esp32_s3_devkit_rain_led4_set_brightness(uint8_t brightness);

// 继电器控制API（与原版相同）
hal_err_t bsp_esp32_s3_devkit_rain_relay_control(uint8_t relay_index, bool state);
hal_err_t bsp_esp32_s3_devkit_rain_relay1_control(bool state);
hal_err_t bsp_esp32_s3_devkit_rain_relay2_control(bool state);

// 舵机控制API（与原版相同）
hal_err_t bsp_esp32_s3_devkit_rain_servo_set_angle(uint8_t servo_index, uint16_t angle);
hal_err_t bsp_esp32_s3_devkit_rain_servo1_set_angle(uint16_t angle);
hal_err_t bsp_esp32_s3_devkit_rain_servo2_set_angle(uint16_t angle);

#ifdef __cplusplus
}
#endif

#endif // BSP_ESP32_S3_DEVKIT_RAIN_H

