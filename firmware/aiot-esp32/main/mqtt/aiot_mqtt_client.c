/**
 * @file mqtt_client.c
 * @brief MQTT客户端实现 - 使用ESP-IDF MQTT组件
 */

#include "aiot_mqtt_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

// ESP-IDF MQTT客户端头文件
#include "mqtt_client.h"

#ifdef ESP_PLATFORM
#include "esp_wifi.h"
#include "esp_timer.h"
#endif

static const char* TAG = "MQTT_CLIENT";

static mqtt_config_t g_mqtt_config = {0};
static mqtt_event_callback_t g_mqtt_callback = NULL;
static mqtt_connection_state_t g_mqtt_state = MQTT_STATE_DISCONNECTED;
static mqtt_statistics_t g_mqtt_stats = {0};
static bool g_mqtt_initialized = false;
static bool g_auto_reconnect = true;
static uint32_t g_reconnect_interval = 5000;
// 移除未使用的重连变量，依赖ESP-IDF自动重连
static esp_mqtt_client_handle_t g_mqtt_client = NULL;

// ESP-IDF MQTT事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    mqtt_event_data_t callback_data = {0};
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🎉 MQTT_EVENT_CONNECTED - Successfully connected to broker");
            ESP_LOGI(TAG, "📊 Broker: %s:%d", g_mqtt_config.broker_url, g_mqtt_config.port);
            
            g_mqtt_state = MQTT_STATE_CONNECTED;
            g_mqtt_stats.state = g_mqtt_state;
            
            callback_data.event = MQTT_EVENT_CONNECTED;
            callback_data.state = g_mqtt_state;
            callback_data.error_code = ESP_OK;
            if (g_mqtt_callback) {
                g_mqtt_callback(&callback_data);
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "🔌 MQTT_EVENT_DISCONNECTED - Connection lost");
            
            g_mqtt_state = MQTT_STATE_DISCONNECTED;
            g_mqtt_stats.state = g_mqtt_state;
            g_mqtt_stats.reconnect_count++;
            
            ESP_LOGI(TAG, "🔄 ESP-IDF will handle automatic reconnection");
            
            callback_data.event = MQTT_EVENT_DISCONNECTED;
            callback_data.state = g_mqtt_state;
            callback_data.error_code = ESP_OK;
            if (g_mqtt_callback) {
                g_mqtt_callback(&callback_data);
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            g_mqtt_stats.messages_sent++;
            callback_data.event = AIOT_MQTT_EVENT_MESSAGE_SENT;
            callback_data.state = g_mqtt_state;
            callback_data.error_code = ESP_OK;
            if (g_mqtt_callback) {
                g_mqtt_callback(&callback_data);
            }
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            
            g_mqtt_stats.messages_received++;
            
            // 构造消息数据
            mqtt_message_t message = {0};
            int topic_len = event->topic_len < (MQTT_MAX_TOPIC_LEN - 1) ? event->topic_len : (MQTT_MAX_TOPIC_LEN - 1);
            int data_len = event->data_len < (MQTT_MAX_PAYLOAD_LEN - 1) ? event->data_len : (MQTT_MAX_PAYLOAD_LEN - 1);
            
            strncpy(message.topic, event->topic, topic_len);
            message.topic[topic_len] = '\0';
            memcpy(message.payload, event->data, data_len);
            message.payload_len = data_len;
            message.timestamp = esp_timer_get_time() / 1000;
            
            ESP_LOGI(TAG, "🔔 准备调用回调函数 (g_mqtt_callback=%p, event=AIOT_MQTT_EVENT_MESSAGE_RECEIVED)", g_mqtt_callback);
            ESP_LOGI(TAG, "🔔 消息内容: topic=%s, payload_len=%d", message.topic, message.payload_len);
            
            callback_data.event = AIOT_MQTT_EVENT_MESSAGE_RECEIVED;
            callback_data.state = g_mqtt_state;
            callback_data.message = &message;
            callback_data.error_code = ESP_OK;
            if (g_mqtt_callback) {
                ESP_LOGI(TAG, "🔔 正在调用回调函数...");
                g_mqtt_callback(&callback_data);
                ESP_LOGI(TAG, "🔔 回调函数调用完成");
            } else {
                ESP_LOGE(TAG, "❌ 回调函数为NULL，无法处理MQTT消息！");
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "🚨 MQTT_EVENT_ERROR - Connection error occurred");
            if (event->error_handle) {
                ESP_LOGE(TAG, "📋 Error details:");
                ESP_LOGE(TAG, "  - Error type: %d", event->error_handle->error_type);
                ESP_LOGE(TAG, "  - ESP TLS error: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "  - TLS stack error: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG, "  - ESP TLS cert verify flags: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
                
                // 根据错误类型提供诊断信息
                switch (event->error_handle->error_type) {
                    case MQTT_ERROR_TYPE_TCP_TRANSPORT:
                        ESP_LOGE(TAG, "💡 Diagnosis: TCP transport error - Check network connectivity");
                        break;
                    case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
                        ESP_LOGE(TAG, "💡 Diagnosis: Connection refused - Check broker address and credentials");
                        break;
                    default:
                        ESP_LOGE(TAG, "💡 Diagnosis: Unknown error type");
                        break;
                }
            }
            
            g_mqtt_state = MQTT_STATE_ERROR;
            g_mqtt_stats.state = g_mqtt_state;
            g_mqtt_stats.messages_failed++;
            if (event->error_handle) {
                g_mqtt_stats.last_error_code = event->error_handle->error_type;
            }
            
            callback_data.event = MQTT_EVENT_ERROR;
            callback_data.state = g_mqtt_state;
            callback_data.error_code = ESP_FAIL;
            if (g_mqtt_callback) {
                g_mqtt_callback(&callback_data);
            }
            break;
            
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_client_init(const mqtt_config_t *config, mqtt_event_callback_t callback)
{
    if (!config || !callback) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // 如果已经初始化，先清理
    if (g_mqtt_client) {
        esp_mqtt_client_destroy(g_mqtt_client);
        g_mqtt_client = NULL;
    }

    // 复制配置
    memcpy(&g_mqtt_config, config, sizeof(mqtt_config_t));
    g_mqtt_callback = callback;
    
    // 构造broker URI
    char broker_uri[256];
    snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s:%d", 
             g_mqtt_config.broker_url, g_mqtt_config.port);
    
    // 配置ESP-IDF MQTT客户端 - 参考稳定连接策略
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = broker_uri,
        },
        .credentials = {
            .client_id = g_mqtt_config.client_id,
        },
        .session = {
            .keepalive = 60,                    // 使用60秒心跳间隔（参考代码配置）
            .disable_clean_session = false,
        },
        .network = {
            .disable_auto_reconnect = false,    // 明确启用自动重连（关键配置）
            .timeout_ms = 5000,                 // 5秒连接超时
        }
    };
    
    // 如果有用户名和密码
    if (strlen(g_mqtt_config.username) > 0) {
        mqtt_cfg.credentials.username = g_mqtt_config.username;
    }
    if (strlen(g_mqtt_config.password) > 0) {
        mqtt_cfg.credentials.authentication.password = g_mqtt_config.password;
    }
    
    // 创建MQTT客户端
    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!g_mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    // 注册事件处理器
    esp_err_t ret = esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(g_mqtt_client);
        g_mqtt_client = NULL;
        return ret;
    }
    
    // 初始化统计信息
    memset(&g_mqtt_stats, 0, sizeof(mqtt_statistics_t));
    g_mqtt_stats.state = MQTT_STATE_DISCONNECTED;
    
    g_mqtt_initialized = true;
    
    ESP_LOGI(TAG, "MQTT client initialized");
    ESP_LOGI(TAG, "Broker: %s", broker_uri);
    ESP_LOGI(TAG, "Client ID: %s", g_mqtt_config.client_id);
    
    return ESP_OK;
}

