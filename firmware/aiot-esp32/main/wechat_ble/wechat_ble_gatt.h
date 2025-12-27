/**
 * @file wechat_ble_gatt.h
 * @brief 微信小程序蓝牙GATT服务定义
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef WECHAT_BLE_GATT_H
#define WECHAT_BLE_GATT_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "wechat_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GATT服务和特征值定义 */
#define WECHAT_BLE_GATTS_APP_ID         0x55
#define WECHAT_BLE_GATTS_NUM_HANDLE     20

/* 广播参数 */
extern esp_ble_adv_params_t wechat_ble_adv_params;
extern esp_ble_adv_data_t wechat_ble_adv_data;
extern esp_ble_adv_data_t wechat_ble_scan_rsp_data;

/**
 * @brief 初始化GATT服务
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_gatt_init(void);

/**
 * @brief 反初始化GATT服务
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_gatt_deinit(void);

/**
 * @brief GATT事件处理函数
 * 
 * @param event 事件类型
 * @param gatts_if GATT接口
 * @param param 事件参数
 */
void wechat_ble_gatt_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/**
 * @brief 发送响应数据
 * 
 * @param cmd 命令类型
 * @param data 数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_gatt_send_response(wechat_ble_cmd_t cmd, const uint8_t *data, uint16_t len);

/**
 * @brief 断开所有连接
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_gatt_disconnect_all(void);

#ifdef __cplusplus
}
#endif

#endif /* WECHAT_BLE_GATT_H */