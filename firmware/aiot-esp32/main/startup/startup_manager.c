/**
 * @file startup_manager.c
 * @brief 启动流程管理器实现
 * 
 * 统一管理设备启动流程，包含详细的LCD UI提示
 */

#include "startup_manager.h"
#include "provisioning/provisioning_client.h"
#include "ota/ota_manager.h"
#include "wifi_config/wifi_config.h"
#include "server/server_config.h"
#include "simple_display.h"
#include "mqtt/aiot_mqtt_client.h"
#include "device/device_control.h"  // 设备控制模块
#include "device/preset_control.h"  // 预设控制模块
#include "device/pwm_control.h"     // PWM控制模块
#include "button/button_handler.h"  // 按钮处理模块
#include "app_config.h"  // 包含产品ID等配置
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"  // JSON解析
#include <string.h>

#define TAG "STARTUP_MGR"
// FIRMWARE_VERSION 已在 DEVICE_CONFIG.h 中定义，此处不再重复定义
// #define FIRMWARE_VERSION CONFIG_AIOT_FIRMWARE_VERSION

// WiFi事件
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define MAX_RETRY 5

// 全局状态
static startup_stage_t s_current_stage = STARTUP_STAGE_INIT;
static startup_status_callback_t s_status_callback = NULL;
static void *s_display = NULL;
static bool s_mqtt_connected = false;
static button_event_cb_t s_button_event_callback = NULL;
static bool s_device_not_registered = false;  // 标记设备未注册（WiFi已连接但设备未注册）

// 配置缓存
static provisioning_config_t s_config = {0};
static unified_server_config_t s_server_config = {0};

/**
 * @brief 更新启动阶段并显示到LCD
 */
static void update_stage(startup_stage_t stage, const char *message) {
    s_current_stage = stage;
    
    // 显示到LCD
    if (s_display) {
        simple_display_show_startup_step(s_display, startup_manager_get_stage_string(stage), message);
    }
    
    // 调用回调
    if (s_status_callback) {
        s_status_callback(stage, message);
    }
    
    ESP_LOGI(TAG, "🔄 [%s] %s", startup_manager_get_stage_string(stage), message);
}

/**
 * @brief WiFi事件处理
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA启动");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "重试连接WiFi，第%d次", s_retry_num);
            
            char msg[64];
            snprintf(msg, sizeof(msg), "重试 %d/%d", s_retry_num, MAX_RETRY);
            update_stage(STARTUP_STAGE_WIFI_CONNECT, msg);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi连接失败");
            update_stage(STARTUP_STAGE_WIFI_CONNECT, "Error: Timeout");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获得IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        char msg[64];
        snprintf(msg, sizeof(msg), IPSTR, IP2STR(&event->ip_info.ip));
        update_stage(STARTUP_STAGE_WIFI_CONNECT, msg);
    }
}

/**
 * @brief MQTT事件处理
 */
