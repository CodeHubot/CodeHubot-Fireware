/**
 * @file module_init.c
 * @brief 模块初始化管理
 * 
 * 按照FIRMWARE_MANUAL.md要求的顺序实现完整的初始化流程
 */

#include "module_init.h"
#include "server/server_config.h"
#include "wifi_config/wifi_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "MODULE_INIT";

// 外部声明的WiFi事件处理器（在main.c中定义）
extern void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * @brief 初始化设备ID和MQTT主题（临时值）
 * 
 * 仅生成临时device_id（MAC基础），不构建任何主题（待获取UUID后再确定）
 */
esp_err_t init_device_id_and_topics(char *device_id)
{
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    // 获取MAC地址
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret != ESP_OK) {
        // 如果WiFi未初始化，使用esp_read_mac
        ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
        return ret;
    }
    }

    // 生成临时设备ID（MAC基础）
    snprintf(device_id, 64, "AIOT_%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Temporary Device ID (MAC-based): %s", device_id);
    ESP_LOGI(TAG, "Will attempt to fetch UUID from backend after WiFi connection");
#else
    // 非ESP平台使用默认ID
    strncpy(device_id, "AIOT_DEFAULT", 64);
#endif

    return ESP_OK;
}

/**
 * @brief 通过MAC地址从后端获取设备UUID
 * 
 * 使用动态构建的URL调用 /api/devices/mac/lookup 接口
 * 硬性约束：如果无法获取UUID，系统不得继续执行
 */
esp_err_t fetch_uuid_by_mac(
    const void *srv_config,  // unified_server_config_t*
                                                   const char *firmware_version,
    const char *hardware_version,
    device_uuid_info_t *uuid_info,
    int max_retries)
{
    if (!srv_config || !uuid_info) {
        return ESP_ERR_INVALID_ARG;
    }

    const unified_server_config_t *config = (const unified_server_config_t *)srv_config;

#ifdef ESP_PLATFORM
    // 获取MAC地址
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret != ESP_OK) {
        ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
                return ret;
        }
    }

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 动态构建URL
    char url[256];
    ret = server_config_build_http_url(config, "/api/devices/mac/lookup", url, sizeof(url));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build URL: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Fetching UUID from backend: %s", url);
    ESP_LOGI(TAG, "MAC Address: %s", mac_str);
    
    // 构建请求体
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "mac_address", mac_str);
    if (firmware_version) {
        cJSON_AddStringToObject(json, "firmware_version", firmware_version);
    }
    if (hardware_version) {
        cJSON_AddStringToObject(json, "hardware_version", hardware_version);
    }
    cJSON_AddStringToObject(json, "device_type", "ESP32-S3");
    
    char *json_string = cJSON_Print(json);
    if (!json_string) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    
    // HTTP事件处理
    static char response_buffer[2048];
    static size_t response_len = 0;
    response_len = 0;

    esp_err_t http_event_handler(esp_http_client_event_t *evt) {
        switch (evt->event_id) {
            case HTTP_EVENT_ON_DATA:
                if (!esp_http_client_is_chunked_response(evt->client)) {
                    int copy_len = evt->data_len;
                    if (response_len + copy_len >= sizeof(response_buffer)) {
                        copy_len = sizeof(response_buffer) - response_len - 1;
                    }
                    memcpy(response_buffer + response_len, evt->data, copy_len);
                    response_len += copy_len;
                    response_buffer[response_len] = '\0';
                }
                break;
            default:
                break;
        }
        return ESP_OK;
    }

    // 重试循环
    int retry_count = 0;
    while (retry_count <= max_retries) {
        if (retry_count > 0) {
            ESP_LOGW(TAG, "Retrying UUID fetch (attempt %d/%d)...", retry_count, max_retries);
            vTaskDelay(pdMS_TO_TICKS(2000)); // 等待2秒后重试
        }
    
    // 配置HTTP客户端
        esp_http_client_config_t http_config = {
            .url = url,
            .event_handler = http_event_handler,
            .timeout_ms = 10000,
        .method = HTTP_METHOD_POST,
    };
    
        esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
            free(json_string);
            cJSON_Delete(json);
            return ESP_ERR_NO_MEM;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
        // 执行请求
        ret = esp_http_client_perform(client);
        if (ret == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200) {
                // 解析响应
                cJSON *response_json = cJSON_Parse(response_buffer);
                if (response_json) {
                    cJSON *device_id = cJSON_GetObjectItem(response_json, "device_id");
                    cJSON *device_uuid = cJSON_GetObjectItem(response_json, "device_uuid");
                    cJSON *device_secret = cJSON_GetObjectItem(response_json, "device_secret");
                    cJSON *mac_address = cJSON_GetObjectItem(response_json, "mac_address");

                    if (cJSON_IsString(device_id) && device_id->valuestring) {
                        strncpy(uuid_info->device_id, device_id->valuestring, sizeof(uuid_info->device_id) - 1);
                        uuid_info->device_id[sizeof(uuid_info->device_id) - 1] = '\0';
                    }
                    if (cJSON_IsString(device_uuid) && device_uuid->valuestring) {
                        strncpy(uuid_info->device_uuid, device_uuid->valuestring, sizeof(uuid_info->device_uuid) - 1);
                        uuid_info->device_uuid[sizeof(uuid_info->device_uuid) - 1] = '\0';
                    }
                    if (cJSON_IsString(device_secret) && device_secret->valuestring) {
                        strncpy(uuid_info->device_secret, device_secret->valuestring, sizeof(uuid_info->device_secret) - 1);
                        uuid_info->device_secret[sizeof(uuid_info->device_secret) - 1] = '\0';
                    }
                    if (cJSON_IsString(mac_address) && mac_address->valuestring) {
                        strncpy(uuid_info->mac_address, mac_address->valuestring, sizeof(uuid_info->mac_address) - 1);
                        uuid_info->mac_address[sizeof(uuid_info->mac_address) - 1] = '\0';
                    }

                    cJSON_Delete(response_json);

                    ESP_LOGI(TAG, "✅ UUID fetch successful");
                    ESP_LOGI(TAG, "   Device ID: %s", uuid_info->device_id);
                    ESP_LOGI(TAG, "   Device UUID: %s", uuid_info->device_uuid);
                    ESP_LOGI(TAG, "   MAC Address: %s", uuid_info->mac_address);

                    esp_http_client_cleanup(client);
                    free(json_string);
                    cJSON_Delete(json);
                    return ESP_OK;
                } else {
                    ESP_LOGE(TAG, "Failed to parse JSON response: %s", response_buffer);
                }
            } else if (status_code == 404) {
                ESP_LOGE(TAG, "Device not registered (404). Please register device in backend first.");
                esp_http_client_cleanup(client);
                free(json_string);
                cJSON_Delete(json);
                return ESP_ERR_NOT_FOUND;
            } else {
                ESP_LOGE(TAG, "HTTP request failed with status: %d", status_code);
                ESP_LOGE(TAG, "Response: %s", response_buffer);
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
        }

        esp_http_client_cleanup(client);
        retry_count++;
    }

    // 所有重试都失败了
    ESP_LOGE(TAG, "❌ Failed to fetch UUID after %d retries", max_retries);
        free(json_string);
    cJSON_Delete(json);
    return ESP_FAIL;
