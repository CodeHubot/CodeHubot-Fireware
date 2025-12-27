/**
 * @file bt_provision_cmd.c
 * @brief 蓝牙配网功能的命令处理和WiFi连接测试实现
 * 
 * 实现JSON命令解析、WiFi连接测试、服务器连接测试等功能
 * 
 * @author AIOT Team
 * @date 2024-01-01
 */

#include "bt_provision.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#endif

// ==================== 私有常量 ====================

static const char* TAG = "BT_PROVISION_CMD";

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

// WiFi事件位定义
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// ==================== 私有变量 ====================

static int g_wifi_retry_num = 0;
static bool g_wifi_connected = false;

// ==================== 外部变量声明 ====================

extern bt_provision_wifi_config_t g_wifi_config;
extern bt_provision_server_config_t g_server_config;
extern EventGroupHandle_t g_wifi_event_group;

// ==================== 私有函数声明 ====================

static bt_provision_err_t bt_provision_handle_get_device_info(cJSON* request, char** response);
static bt_provision_err_t bt_provision_handle_set_wifi_config(cJSON* request, char** response);
static bt_provision_err_t bt_provision_handle_set_server_config(cJSON* request, char** response);
static bt_provision_err_t bt_provision_handle_start_provision(cJSON* request, char** response);
static bt_provision_err_t bt_provision_handle_get_provision_status(cJSON* request, char** response);
static bt_provision_err_t bt_provision_handle_reset_config(cJSON* request, char** response);

static bt_provision_err_t bt_provision_test_wifi_connection(const bt_provision_wifi_config_t* config);
static bt_provision_err_t bt_provision_test_server_connection(const bt_provision_server_config_t* config);
static esp_err_t bt_provision_http_event_handler(esp_http_client_event_t *evt);

// ==================== 公共函数实现 ====================

bt_provision_err_t bt_provision_process_command(const char* json_data)
{
    if (!json_data) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    cJSON* json = cJSON_Parse(json_data);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return BT_PROVISION_ERR_JSON_PARSE_FAILED;
    }
    
    cJSON* cmd_item = cJSON_GetObjectItem(json, "cmd");
    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'cmd' field");
        cJSON_Delete(json);
        return BT_PROVISION_ERR_JSON_PARSE_FAILED;
    }
    
    const char* cmd = cmd_item->valuestring;
    char* response = NULL;
    bt_provision_err_t ret = BT_PROVISION_ERR_OK;
    
    ESP_LOGI(TAG, "Processing command: %s", cmd);
    
    // 根据命令类型处理
    if (strcmp(cmd, "get_device_info") == 0) {
        ret = bt_provision_handle_get_device_info(json, &response);
    } else if (strcmp(cmd, "set_wifi_config") == 0) {
        ret = bt_provision_handle_set_wifi_config(json, &response);
    } else if (strcmp(cmd, "set_server_config") == 0) {
        ret = bt_provision_handle_set_server_config(json, &response);
    } else if (strcmp(cmd, "start_provision") == 0) {
        ret = bt_provision_handle_start_provision(json, &response);
    } else if (strcmp(cmd, "get_provision_status") == 0) {
        ret = bt_provision_handle_get_provision_status(json, &response);
    } else if (strcmp(cmd, "reset_config") == 0) {
        ret = bt_provision_handle_reset_config(json, &response);
    } else {
        ESP_LOGE(TAG, "Unknown command: %s", cmd);
        ret = BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 发送响应
    if (response) {
        extern bt_provision_err_t bt_provision_send_notification(const char* data);
        bt_provision_send_notification(response);
        free(response);
    }
    
    cJSON_Delete(json);
    return ret;
}

char* bt_provision_create_response(const char* cmd, int seq, const char* status, const char* message, cJSON* data)
{
    cJSON* response = cJSON_CreateObject();
    if (!response) {
        return NULL;
    }
    
    cJSON_AddStringToObject(response, "cmd", cmd);
    cJSON_AddNumberToObject(response, "seq", seq);
    cJSON_AddStringToObject(response, "status", status);
    
    if (message) {
        cJSON_AddStringToObject(response, "message", message);
    }
    
    if (data) {
        cJSON_AddItemToObject(response, "data", data);
    }
    
    char* json_string = cJSON_Print(response);
    cJSON_Delete(response);
    
    return json_string;
}

