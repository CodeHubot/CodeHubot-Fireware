/**
 * @file bt_provision_ble.c
 * @brief 蓝牙配网功能的BLE GATT服务器实现
 * 
 * 实现BLE GATT服务器，处理配网命令和数据传输
 * 
 * @author AIOT Team
 * @date 2024-01-01
 */

#include "bt_provision.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"

// ==================== 私有常量 ====================

static const char* TAG = "BT_PROVISION_BLE";

// GATT服务和特征定义
static const uint16_t GATTS_SERVICE_UUID_PROVISION = 0x1800;
static const uint16_t GATTS_CHAR_UUID_WRITE = 0x2A00;
static const uint16_t GATTS_CHAR_UUID_READ = 0x2A01;
static const uint16_t GATTS_CHAR_UUID_NOTIFY = 0x2A02;

#define GATTS_NUM_HANDLE_PROVISION 8
#define GATTS_CHAR_VAL_LEN_MAX 512

// 广播数据
static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

// ==================== 私有变量 ====================

static uint16_t provision_handle_table[GATTS_NUM_HANDLE_PROVISION];
static uint8_t char_value[GATTS_CHAR_VAL_LEN_MAX] = {0};

// 广播参数
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// 广播数据
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(GATTS_SERVICE_UUID_PROVISION),
    .p_service_uuid = (uint8_t*)&GATTS_SERVICE_UUID_PROVISION,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// 扫描响应数据
static esp_ble_adv_data_t scan_rsp_data = {
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
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// GATT服务定义
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;

// 服务属性表
static const esp_gatts_attr_db_t gatt_db[GATTS_NUM_HANDLE_PROVISION] = {
    // Service Declaration
    [0] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_PROVISION), (uint8_t *)&GATTS_SERVICE_UUID_PROVISION}},

    // Write Characteristic Declaration
    [1] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},
    // Write Characteristic Value
    [2] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_WRITE, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    // Read Characteristic Declaration
    [3] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read}},
    // Read Characteristic Value
    [4] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_READ, ESP_GATT_PERM_READ,
            GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    // Notify Characteristic Declaration
    [5] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write_notify}},
    // Notify Characteristic Value
    [6] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_NOTIFY, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},
    // Client Characteristic Configuration Descriptor
    [7] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t), sizeof(uint16_t), NULL}},
};

// ==================== 外部变量声明 ====================

extern uint16_t g_gatts_if;
extern uint16_t g_conn_id;
extern uint16_t g_service_handle;
extern uint16_t g_char_handle_write;
extern uint16_t g_char_handle_read;
extern uint16_t g_char_handle_notify;

// ==================== 私有函数声明 ====================

static void bt_provision_prepare_write_event_env(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void bt_provision_exec_write_event_env(esp_ble_gatts_cb_param_t *param);
bt_provision_err_t bt_provision_send_notification(const char* data);
static bt_provision_err_t bt_provision_process_write_data(const uint8_t* data, uint16_t len);

// ==================== 公共函数实现 ====================

esp_err_t bt_provision_start_ble_advertising(void)
{
    esp_err_t ret = ESP_OK;
    
    // 设置设备名称
    ret = esp_ble_gap_set_device_name("AIOT-Device");
    if (ret) {
        ESP_LOGE(TAG, "set device name failed, error code = %x", ret);
        return ret;
    }
    
    // 配置广播数据
    ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret) {
        ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        return ret;
    }
    adv_config_done |= adv_config_flag;
    
    // 配置扫描响应数据
    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret) {
        ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
        return ret;
    }
    adv_config_done |= scan_rsp_config_flag;
    
    ESP_LOGI(TAG, "BLE advertising configuration completed");
    return ESP_OK;
}

esp_err_t bt_provision_stop_ble_advertising(void)
{
    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "stop advertising failed, error code = %x", ret);
    } else {
        ESP_LOGI(TAG, "BLE advertising stopped successfully");
    }
    return ret;
}

