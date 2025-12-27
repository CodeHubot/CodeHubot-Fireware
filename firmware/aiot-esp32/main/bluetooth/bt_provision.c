/**
 * @file bt_provision.c
 * @brief 蓝牙配网功能实现文件
 * 
 * 实现通过蓝牙BLE配置WiFi和服务器信息的功能
 * 
 * @author AIOT Team
 * @date 2024-01-01
 */

#include "bt_provision.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#endif

// ==================== 私有常量 ====================

static const char* TAG = "BT_PROVISION";

// GATT服务和特征句柄
#define GATTS_SERVICE_UUID_TEST_A     0x00FF
#define GATTS_CHAR_UUID_TEST_A        0xFF01
#define GATTS_CHAR_UUID_TEST_B        0xFF02
#define GATTS_CHAR_UUID_TEST_C        0xFF03

#define GATTS_NUM_HANDLE_TEST_A       4

// ==================== 私有变量 ====================

static bool g_bt_provision_initialized = false;
static bt_provision_config_t g_provision_config = {0};
static bt_provision_state_t g_provision_state = BT_PROVISION_STATE_IDLE;
bt_provision_wifi_config_t g_wifi_config = {0};
bt_provision_server_config_t g_server_config = {0};
static char g_status_message[BT_PROVISION_MESSAGE_MAX] = {0};
static uint8_t g_provision_progress = 0;

#ifdef ESP_PLATFORM
uint16_t g_gatts_if = ESP_GATT_IF_NONE;
uint16_t g_conn_id = 0;
uint16_t g_service_handle = 0;
uint16_t g_char_handle_write = 0;
uint16_t g_char_handle_read = 0;
uint16_t g_char_handle_notify = 0;
static esp_timer_handle_t g_provision_timer = NULL;
EventGroupHandle_t g_wifi_event_group = NULL;
static esp_event_handler_instance_t g_wifi_handler_instance = NULL;
static esp_event_handler_instance_t g_ip_handler_instance = NULL;

// WiFi事件位
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#endif

// ==================== 私有函数声明 ====================

static bt_provision_err_t bt_provision_nvs_init(void);
static bt_provision_err_t bt_provision_load_config(void);
static bt_provision_err_t bt_provision_save_wifi_config(const bt_provision_wifi_config_t* config);
static bt_provision_err_t bt_provision_save_server_config(const bt_provision_server_config_t* config);
void bt_provision_set_state(bt_provision_state_t state, const char* message);
static void bt_provision_notify_event(bt_provision_state_t state, bt_provision_err_t error, const char* message);
// 这些函数在bt_provision_cmd.c中定义
static void bt_provision_timeout_callback(void* arg);

#ifdef ESP_PLATFORM
void bt_provision_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
void bt_provision_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
void bt_provision_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
esp_err_t bt_provision_start_ble_advertising(void);
esp_err_t bt_provision_stop_ble_advertising(void);
#endif

// ==================== 公共函数实现 ====================