static void mqtt_event_callback(const mqtt_event_data_t *event_data) {
    if (!event_data) {
        return;
    }
    
    // 处理自定义事件（在switch之外，避免枚举类型错误）
    if (event_data->event == AIOT_MQTT_EVENT_MESSAGE_RECEIVED) {
        ESP_LOGI(TAG, "📨 收到MQTT消息: topic=%s", 
                 event_data->message ? event_data->message->topic : "unknown");
        
        // 处理控制命令
        if (event_data->message && 
            strlen(s_config.mqtt_topic_control) > 0 &&
            strncmp(event_data->message->topic, s_config.mqtt_topic_control, 
                    strlen(s_config.mqtt_topic_control)) == 0) {
            
            ESP_LOGI(TAG, "🎯 控制命令: %.*s", 
                     event_data->message->payload_len, 
                     (char*)event_data->message->payload);
            
            // 解析JSON命令
            char *payload = malloc(event_data->message->payload_len + 1);
            if (payload) {
                memcpy(payload, event_data->message->payload, event_data->message->payload_len);
                payload[event_data->message->payload_len] = '\0';
                
                // 解析JSON检查cmd字段
                cJSON *json = cJSON_Parse(payload);
                if (json) {
                    cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
                    const char *cmd_str = NULL;
                    if (cmd_item && cJSON_IsString(cmd_item)) {
                        cmd_str = cmd_item->valuestring;
                        ESP_LOGI(TAG, "📝 命令类型: '%s'", cmd_str);
                    }
                    
                    bool is_preset = (cmd_str && strcmp(cmd_str, "preset") == 0);
                    cJSON_Delete(json);
                    
                    if (is_preset) {
                        // 预设命令处理
                        preset_control_command_t preset_cmd;
                        esp_err_t ret = preset_control_parse_json_command(payload, &preset_cmd);
                        if (ret == ESP_OK) {
                            preset_control_result_t preset_result;
                            ret = preset_control_execute(&preset_cmd, &preset_result);
                            if (ret == ESP_OK && preset_result.success) {
                                ESP_LOGI(TAG, "✅ 预设命令执行成功");
                            } else {
                                ESP_LOGE(TAG, "❌ 预设命令执行失败: %s", 
                                        preset_result.error_msg ? preset_result.error_msg : esp_err_to_name(ret));
                            }
                            preset_control_free_command(&preset_cmd);
                        }
                    } else {
                        // 设备控制命令处理
                        device_control_command_t device_cmd;
                        esp_err_t ret = device_control_parse_json_command(payload, &device_cmd);
                        if (ret == ESP_OK) {
                            device_control_result_t device_result;
                            ret = device_control_execute(&device_cmd, &device_result);
                            if (ret == ESP_OK && device_result.success) {
                                ESP_LOGI(TAG, "✅ 设备控制命令执行成功");
                            } else {
                                ESP_LOGE(TAG, "❌ 设备控制命令执行失败: %s", 
                                        device_result.error_msg ? device_result.error_msg : esp_err_to_name(ret));
                            }
                        } else {
                            ESP_LOGE(TAG, "❌ 命令解析失败: %s", esp_err_to_name(ret));
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "❌ JSON解析失败");
                }
                
                free(payload);
            }
        }
        return;  // 处理完自定义事件后直接返回
    }
    
    // 处理标准MQTT事件
    switch (event_data->event) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ MQTT已连接");
            s_mqtt_connected = true;
            update_stage(STARTUP_STAGE_MQTT_CONNECT, "Connected OK");
            
            // 连接成功后订阅控制主题（用于接收服务器命令）
            ESP_LOGI(TAG, "📋 订阅MQTT主题:");
            
            // 订阅控制主题（接收服务器控制命令）
            if (strlen(s_config.mqtt_topic_control) > 0) {
                ESP_LOGI(TAG, "   控制主题: %s", s_config.mqtt_topic_control);
                esp_err_t ret = mqtt_client_subscribe(s_config.mqtt_topic_control, MQTT_QOS_1);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "   ✅ 订阅成功");
                } else {
                    ESP_LOGW(TAG, "   ⚠️  订阅失败: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGW(TAG, "   ⚠️  控制主题为空，跳过订阅");
            }
            
            // 数据主题和状态主题仅用于设备上报，无需订阅
            if (strlen(s_config.mqtt_topic_data) > 0) {
                ESP_LOGI(TAG, "   数据主题（上报用）: %s", s_config.mqtt_topic_data);
            }
            if (strlen(s_config.mqtt_topic_status) > 0) {
                ESP_LOGI(TAG, "   状态主题（上报用）: %s", s_config.mqtt_topic_status);
            }
            
            ESP_LOGI(TAG, "📋 MQTT主题配置完成");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "❌ MQTT断开");
            s_mqtt_connected = false;
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ MQTT错误");
            break;
            
        default:
            break;
    }
}

/**
 * @brief 初始化NVS
 */
static esp_err_t init_nvs(void) {
    update_stage(STARTUP_STAGE_NVS, "Initializing...");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "擦除NVS并重新初始化");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        update_stage(STARTUP_STAGE_NVS, "Init Success");
        vTaskDelay(pdMS_TO_TICKS(1500)); // Display for 1.5s
    } else {
        update_stage(STARTUP_STAGE_NVS, "Error: Init Failed");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display error for 2s
    }
    
    return ret;
}

/**
 * @brief 检查并连接WiFi
 */