void bt_provision_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_wifi_retry_num < 5) {
            esp_wifi_connect();
            g_wifi_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
        }
        g_wifi_connected = false;
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_retry_num = 0;
        g_wifi_connected = true;
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ==================== 私有函数实现 ====================

static bt_provision_err_t bt_provision_handle_get_device_info(cJSON* request, char** response)
{
    cJSON* seq_item = cJSON_GetObjectItem(request, "seq");
    int seq = seq_item ? seq_item->valueint : 0;
    
    bt_provision_device_info_t device_info;
    bt_provision_err_t ret = bt_provision_get_device_info(&device_info);
    
    if (ret == BT_PROVISION_ERR_OK) {
        cJSON* data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "device_name", device_info.device_name);
        cJSON_AddStringToObject(data, "mac_address", device_info.mac_address);
        cJSON_AddStringToObject(data, "firmware_version", device_info.firmware_version);
        cJSON_AddStringToObject(data, "chip_model", device_info.chip_model);
        cJSON_AddStringToObject(data, "wifi_status", device_info.wifi_status);
        cJSON_AddStringToObject(data, "provision_status", device_info.provision_status);
        
        *response = bt_provision_create_response("get_device_info", seq, "success", "Device info retrieved", data);
    } else {
        *response = bt_provision_create_response("get_device_info", seq, "error", 
                                                bt_provision_get_error_string(ret), NULL);
    }
    
    return ret;
}

