/**
 * @file wechat_ble_gatt.c
 * @brief 微信小程序蓝牙GATT服务实现
 * @version 1.0
 * @date 2024-01-20
 */

#include "wechat_ble_gatt.h"
#include "wechat_ble_cmd.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include <string.h>

static const char* TAG = "WECHAT_BLE_GATT";

// GATT服务相关变量
static bool g_gatt_initialized = false;
static esp_gatt_if_t g_gatts_if = ESP_GATT_IF_NONE;
static uint16_t g_conn_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_char_handle = 0;

// 外部状态管理函数声明
extern void wechat_ble_set_connection_state(bool connected, uint16_t conn_id);
extern void wechat_ble_trigger_event_callback(wechat_ble_event_type_t event_type);

// 服务UUID (16位)
static const uint16_t wechat_ble_service_uuid = 0x1234;
static const uint16_t wechat_ble_char_uuid = 0x2345;

// GATT UUID定义
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// 特征值属性
static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint16_t char_config_ccc = 0x0000;

// 常量定义
#define CHAR_DECLARATION_SIZE 1
#define WECHAT_BLE_MAX_DATA_LEN 512

// 广播参数
esp_ble_adv_params_t wechat_ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// 广播数据
static uint8_t adv_service_uuid128[16] = {
    0x34, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

esp_ble_adv_data_t wechat_ble_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

esp_ble_adv_data_t wechat_ble_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = 0,
};

// GATT属性表
static const esp_gatts_attr_db_t gatt_db[4] = {
    // 主服务声明
    [0] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(wechat_ble_service_uuid), (uint8_t *)&wechat_ble_service_uuid}
    },

    // 特征值声明
    [1] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}
    },

    // 特征值
    [2] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_16, (uint8_t *)&wechat_ble_char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         WECHAT_BLE_MAX_DATA_LEN, 0, NULL}
    },

    // Client Characteristic Configuration Descriptor
    [3] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&char_config_ccc}
    },
};

// 私有函数声明
static void wechat_ble_gatt_create_service(void);

esp_err_t wechat_ble_gatt_init(void)
{
    if (g_gatt_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WeChat BLE GATT service");

    // 注册GATT回调
    esp_err_t ret = esp_ble_gatts_register_callback(wechat_ble_gatt_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATT callback: %s", esp_err_to_name(ret));
        return ret;
    }

    // 注册应用
    ret = esp_ble_gatts_app_register(WECHAT_BLE_GATTS_APP_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATT app: %s", esp_err_to_name(ret));
        return ret;
    }

    g_gatt_initialized = true;
    return ESP_OK;
}

esp_err_t wechat_ble_gatt_deinit(void)
{
    if (!g_gatt_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WeChat BLE GATT service");

    if (g_gatts_if != ESP_GATT_IF_NONE) {
        esp_ble_gatts_app_unregister(g_gatts_if);
        g_gatts_if = ESP_GATT_IF_NONE;
    }

    g_gatt_initialized = false;
    return ESP_OK;
}

void wechat_ble_gatt_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATT app registered, app_id: %d", param->reg.app_id);
            g_gatts_if = gatts_if;
            wechat_ble_gatt_create_service();
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(TAG, "Service created, service_handle: %d", param->create.service_handle);
            g_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(g_service_handle);
            break;

        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Service started");
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Client connected, conn_id: %d", param->connect.conn_id);
            g_conn_id = param->connect.conn_id;
            wechat_ble_set_connection_state(true, g_conn_id);
            wechat_ble_trigger_event_callback(WECHAT_BLE_EVENT_CONNECTED);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Client disconnected, conn_id: %d", param->disconnect.conn_id);
            wechat_ble_set_connection_state(false, 0);
            wechat_ble_trigger_event_callback(WECHAT_BLE_EVENT_DISCONNECTED);
            g_conn_id = 0;
            // 重新开始广播
            esp_err_t ret = esp_ble_gap_start_advertising(&wechat_ble_adv_params);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "restart advertising failed, error code = %x", ret);
            }
            break;

        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(TAG, "Data received, len: %d", param->write.len);
            ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);
            
            // 处理接收到的命令
            wechat_ble_cmd_process(param->write.value, param->write.len);
            
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;

        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "Read request");
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 0;
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            break;

        default:
            ESP_LOGD(TAG, "Unhandled GATT event: %d", event);
            break;
    }
}

esp_err_t wechat_ble_gatt_send_response(wechat_ble_cmd_t cmd, const uint8_t *data, uint16_t len)
{
    if (!g_gatt_initialized || g_gatts_if == ESP_GATT_IF_NONE || g_conn_id == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (len > WECHAT_BLE_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id, g_char_handle, len, (uint8_t*)data, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send indication: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Response sent successfully, len: %d", len);
    return ESP_OK;
}

esp_err_t wechat_ble_gatt_disconnect_all(void)
{
    if (!g_gatt_initialized || g_gatts_if == ESP_GATT_IF_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_conn_id != 0) {
        esp_err_t ret = esp_ble_gatts_close(g_gatts_if, g_conn_id);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    return ESP_OK;
}

// 私有函数实现
static void wechat_ble_gatt_create_service(void)
{
    esp_ble_gatts_create_attr_tab(gatt_db, g_gatts_if, sizeof(gatt_db) / sizeof(gatt_db[0]), 0);
}