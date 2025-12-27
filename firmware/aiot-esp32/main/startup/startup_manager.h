/**
 * @file startup_manager.h
 * @brief 启动流程管理器
 * 
 * 统一管理设备启动流程，包含详细的LCD UI提示
 */

#ifndef STARTUP_MANAGER_H
#define STARTUP_MANAGER_H

#include "esp_err.h"
#include "button/button_handler.h"  // For button_event_cb_t

#ifdef __cplusplus
extern "C" {
#endif

/** 启动阶段 */
typedef enum {
    STARTUP_STAGE_INIT,             ///< 初始化
    STARTUP_STAGE_NVS,              ///< NVS初始化
    STARTUP_STAGE_WIFI_CHECK,       ///< 检查WiFi配置
    STARTUP_STAGE_WIFI_CONNECT,     ///< WiFi连接
    STARTUP_STAGE_GET_CONFIG,       ///< 获取设备配置
    STARTUP_STAGE_CHECK_OTA,        ///< 检查固件更新
    STARTUP_STAGE_OTA_UPDATE,       ///< OTA更新
    STARTUP_STAGE_MQTT_CONNECT,     ///< MQTT连接
    STARTUP_STAGE_SENSORS_INIT,     ///< 传感器初始化
    STARTUP_STAGE_COMPLETED,        ///< 启动完成
    STARTUP_STAGE_ERROR,            ///< 启动错误
} startup_stage_t;

/** 启动状态回调 */
typedef void (*startup_status_callback_t)(startup_stage_t stage, const char *message);

/**
 * @brief 执行完整的启动流程
 * 
 * 包含所有必要的初始化步骤，并在LCD上显示进度
 * 
 * @param display LCD显示句柄（可以为NULL）
 * @param status_callback 状态回调函数（可选）
 * @param button_callback 按钮事件回调函数（可选）
 * 
 * @return 
 *   - ESP_OK: 启动成功
 *   - ESP_FAIL: 启动失败
 */
esp_err_t startup_manager_run(void *display, startup_status_callback_t status_callback, button_event_cb_t button_callback);

/**
 * @brief 获取当前启动阶段
 * 
 * @return 当前启动阶段
 */
startup_stage_t startup_manager_get_stage(void);

/**
 * @brief 获取启动阶段的描述
 * 
 * @param stage 启动阶段
 * @return 阶段描述字符串
 */
const char* startup_manager_get_stage_string(startup_stage_t stage);

/**
 * @brief 获取设备ID
 * 
 * @return 设备ID字符串，如果未初始化则返回NULL
 */
const char* startup_manager_get_device_id(void);

/**
 * @brief 获取设备UUID
 * 
 * @return 设备UUID字符串，如果未初始化则返回NULL
 */
const char* startup_manager_get_device_uuid(void);

/**
 * @brief 检查设备是否未注册（WiFi已连接但设备未在后端注册）
 * 
 * @return true: 设备未注册，false: 设备已注册或状态未知
 */
bool startup_manager_is_device_not_registered(void);

#ifdef __cplusplus
}
#endif

#endif // STARTUP_MANAGER_H