static esp_err_t connect_wifi(void) {
    update_stage(STARTUP_STAGE_WIFI_CHECK, "Checking Config...");
    
    // 检查是否需要强制进入配网模式（长按BOOT按钮触发）
    if (wifi_config_should_start()) {
        ESP_LOGW(TAG, "检测到强制配网标志，需要进入配网模式");
        update_stage(STARTUP_STAGE_WIFI_CHECK, "Need Provisioning");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display message for 2s
        return ESP_ERR_NOT_FOUND; // 返回NOT_FOUND让主程序进入配网模式
    }
    
    // 加载WiFi配置
    wifi_config_data_t wifi_cfg;
    if (wifi_config_load(&wifi_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "未找到WiFi配置");
        update_stage(STARTUP_STAGE_WIFI_CHECK, "Error: Need Config");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display error for 2s
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "WiFi配置: SSID=%s", wifi_cfg.ssid);
    
    // 显示找到的WiFi配置（包含SSID）
    char wifi_msg[64];
    snprintf(wifi_msg, sizeof(wifi_msg), "Found: %s", wifi_cfg.ssid);
    update_stage(STARTUP_STAGE_WIFI_CHECK, wifi_msg);
    vTaskDelay(pdMS_TO_TICKS(1500)); // Display for 1.5s
    
    // 初始化WiFi（显示正在连接的SSID）
    snprintf(wifi_msg, sizeof(wifi_msg), "Connect to: %s", wifi_cfg.ssid);
    update_stage(STARTUP_STAGE_WIFI_CONNECT, wifi_msg);
    
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册WiFi事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                         ESP_EVENT_ANY_ID,
                                                         &wifi_event_handler,
                                                         NULL,
                                                         NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                         IP_EVENT_STA_GOT_IP,
                                                         &wifi_event_handler,
                                                         NULL,
                                                         NULL));
    
    // 配置并启动WiFi
    wifi_config_t wifi_config = {
        .sta = {
            // ✅ 自适应WiFi认证模式：允许所有加密方式
            // 从开放网络到WPA3都支持，ESP32会自动选择最合适的模式
            .threshold.authmode = WIFI_AUTH_OPEN,  // 允许所有认证模式（包括WPA、WPA2、WPA3）
            .pmf_cfg = {
                .capable = true,   // 支持PMF（Protected Management Frames）
                .required = false  // 但不强制要求（兼容性更好）
            },
            // scan_method用于处理隐藏SSID
            .scan_method = WIFI_ALL_CHANNEL_SCAN,  // 全信道扫描（兼容隐藏SSID）
        },
    };
    // 安全复制SSID和密码，确保null终止
    memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
    memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
    strncpy((char *)wifi_config.sta.ssid, wifi_cfg.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, wifi_cfg.password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_LOGI(TAG, "🔐 WiFi认证配置:");
    ESP_LOGI(TAG, "   认证模式: 自适应 (OPEN~WPA3)");
    ESP_LOGI(TAG, "   PMF支持: 是 (可选)");
    ESP_LOGI(TAG, "   扫描方式: 全信道扫描");
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    update_stage(STARTUP_STAGE_WIFI_CONNECT, "Connecting...");
    
    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ WiFi连接成功");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ WiFi连接失败");
        return ESP_FAIL;
    }
    
    return ESP_FAIL;
}

/**
 * @brief 获取设备配置
 */