bt_provision_err_t bt_provision_init(const bt_provision_config_t* config)
{
    if (g_bt_provision_initialized) {
        return BT_PROVISION_ERR_OK;
    }
    
    if (!config) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 复制配置
    memcpy(&g_provision_config, config, sizeof(bt_provision_config_t));
    
    // 初始化NVS
    bt_provision_err_t ret = bt_provision_nvs_init();
    if (ret != BT_PROVISION_ERR_OK) {
        return ret;
    }
    
    // 加载已保存的配置
    ret = bt_provision_load_config();
    if (ret != BT_PROVISION_ERR_OK) {
        ESP_LOGW(TAG, "Failed to load config, using defaults");
    }
    
#ifdef ESP_PLATFORM
    // 初始化WiFi事件组
    g_wifi_event_group = xEventGroupCreate();
    if (!g_wifi_event_group) {
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    // 注册WiFi事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &bt_provision_wifi_event_handler,
                                                        NULL,
                                                        &g_wifi_handler_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &bt_provision_wifi_event_handler,
                                                        NULL,
                                                        &g_ip_handler_instance));
    
    // 初始化蓝牙
    esp_err_t esp_ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(esp_ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_ret = esp_bt_controller_init(&bt_cfg);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize controller failed: %s", esp_err_to_name(esp_ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    esp_ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable controller failed: %s", esp_err_to_name(esp_ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    esp_ret = esp_bluedroid_init();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Init bluetooth failed: %s", esp_err_to_name(esp_ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    esp_ret = esp_bluedroid_enable();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable bluetooth failed: %s", esp_err_to_name(esp_ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    // 注册GATT回调
    esp_ret = esp_ble_gatts_register_callback(bt_provision_gatts_event_handler);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "gatts register error, error code = %x", esp_ret);
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    esp_ret = esp_ble_gap_register_callback(bt_provision_gap_event_handler);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "gap register error, error code = %x", esp_ret);
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    // 创建定时器
    const esp_timer_create_args_t timer_args = {
        .callback = &bt_provision_timeout_callback,
        .arg = NULL,
        .name = "bt_provision_timer"
    };
    esp_ret = esp_timer_create(&timer_args, &g_provision_timer);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(esp_ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
#endif
    
    g_bt_provision_initialized = true;
    bt_provision_set_state(BT_PROVISION_STATE_IDLE, "Bluetooth provisioning initialized");
    
    ESP_LOGI(TAG, "Bluetooth provisioning initialized successfully");
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_deinit(void)
{
    if (!g_bt_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    // 停止配网
    bt_provision_stop();
    
#ifdef ESP_PLATFORM
    // 清理定时器
    if (g_provision_timer) {
        esp_timer_delete(g_provision_timer);
        g_provision_timer = NULL;
    }
    
    // 清理WiFi事件组
    if (g_wifi_event_group) {
        vEventGroupDelete(g_wifi_event_group);
        g_wifi_event_group = NULL;
    }
    
    // 注销事件处理器
    if (g_wifi_handler_instance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, g_wifi_handler_instance);
        g_wifi_handler_instance = NULL;
    }
    if (g_ip_handler_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, g_ip_handler_instance);
        g_ip_handler_instance = NULL;
    }
    
    // 清理蓝牙
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
#endif
    
    g_bt_provision_initialized = false;
    ESP_LOGI(TAG, "Bluetooth provisioning deinitialized");
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_start(void)
{
    if (!g_bt_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    if (g_provision_state != BT_PROVISION_STATE_IDLE) {
        return BT_PROVISION_ERR_OK; // 已经在运行
    }
    
#ifdef ESP_PLATFORM
    // 开始BLE广播
    esp_err_t ret = bt_provision_start_ble_advertising();
    if (ret != ESP_OK) {
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    // 启动超时定时器
    esp_timer_start_once(g_provision_timer, g_provision_config.advertising_timeout_ms * 1000);
#endif
    
    bt_provision_set_state(BT_PROVISION_STATE_ADVERTISING, "Started BLE advertising");
    ESP_LOGI(TAG, "Bluetooth provisioning started");
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_stop(void)
{
    if (!g_bt_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
#ifdef ESP_PLATFORM
    // 停止定时器
    if (g_provision_timer) {
        esp_timer_stop(g_provision_timer);
    }
    
    // 停止BLE广播
    bt_provision_stop_ble_advertising();
#endif
    
    bt_provision_set_state(BT_PROVISION_STATE_IDLE, "Bluetooth provisioning stopped");
    ESP_LOGI(TAG, "Bluetooth provisioning stopped");
    return BT_PROVISION_ERR_OK;
}

bt_provision_state_t bt_provision_get_state(void)
{
    return g_provision_state;
}

bt_provision_err_t bt_provision_get_status(bt_provision_status_t* status)
{
    if (!status) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    status->state = g_provision_state;
    status->progress = g_provision_progress;
    strncpy(status->message, g_status_message, BT_PROVISION_MESSAGE_MAX - 1);
    status->message[BT_PROVISION_MESSAGE_MAX - 1] = '\0';
    
    // 获取WiFi状态
    if (g_wifi_config.configured) {
        strcpy(status->wifi_status, "configured");
    } else {
        strcpy(status->wifi_status, "not_configured");
    }
    
    // 获取服务器状态
    if (g_server_config.configured) {
        strcpy(status->server_status, "configured");
    } else {
        strcpy(status->server_status, "not_configured");
    }
    
    // 获取WiFi IP地址
    strcpy(status->wifi_ip, "0.0.0.0");
#ifdef ESP_PLATFORM
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            snprintf(status->wifi_ip, sizeof(status->wifi_ip), IPSTR, IP2STR(&ip_info.ip));
        }
    }
#endif
    
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_get_device_info(bt_provision_device_info_t* info)
{
    if (!info) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 设备名称
    strncpy(info->device_name, g_provision_config.device_name, BT_PROVISION_DEVICE_NAME_MAX - 1);
    info->device_name[BT_PROVISION_DEVICE_NAME_MAX - 1] = '\0';
    
    // MAC地址
    strcpy(info->mac_address, "00:00:00:00:00:00");
#ifdef ESP_PLATFORM
    uint8_t mac[6];
    if (esp_efuse_mac_get_default(mac) == ESP_OK) {
        snprintf(info->mac_address, sizeof(info->mac_address), 
                "%02X:%02X:%02X:%02X:%02X:%02X", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
#endif
    
    // 固件版本和芯片型号
    strcpy(info->firmware_version, "1.0.0");
    strcpy(info->chip_model, "ESP32-S3");
    
    // WiFi状态
    if (g_wifi_config.configured) {
        strcpy(info->wifi_status, "configured");
    } else {
        strcpy(info->wifi_status, "not_configured");
    }
    
    // 配网状态
    strcpy(info->provision_status, bt_provision_get_state_string(g_provision_state));
    
    return BT_PROVISION_ERR_OK;
}

bool bt_provision_is_wifi_configured(void)
{
    return g_wifi_config.configured;
}

bool bt_provision_is_server_configured(void)
{
    return g_server_config.configured;
}

bt_provision_err_t bt_provision_get_wifi_config(bt_provision_wifi_config_t* config)
{
    if (!config) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    memcpy(config, &g_wifi_config, sizeof(bt_provision_wifi_config_t));
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_get_server_config(bt_provision_server_config_t* config)
{
    if (!config) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    memcpy(config, &g_server_config, sizeof(bt_provision_server_config_t));
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_set_wifi_config(const bt_provision_wifi_config_t* config)
{
    if (!config) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    memcpy(&g_wifi_config, config, sizeof(bt_provision_wifi_config_t));
    g_wifi_config.configured = true;
    
    return bt_provision_save_wifi_config(&g_wifi_config);
}

bt_provision_err_t bt_provision_set_server_config(const bt_provision_server_config_t* config)
{
    if (!config) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    memcpy(&g_server_config, config, sizeof(bt_provision_server_config_t));
    g_server_config.configured = true;
    
    return bt_provision_save_server_config(&g_server_config);
}

bt_provision_err_t bt_provision_reset_config(bool reset_wifi, bool reset_server)
{
    bt_provision_err_t ret = BT_PROVISION_ERR_OK;
    
#ifdef ESP_PLATFORM
    nvs_handle_t nvs_handle;
    
    if (reset_wifi) {
        esp_err_t esp_ret = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &nvs_handle);
        if (esp_ret == ESP_OK) {
            nvs_erase_all(nvs_handle);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            memset(&g_wifi_config, 0, sizeof(bt_provision_wifi_config_t));
            ESP_LOGI(TAG, "WiFi configuration reset");
        } else {
            ret = BT_PROVISION_ERR_STORAGE_FAILED;
        }
    }
    
    if (reset_server) {
        esp_err_t esp_ret = nvs_open(NVS_NAMESPACE_SERVER, NVS_READWRITE, &nvs_handle);
        if (esp_ret == ESP_OK) {
            nvs_erase_all(nvs_handle);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            memset(&g_server_config, 0, sizeof(bt_provision_server_config_t));
            ESP_LOGI(TAG, "Server configuration reset");
        } else {
            ret = BT_PROVISION_ERR_STORAGE_FAILED;
        }
    }
#endif
    
    return ret;
}

const char* bt_provision_get_error_string(bt_provision_err_t error)
{
    switch (error) {
        case BT_PROVISION_ERR_OK:                   return "Success";
        case BT_PROVISION_ERR_INVALID_PARAM:        return "Invalid parameter";
        case BT_PROVISION_ERR_WIFI_CONNECT_FAILED:  return "WiFi connection failed";
        case BT_PROVISION_ERR_SERVER_CONNECT_FAILED: return "Server connection failed";
        case BT_PROVISION_ERR_TIMEOUT:              return "Timeout";
        case BT_PROVISION_ERR_STORAGE_FAILED:       return "Storage operation failed";
        case BT_PROVISION_ERR_BLE_FAILED:           return "Bluetooth operation failed";
        case BT_PROVISION_ERR_ALREADY_CONFIGURED:   return "Already configured";
        case BT_PROVISION_ERR_NOT_INITIALIZED:      return "Not initialized";
        case BT_PROVISION_ERR_JSON_PARSE_FAILED:    return "JSON parse failed";
        default:                                    return "Unknown error";
    }
}

const char* bt_provision_get_state_string(bt_provision_state_t state)
{
    switch (state) {
        case BT_PROVISION_STATE_IDLE:           return "idle";
        case BT_PROVISION_STATE_ADVERTISING:    return "advertising";
        case BT_PROVISION_STATE_CONNECTED:      return "connected";
        case BT_PROVISION_STATE_CONFIGURING:    return "configuring";
        case BT_PROVISION_STATE_WIFI_CONNECTING: return "wifi_connecting";
        case BT_PROVISION_STATE_SERVER_TESTING: return "server_testing";
        case BT_PROVISION_STATE_SUCCESS:        return "success";
        case BT_PROVISION_STATE_FAILED:         return "failed";
        case BT_PROVISION_STATE_TIMEOUT:        return "timeout";
        default:                                return "unknown";
    }
}

// ==================== 私有函数实现 ====================

static bt_provision_err_t bt_provision_nvs_init(void)
{
#ifdef ESP_PLATFORM
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
#endif
    return BT_PROVISION_ERR_OK;
}

static bt_provision_err_t bt_provision_load_config(void)
{
#ifdef ESP_PLATFORM
    nvs_handle_t nvs_handle;
    size_t required_size;
    
    // 加载WiFi配置
    esp_err_t ret = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        required_size = sizeof(bt_provision_wifi_config_t);
        ret = nvs_get_blob(nvs_handle, "config", &g_wifi_config, &required_size);
        nvs_close(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WiFi configuration loaded");
        }
    }
    
    // 加载服务器配置
    ret = nvs_open(NVS_NAMESPACE_SERVER, NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        required_size = sizeof(bt_provision_server_config_t);
        ret = nvs_get_blob(nvs_handle, "config", &g_server_config, &required_size);
        nvs_close(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Server configuration loaded");
        }
    }
#endif
    
    return BT_PROVISION_ERR_OK;
}

static bt_provision_err_t bt_provision_save_wifi_config(const bt_provision_wifi_config_t* config)
{
#ifdef ESP_PLATFORM
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return BT_PROVISION_ERR_STORAGE_FAILED;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", config, sizeof(bt_provision_wifi_config_t));
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return BT_PROVISION_ERR_STORAGE_FAILED;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        return BT_PROVISION_ERR_STORAGE_FAILED;
    }
    
    ESP_LOGI(TAG, "WiFi configuration saved");
#endif
    return BT_PROVISION_ERR_OK;
}

static bt_provision_err_t bt_provision_save_server_config(const bt_provision_server_config_t* config)
{
#ifdef ESP_PLATFORM
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SERVER, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return BT_PROVISION_ERR_STORAGE_FAILED;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", config, sizeof(bt_provision_server_config_t));
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return BT_PROVISION_ERR_STORAGE_FAILED;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        return BT_PROVISION_ERR_STORAGE_FAILED;
    }
    
    ESP_LOGI(TAG, "Server configuration saved");
#endif
    return BT_PROVISION_ERR_OK;
}

void bt_provision_set_state(bt_provision_state_t state, const char* message)
{
    g_provision_state = state;
    if (message) {
        strncpy(g_status_message, message, BT_PROVISION_MESSAGE_MAX - 1);
        g_status_message[BT_PROVISION_MESSAGE_MAX - 1] = '\0';
    }
    
    // 更新进度
    switch (state) {
        case BT_PROVISION_STATE_IDLE:           g_provision_progress = 0; break;
        case BT_PROVISION_STATE_ADVERTISING:    g_provision_progress = 10; break;
        case BT_PROVISION_STATE_CONNECTED:      g_provision_progress = 20; break;
        case BT_PROVISION_STATE_CONFIGURING:    g_provision_progress = 40; break;
        case BT_PROVISION_STATE_WIFI_CONNECTING: g_provision_progress = 60; break;
        case BT_PROVISION_STATE_SERVER_TESTING: g_provision_progress = 80; break;
        case BT_PROVISION_STATE_SUCCESS:        g_provision_progress = 100; break;
        case BT_PROVISION_STATE_FAILED:         g_provision_progress = 0; break;
        case BT_PROVISION_STATE_TIMEOUT:        g_provision_progress = 0; break;
    }
    
    ESP_LOGI(TAG, "State changed to %s: %s", bt_provision_get_state_string(state), message ? message : "");
}

static void bt_provision_notify_event(bt_provision_state_t state, bt_provision_err_t error, const char* message)
{
    if (g_provision_config.event_callback) {
        g_provision_config.event_callback(state, error, message);
    }
}

static void bt_provision_timeout_callback(void* arg)
{
    ESP_LOGW(TAG, "Provisioning timeout");
    bt_provision_set_state(BT_PROVISION_STATE_TIMEOUT, "Provisioning timeout");
    bt_provision_notify_event(BT_PROVISION_STATE_TIMEOUT, BT_PROVISION_ERR_TIMEOUT, "Provisioning timeout");
    bt_provision_stop();
}

// 注意：由于代码长度限制，这里只实现了核心框架
// 完整的BLE GATT服务器实现、WiFi连接测试、服务器连接测试等功能
// 需要在后续的文件中继续实现