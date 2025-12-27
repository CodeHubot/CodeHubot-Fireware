#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "device_registration.h"
#include "app_config.h"  // 产品配置（PRODUCT_ID, PRODUCT_VERSION等）

static const char *TAG = "DEVICE_REG";

// NVS存储键名
#define NVS_NAMESPACE "device_reg"
#define NVS_KEY_DEVICE_ID "device_id"
#define NVS_KEY_DEVICE_UUID "device_uuid"
#define NVS_KEY_DEVICE_SECRET "device_secret"
#define NVS_KEY_MAC_ADDRESS "mac_address"
#define NVS_KEY_REGISTERED "registered"

// HTTP响应缓冲区大小
#define HTTP_RESPONSE_BUFFER_SIZE 2048

// 设备注册模块状态
static device_registration_state_t g_reg_state = DEVICE_REG_STATE_IDLE;
static device_registration_config_t g_reg_config;
static device_registration_info_t g_reg_info;
static TaskHandle_t g_reg_task_handle = NULL;
static bool g_reg_initialized = false;
static char g_response_buffer[HTTP_RESPONSE_BUFFER_SIZE];

// HTTP事件处理函数
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // 将响应数据复制到缓冲区
                int copy_len = evt->data_len;
                if (copy_len >= HTTP_RESPONSE_BUFFER_SIZE) {
                    copy_len = HTTP_RESPONSE_BUFFER_SIZE - 1;
                }
                memcpy(g_response_buffer, evt->data, copy_len);
                g_response_buffer[copy_len] = '\0';
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// 获取MAC地址
static esp_err_t get_mac_address(char *mac_str, size_t mac_str_size)
{
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
        return ret;
    }
    
    snprintf(mac_str, mac_str_size, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return ESP_OK;
}

