/**
 * @file button_handler.h
 * @brief 按键处理模块
 * 
 * 提供Boot按键的检测和处理功能：
 * - 短按：普通功能（预留）
 * - 长按：进入WiFi配网模式
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 按键事件类型
typedef enum {
    BUTTON_EVENT_CLICK = 0,     // 短按
    BUTTON_EVENT_LONG_PRESS,    // 长按
    BUTTON_EVENT_DOUBLE_CLICK,  // 双击（预留）
} button_event_t;

// 按键事件回调函数类型
typedef void (*button_event_cb_t)(button_event_t event);

/**
 * @brief 初始化按键处理模块
 * 
 * @param event_cb 按键事件回调函数
 * @return esp_err_t 
 */
esp_err_t button_handler_init(button_event_cb_t event_cb);

/**
 * @brief 反初始化按键处理模块
 * 
 * @return esp_err_t 
 */
esp_err_t button_handler_deinit(void);

/**
 * @brief WiFi初始化后重新启用按键中断
 * 
 * WiFi初始化可能会影响GPIO中断服务，此函数用于重新注册按键的GPIO中断
 * 
 * @return esp_err_t 
 */
esp_err_t button_handler_reinit_after_wifi(void);

/**
 * @brief 获取Boot按键当前状态
 * 
 * @return true 按键按下
 * @return false 按键释放
 */
bool button_handler_get_boot_state(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_HANDLER_H