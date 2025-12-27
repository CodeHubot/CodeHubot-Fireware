/**
 * @file mqtt_command.h
 * @brief MQTT命令处理模块
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef MQTT_COMMAND_H
#define MQTT_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "aiot_mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 命令类型定义 */
typedef enum {
    MQTT_CMD_GET_STATUS = 0x01,
    MQTT_CMD_SET_CONFIG = 0x02,
    MQTT_CMD_RESTART_DEVICE = 0x03,
    MQTT_CMD_FACTORY_RESET = 0x04,
    MQTT_CMD_OTA_UPDATE = 0x05,
    MQTT_CMD_SET_WIFI = 0x06,
    MQTT_CMD_SET_MQTT = 0x07,
    MQTT_CMD_GET_SENSOR_DATA = 0x08,
    MQTT_CMD_SET_SENSOR_INTERVAL = 0x09,
    MQTT_CMD_CALIBRATE_SENSOR = 0x0A,
    MQTT_CMD_SET_ALARM_THRESHOLD = 0x0B,
    MQTT_CMD_CLEAR_ALARM = 0x0C,
    MQTT_CMD_GET_LOG = 0x0D,
    MQTT_CMD_SET_LOG_LEVEL = 0x0E,
    MQTT_CMD_CUSTOM = 0xFF
} mqtt_command_type_t;

/* 命令响应状态 */
typedef enum {
    MQTT_CMD_STATUS_SUCCESS = 0x00,
    MQTT_CMD_STATUS_INVALID_CMD = 0x01,
    MQTT_CMD_STATUS_INVALID_PARAM = 0x02,
    MQTT_CMD_STATUS_BUSY = 0x03,
    MQTT_CMD_STATUS_ERROR = 0x04,
    MQTT_CMD_STATUS_NOT_SUPPORTED = 0x05,
    MQTT_CMD_STATUS_TIMEOUT = 0x06
} mqtt_command_status_t;

/* 命令包结构体 */
typedef struct {
    uint8_t cmd;            /* 命令类型 */
    uint8_t seq;            /* 序列号 */
    uint16_t len;           /* 数据长度 */
    uint8_t data[];         /* 命令数据 */
} __attribute__((packed)) mqtt_command_packet_t;

/* 响应包结构体 */
typedef struct {
    uint8_t cmd;            /* 命令类型 */
    uint8_t seq;            /* 序列号 */
    uint8_t status;         /* 状态码 */
    uint16_t len;           /* 数据长度 */
    uint8_t data[];         /* 响应数据 */
} __attribute__((packed)) mqtt_command_response_t;

/* WiFi配置命令数据 */
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t security_type;
} mqtt_cmd_wifi_config_t;

/* MQTT配置命令数据 */
typedef struct {
    char broker_url[128];
    uint16_t port;
    char username[64];
    char password[64];
    char client_id[64];
    bool use_ssl;
} mqtt_cmd_mqtt_config_t;

/* 传感器间隔配置 */
typedef struct {
    uint8_t sensor_type;
    uint32_t interval_ms;
} mqtt_cmd_sensor_interval_t;

/* 告警阈值配置 */
typedef struct {
    uint8_t sensor_type;
    float min_threshold;
    float max_threshold;
    bool enable;
} mqtt_cmd_alarm_threshold_t;

/* OTA更新命令数据 */
typedef struct {
    char url[256];
    char version[32];
    char hash[64];
    bool force_update;
} mqtt_cmd_ota_update_t;

/* 命令处理回调函数 */
typedef esp_err_t (*mqtt_command_handler_t)(uint8_t seq, const uint8_t *data, uint16_t len);

/* 命令处理器结构体 */
typedef struct {
    mqtt_command_type_t cmd_type;
    mqtt_command_handler_t handler;
    const char *description;
} mqtt_command_handler_entry_t;

/**
 * @brief 初始化MQTT命令处理模块
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_command_init(void);

/**
 * @brief 反初始化MQTT命令处理模块
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_command_deinit(void);

/**
 * @brief 处理接收到的命令
 * 
 * @param topic 主题
 * @param data 命令数据
 * @param data_len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_process(const char *topic, const uint8_t *data, size_t data_len);

/**
 * @brief 发送命令响应
 * 
 * @param cmd 命令类型
 * @param seq 序列号
 * @param status 状态码
 * @param data 响应数据
 * @param data_len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_send_response(uint8_t cmd, uint8_t seq, uint8_t status, 
                                     const uint8_t *data, uint16_t data_len);

/**
 * @brief 注册命令处理器
 * 
 * @param cmd_type 命令类型
 * @param handler 处理函数
 * @param description 描述
 * @return esp_err_t 
 */
esp_err_t mqtt_command_register_handler(mqtt_command_type_t cmd_type, 
                                        mqtt_command_handler_t handler,
                                        const char *description);

/**
 * @brief 注销命令处理器
 * 
 * @param cmd_type 命令类型
 * @return esp_err_t 
 */
esp_err_t mqtt_command_unregister_handler(mqtt_command_type_t cmd_type);

/**
 * @brief 处理获取状态命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_get_status(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理设备重启命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_restart_device(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理恢复出厂设置命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_factory_reset(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理WiFi配置命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_set_wifi(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理MQTT配置命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_set_mqtt(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理OTA更新命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_ota_update(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理传感器间隔设置命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_set_sensor_interval(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 处理告警阈值设置命令
 * 
 * @param seq 序列号
 * @param data 命令数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t mqtt_command_handle_set_alarm_threshold(uint8_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief 获取命令类型字符串
 * 
 * @param cmd_type 命令类型
 * @return const char* 命令字符串
 */
const char *mqtt_command_get_type_string(mqtt_command_type_t cmd_type);

/**
 * @brief 获取状态码字符串
 * 
 * @param status 状态码
 * @return const char* 状态字符串
 */
const char *mqtt_command_get_status_string(mqtt_command_status_t status);

/**
 * @brief 验证命令包格式
 * 
 * @param data 数据
 * @param data_len 数据长度
 * @return bool 格式是否正确
 */
bool mqtt_command_validate_packet(const uint8_t *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_COMMAND_H */