static bt_provision_err_t bt_provision_handle_set_wifi_config(cJSON* request, char** response)
{
    cJSON* seq_item = cJSON_GetObjectItem(request, "seq");
    int seq = seq_item ? seq_item->valueint : 0;
    
    cJSON* data = cJSON_GetObjectItem(request, "data");
    if (!data) {
        *response = bt_provision_create_response("set_wifi_config", seq, "error", "Missing data field", NULL);
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    cJSON* ssid_item = cJSON_GetObjectItem(data, "ssid");
    cJSON* password_item = cJSON_GetObjectItem(data, "password");
    cJSON* security_item = cJSON_GetObjectItem(data, "security");
    
    if (!ssid_item || !cJSON_IsString(ssid_item)) {
        *response = bt_provision_create_response("set_wifi_config", seq, "error", "Missing or invalid SSID", NULL);
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    bt_provision_wifi_config_t wifi_config = {0};
    strncpy(wifi_config.ssid, ssid_item->valuestring, BT_PROVISION_SSID_MAX - 1);
    
    if (password_item && cJSON_IsString(password_item)) {
        strncpy(wifi_config.password, password_item->valuestring, BT_PROVISION_PASSWORD_MAX - 1);
    }
    
    if (security_item && cJSON_IsNumber(security_item)) {
        wifi_config.security = security_item->valueint;
    } else {
        wifi_config.security = BT_PROVISION_WIFI_AUTH_WPA2_PSK;
    }
    
    wifi_config.configured = true;
    
    bt_provision_err_t ret = bt_provision_set_wifi_config(&wifi_config);
    
    if (ret == BT_PROVISION_ERR_OK) {
        *response = bt_provision_create_response("set_wifi_config", seq, "success", "WiFi config saved", NULL);
    } else {
        *response = bt_provision_create_response("set_wifi_config", seq, "error", 
                                                bt_provision_get_error_string(ret), NULL);
    }
    
    return ret;
}

static bt_provision_err_t bt_provision_handle_set_server_config(cJSON* request, char** response)
{
    cJSON* seq_item = cJSON_GetObjectItem(request, "seq");
    int seq = seq_item ? seq_item->valueint : 0;
    
    cJSON* data = cJSON_GetObjectItem(request, "data");
    if (!data) {
        *response = bt_provision_create_response("set_server_config", seq, "error", "Missing data field", NULL);
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    cJSON* url_item = cJSON_GetObjectItem(data, "url");
    cJSON* port_item = cJSON_GetObjectItem(data, "port");
    cJSON* api_key_item = cJSON_GetObjectItem(data, "api_key");
    
    if (!url_item || !cJSON_IsString(url_item)) {
        *response = bt_provision_create_response("set_server_config", seq, "error", "Missing or invalid server URL", NULL);
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    bt_provision_server_config_t server_config = {0};
    strncpy(server_config.server_url, url_item->valuestring, BT_PROVISION_SERVER_URL_MAX - 1);
    
    if (port_item && cJSON_IsNumber(port_item)) {
        server_config.server_port = port_item->valueint;
    } else {
        server_config.server_port = 80;
    }
    
    if (api_key_item && cJSON_IsString(api_key_item)) {
        strncpy(server_config.api_key, api_key_item->valuestring, BT_PROVISION_API_KEY_MAX - 1);
    }
    
    server_config.configured = true;
    
    bt_provision_err_t ret = bt_provision_set_server_config(&server_config);
    
    if (ret == BT_PROVISION_ERR_OK) {
        *response = bt_provision_create_response("set_server_config", seq, "success", "Server config saved", NULL);
    } else {
        *response = bt_provision_create_response("set_server_config", seq, "error", 
                                                bt_provision_get_error_string(ret), NULL);
    }
    
    return ret;
}

static bt_provision_err_t bt_provision_handle_start_provision(cJSON* request, char** response)
{
    cJSON* seq_item = cJSON_GetObjectItem(request, "seq");
    int seq = seq_item ? seq_item->valueint : 0;
    
    extern void bt_provision_set_state(bt_provision_state_t state, const char* message);
    bt_provision_set_state(BT_PROVISION_STATE_CONFIGURING, "Starting provisioning process");
    
    bt_provision_err_t ret = BT_PROVISION_ERR_OK;
    
    // 测试WiFi连接
    if (g_wifi_config.configured) {
        bt_provision_set_state(BT_PROVISION_STATE_WIFI_CONNECTING, "Testing WiFi connection");
        ret = bt_provision_test_wifi_connection(&g_wifi_config);
        
        if (ret != BT_PROVISION_ERR_OK) {
            bt_provision_set_state(BT_PROVISION_STATE_FAILED, "WiFi connection failed");
            *response = bt_provision_create_response("start_provision", seq, "error", "WiFi connection failed", NULL);
            return ret;
        }
    } else {
        bt_provision_set_state(BT_PROVISION_STATE_FAILED, "WiFi not configured");
        *response = bt_provision_create_response("start_provision", seq, "error", "WiFi not configured", NULL);
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 测试服务器连接
    if (g_server_config.configured) {
        bt_provision_set_state(BT_PROVISION_STATE_SERVER_TESTING, "Testing server connection");
        ret = bt_provision_test_server_connection(&g_server_config);
        
        if (ret != BT_PROVISION_ERR_OK) {
            bt_provision_set_state(BT_PROVISION_STATE_FAILED, "Server connection failed");
            *response = bt_provision_create_response("start_provision", seq, "error", "Server connection failed", NULL);
            return ret;
        }
    }
    
    bt_provision_set_state(BT_PROVISION_STATE_SUCCESS, "Provisioning completed successfully");
    *response = bt_provision_create_response("start_provision", seq, "success", "Provisioning completed", NULL);
    
    return BT_PROVISION_ERR_OK;
}

static bt_provision_err_t bt_provision_handle_get_provision_status(cJSON* request, char** response)
{
    cJSON* seq_item = cJSON_GetObjectItem(request, "seq");
    int seq = seq_item ? seq_item->valueint : 0;
    
    bt_provision_status_t status;
    bt_provision_err_t ret = bt_provision_get_status(&status);
    
    if (ret == BT_PROVISION_ERR_OK) {
        cJSON* data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "state", bt_provision_get_state_string(status.state));
        cJSON_AddNumberToObject(data, "progress", status.progress);
        cJSON_AddStringToObject(data, "message", status.message);
        cJSON_AddStringToObject(data, "wifi_status", status.wifi_status);
        cJSON_AddStringToObject(data, "server_status", status.server_status);
        cJSON_AddStringToObject(data, "wifi_ip", status.wifi_ip);
        
        *response = bt_provision_create_response("get_provision_status", seq, "success", "Status retrieved", data);
    } else {
        *response = bt_provision_create_response("get_provision_status", seq, "error", 
                                                bt_provision_get_error_string(ret), NULL);
    }
    
    return ret;
}

static bt_provision_err_t bt_provision_handle_reset_config(cJSON* request, char** response)
{
    cJSON* seq_item = cJSON_GetObjectItem(request, "seq");
    int seq = seq_item ? seq_item->valueint : 0;
    
    cJSON* data = cJSON_GetObjectItem(request, "data");
    bool reset_wifi = true;
    bool reset_server = true;
    
    if (data) {
        cJSON* wifi_item = cJSON_GetObjectItem(data, "reset_wifi");
        cJSON* server_item = cJSON_GetObjectItem(data, "reset_server");
        
        if (wifi_item && cJSON_IsBool(wifi_item)) {
            reset_wifi = cJSON_IsTrue(wifi_item);
        }
        if (server_item && cJSON_IsBool(server_item)) {
            reset_server = cJSON_IsTrue(server_item);
        }
    }
    
    bt_provision_err_t ret = bt_provision_reset_config(reset_wifi, reset_server);
    
    if (ret == BT_PROVISION_ERR_OK) {
        *response = bt_provision_create_response("reset_config", seq, "success", "Configuration reset", NULL);
    } else {
        *response = bt_provision_create_response("reset_config", seq, "error", 
                                                bt_provision_get_error_string(ret), NULL);
    }
    
    return ret;
}

static bt_provision_err_t bt_provision_test_wifi_connection(const bt_provision_wifi_config_t* config)
{
    if (!config || !config->configured) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
#ifdef ESP_PLATFORM
    // 初始化WiFi
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    
    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(10000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", config->ssid);
        return BT_PROVISION_ERR_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", config->ssid);
        return BT_PROVISION_ERR_WIFI_CONNECT_FAILED;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return BT_PROVISION_ERR_TIMEOUT;
    }
#else
    // 非ESP平台的模拟实现
    ESP_LOGI(TAG, "Simulating WiFi connection test for SSID: %s", config->ssid);
    return BT_PROVISION_ERR_OK;
#endif
}

static bt_provision_err_t bt_provision_test_server_connection(const bt_provision_server_config_t* config)
{
    if (!config || !config->configured) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
#ifdef ESP_PLATFORM
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/health", config->server_url, config->server_port);
    
    esp_http_client_config_t http_config = {
        .url = url,
        .event_handler = bt_provision_http_event_handler,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    
    // 添加认证头
    if (strlen(config->api_key) > 0) {
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", config->api_key);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d", status_code);
        
        esp_http_client_cleanup(client);
        
        if (status_code >= 200 && status_code < 300) {
            return BT_PROVISION_ERR_OK;
        } else {
            return BT_PROVISION_ERR_SERVER_CONNECT_FAILED;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return BT_PROVISION_ERR_SERVER_CONNECT_FAILED;
    }
#else
    // 非ESP平台的模拟实现
    ESP_LOGI(TAG, "Simulating server connection test for URL: %s:%d", config->url, config->port);
    return BT_PROVISION_ERR_OK;
#endif
}

static esp_err_t bt_provision_http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;
    static int output_len;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_http_client_get_and_clear_last_tls_error(evt->client, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

// 实现bt_provision_test_wifi函数
bt_provision_err_t bt_provision_test_wifi(void)
{
    if (!g_wifi_config.configured) {
        ESP_LOGE(TAG, "WiFi not configured");
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    return bt_provision_test_wifi_connection(&g_wifi_config);
}