static esp_err_t get_device_config(void) {
    update_stage(STARTUP_STAGE_GET_CONFIG, "Loading Server...");
    
    // 加载服务器配置
    if (server_config_load_from_nvs(&s_server_config) != ESP_OK) {
        ESP_LOGE(TAG, "❌ 未找到服务器配置");
        update_stage(STARTUP_STAGE_GET_CONFIG, "Error: Server Not Config");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display error for 2s
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "服务器: %s", s_server_config.base_address);
    
    // 显示服务器地址（提取域名或IP，去掉http://前缀）
    char server_msg[64];
    const char *server_display = s_server_config.base_address;
    if (strstr(server_display, "http://") == server_display) {
        server_display += 7;  // 跳过 "http://"
    } else if (strstr(server_display, "https://") == server_display) {
        server_display += 8;  // 跳过 "https://"
    }
    snprintf(server_msg, sizeof(server_msg), "Server: %.40s", server_display);
    update_stage(STARTUP_STAGE_GET_CONFIG, server_msg);
    vTaskDelay(pdMS_TO_TICKS(1500)); // Display for 1.5s
    
    // 获取设备配置
    update_stage(STARTUP_STAGE_GET_CONFIG, "Fetching Info...");
    
    esp_err_t ret = provisioning_client_get_config(
        s_server_config.base_address,
        PRODUCT_ID,  // 产品ID（必需参数）
        FIRMWARE_VERSION,
        &s_config
    );
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 设备配置获取成功");
        ESP_LOGI(TAG, "   Device ID: %s", s_config.device_id);
        ESP_LOGI(TAG, "   Device UUID: %s", s_config.device_uuid);
        
        // 显示获取到的UUID（限制长度以避免溢出）
        char uuid_msg[64];
        snprintf(uuid_msg, sizeof(uuid_msg), "UUID: %.50s", s_config.device_uuid);
        update_stage(STARTUP_STAGE_GET_CONFIG, uuid_msg);
        vTaskDelay(pdMS_TO_TICKS(1500)); // Display success for 1.5s
        s_device_not_registered = false;  // 清除标记
        return ESP_OK;
    } else if (ret == ESP_ERR_NOT_FOUND) {
        // WiFi已连接，但设备未注册（404错误）
        // 获取MAC地址用于显示
        uint8_t mac[6];
        esp_err_t mac_ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        if (mac_ret == ESP_OK) {
            ESP_LOGE(TAG, "❌ 设备未注册（WiFi已连接，但设备未在后端注册）");
            ESP_LOGE(TAG, "   请先在管理页面注册设备，MAC地址: %02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            ESP_LOGE(TAG, "❌ 设备未注册（WiFi已连接，但设备未在后端注册）");
        }
        update_stage(STARTUP_STAGE_GET_CONFIG, "Error: Not Registered");
        vTaskDelay(pdMS_TO_TICKS(3000)); // Display error for 3s
        
        // 标记为设备未注册（不是需要配网）
        s_device_not_registered = true;
        
        // 返回ESP_FAIL而不是ESP_ERR_NOT_FOUND，以便主程序区分
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "❌ 配置获取失败");
        update_stage(STARTUP_STAGE_GET_CONFIG, "Error: Config Failed");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display error for 2s
        return ret;
    }
}

/**
 * @brief OTA进度回调
 */
static void ota_progress_callback(int progress, size_t speed) {
    char msg[64];
    snprintf(msg, sizeof(msg), "%d%% (%uKB/s)", progress, (unsigned int)(speed / 1024));
    update_stage(STARTUP_STAGE_OTA_UPDATE, msg);
}

/**
 * @brief 检查并执行OTA更新
 */
static esp_err_t check_and_update_ota(void) {
    if (!s_config.has_firmware_update) {
        ESP_LOGI(TAG, "✅ 固件已是最新版本");
        update_stage(STARTUP_STAGE_CHECK_OTA, "Already Latest");
        vTaskDelay(pdMS_TO_TICKS(1500)); // Display for 1.5s
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "⚠️ 发现固件更新: %s", s_config.firmware_version);
    ESP_LOGI(TAG, "   更新内容: %s", s_config.firmware_changelog);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "新版本: %s", s_config.firmware_version);
    update_stage(STARTUP_STAGE_CHECK_OTA, msg);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 询问用户（这里简化为自动更新）
    update_stage(STARTUP_STAGE_CHECK_OTA, "Preparing Update...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 开始OTA更新
    update_stage(STARTUP_STAGE_OTA_UPDATE, "Downloading...");
    
    esp_err_t ret = ota_manager_start_upgrade(
        s_config.firmware_url,
        ota_progress_callback
    );
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ OTA更新成功");
        update_stage(STARTUP_STAGE_OTA_UPDATE, "Update Success");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        update_stage(STARTUP_STAGE_OTA_UPDATE, "Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 重启
        esp_restart();
    } else {
        ESP_LOGE(TAG, "❌ OTA更新失败");
        update_stage(STARTUP_STAGE_OTA_UPDATE, "Error: OTA Failed");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return ret;
    }
    
    return ESP_OK;
}

/**
 * @brief 连接MQTT
 */
