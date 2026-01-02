/**
 * @file device_config.c
 * @brief 设备配置获取客户端实现
 */

#include "device_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "DeviceConfig";

#define HTTP_RESPONSE_BUFFER_SIZE 4096

/**
 * @brief HTTP事件处理
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // 数据接收在主函数中处理
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief 从服务器获取设备配置
 */
esp_err_t device_config_get_from_server(
    const char *server_address,
    const char *product_id,
    const char *firmware_version,
    device_config_t *config
)
{
    if (!server_address || !product_id || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "开始获取设备配置...");
    ESP_LOGI(TAG, "服务器: %s", server_address);
    ESP_LOGI(TAG, "产品ID: %s", product_id);
    
    // 获取MAC地址
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取MAC地址失败");
        return ret;
    }
    
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ESP_LOGI(TAG, "MAC地址: %s", mac_str);
    
    // 构建URL（server_address已包含协议和端口，如 http://conf.aiot.powertechhub.com:8001）
    char url[512];
    snprintf(url, sizeof(url), 
             "%s/device/info?mac=%s&product_id=%s&firmware_version=%s",
             server_address, mac_str, product_id, firmware_version ? firmware_version : "1.0.0");
    
    ESP_LOGI(TAG, "请求URL: %s", url);
    
    // 配置HTTP客户端
    esp_http_client_config_t http_config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        return ESP_FAIL;
    }
    
    // 分配响应缓冲区
    char *response_buffer = malloc(HTTP_RESPONSE_BUFFER_SIZE);
    if (!response_buffer) {
        ESP_LOGE(TAG, "分配响应缓冲区失败");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    // 发送HTTP GET请求
    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP请求失败: %s", esp_err_to_name(ret));
        free(response_buffer);
        esp_http_client_cleanup(client);
        return ret;
    }
    
    // 读取响应
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "HTTP状态码: %d", status_code);
    
    if (status_code == 200) {
        // 读取响应体
        int read_len = esp_http_client_read(client, response_buffer, HTTP_RESPONSE_BUFFER_SIZE - 1);
        if (read_len > 0) {
            response_buffer[read_len] = '\0';
            ESP_LOGI(TAG, "响应长度: %d", read_len);
            ESP_LOGD(TAG, "响应内容: %s", response_buffer);
            
            // 解析JSON
            cJSON *root = cJSON_Parse(response_buffer);
            if (root) {
                cJSON *device_id = cJSON_GetObjectItem(root, "device_id");
                cJSON *device_uuid = cJSON_GetObjectItem(root, "device_uuid");
                cJSON *mqtt_config = cJSON_GetObjectItem(root, "mqtt_config");
                
                if (device_id && device_uuid) {
                    strncpy(config->device_id, device_id->valuestring, sizeof(config->device_id) - 1);
                    strncpy(config->device_uuid, device_uuid->valuestring, sizeof(config->device_uuid) - 1);
                    strncpy(config->mac_address, mac_str, sizeof(config->mac_address) - 1);
                    
                    // 解析MQTT配置
                    if (mqtt_config) {
                        config->has_mqtt_config = true;
                        
                        cJSON *broker = cJSON_GetObjectItem(mqtt_config, "broker");
                        cJSON *port = cJSON_GetObjectItem(mqtt_config, "port");
                        cJSON *username = cJSON_GetObjectItem(mqtt_config, "username");
                        cJSON *password = cJSON_GetObjectItem(mqtt_config, "password");
                        
                        if (broker) strncpy(config->mqtt_broker, broker->valuestring, sizeof(config->mqtt_broker) - 1);
                        if (port) config->mqtt_port = port->valueint;
                        if (username) strncpy(config->mqtt_username, username->valuestring, sizeof(config->mqtt_username) - 1);
                        if (password) strncpy(config->mqtt_password, password->valuestring, sizeof(config->mqtt_password) - 1);
                        
                        // MQTT主题（在topics对象中）
                        cJSON *topics = cJSON_GetObjectItem(mqtt_config, "topics");
                        if (topics) {
                            cJSON *topic_data = cJSON_GetObjectItem(topics, "data");
                            cJSON *topic_control = cJSON_GetObjectItem(topics, "control");
                            cJSON *topic_status = cJSON_GetObjectItem(topics, "status");
                            cJSON *topic_heartbeat = cJSON_GetObjectItem(topics, "heartbeat");
                            
                            if (topic_data) strncpy(config->mqtt_topic_data, topic_data->valuestring, sizeof(config->mqtt_topic_data) - 1);
                            if (topic_control) strncpy(config->mqtt_topic_control, topic_control->valuestring, sizeof(config->mqtt_topic_control) - 1);
                            if (topic_status) strncpy(config->mqtt_topic_status, topic_status->valuestring, sizeof(config->mqtt_topic_status) - 1);
                            if (topic_heartbeat) strncpy(config->mqtt_topic_heartbeat, topic_heartbeat->valuestring, sizeof(config->mqtt_topic_heartbeat) - 1);
                        }
                    }
                    
                    cJSON_Delete(root);
                    
                    ESP_LOGI(TAG, "✅ 设备配置获取成功");
                    ESP_LOGI(TAG, "   Device ID: %s", config->device_id);
                    ESP_LOGI(TAG, "   Device UUID: %s", config->device_uuid);
                    if (config->has_mqtt_config) {
                        ESP_LOGI(TAG, "   MQTT Broker: %s:%d", config->mqtt_broker, config->mqtt_port);
                        ESP_LOGI(TAG, "   MQTT用户名: %s", config->mqtt_username);
                        ESP_LOGD(TAG, "   MQTT密码: %s", config->mqtt_password);
                        if (strlen(config->mqtt_topic_data) > 0) {
                            ESP_LOGI(TAG, "   数据主题: %s", config->mqtt_topic_data);
                        }
                        if (strlen(config->mqtt_topic_control) > 0) {
                            ESP_LOGI(TAG, "   控制主题: %s", config->mqtt_topic_control);
                        }
                    }
                    
                    ret = ESP_OK;
                } else {
                    ESP_LOGE(TAG, "JSON数据不完整");
                    cJSON_Delete(root);
                    ret = ESP_FAIL;
                }
            } else {
                ESP_LOGE(TAG, "JSON解析失败");
                ret = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "读取响应失败");
            ret = ESP_FAIL;
        }
    } else if (status_code == 404) {
        ESP_LOGW(TAG, "❌ 设备未注册（404）");
        ESP_LOGW(TAG, "   请先在管理页面注册设备");
        ESP_LOGW(TAG, "   MAC地址: %s", mac_str);
        ret = ESP_ERR_NOT_FOUND;
    } else {
        ESP_LOGE(TAG, "HTTP请求失败，状态码: %d", status_code);
        ret = ESP_FAIL;
    }
    
    free(response_buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    return ret;
}