void bt_provision_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0) {
            esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "start advertising failed, error code = %x", ret);
            }
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0) {
            esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "start advertising failed, error code = %x", ret);
            }
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "advertising start failed");
        } else {
            ESP_LOGI(TAG, "advertising start successfully");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "advertising stop failed");
        } else {
            ESP_LOGI(TAG, "stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                param->update_conn_params.status,
                param->update_conn_params.min_int,
                param->update_conn_params.max_int,
                param->update_conn_params.conn_int,
                param->update_conn_params.latency,
                param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

void bt_provision_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name("AIOT-Device");
        if (set_dev_name_ret) {
            ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        
        esp_err_t ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, GATTS_NUM_HANDLE_PROVISION, 0);
        if (ret) {
            ESP_LOGE(TAG, "create attr table failed, error code = %x", ret);
        }
        break;
    }
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_READ_EVT");
        break;
    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep) {
            ESP_LOGI(TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            
            if (provision_handle_table[2] == param->write.handle && param->write.len > 0) {
                // 处理写入的配网数据
                bt_provision_process_write_data(param->write.value, param->write.len);
            }
            
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        } else {
            bt_provision_prepare_write_event_env(gatts_if, param);
        }
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");
        bt_provision_exec_write_event_env(param);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
        g_conn_id = param->connect.conn_id;
        g_gatts_if = gatts_if;
        
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x20;
        conn_params.min_int = 0x10;
        conn_params.timeout = 400;
        esp_ble_gap_update_conn_params(&conn_params);
        
        // 更新配网状态
        extern void bt_provision_set_state(bt_provision_state_t state, const char* message);
        bt_provision_set_state(BT_PROVISION_STATE_CONNECTED, "BLE client connected");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
        esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "restart advertising failed, error code = %x", ret);
        }
        
        // 更新配网状态
        extern void bt_provision_set_state(bt_provision_state_t state, const char* message);
        bt_provision_set_state(BT_PROVISION_STATE_ADVERTISING, "BLE client disconnected, restarting advertising");
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        } else if (param->add_attr_tab.num_handle != GATTS_NUM_HANDLE_PROVISION) {
            ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) doesn't equal to GATTS_NUM_HANDLE_PROVISION(%d)",
                     param->add_attr_tab.num_handle, GATTS_NUM_HANDLE_PROVISION);
        } else {
            ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d", param->add_attr_tab.num_handle);
            memcpy(provision_handle_table, param->add_attr_tab.handles, sizeof(provision_handle_table));
            
            g_service_handle = provision_handle_table[0];
            g_char_handle_write = provision_handle_table[2];
            g_char_handle_read = provision_handle_table[4];
            g_char_handle_notify = provision_handle_table[6];
            
            esp_ble_gatts_start_service(provision_handle_table[0]);
        }
        break;
    }
    default:
        break;
    }
}

// ==================== 私有函数实现 ====================

static void bt_provision_prepare_write_event_env(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(TAG, "prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.offset > GATTS_CHAR_VAL_LEN_MAX) {
        status = ESP_GATT_INVALID_OFFSET;
    } else if ((param->write.offset + param->write.len) > GATTS_CHAR_VAL_LEN_MAX) {
        status = ESP_GATT_INVALID_ATTR_LEN;
    }
    
    esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
    if (gatt_rsp) {
        gatt_rsp->attr_value.len = param->write.len;
        gatt_rsp->attr_value.handle = param->write.handle;
        gatt_rsp->attr_value.offset = param->write.offset;
        gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
        memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
        esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
        if (response_err != ESP_OK) {
            ESP_LOGE(TAG, "Send response error");
        }
        free(gatt_rsp);
    } else {
        ESP_LOGE(TAG, "malloc failed");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_NO_RESOURCES, NULL);
    }
}

static void bt_provision_exec_write_event_env(esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        esp_log_buffer_hex(TAG, char_value, GATTS_CHAR_VAL_LEN_MAX);
    } else {
        ESP_LOGI(TAG, "ESP_GATT_PREP_WRITE_CANCEL");
    }
}

bt_provision_err_t bt_provision_send_notification(const char* data)
{
    if (!data || g_gatts_if == ESP_GATT_IF_NONE) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    uint16_t len = strlen(data);
    if (len > GATTS_CHAR_VAL_LEN_MAX) {
        len = GATTS_CHAR_VAL_LEN_MAX;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id, g_char_handle_notify, len, (uint8_t*)data, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send notification failed: %s", esp_err_to_name(ret));
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    return BT_PROVISION_ERR_OK;
}

static bt_provision_err_t bt_provision_process_write_data(const uint8_t* data, uint16_t len)
{
    if (!data || len == 0) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 确保数据以null结尾
    char* json_str = malloc(len + 1);
    if (!json_str) {
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    memcpy(json_str, data, len);
    json_str[len] = '\0';
    
    ESP_LOGI(TAG, "Received JSON: %s", json_str);
    
    // 处理JSON命令
    extern bt_provision_err_t bt_provision_process_command(const char* json_data);
    bt_provision_err_t ret = bt_provision_process_command(json_str);
    
    free(json_str);
    return ret;
}

#endif // ESP_PLATFORM