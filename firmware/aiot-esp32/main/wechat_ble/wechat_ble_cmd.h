/**
 * @file wechat_ble_cmd.h
 * @brief 微信小程序蓝牙命令处理模块
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef WECHAT_BLE_CMD_H
#define WECHAT_BLE_CMD_H

#include <stdint.h>
#include "esp_err.h"
#include "wechat_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 命令包格式定义 */
typedef struct {
    uint8_t cmd;        /* 命令类型 */
    uint8_t seq;        /* 序列号 */
    uint16_t len;       /* 数据长度 */
    uint8_t data[];     /* 数据内容 */
} __attribute__((packed)) wechat_ble_cmd_packet_t;

/* 响应包格式定义 */
typedef struct {
    uint8_t cmd;        /* 命令类型 */
    uint8_t seq;        /* 序列号 */
    uint8_t status;     /* 状态码 */
    uint16_t len;       /* 数据长度 */
    uint8_t data[];     /* 数据内容 */
} __attribute__((packed)) wechat_ble_rsp_packet_t;

/* 状态码定义 */
typedef enum {
    WECHAT_BLE_STATUS_SUCCESS = 0x00,
    WECHAT_BLE_STATUS_INVALID_CMD = 0x01,
    WECHAT_BLE_STATUS_INVALID_PARAM = 0x02,
    WECHAT_BLE_STATUS_BUSY = 0x03,
    WECHAT_BLE_STATUS_ERROR = 0x04,
    WECHAT_BLE_STATUS_NOT_SUPPORTED = 0x05
} wechat_ble_status_code_t;

/**
 * @brief 初始化命令处理模块
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_init(void);

/**
 * @brief 反初始化命令处理模块
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_deinit(void);

/**
 * @brief 处理接收到的命令
 * 
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_process(const uint8_t *data, uint16_t len);

/**
 * @brief 发送响应
 * 
 * @param cmd 命令类型
 * @param seq 序列号
 * @param status 状态码
 * @param data 响应数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_send_response(uint8_t cmd, uint8_t seq, uint8_t status, const uint8_t *data, uint16_t len);

/**
 * @brief 处理获取设备信息命令
 * 
 * @param seq 序列号
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_get_device_info(uint8_t seq);

/**
 * @brief 处理WiFi配置命令
 * 
 * @param seq 序列号
 * @param data 配置数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_wifi_config(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理MQTT配置命令
 * 
 * @param seq 序列号
 * @param data 配置数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_mqtt_config(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理获取状态命令
 * 
 * @param seq 序列号
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_get_status(uint8_t seq);

/**
 * @brief 处理设备重启命令
 * 
 * @param seq 序列号
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_restart_device(uint8_t seq);

/**
 * @brief 处理恢复出厂设置命令
 * 
 * @param seq 序列号
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_factory_reset(uint8_t seq);

/**
 * @brief 处理OTA升级命令
 * 
 * @param seq 序列号
 * @param data OTA数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_cmd_handle_ota_update(uint8_t seq, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* WECHAT_BLE_CMD_H */