#else
    // 非ESP平台
    ESP_LOGW(TAG, "UUID fetch skipped (non-ESP platform)");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

/**
 * @brief 初始化网络服务
 * 
 * 调用fetch_uuid_by_mac获取设备UUID
 * 如果失败，系统进入停机状态
 */
esp_err_t init_network_services(
    const void *srv_config,  // unified_server_config_t*
    const char *firmware_version,
    const char *hardware_version,
    device_uuid_info_t *uuid_info)
{
    if (!srv_config || !uuid_info) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing network services...");
    ESP_LOGI(TAG, "Fetching UUID from backend (MAC lookup)...");

    // 调用fetch_uuid_by_mac，最多重试3次
    esp_err_t ret = fetch_uuid_by_mac(srv_config, firmware_version, hardware_version, uuid_info, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SYSTEM HALTED: UUID fetch failed, cannot proceed");
        ESP_LOGE(TAG, "   Error: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "   System will not continue initialization");
        
        // 进入永久错误状态
        while (1) {
            ESP_LOGE(TAG, "SYSTEM HALTED: UUID fetch failed, cannot proceed");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    ESP_LOGI(TAG, "✅ Network services initialized successfully");
    return ESP_OK;
}

/**
 * @brief 初始化WiFi和网络
 * 
 * 从NVS加载WiFi配置，若缺失则进入配网模式
 */
esp_err_t init_wifi_and_network(void)
{
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Initializing WiFi and network...");
    
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 检查是否需要进入配网模式
    if (wifi_config_should_start()) {
        ESP_LOGI(TAG, "Provisioning mode detected, entering provisioning mode");
        
        wifi_config_clear_force_flag();
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        wifi_config_init(NULL);  // 使用默认事件处理器
        
        esp_err_t ret = wifi_config_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start provisioning mode: %s", esp_err_to_name(ret));
            return ret;
        }
        
        ESP_LOGI(TAG, "Provisioning mode started successfully, waiting for user configuration");
        return ESP_OK;
    }

    // 尝试加载已保存的WiFi配置
    ESP_LOGI(TAG, "[WiFi DEBUG] 尝试加载已保存的WiFi配置...");
    wifi_config_data_t wifi_config;
    esp_err_t ret = wifi_config_load(&wifi_config);
    if (ret != ESP_OK || !wifi_config.configured || strlen(wifi_config.ssid) == 0) {
        ESP_LOGW(TAG, "[WiFi DEBUG] ⚠️ 未找到有效的WiFi配置，进入配网模式");
        ESP_LOGW(TAG, "[WiFi DEBUG]    wifi_config_load返回: %s (错误码: %d)", esp_err_to_name(ret), ret);
        ESP_LOGW(TAG, "[WiFi DEBUG]    configured标志: %s", wifi_config.configured ? "true" : "false");
        ESP_LOGW(TAG, "[WiFi DEBUG]    SSID长度: %zu", strlen(wifi_config.ssid));
        
        // 进入配网模式
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        wifi_config_init(NULL);
        
        ret = wifi_config_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[WiFi DEBUG] ❌ 启动配网模式失败: %s", esp_err_to_name(ret));
            return ret;
        }
        
        ESP_LOGI(TAG, "[WiFi DEBUG] ✅ 配网模式启动成功，等待用户配置");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[WiFi DEBUG] ✅ 找到有效的WiFi配置，使用已保存的配置");
    ESP_LOGI(TAG, "[WiFi DEBUG] 📋 使用的WiFi配置:");
    ESP_LOGI(TAG, "[WiFi DEBUG]    SSID: '%s'", wifi_config.ssid);
    ESP_LOGI(TAG, "[WiFi DEBUG]    密码: %s", strlen(wifi_config.password) > 0 ? "*** (已设置)" : "(空)");
    ESP_LOGI(TAG, "[WiFi DEBUG]    配置标志: %s", wifi_config.configured ? "是" : "否");

    // 创建默认WiFi STA
    esp_netif_create_default_wifi_sta();

    // 注册WiFi事件处理器（必须在初始化WiFi之前注册）
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 配置WiFi连接参数（自适应认证模式）
    wifi_config_t esp_wifi_config = {
        .sta = {
            // ✅ 自适应WiFi认证模式：允许所有加密方式
            .threshold.authmode = WIFI_AUTH_OPEN,  // 允许从开放网络到WPA3的所有模式
            .pmf_cfg = {
                .capable = true,   // 支持PMF（Protected Management Frames）
                .required = false  // 但不强制要求（更好的兼容性）
            },
            .scan_method = WIFI_ALL_CHANNEL_SCAN,  // 全信道扫描（支持隐藏SSID）
        },
    };

    // 安全复制SSID和密码，确保null终止
    memset(esp_wifi_config.sta.ssid, 0, sizeof(esp_wifi_config.sta.ssid));
    memset(esp_wifi_config.sta.password, 0, sizeof(esp_wifi_config.sta.password));
    strncpy((char*)esp_wifi_config.sta.ssid, wifi_config.ssid, sizeof(esp_wifi_config.sta.ssid) - 1);
    strncpy((char*)esp_wifi_config.sta.password, wifi_config.password, sizeof(esp_wifi_config.sta.password) - 1);
    
    ESP_LOGI(TAG, "[WiFi DEBUG] 配置WiFi连接参数:");
    ESP_LOGI(TAG, "[WiFi DEBUG]    SSID: '%s'", esp_wifi_config.sta.ssid);
    ESP_LOGI(TAG, "[WiFi DEBUG]    密码长度: %zu", strlen((char*)esp_wifi_config.sta.password));
    ESP_LOGI(TAG, "[WiFi DEBUG]    认证模式: 自适应 (OPEN~WPA3)");
    ESP_LOGI(TAG, "[WiFi DEBUG]    PMF: capable=true, required=false");
    ESP_LOGI(TAG, "[WiFi DEBUG]    扫描方式: 全信道扫描");

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &esp_wifi_config));
    ESP_LOGI(TAG, "[WiFi DEBUG] ✅ WiFi配置已设置到ESP-IDF");
    
    // 注意：ESP-IDF的WiFi模块在断开连接时会自动触发WIFI_EVENT_STA_DISCONNECTED事件
    // 我们在事件处理器中调用esp_wifi_connect()即可实现自动重连
    // 不需要额外的自动重连配置函数（某些ESP-IDF版本可能不支持）
    ESP_LOGI(TAG, "[WiFi DEBUG] ✅ WiFi自动重连机制：通过事件处理器实现（断开时自动调用esp_wifi_connect()）");
    
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "[WiFi DEBUG] ✅ WiFi驱动已启动");

    ESP_LOGI(TAG, "[WiFi DEBUG] 📡 正在连接到WiFi: '%s'", wifi_config.ssid);
    ESP_LOGI(TAG, "[WiFi DEBUG] 等待WiFi事件处理器触发连接...");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "WiFi initialization skipped (non-ESP platform)");
    return ESP_OK;
#endif
}

/**
 * @brief 处理配网模式
 * 
 * 进入配网模式，等待用户配置WiFi和服务器地址
 */
esp_err_t handle_config_mode(void)
{
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Handling configuration mode...");

    // 检查是否已经在配网模式（检查实际状态，而不是只检查NVS标志）
    wifi_config_state_t current_state = wifi_config_get_state();
    if (current_state != WIFI_CONFIG_STATE_IDLE && current_state != WIFI_CONFIG_STATE_FAILED) {
        ESP_LOGI(TAG, "Already in provisioning mode (state: %d)", current_state);
        return ESP_OK;
    }

    // 启动配网模式
    // 注意：不在这里初始化WiFi，让wifi_config_start()来处理
    // 这样可以确保WiFi初始化逻辑统一管理
    wifi_config_init(NULL);
    
    esp_err_t ret = wifi_config_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Provisioning mode started successfully");
        return ESP_OK;
#else
    ESP_LOGI(TAG, "Configuration mode skipped (non-ESP platform)");
        return ESP_OK;
#endif
}

