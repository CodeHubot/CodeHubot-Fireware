/**
 * @file bsp_esp32_s3_devkit.h
 * @brief ESP32-S3 DevKit 板级支持包接口
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BSP_ESP32_S3_DEVKIT_H
#define BSP_ESP32_S3_DEVKIT_H

#include "../../main/hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册ESP32-S3 DevKit BSP接口
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_s3_devkit_register(void);

/**
 * @brief 打印ESP32-S3 DevKit配置信息
 */
void bsp_esp32_s3_devkit_print_config(void);

// LED控制API
hal_err_t bsp_esp32_s3_devkit_led1_control(bool state);
hal_err_t bsp_esp32_s3_devkit_led1_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_led2_control(bool state);
hal_err_t bsp_esp32_s3_devkit_led2_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_led3_control(bool state);
hal_err_t bsp_esp32_s3_devkit_led3_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_led4_control(bool state);
hal_err_t bsp_esp32_s3_devkit_led4_set_brightness(uint8_t brightness);

// 继电器控制API
hal_err_t bsp_esp32_s3_devkit_relay_control(uint8_t relay_index, bool state);
hal_err_t bsp_esp32_s3_devkit_relay1_control(bool state);
hal_err_t bsp_esp32_s3_devkit_relay2_control(bool state);

// 舵机控制API
hal_err_t bsp_esp32_s3_devkit_servo_set_angle(uint8_t servo_index, uint16_t angle);
hal_err_t bsp_esp32_s3_devkit_servo1_set_angle(uint16_t angle);
hal_err_t bsp_esp32_s3_devkit_servo2_set_angle(uint16_t angle);

#ifdef __cplusplus
}
#endif

#endif // BSP_ESP32_S3_DEVKIT_H