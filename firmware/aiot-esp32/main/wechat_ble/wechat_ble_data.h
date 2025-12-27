/**
 * @file wechat_ble_data.h
 * @brief 微信小程序蓝牙数据管理模块
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef WECHAT_BLE_DATA_H
#define WECHAT_BLE_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "wechat_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化数据管理模块
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_init(void);

/**
 * @brief 反初始化数据管理模块
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_deinit(void);

/**
 * @brief 获取设备信息
 * 
 * @param device_info 设备信息结构体指针
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_get_device_info(wechat_ble_device_info_t *device_info);

/**
 * @brief 获取状态信息
 * 
 * @param status 状态信息结构体指针
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_get_status(wechat_ble_status_t *status);

/**
 * @brief 保存WiFi配置
 * 
 * @param wifi_config WiFi配置
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_save_wifi_config(const wechat_ble_wifi_config_t *wifi_config);

/**
 * @brief 加载WiFi配置
 * 
 * @param wifi_config WiFi配置
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_load_wifi_config(wechat_ble_wifi_config_t *wifi_config);

/**
 * @brief 保存MQTT配置
 * 
 * @param mqtt_config MQTT配置
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_save_mqtt_config(const wechat_ble_mqtt_config_t *mqtt_config);

/**
 * @brief 加载MQTT配置
 * 
 * @param mqtt_config MQTT配置
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_load_mqtt_config(wechat_ble_mqtt_config_t *mqtt_config);

/**
 * @brief 清除所有配置
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_clear_all_config(void);

/**
 * @brief 检查WiFi配置是否存在
 * 
 * @return true 配置存在
 * @return false 配置不存在
 */
bool wechat_ble_data_has_wifi_config(void);

/**
 * @brief 检查MQTT配置是否存在
 * 
 * @return true 配置存在
 * @return false 配置不存在
 */
bool wechat_ble_data_has_mqtt_config(void);

/**
 * @brief 发送数据
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_data_send(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* WECHAT_BLE_DATA_H */