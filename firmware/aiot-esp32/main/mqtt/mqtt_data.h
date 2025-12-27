/**
 * @file mqtt_data.h
 * @brief MQTT数据管理模块
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef MQTT_DATA_H
#define MQTT_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "aiot_mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 数据类型定义 */
typedef enum {
    MQTT_DATA_TYPE_SENSOR = 0,
    MQTT_DATA_TYPE_STATUS,
    MQTT_DATA_TYPE_ALARM,
    MQTT_DATA_TYPE_CONFIG,
    MQTT_DATA_TYPE_HEARTBEAT,
    MQTT_DATA_TYPE_LOG,
    MQTT_DATA_TYPE_OTA,
    MQTT_DATA_TYPE_CUSTOM
} mqtt_data_type_t;

/* 传感器数据结构体 */
typedef struct {
    float temperature;
    float humidity;
    float pressure;
    uint16_t light;
    uint16_t noise;
    uint32_t timestamp;
} mqtt_sensor_data_t;

/* 设备状态数据 */
typedef struct {
    bool wifi_connected;
    bool mqtt_connected;
    uint8_t battery_level;
    uint32_t uptime;
    uint32_t free_heap;
    uint32_t min_free_heap;
    char firmware_version[32];
    uint32_t timestamp;
} mqtt_status_data_t;

/* 告警数据 */
typedef struct {
    uint8_t alarm_type;
    uint8_t alarm_level;
    char alarm_message[128];
    uint32_t timestamp;
} mqtt_alarm_data_t;

/* 心跳数据 */
typedef struct {
    uint32_t sequence;
    uint32_t timestamp;
    uint8_t status;
} mqtt_heartbeat_data_t;

/* 数据缓存项 */
typedef struct {
    mqtt_data_type_t type;
    uint8_t data[MQTT_MAX_PAYLOAD_LEN];
    size_t data_len;
    char topic[MQTT_MAX_TOPIC_LEN];
    mqtt_qos_level_t qos;
    bool retain;
    uint32_t timestamp;
    uint8_t retry_count;
} mqtt_data_cache_item_t;

/* 主题配置 */
typedef struct {
    char device_id[32];
    char base_topic[64];
    char sensor_topic[MQTT_MAX_TOPIC_LEN];
    char status_topic[MQTT_MAX_TOPIC_LEN];
    char alarm_topic[MQTT_MAX_TOPIC_LEN];
    char config_topic[MQTT_MAX_TOPIC_LEN];
    char heartbeat_topic[MQTT_MAX_TOPIC_LEN];
    char log_topic[MQTT_MAX_TOPIC_LEN];
    char ota_topic[MQTT_MAX_TOPIC_LEN];
    char command_topic[MQTT_MAX_TOPIC_LEN];
} mqtt_topic_config_t;

/**
 * @brief 初始化MQTT数据管理模块
 * 
 * @param topic_config 主题配置
 * @return esp_err_t 
 */
esp_err_t mqtt_data_init(const mqtt_topic_config_t *topic_config);

/**
 * @brief 反初始化MQTT数据管理模块
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_data_deinit(void);

/**
 * @brief 发送传感器数据
 * 
 * @param sensor_data 传感器数据
 * @return esp_err_t 
 */
esp_err_t mqtt_data_send_sensor_data(const mqtt_sensor_data_t *sensor_data);

/**
 * @brief 发送状态数据
 * 
 * @param status_data 状态数据
 * @return esp_err_t 
 */
esp_err_t mqtt_data_send_status_data(const mqtt_status_data_t *status_data);

/**
 * @brief 发送告警数据
 * 
 * @param alarm_data 告警数据
 * @return esp_err_t 
 */
esp_err_t mqtt_data_send_alarm_data(const mqtt_alarm_data_t *alarm_data);

/**
 * @brief 发送心跳数据
 * 
 * @param heartbeat_data 心跳数据
 * @return esp_err_t 
 */
esp_err_t mqtt_data_send_heartbeat(const mqtt_heartbeat_data_t *heartbeat_data);

/**
 * @brief 发送自定义数据
 * 
 * @param topic 主题
 * @param data 数据
 * @param data_len 数据长度
 * @param qos QoS等级
 * @param retain 保留标志
 * @return esp_err_t 
 */
esp_err_t mqtt_data_send_custom(const char *topic, const void *data, size_t data_len,
                                mqtt_qos_level_t qos, bool retain);

/**
 * @brief 缓存数据(离线时使用)
 * 
 * @param type 数据类型
 * @param data 数据
 * @param data_len 数据长度
 * @param topic 主题
 * @param qos QoS等级
 * @param retain 保留标志
 * @return esp_err_t 
 */
esp_err_t mqtt_data_cache_data(mqtt_data_type_t type, const void *data, size_t data_len,
                               const char *topic, mqtt_qos_level_t qos, bool retain);

/**
 * @brief 发送缓存的数据
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_data_send_cached_data(void);

/**
 * @brief 清除缓存数据
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_data_clear_cache(void);

/**
 * @brief 获取缓存数据数量
 * 
 * @return size_t 缓存数据数量
 */
size_t mqtt_data_get_cache_count(void);

/**
 * @brief 设置数据发送间隔
 * 
 * @param type 数据类型
 * @param interval_ms 发送间隔(毫秒)
 * @return esp_err_t 
 */
esp_err_t mqtt_data_set_send_interval(mqtt_data_type_t type, uint32_t interval_ms);

/**
 * @brief 启用/禁用数据压缩
 * 
 * @param enable 是否启用
 * @return esp_err_t 
 */
esp_err_t mqtt_data_set_compression(bool enable);

/**
 * @brief 序列化传感器数据为JSON
 * 
 * @param sensor_data 传感器数据
 * @param json_buffer JSON缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t mqtt_data_serialize_sensor_data(const mqtt_sensor_data_t *sensor_data, 
                                          char *json_buffer, size_t buffer_size);

/**
 * @brief 序列化状态数据为JSON
 * 
 * @param status_data 状态数据
 * @param json_buffer JSON缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t mqtt_data_serialize_status_data(const mqtt_status_data_t *status_data,
                                          char *json_buffer, size_t buffer_size);

/**
 * @brief 序列化告警数据为JSON
 * 
 * @param alarm_data 告警数据
 * @param json_buffer JSON缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t mqtt_data_serialize_alarm_data(const mqtt_alarm_data_t *alarm_data,
                                         char *json_buffer, size_t buffer_size);

/**
 * @brief 获取数据类型字符串
 * 
 * @param type 数据类型
 * @return const char* 类型字符串
 */
const char *mqtt_data_get_type_string(mqtt_data_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_DATA_H */