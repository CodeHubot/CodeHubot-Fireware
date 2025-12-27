/**
 * @file provisioning_client.c
 * @brief 配置服务客户端实现（GET请求方式）
 * 
 * 调用新的配置服务GET接口获取设备配置
 */

#include "provisioning_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "cJSON.h"
#include <string.h>

#define TAG "PROVISION_CLIENT"
#define MAX_HTTP_RECV_BUFFER 8192

static char http_response_buffer[MAX_HTTP_RECV_BUFFER];
static int http_response_len = 0;

/**
 * @brief HTTP事件处理器
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (http_response_len + evt->data_len < MAX_HTTP_RECV_BUFFER) {
                memcpy(http_response_buffer + http_response_len, evt->data, evt->data_len);
                http_response_len += evt->data_len;
                http_response_buffer[http_response_len] = '\0';
            } else {
                ESP_LOGW(TAG, "响应缓冲区已满");
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t provisioning_client_get_config(
    const char *server_address,
    const char *product_id,
    const char *firmware_version,
    provisioning_config_t *config)
{
    if (!server_address || !config || !product_id || strlen(product_id) == 0) {
        ESP_LOGE(TAG, "参数错误: server_address=%p, product_id=%s, config=%p", 
                 server_address, product_id ? product_id : "NULL", config);
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(config, 0, sizeof(provisioning_config_t));
    
    // 获取MAC地址
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取MAC地址失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // 构建URL（GET请求，product_id 为必需参数）
    char url[512];
    if (firmware_version && strlen(firmware_version) > 0) {
        snprintf(url, sizeof(url), "%s/device/info?mac=%s&product_id=%s&firmware_version=%s",
                 server_address, mac_str, product_id, firmware_version);
    } else {
        snprintf(url, sizeof(url), "%s/device/info?mac=%s&product_id=%s",
                 server_address, mac_str, product_id);
    }
    
    ESP_LOGI(TAG, "🌐 请求设备配置: %s", url);
    
    // 重置响应缓冲区
    http_response_len = 0;
    memset(http_response_buffer, 0, sizeof(http_response_buffer));
    
    // 配置HTTP客户端（支持HTTP和HTTPS）
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .timeout_ms = 15000,
        .buffer_size = MAX_HTTP_RECV_BUFFER,
        .buffer_size_tx = 1024,
        // HTTPS配置：跳过证书验证（开发测试用）
        .transport_type = HTTP_TRANSPORT_UNKNOWN,  // 自动检测HTTP/HTTPS
        .skip_cert_common_name_check = true,
        .use_global_ca_store = false,
        .crt_bundle_attach = NULL,  // 不使用证书包
        .keep_alive_enable = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        ESP_LOGE(TAG, "❌ HTTP客户端初始化失败");
        return ESP_FAIL;
    }
    
    // 发送请求
    ret = esp_http_client_perform(client);
    
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP状态码: %d", status_code);
        
        if (status_code == 200) {
            // 解析JSON响应
            cJSON *root = cJSON_Parse(http_response_buffer);
            if (root) {
                // 提取设备信息
                cJSON *device_id = cJSON_GetObjectItem(root, "device_id");
                cJSON *device_uuid = cJSON_GetObjectItem(root, "device_uuid");
                cJSON *mac_address = cJSON_GetObjectItem(root, "mac_address");
                cJSON *product_id = cJSON_GetObjectItem(root, "product_id");
                
                if (device_id && cJSON_IsString(device_id)) {
                    strncpy(config->device_id, device_id->valuestring, sizeof(config->device_id) - 1);
                }
                if (device_uuid && cJSON_IsString(device_uuid)) {
                    strncpy(config->device_uuid, device_uuid->valuestring, sizeof(config->device_uuid) - 1);
                }
                if (mac_address && cJSON_IsString(mac_address)) {
                    strncpy(config->mac_address, mac_address->valuestring, sizeof(config->mac_address) - 1);
                }
                if (product_id && cJSON_IsString(product_id)) {
                    strncpy(config->product_id, product_id->valuestring, sizeof(config->product_id) - 1);
                }
                
                // 提取MQTT配置
                cJSON *mqtt_config = cJSON_GetObjectItem(root, "mqtt_config");
                if (mqtt_config && cJSON_IsObject(mqtt_config)) {
                    cJSON *broker = cJSON_GetObjectItem(mqtt_config, "broker");
                    cJSON *port = cJSON_GetObjectItem(mqtt_config, "port");
                    cJSON *username = cJSON_GetObjectItem(mqtt_config, "username");
                    cJSON *password = cJSON_GetObjectItem(mqtt_config, "password");
                    cJSON *use_ssl = cJSON_GetObjectItem(mqtt_config, "use_ssl");
                    
                    if (broker && cJSON_IsString(broker)) {
                        strncpy(config->mqtt_broker, broker->valuestring, sizeof(config->mqtt_broker) - 1);
                    }
                    if (port && cJSON_IsNumber(port)) {
                        config->mqtt_port = port->valueint;
                    }
                    if (username && cJSON_IsString(username)) {
                        strncpy(config->mqtt_username, username->valuestring, sizeof(config->mqtt_username) - 1);
                    }
                    if (password && cJSON_IsString(password)) {
                        strncpy(config->mqtt_password, password->valuestring, sizeof(config->mqtt_password) - 1);
                    }
                    if (use_ssl && cJSON_IsBool(use_ssl)) {
                        config->mqtt_use_ssl = cJSON_IsTrue(use_ssl);
                    }
                    
                    // 提取MQTT主题
                    cJSON *topics = cJSON_GetObjectItem(mqtt_config, "topics");
                    if (topics && cJSON_IsObject(topics)) {
                        cJSON *data_topic = cJSON_GetObjectItem(topics, "data");
                        cJSON *control_topic = cJSON_GetObjectItem(topics, "control");
                        cJSON *status_topic = cJSON_GetObjectItem(topics, "status");
                        cJSON *heartbeat_topic = cJSON_GetObjectItem(topics, "heartbeat");
                        
                        if (data_topic && cJSON_IsString(data_topic)) {
                            strncpy(config->mqtt_topic_data, data_topic->valuestring, sizeof(config->mqtt_topic_data) - 1);
                        }
                        if (control_topic && cJSON_IsString(control_topic)) {
                            strncpy(config->mqtt_topic_control, control_topic->valuestring, sizeof(config->mqtt_topic_control) - 1);
                        }
                        if (status_topic && cJSON_IsString(status_topic)) {
                            strncpy(config->mqtt_topic_status, status_topic->valuestring, sizeof(config->mqtt_topic_status) - 1);
                        }
                        if (heartbeat_topic && cJSON_IsString(heartbeat_topic)) {
                            strncpy(config->mqtt_topic_heartbeat, heartbeat_topic->valuestring, sizeof(config->mqtt_topic_heartbeat) - 1);
                        }
                    }
                    
                    config->has_mqtt_config = true;
                }
                
                // 提取固件更新信息
                cJSON *firmware_update = cJSON_GetObjectItem(root, "firmware_update");
                if (firmware_update && !cJSON_IsNull(firmware_update)) {
                    cJSON *available = cJSON_GetObjectItem(firmware_update, "available");
                    cJSON *version = cJSON_GetObjectItem(firmware_update, "version");
                    cJSON *url_obj = cJSON_GetObjectItem(firmware_update, "download_url");
                    cJSON *size = cJSON_GetObjectItem(firmware_update, "file_size");
                    cJSON *checksum = cJSON_GetObjectItem(firmware_update, "checksum");
                    cJSON *changelog = cJSON_GetObjectItem(firmware_update, "changelog");
                    
                    if (available && cJSON_IsTrue(available)) {
                        config->has_firmware_update = true;
                        
                        if (version && cJSON_IsString(version)) {
                            strncpy(config->firmware_version, version->valuestring, sizeof(config->firmware_version) - 1);
                        }
                        if (url_obj && cJSON_IsString(url_obj)) {
                            strncpy(config->firmware_url, url_obj->valuestring, sizeof(config->firmware_url) - 1);
                        }
                        if (size && cJSON_IsNumber(size)) {
                            config->firmware_size = size->valueint;
                        }
                        if (checksum && cJSON_IsString(checksum)) {
                            strncpy(config->firmware_checksum, checksum->valuestring, sizeof(config->firmware_checksum) - 1);
                        }
                        if (changelog && cJSON_IsString(changelog)) {
                            strncpy(config->firmware_changelog, changelog->valuestring, sizeof(config->firmware_changelog) - 1);
                        }
                        
                        ESP_LOGI(TAG, "⚠️ 发现固件更新: %s", config->firmware_version);
                    }
                }
                
                cJSON_Delete(root);
                
                ESP_LOGI(TAG, "✅ 配置获取成功:");
                ESP_LOGI(TAG, "   Device ID: %s", config->device_id);
                ESP_LOGI(TAG, "   Device UUID: %s", config->device_uuid);
                ESP_LOGI(TAG, "   MQTT Broker: %s:%d", config->mqtt_broker, config->mqtt_port);
                ESP_LOGI(TAG, "   固件更新: %s", config->has_firmware_update ? "有" : "无");
                
                ret = ESP_OK;
            } else {
                ESP_LOGE(TAG, "❌ JSON解析失败");
                ret = ESP_FAIL;
            }
        } else if (status_code == 404) {
            ESP_LOGE(TAG, "❌ 设备未注册（404）");
            ret = ESP_ERR_NOT_FOUND;
        } else {
            ESP_LOGE(TAG, "❌ HTTP请求失败: %d", status_code);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP请求失败: %s", esp_err_to_name(ret));
    }
    
    esp_http_client_cleanup(client);
    return ret;
}