esp_err_t mqtt_client_deinit(void)
{
    if (!g_mqtt_initialized) {
        ESP_LOGW(TAG, "MQTT client not initialized");
        return ESP_OK;
    }
    
    // 先断开连接
    if (g_mqtt_state == MQTT_STATE_CONNECTED && g_mqtt_client) {
        esp_mqtt_client_stop(g_mqtt_client);
    }
    
    // 销毁MQTT客户端
    if (g_mqtt_client) {
        esp_mqtt_client_destroy(g_mqtt_client);
        g_mqtt_client = NULL;
    }
    
    // 清理资源
    g_mqtt_initialized = false;
    g_mqtt_callback = NULL;
    g_mqtt_state = MQTT_STATE_DISCONNECTED;
    memset(&g_mqtt_config, 0, sizeof(mqtt_config_t));
    memset(&g_mqtt_stats, 0, sizeof(mqtt_statistics_t));
    
    ESP_LOGI(TAG, "MQTT client deinitialized");
    
    return ESP_OK;
}

esp_err_t mqtt_client_connect(void)
{
    if (!g_mqtt_initialized || !g_mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mqtt_state == MQTT_STATE_CONNECTED) {
        ESP_LOGW(TAG, "Already connected");
        return ESP_OK;
    }
    
    // 检查 WiFi 连接状态
#ifdef ESP_PLATFORM
    wifi_ap_record_t ap_info;
    esp_err_t wifi_ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (wifi_ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi not connected, cannot connect to MQTT broker");
        g_mqtt_state = MQTT_STATE_DISCONNECTED;
        g_mqtt_stats.state = g_mqtt_state;
        
        // 触发断开连接事件
        if (g_mqtt_callback) {
            mqtt_event_data_t event_data = {
                .event = MQTT_EVENT_DISCONNECTED,
                .state = g_mqtt_state,
                .message = NULL,
                .error_code = ESP_ERR_WIFI_NOT_CONNECT,
                .user_data = NULL
            };
            g_mqtt_callback(&event_data);
        }
        
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
#endif
    
    ESP_LOGI(TAG, "Connecting to MQTT broker: %s:%d", 
             g_mqtt_config.broker_url, g_mqtt_config.port);
    
    // 启动MQTT客户端
    esp_err_t ret = esp_mqtt_client_start(g_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        g_mqtt_state = MQTT_STATE_ERROR;
        g_mqtt_stats.state = g_mqtt_state;
        g_mqtt_stats.messages_failed++;
        return ret;
    }
    
    g_mqtt_state = MQTT_STATE_CONNECTING;
    g_mqtt_stats.state = g_mqtt_state;
    
    ESP_LOGI(TAG, "MQTT client started, connecting...");
    return ESP_OK;
}

esp_err_t mqtt_client_disconnect(void)
{
    if (!g_mqtt_initialized || !g_mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (g_mqtt_state == MQTT_STATE_DISCONNECTED) {
        ESP_LOGW(TAG, "MQTT already disconnected");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Disconnecting from MQTT broker");
    
    // 停止MQTT客户端
    esp_err_t ret = esp_mqtt_client_stop(g_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    g_mqtt_state = MQTT_STATE_DISCONNECTED;
    g_mqtt_stats.state = g_mqtt_state;

    ESP_LOGI(TAG, "MQTT disconnected");
    return ESP_OK;
}

esp_err_t mqtt_client_publish(const char *topic, const void *payload, size_t payload_len, 
                              mqtt_qos_level_t qos, bool retain)
{
    if (!g_mqtt_initialized || !g_mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mqtt_state != MQTT_STATE_CONNECTED) {
        ESP_LOGE(TAG, "Not connected to broker");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!topic || !payload) {
        ESP_LOGE(TAG, "Invalid topic or payload");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Publishing to topic: %s", topic);
    ESP_LOGI(TAG, "Payload length: %d bytes", payload_len);
    
    // 使用ESP-IDF MQTT客户端发布消息
    int msg_id = esp_mqtt_client_publish(g_mqtt_client, topic, (const char*)payload, payload_len, qos, retain ? 1 : 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish message");
        g_mqtt_stats.messages_failed++;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Message published with msg_id: %d", msg_id);
    return ESP_OK;
}

esp_err_t mqtt_client_subscribe(const char *topic, mqtt_qos_level_t qos)
{
    if (!g_mqtt_initialized || !g_mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mqtt_state != MQTT_STATE_CONNECTED) {
        ESP_LOGE(TAG, "Not connected to broker");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!topic) {
        ESP_LOGE(TAG, "Invalid topic");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Subscribing to topic: %s (QoS: %d)", topic, qos);
    
    // 使用ESP-IDF MQTT客户端订阅主题
    int msg_id = esp_mqtt_client_subscribe(g_mqtt_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Subscribed to topic with msg_id: %d", msg_id);
    return ESP_OK;
}

esp_err_t mqtt_client_unsubscribe(const char *topic)
{
    if (!g_mqtt_initialized) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mqtt_state != MQTT_STATE_CONNECTED) {
        ESP_LOGE(TAG, "Not connected to broker");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!topic) {
        ESP_LOGE(TAG, "Invalid topic");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Unsubscribing from topic: %s", topic);
    
    // TODO: 实现实际的取消订阅逻辑
    
    return ESP_OK;
}

mqtt_connection_state_t mqtt_client_get_state(void)
{
    return g_mqtt_state;
}

bool mqtt_client_is_connected(void)
{
    return g_mqtt_state == MQTT_STATE_CONNECTED;
}

esp_err_t mqtt_client_get_statistics(mqtt_statistics_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(stats, &g_mqtt_stats, sizeof(mqtt_statistics_t));
    return ESP_OK;
}

esp_err_t mqtt_client_reset_statistics(void)
{
    memset(&g_mqtt_stats, 0, sizeof(mqtt_statistics_t));
    g_mqtt_stats.state = g_mqtt_state;
    return ESP_OK;
}

esp_err_t mqtt_client_set_will(const char *topic, const void *payload, size_t payload_len,
                               mqtt_qos_level_t qos, bool retain)
{
    if (!g_mqtt_initialized) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // TODO: 实现遗嘱消息设置
    
    return ESP_OK;
}

esp_err_t mqtt_client_update_config(const mqtt_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&g_mqtt_config, config, sizeof(mqtt_config_t));
    
    return ESP_OK;
}

const char *mqtt_client_get_state_string(mqtt_connection_state_t state)
{
    switch (state) {
        case MQTT_STATE_DISCONNECTED: return "DISCONNECTED";
        case MQTT_STATE_CONNECTING: return "CONNECTING";
        case MQTT_STATE_CONNECTED: return "CONNECTED";
        case MQTT_STATE_RECONNECTING: return "RECONNECTING";
        case MQTT_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

esp_err_t mqtt_client_set_auto_reconnect(bool enable)
{
    g_auto_reconnect = enable;
    return ESP_OK;
}

esp_err_t mqtt_client_set_reconnect_interval(uint32_t interval_ms)
{
    g_reconnect_interval = interval_ms;
    return ESP_OK;
}

esp_err_t mqtt_client_reset_reconnect_attempts(void)
{
    g_auto_reconnect = true;
    ESP_LOGI(TAG, "🔄 Auto-reconnect enabled");
    return ESP_OK;
}

uint32_t mqtt_client_get_reconnect_attempts(void)
{
    return 0; // 依赖ESP-IDF自动重连，不再跟踪重连次数
}

uint32_t mqtt_client_get_reconnect_interval(void)
{
    return g_reconnect_interval;
}