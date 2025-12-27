/**
 * @file bsp_esp32_s3_devkit_lite.h
 * @brief ESP32-S3 DevKit Lite 板级支持包接口（精简版）
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BSP_ESP32_S3_DEVKIT_LITE_H
#define BSP_ESP32_S3_DEVKIT_LITE_H

#include "../../main/hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册ESP32-S3 DevKit Lite BSP接口
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_s3_devkit_lite_register(void);

/**
 * @brief 打印ESP32-S3 DevKit Lite配置信息
 */
void bsp_esp32_s3_devkit_lite_print_config(void);

// LED控制API
hal_err_t bsp_esp32_s3_devkit_lite_led1_control(bool state);
hal_err_t bsp_esp32_s3_devkit_lite_led1_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_lite_led2_control(bool state);
hal_err_t bsp_esp32_s3_devkit_lite_led2_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_lite_led3_control(bool state);
hal_err_t bsp_esp32_s3_devkit_lite_led3_set_brightness(uint8_t brightness);
hal_err_t bsp_esp32_s3_devkit_lite_led4_control(bool state);
hal_err_t bsp_esp32_s3_devkit_lite_led4_set_brightness(uint8_t brightness);

// 继电器控制API
hal_err_t bsp_esp32_s3_devkit_lite_relay_control(uint8_t relay_index, bool state);
hal_err_t bsp_esp32_s3_devkit_lite_relay1_control(bool state);
hal_err_t bsp_esp32_s3_devkit_lite_relay2_control(bool state);

#ifdef __cplusplus
}
#endif

#endif // BSP_ESP32_S3_DEVKIT_LITE_H