static esp_err_t connect_mqtt(void) {
    if (!s_config.has_mqtt_config) {
        ESP_LOGW(TAG, "⚠️ 无MQTT配置");
        update_stage(STARTUP_STAGE_MQTT_CONNECT, "No MQTT Config");
        vTaskDelay(pdMS_TO_TICKS(1500)); // Display warning for 1.5s
        return ESP_OK; // 不是致命错误
    }
    
    // 显示MQTT服务器地址
    char mqtt_msg[64];
    snprintf(mqtt_msg, sizeof(mqtt_msg), "MQTT: %.40s", s_config.mqtt_broker);
    update_stage(STARTUP_STAGE_MQTT_CONNECT, mqtt_msg);
    vTaskDelay(pdMS_TO_TICKS(1500)); // Display for 1.5s
    
    // 初始化MQTT客户端
    mqtt_config_t mqtt_config = {0};
    strncpy(mqtt_config.broker_url, s_config.mqtt_broker, sizeof(mqtt_config.broker_url) - 1);
    mqtt_config.port = s_config.mqtt_port;
    strncpy(mqtt_config.client_id, s_config.device_uuid, sizeof(mqtt_config.client_id) - 1);
    strncpy(mqtt_config.username, s_config.mqtt_username, sizeof(mqtt_config.username) - 1);
    strncpy(mqtt_config.password, s_config.mqtt_password, sizeof(mqtt_config.password) - 1);
    mqtt_config.use_ssl = s_config.mqtt_use_ssl;
    mqtt_config.clean_session = true;
    mqtt_config.keepalive = 120;
    mqtt_config.reconnect_timeout = 10000;
    
    // 初始化MQTT客户端（包含事件回调）
    esp_err_t ret = mqtt_client_init(&mqtt_config, mqtt_event_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ MQTT初始化失败");
        update_stage(STARTUP_STAGE_MQTT_CONNECT, "Error: Init Failed");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display error for 2s
        return ret;
    }
    
    // 注意：主题订阅在MQTT连接成功后（mqtt_event_callback中）自动进行
    
    update_stage(STARTUP_STAGE_MQTT_CONNECT, "Connecting...");
    
    ret = mqtt_client_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ MQTT连接失败");
        update_stage(STARTUP_STAGE_MQTT_CONNECT, "Error: Connect Failed");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Display error for 2s
        return ret;
    }
    
    // 等待连接成功（最多10秒）
    for (int i = 0; i < 20; i++) {
        if (s_mqtt_connected) {
            ESP_LOGI(TAG, "✅ MQTT连接成功");
            vTaskDelay(pdMS_TO_TICKS(1000));
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGW(TAG, "⚠️ MQTT连接超时（后台继续尝试）");
    update_stage(STARTUP_STAGE_MQTT_CONNECT, "Connecting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    return ESP_OK; // 不阻塞启动流程
}

/**
 * @brief 初始化传感器
 */
static esp_err_t init_sensors(void) {
    update_stage(STARTUP_STAGE_SENSORS_INIT, "Initializing...");
    
    // TODO: 初始化DHT11, DS18B20等传感器
    vTaskDelay(pdMS_TO_TICKS(800));
    
    update_stage(STARTUP_STAGE_SENSORS_INIT, "Init Complete");
    vTaskDelay(pdMS_TO_TICKS(1500)); // Display success for 1.5s
    
    return ESP_OK;
}

esp_err_t startup_manager_run(void *display, startup_status_callback_t status_callback, button_event_cb_t button_callback) {
    s_display = display;
    s_status_callback = status_callback;
    s_button_event_callback = button_callback;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  AIOT设备启动流程");
    ESP_LOGI(TAG, "  固件版本: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "========================================");
    
    // 0. 初始化 - 显示固件版本
    char init_msg[64];
    snprintf(init_msg, sizeof(init_msg), "FW: %s", FIRMWARE_VERSION);
    update_stage(STARTUP_STAGE_INIT, init_msg);
    vTaskDelay(pdMS_TO_TICKS(1500)); // Display welcome screen for 1.5s
    
    // 显示产品ID
    char product_msg[64];
    snprintf(product_msg, sizeof(product_msg), "Product: %.40s", PRODUCT_ID);
    update_stage(STARTUP_STAGE_INIT, product_msg);
    vTaskDelay(pdMS_TO_TICKS(1500)); // Display Product ID for 1.5s
    
    // 显示MAC地址
    uint8_t mac[6];
    esp_err_t mac_ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (mac_ret == ESP_OK) {
        char mac_msg[64];
        snprintf(mac_msg, sizeof(mac_msg), "MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        update_stage(STARTUP_STAGE_INIT, mac_msg);
        vTaskDelay(pdMS_TO_TICKS(1500)); // Display MAC for 1.5s
    }
    
    // 1. 初始化OTA管理器并标记当前固件有效
    ota_manager_init();
    ota_manager_mark_valid();
    
    // 2. 初始化NVS
    ret = init_nvs();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 2.5. 在启动早期初始化按钮处理模块（NVS初始化后即可初始化，支持启动时随时长按Boot进入配网）
    if (s_button_event_callback != NULL) {
        ESP_LOGI(TAG, "📋 初始化按钮处理模块（早期初始化，支持启动时随时长按Boot进入配网）...");
        ret = button_handler_init(s_button_event_callback);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ 按钮处理模块初始化成功（可在启动过程中随时长按Boot进入配网）");
        } else {
            ESP_LOGW(TAG, "⚠️ 按钮处理模块初始化失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "ℹ️ 未提供按钮回调，跳过按钮初始化");
    }
    
    // 3. 连接WiFi
    ret = connect_wifi();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 3.5. WiFi连接成功后重新初始化按钮（WiFi初始化后需要重新配置GPIO以确保按钮中断正常工作）
    if (s_button_event_callback != NULL) {
        ESP_LOGI(TAG, "📋 WiFi初始化后重新启用按键中断...");
        ret = button_handler_reinit_after_wifi();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "⚠️ 按钮重新初始化失败: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "✅ 按键中断重新启用成功");
        }
    }
    
    // 4. 获取设备配置
    ret = get_device_config();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 5. 检查并执行OTA更新
    update_stage(STARTUP_STAGE_CHECK_OTA, "Checking Updates...");
    ret = check_and_update_ota();
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        // OTA失败不是致命错误，继续运行
        ESP_LOGW(TAG, "⚠️ OTA更新跳过");
    }
    
    // 6. 连接MQTT
    ret = connect_mqtt();
    if (ret != ESP_OK) {
        // MQTT失败不是致命错误
        ESP_LOGW(TAG, "⚠️ MQTT连接跳过");
    }
    
    // 6.5. 初始化设备控制模块和预设控制模块
    ESP_LOGI(TAG, "📋 初始化设备控制模块...");
    ret = device_control_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 设备控制模块初始化成功");
    } else {
        ESP_LOGE(TAG, "❌ 设备控制模块初始化失败: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "📋 初始化预设控制模块...");
    ret = preset_control_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 预设控制模块初始化成功");
    } else {
        ESP_LOGE(TAG, "❌ 预设控制模块初始化失败: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "📋 初始化PWM控制模块...");
    ret = pwm_control_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ PWM控制模块初始化成功");
    } else {
        ESP_LOGE(TAG, "❌ PWM控制模块初始化失败: %s", esp_err_to_name(ret));
    }
    
    // 7. 初始化传感器
    ret = init_sensors();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ 传感器初始化失败");
    }
    
    // 8. 启动完成
    update_stage(STARTUP_STAGE_COMPLETED, "Startup Complete");
    vTaskDelay(pdMS_TO_TICKS(2000)); // Display completion for 2s before switching to detailed info
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ✅ 设备启动完成");
    ESP_LOGI(TAG, "  Device ID: %s", s_config.device_id);
    ESP_LOGI(TAG, "  Device UUID: %s", s_config.device_uuid);
    ESP_LOGI(TAG, "  MQTT: %s", s_mqtt_connected ? "已连接" : "未连接");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

startup_stage_t startup_manager_get_stage(void) {
    return s_current_stage;
}

const char* startup_manager_get_stage_string(startup_stage_t stage) {
    switch (stage) {
        case STARTUP_STAGE_INIT:           return "Initializing";
        case STARTUP_STAGE_NVS:            return "NVS Init";
        case STARTUP_STAGE_WIFI_CHECK:     return "WiFi Check";
        case STARTUP_STAGE_WIFI_CONNECT:   return "WiFi Connect";
        case STARTUP_STAGE_GET_CONFIG:     return "Get Config";
        case STARTUP_STAGE_CHECK_OTA:      return "Check OTA";
        case STARTUP_STAGE_OTA_UPDATE:     return "OTA Update";
        case STARTUP_STAGE_MQTT_CONNECT:   return "MQTT Connect";
        case STARTUP_STAGE_SENSORS_INIT:   return "Sensors Init";
        case STARTUP_STAGE_COMPLETED:      return "Completed";
        case STARTUP_STAGE_ERROR:          return "Error";
        default:                           return "Unknown";
    }
}

const char* startup_manager_get_device_id(void)
{
    if (strlen(s_config.device_id) > 0) {
        return s_config.device_id;
    }
    return NULL;
}

const char* startup_manager_get_device_uuid(void)
{
    if (strlen(s_config.device_uuid) > 0) {
        return s_config.device_uuid;
    }
    return NULL;
}

bool startup_manager_is_device_not_registered(void)
{
    return s_device_not_registered;
}