// 解析JSON响应
static esp_err_t parse_registration_response(const char *json_str, device_registration_info_t *info)
{
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }
    
    cJSON *device_id = cJSON_GetObjectItem(json, "device_id");
    cJSON *device_uuid = cJSON_GetObjectItem(json, "device_uuid");
    cJSON *device_secret = cJSON_GetObjectItem(json, "device_secret");
    cJSON *mac_address = cJSON_GetObjectItem(json, "mac_address");
    
    if (cJSON_IsString(device_id) && device_id->valuestring != NULL) {
        strncpy(info->device_id, device_id->valuestring, sizeof(info->device_id) - 1);
    }
    
    if (cJSON_IsString(device_uuid) && device_uuid->valuestring != NULL) {
        strncpy(info->device_uuid, device_uuid->valuestring, sizeof(info->device_uuid) - 1);
    }
    
    if (cJSON_IsString(device_secret) && device_secret->valuestring != NULL) {
        strncpy(info->device_secret, device_secret->valuestring, sizeof(info->device_secret) - 1);
    }
    
    if (cJSON_IsString(mac_address) && mac_address->valuestring != NULL) {
        strncpy(info->mac_address, mac_address->valuestring, sizeof(info->mac_address) - 1);
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

// 步骤1: MAC地址查询 - 获取device_id, uuid, secret
static esp_err_t perform_mac_lookup(const char *firmware_version, const char *hardware_version)
{
    char mac_str[18];
    esp_err_t ret = get_mac_address(mac_str, sizeof(mac_str));
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 构建URL
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/api/devices/mac/lookup", 
             g_reg_config.server_url, g_reg_config.server_port);
    
    // 构建POST数据
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "mac_address", mac_str);
    cJSON_AddStringToObject(json, "device_type", "ESP32-S3");
    cJSON_AddStringToObject(json, "firmware_version", firmware_version);
    cJSON_AddStringToObject(json, "hardware_version", hardware_version);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "📡 Step 1: MAC Lookup - Querying device credentials");
    ESP_LOGI(TAG, "   MAC: %s", mac_str);
    
    // 配置HTTP客户端
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = g_reg_config.timeout_ms,
        .method = HTTP_METHOD_POST,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(json_string);
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 设置请求头
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    // 执行请求
    ret = esp_http_client_perform(client);
    
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            // 解析响应
            ret = parse_registration_response(g_response_buffer, &g_reg_info);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ MAC Lookup successful");
                ESP_LOGI(TAG, "   Device ID: %s", g_reg_info.device_id);
                ESP_LOGI(TAG, "   UUID: %s", g_reg_info.device_uuid);
                ESP_LOGI(TAG, "   Secret: %s", g_reg_info.device_secret);
            }
        } else {
            ESP_LOGE(TAG, "❌ MAC Lookup failed with status: %d", status_code);
            ESP_LOGE(TAG, "   Response: %s", g_response_buffer);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP request failed: %s", esp_err_to_name(ret));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// 步骤2: 正式注册 - 发送产品信息，完成正式注册
static esp_err_t perform_formal_registration(const char *firmware_version, const char *hardware_version,
                                              const char *product_code, const char *product_version,
                                              const char *manufacturer, const char *model)
{
    if (strlen(g_reg_info.device_id) == 0 || strlen(g_reg_info.device_secret) == 0) {
        ESP_LOGE(TAG, "Device ID or Secret is empty, cannot perform formal registration");
        return ESP_FAIL;
    }
    
    char mac_str[18];
    esp_err_t ret = get_mac_address(mac_str, sizeof(mac_str));
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 构建URL
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/api/devices/register", 
             g_reg_config.server_url, g_reg_config.server_port);
    
    // 构建POST数据 - 包含产品信息
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "device_id", g_reg_info.device_id);
    cJSON_AddStringToObject(json, "device_secret", g_reg_info.device_secret);
    cJSON_AddStringToObject(json, "firmware_version", firmware_version);
    cJSON_AddStringToObject(json, "hardware_version", hardware_version);
    cJSON_AddStringToObject(json, "manufacturer", manufacturer);
    cJSON_AddStringToObject(json, "model", model);
    cJSON_AddStringToObject(json, "mac_address", mac_str);
    
    // 产品信息
    cJSON_AddStringToObject(json, "product_code", product_code);
    cJSON_AddStringToObject(json, "product_version", product_version);
    
    // 设备能力
    cJSON *capabilities = cJSON_CreateObject();
    cJSON_AddBoolToObject(capabilities, "temperature", true);
    cJSON_AddBoolToObject(capabilities, "humidity", true);
    cJSON_AddBoolToObject(capabilities, "wifi", true);
    cJSON_AddBoolToObject(capabilities, "mqtt", true);
    cJSON_AddItemToObject(json, "device_capabilities", capabilities);
    
    // 传感器配置
    cJSON *sensor_config = cJSON_CreateObject();
    cJSON *temp_sensor = cJSON_CreateObject();
    cJSON_AddStringToObject(temp_sensor, "type", "analog");
    cJSON_AddNumberToObject(temp_sensor, "pin", 34);
    cJSON_AddStringToObject(temp_sensor, "unit", "°C");
    cJSON_AddItemToObject(sensor_config, "temperature", temp_sensor);
    
    cJSON *humi_sensor = cJSON_CreateObject();
    cJSON_AddStringToObject(humi_sensor, "type", "analog");
    cJSON_AddNumberToObject(humi_sensor, "pin", 35);
    cJSON_AddStringToObject(humi_sensor, "unit", "%");
    cJSON_AddItemToObject(sensor_config, "humidity", humi_sensor);
    cJSON_AddItemToObject(json, "device_sensor_config", sensor_config);
    
    // 控制配置
    cJSON *control_config = cJSON_CreateObject();
    cJSON *led = cJSON_CreateObject();
    cJSON_AddStringToObject(led, "type", "digital_output");
    cJSON_AddNumberToObject(led, "pin", 2);
    cJSON_AddStringToObject(led, "name", "LED");
    cJSON_AddItemToObject(control_config, "led", led);
    
    cJSON *relay = cJSON_CreateObject();
    cJSON_AddStringToObject(relay, "type", "digital_output");
    cJSON_AddNumberToObject(relay, "pin", 26);
    cJSON_AddStringToObject(relay, "name", "继电器");
    cJSON_AddItemToObject(control_config, "relay", relay);
    cJSON_AddItemToObject(json, "device_control_config", control_config);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "📡 Step 2: Formal Registration - Sending product information");
    ESP_LOGI(TAG, "   Product: %s v%s", product_code, product_version);
    
    // 配置HTTP客户端
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = g_reg_config.timeout_ms,
        .method = HTTP_METHOD_POST,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(json_string);
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 设置请求头
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    // 执行请求
    ret = esp_http_client_perform(client);
    
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            ESP_LOGI(TAG, "✅ Formal Registration successful");
            ESP_LOGI(TAG, "   Response: %s", g_response_buffer);
            ret = ESP_OK;
        } else {
            ESP_LOGE(TAG, "❌ Formal Registration failed with status: %d", status_code);
            ESP_LOGE(TAG, "   Response: %s", g_response_buffer);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP request failed: %s", esp_err_to_name(ret));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// 执行完整的设备注册流程（2步）
static esp_err_t perform_device_registration(const char *firmware_version, const char *hardware_version)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "🚀 Starting Device Registration Process");
    ESP_LOGI(TAG, "========================================");
    
    // 步骤1: MAC地址查询
    esp_err_t ret = perform_mac_lookup(firmware_version, hardware_version);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Step 1 failed: MAC Lookup");
        return ret;
    }
    
    ESP_LOGI(TAG, "   ⏸️  Waiting 2 seconds before formal registration...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 步骤2: 正式注册（带产品信息）
    // 使用配置的产品ID（从app_config.h读取，可通过Kconfig配置）
    ret = perform_formal_registration(firmware_version, hardware_version,
                                      PRODUCT_ID, PRODUCT_VERSION,
                                      MANUFACTURER, MODEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Step 2 failed: Formal Registration");
        return ret;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ Device Registration Complete!");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

// 设备注册任务
static void device_registration_task(void *pvParameters)
{
    char *firmware_version = (char *)pvParameters;
    char *hardware_version = firmware_version + strlen(firmware_version) + 1;
    
    g_reg_state = DEVICE_REG_STATE_REGISTERING;
    
    esp_err_t ret = ESP_FAIL;
    int retry_count = 0;
    
    while (retry_count < g_reg_config.max_retry_count) {
        ESP_LOGI(TAG, "Attempting device registration (attempt %d/%d)", 
                 retry_count + 1, g_reg_config.max_retry_count);
        
        ret = perform_device_registration(firmware_version, hardware_version);
        
        if (ret == ESP_OK) {
            // 注册成功，保存到NVS
            device_registration_save_to_nvs(&g_reg_info);
            g_reg_state = DEVICE_REG_STATE_REGISTERED;
            
            // 调用回调函数
            if (g_reg_config.event_callback) {
                g_reg_config.event_callback(DEVICE_REG_EVENT_SUCCESS, &g_reg_info);
            }
            break;
        }
        
        retry_count++;
        if (retry_count < g_reg_config.max_retry_count) {
            ESP_LOGW(TAG, "Registration failed, retrying in 5 seconds...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    
    if (ret != ESP_OK) {
        g_reg_state = DEVICE_REG_STATE_FAILED;
        
        // 调用回调函数
        if (g_reg_config.event_callback) {
            if (retry_count >= g_reg_config.max_retry_count) {
                g_reg_config.event_callback(DEVICE_REG_EVENT_TIMEOUT, NULL);
            } else {
                g_reg_config.event_callback(DEVICE_REG_EVENT_FAILED, NULL);
            }
        }
    }
    
    // 清理任务句柄
    g_reg_task_handle = NULL;
    vTaskDelete(NULL);
}

// 初始化设备注册模块
esp_err_t device_registration_init(const device_registration_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_reg_initialized) {
        ESP_LOGW(TAG, "Device registration already initialized");
        return ESP_OK;
    }
    
    memcpy(&g_reg_config, config, sizeof(device_registration_config_t));
    g_reg_state = DEVICE_REG_STATE_IDLE;
    g_reg_initialized = true;
    
    ESP_LOGI(TAG, "Device registration module initialized");
    return ESP_OK;
}

// 去初始化设备注册模块
esp_err_t device_registration_deinit(void)
{
    if (!g_reg_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_reg_task_handle != NULL) {
        vTaskDelete(g_reg_task_handle);
        g_reg_task_handle = NULL;
    }
    
    memset(&g_reg_config, 0, sizeof(device_registration_config_t));
    memset(&g_reg_info, 0, sizeof(device_registration_info_t));
    g_reg_state = DEVICE_REG_STATE_IDLE;
    g_reg_initialized = false;
    
    ESP_LOGI(TAG, "Device registration module deinitialized");
    return ESP_OK;
}

// 启动设备注册
esp_err_t device_registration_start(const char *firmware_version, const char *hardware_version)
{
    if (!g_reg_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_reg_task_handle != NULL) {
        ESP_LOGW(TAG, "Device registration already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 准备任务参数
    size_t fw_len = strlen(firmware_version) + 1;
    size_t hw_len = strlen(hardware_version) + 1;
    char *task_params = malloc(fw_len + hw_len);
    if (task_params == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    strcpy(task_params, firmware_version);
    strcpy(task_params + fw_len, hardware_version);
    
    // 创建注册任务
    BaseType_t ret = xTaskCreate(device_registration_task, "device_reg", 4096, 
                                task_params, 5, &g_reg_task_handle);
    
    if (ret != pdPASS) {
        free(task_params);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Device registration started");
    return ESP_OK;
}

// 获取注册状态
device_registration_state_t device_registration_get_state(void)
{
    return g_reg_state;
}

// 获取注册信息
esp_err_t device_registration_get_info(device_registration_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(info, &g_reg_info, sizeof(device_registration_info_t));
    return ESP_OK;
}

// 检查是否已注册
bool device_registration_is_registered(void)
{
    if (g_reg_state == DEVICE_REG_STATE_REGISTERED) {
        return true;
    }
    
    device_registration_info_t temp_info;
    return device_registration_load_from_nvs(&temp_info) == ESP_OK;
}

// 清除注册信息
esp_err_t device_registration_clear(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    nvs_erase_all(nvs_handle);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    memset(&g_reg_info, 0, sizeof(device_registration_info_t));
    g_reg_state = DEVICE_REG_STATE_IDLE;
    
    ESP_LOGI(TAG, "Device registration info cleared");
    return ESP_OK;
}

// 从NVS加载注册信息
esp_err_t device_registration_load_from_nvs(device_registration_info_t *info)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t required_size;
    
    // 读取各个字段
    required_size = sizeof(info->device_id);
    ret = nvs_get_str(nvs_handle, NVS_KEY_DEVICE_ID, info->device_id, &required_size);
    if (ret != ESP_OK) goto cleanup;
    
    required_size = sizeof(info->device_uuid);
    ret = nvs_get_str(nvs_handle, NVS_KEY_DEVICE_UUID, info->device_uuid, &required_size);
    if (ret != ESP_OK) goto cleanup;
    
    required_size = sizeof(info->device_secret);
    ret = nvs_get_str(nvs_handle, NVS_KEY_DEVICE_SECRET, info->device_secret, &required_size);
    if (ret != ESP_OK) goto cleanup;
    
    required_size = sizeof(info->mac_address);
    ret = nvs_get_str(nvs_handle, NVS_KEY_MAC_ADDRESS, info->mac_address, &required_size);
    if (ret != ESP_OK) goto cleanup;
    
cleanup:
    nvs_close(nvs_handle);
    return ret;
}

// 保存注册信息到NVS
esp_err_t device_registration_save_to_nvs(const device_registration_info_t *info)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 保存各个字段
    ret = nvs_set_str(nvs_handle, NVS_KEY_DEVICE_ID, info->device_id);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, NVS_KEY_DEVICE_UUID, info->device_uuid);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, NVS_KEY_DEVICE_SECRET, info->device_secret);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, NVS_KEY_MAC_ADDRESS, info->mac_address);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_u8(nvs_handle, NVS_KEY_REGISTERED, 1);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_commit(nvs_handle);
    
cleanup:
    nvs_close(nvs_handle);
    return ret;
}