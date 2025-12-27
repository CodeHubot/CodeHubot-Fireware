/**
 * @file bsp_esp32_c3_mini.h
 * @brief ESP32-C3 Mini 板级支持包接口
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BSP_ESP32_C3_MINI_H
#define BSP_ESP32_C3_MINI_H

#include "../../main/hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册ESP32-C3 Mini BSP接口
 * 
 * @return hal_err_t 操作结果
 */
hal_err_t bsp_esp32_c3_mini_register(void);

/**
 * @brief 打印ESP32-C3 Mini配置信息
 */
void bsp_esp32_c3_mini_print_config(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_ESP32_C3_MINI_H