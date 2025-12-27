/**
 * @file mqtt_client.h
 * @brief MQTT客户端管理器
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef AIOT_MQTT_CLIENT_H
#define AIOT_MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "mqtt_client.h"

/* 自定义MQTT事件类型 */
#define AIOT_MQTT_EVENT_MESSAGE_SENT      (100)
#define AIOT_MQTT_EVENT_MESSAGE_RECEIVED  (101)

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT配置参数 */
#define MQTT_MAX_BROKER_LEN     128
#define MQTT_MAX_USERNAME_LEN   64
#define MQTT_MAX_PASSWORD_LEN   64
#define MQTT_MAX_CLIENT_ID_LEN  64
#define MQTT_MAX_TOPIC_LEN      128
#define MQTT_MAX_PAYLOAD_LEN    1024
#define MQTT_KEEPALIVE_SEC      60
#define MQTT_RECONNECT_TIMEOUT  5000

/* MQTT连接状态 */
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_RECONNECTING,
    MQTT_STATE_ERROR
} mqtt_connection_state_t;

/* MQTT QoS等级 */
typedef enum {
    MQTT_QOS_0 = 0,
    MQTT_QOS_1 = 1,
    MQTT_QOS_2 = 2
} mqtt_qos_level_t;

/* MQTT配置结构体 */
typedef struct {
    char broker_url[MQTT_MAX_BROKER_LEN];
    uint16_t port;
    char username[MQTT_MAX_USERNAME_LEN];
    char password[MQTT_MAX_PASSWORD_LEN];
    char client_id[MQTT_MAX_CLIENT_ID_LEN];
    bool use_ssl;
    bool clean_session;
    uint16_t keepalive;
    uint32_t reconnect_timeout;
    const char *cert_pem;
    const char *client_cert_pem;
    const char *client_key_pem;
} mqtt_config_t;

/* MQTT消息结构体 */
typedef struct {
    char topic[MQTT_MAX_TOPIC_LEN];
    uint8_t payload[MQTT_MAX_PAYLOAD_LEN];
    size_t payload_len;
    mqtt_qos_level_t qos;
    bool retain;
    uint32_t timestamp;
} mqtt_message_t;

/* MQTT统计信息 */
typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t messages_failed;
    uint32_t reconnect_count;
    uint32_t last_error_code;
    uint32_t uptime_seconds;
    mqtt_connection_state_t state;
} mqtt_statistics_t;

/* MQTT事件数据 */
typedef struct {
    esp_mqtt_event_id_t event;
    mqtt_connection_state_t state;
    const mqtt_message_t *message;
    esp_err_t error_code;
    void *user_data;
} mqtt_event_data_t;

/* MQTT事件回调函数 */
typedef void (*mqtt_event_callback_t)(const mqtt_event_data_t *event_data);

/**
 * @brief 初始化MQTT客户端
 * 
 * @param config MQTT配置
 * @param callback 事件回调函数
 * @return esp_err_t 
 */
esp_err_t mqtt_client_init(const mqtt_config_t *config, mqtt_event_callback_t callback);

/**
 * @brief 反初始化MQTT客户端
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_client_deinit(void);

/**
 * @brief 连接到MQTT代理
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_client_connect(void);

/**
 * @brief 断开MQTT连接
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_client_disconnect(void);

/**
 * @brief 发布消息
 * 
 * @param topic 主题
 * @param payload 消息内容
 * @param payload_len 消息长度
 * @param qos QoS等级
 * @param retain 保留标志
 * @return esp_err_t 
 */
esp_err_t mqtt_client_publish(const char *topic, const void *payload, size_t payload_len, 
                              mqtt_qos_level_t qos, bool retain);

/**
 * @brief 订阅主题
 * 
 * @param topic 主题
 * @param qos QoS等级
 * @return esp_err_t 
 */
esp_err_t mqtt_client_subscribe(const char *topic, mqtt_qos_level_t qos);

/**
 * @brief 取消订阅主题
 * 
 * @param topic 主题
 * @return esp_err_t 
 */
esp_err_t mqtt_client_unsubscribe(const char *topic);

/**
 * @brief 获取连接状态
 * 
 * @return mqtt_connection_state_t 连接状态
 */
mqtt_connection_state_t mqtt_client_get_state(void);

/**
 * @brief 检查是否已连接
 * 
 * @return true 已连接
 * @return false 未连接
 */
bool mqtt_client_is_connected(void);

/**
 * @brief 获取统计信息
 * 
 * @param stats 统计信息结构体
 * @return esp_err_t 
 */
esp_err_t mqtt_client_get_statistics(mqtt_statistics_t *stats);

/**
 * @brief 重置统计信息
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_client_reset_statistics(void);

/**
 * @brief 设置遗嘱消息
 * 
 * @param topic 遗嘱主题
 * @param payload 遗嘱消息
 * @param payload_len 消息长度
 * @param qos QoS等级
 * @param retain 保留标志
 * @return esp_err_t 
 */
esp_err_t mqtt_client_set_will(const char *topic, const void *payload, size_t payload_len,
                               mqtt_qos_level_t qos, bool retain);

/**
 * @brief 更新配置
 * 
 * @param config 新的MQTT配置
 * @return esp_err_t 
 */
esp_err_t mqtt_client_update_config(const mqtt_config_t *config);

/**
 * @brief 获取状态字符串
 * 
 * @param state 连接状态
 * @return const char* 状态字符串
 */
const char *mqtt_client_get_state_string(mqtt_connection_state_t state);

/**
 * @brief 启用/禁用自动重连
 * 
 * @param enable 是否启用
 * @return esp_err_t 
 */
esp_err_t mqtt_client_set_auto_reconnect(bool enable);

/**
 * @brief 设置重连间隔
 * 
 * @param interval_ms 重连间隔(毫秒)
 * @return esp_err_t 
 */
esp_err_t mqtt_client_set_reconnect_interval(uint32_t interval_ms);

/**
 * @brief 重置重连尝试计数
 * 
 * @return esp_err_t 
 */
esp_err_t mqtt_client_reset_reconnect_attempts(void);

/**
 * @brief 获取当前重连尝试次数
 * 
 * @return uint32_t 重连尝试次数
 */
uint32_t mqtt_client_get_reconnect_attempts(void);

/**
 * @brief 获取当前重连间隔
 * 
 * @return uint32_t 重连间隔(毫秒)
 */
uint32_t mqtt_client_get_reconnect_interval(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */