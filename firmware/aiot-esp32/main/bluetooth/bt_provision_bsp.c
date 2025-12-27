/**
 * @file bt_provision_bsp.c
 * @brief 蓝牙配网功能的BSP集成实现
 * 
 * 将蓝牙配网功能集成到现有的BSP架构中
 * 
 * @author AIOT Team
 * @date 2024-01-01
 */

#include "../bsp/bsp_interface.h"
#include "bt_provision.h"
#include <string.h>
#include <stdio.h>

// ==================== 私有常量 ====================

static const char* TAG = "BT_PROVISION_BSP";

// ==================== 私有变量 ====================

static bool g_bsp_provision_initialized = false;
static bt_provision_config_t g_bsp_provision_config = {0};

// ==================== 私有函数声明 ====================

static void bt_provision_bsp_event_callback(bt_provision_state_t state, bt_provision_err_t error, const char* message);

// ==================== 公共函数实现 ====================

bt_provision_err_t bt_provision_bsp_init(bsp_interface_t* bsp)
{
    if (!bsp) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    if (g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_OK;
    }
    
    // 获取板级信息
    const bsp_board_info_t* board_info = bsp->get_board_info();
    if (!board_info) {
        ESP_LOGE(TAG, "Failed to get board info");
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 检查BSP是否支持蓝牙
    if (!board_info->has_bluetooth) {
        ESP_LOGE(TAG, "BSP does not support Bluetooth");
        return BT_PROVISION_ERR_BLE_FAILED;
    }
    
    // 配置蓝牙配网参数
    memset(&g_bsp_provision_config, 0, sizeof(bt_provision_config_t));
    
    // 从BSP获取设备名称
    if (board_info->board_name && strlen(board_info->board_name) > 0) {
        snprintf(g_bsp_provision_config.device_name, BT_PROVISION_DEVICE_NAME_MAX, 
                "AIOT-%s", board_info->board_name);
    } else {
        strcpy(g_bsp_provision_config.device_name, "AIOT-Device");
    }
    
    // 设置超时参数
    g_bsp_provision_config.advertising_timeout_ms = 60000; // 60秒
    // 注意：这些字段在头文件中没有定义，先注释掉
    // g_bsp_provision_config.wifi_connect_timeout_ms = 30000; // 30秒
    // g_bsp_provision_config.server_connect_timeout_ms = 10000; // 10秒
    
    // 设置事件回调
    g_bsp_provision_config.event_callback = bt_provision_bsp_event_callback;
    
    // 初始化蓝牙配网功能
    bt_provision_err_t ret = bt_provision_init(&g_bsp_provision_config);
    if (ret != BT_PROVISION_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bluetooth provisioning: %s", 
                bt_provision_get_error_string(ret));
        return ret;
    }
    
    g_bsp_provision_initialized = true;
    ESP_LOGI(TAG, "Bluetooth provisioning BSP integration initialized for device: %s", 
            g_bsp_provision_config.device_name);
    
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_bsp_deinit(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    bt_provision_err_t ret = bt_provision_deinit();
    if (ret == BT_PROVISION_ERR_OK) {
        g_bsp_provision_initialized = false;
        ESP_LOGI(TAG, "Bluetooth provisioning BSP integration deinitialized");
    }
    
    return ret;
}

bt_provision_err_t bt_provision_bsp_start_auto_provision(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    // 检查是否已经配置完成
    if (bt_provision_is_wifi_configured() && bt_provision_is_server_configured()) {
        ESP_LOGI(TAG, "Device already provisioned, skipping auto provision");
        return BT_PROVISION_ERR_ALREADY_CONFIGURED;
    }
    
    ESP_LOGI(TAG, "Starting auto provisioning mode");
    return bt_provision_start();
}

bt_provision_err_t bt_provision_bsp_force_provision(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Starting forced provisioning mode");
    return bt_provision_start();
}

bt_provision_err_t bt_provision_bsp_stop_provision(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    return bt_provision_stop();
}

bool bt_provision_bsp_is_provisioned(void)
{
    if (!g_bsp_provision_initialized) {
        return false;
    }
    
    return bt_provision_is_wifi_configured() && bt_provision_is_server_configured();
}

bt_provision_state_t bt_provision_bsp_get_state(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_STATE_IDLE;
    }
    
    return bt_provision_get_state();
}

bt_provision_err_t bt_provision_bsp_get_wifi_config(bt_provision_wifi_config_t* config)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    return bt_provision_get_wifi_config(config);
}

bt_provision_err_t bt_provision_bsp_get_server_config(bt_provision_server_config_t* config)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    return bt_provision_get_server_config(config);
}

bt_provision_err_t bt_provision_bsp_reset_all_config(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Resetting all provisioning configuration");
    return bt_provision_reset_config(true, true);
}

bt_provision_err_t bt_provision_bsp_reset_wifi_config(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Resetting WiFi configuration");
    return bt_provision_reset_config(true, false);
}

bt_provision_err_t bt_provision_bsp_reset_server_config(void)
{
    if (!g_bsp_provision_initialized) {
        return BT_PROVISION_ERR_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Resetting server configuration");
    return bt_provision_reset_config(false, true);
}

bt_provision_err_t bt_provision_bsp_get_status_info(char* status_buffer, size_t buffer_size)
{
    if (!g_bsp_provision_initialized || !status_buffer || buffer_size == 0) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    bt_provision_status_t status;
    bt_provision_err_t ret = bt_provision_get_status(&status);
    if (ret != BT_PROVISION_ERR_OK) {
        return ret;
    }
    
    snprintf(status_buffer, buffer_size,
            "State: %s, Progress: %d%%, WiFi: %s, Server: %s, IP: %s, Message: %s",
            bt_provision_get_state_string(status.state),
            status.progress,
            status.wifi_status,
            status.server_status,
            status.wifi_ip,
            status.message);
    
    return BT_PROVISION_ERR_OK;
}

bt_provision_err_t bt_provision_bsp_get_device_info(char* info_buffer, size_t buffer_size)
{
    if (!g_bsp_provision_initialized || !info_buffer || buffer_size == 0) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    bt_provision_device_info_t device_info;
    bt_provision_err_t ret = bt_provision_get_device_info(&device_info);
    if (ret != BT_PROVISION_ERR_OK) {
        return ret;
    }
    
    snprintf(info_buffer, buffer_size,
            "Device: %s, MAC: %s, Firmware: %s, Chip: %s, WiFi: %s, Provision: %s",
            device_info.device_name,
            device_info.mac_address,
            device_info.firmware_version,
            device_info.chip_model,
            device_info.wifi_status,
            device_info.provision_status);
    
    return BT_PROVISION_ERR_OK;
}

// ==================== 私有函数实现 ====================

static void bt_provision_bsp_event_callback(bt_provision_state_t state, bt_provision_err_t error, const char* message)
{
    const char* state_str = bt_provision_get_state_string(state);
    const char* error_str = bt_provision_get_error_string(error);
    
    if (error == BT_PROVISION_ERR_OK) {
        ESP_LOGI(TAG, "Provision event: %s - %s", state_str, message ? message : "");
    } else {
        ESP_LOGE(TAG, "Provision error: %s - %s (%s)", state_str, error_str, message ? message : "");
    }
    
    // 根据状态执行相应的BSP操作
    switch (state) {
        case BT_PROVISION_STATE_ADVERTISING:
            // 可以在这里控制LED指示灯闪烁，表示正在广播
            ESP_LOGI(TAG, "Device is advertising for provisioning");
            break;
            
        case BT_PROVISION_STATE_CONNECTED:
            // 可以在这里控制LED指示灯常亮，表示已连接
            ESP_LOGI(TAG, "Client connected for provisioning");
            break;
            
        case BT_PROVISION_STATE_SUCCESS:
            // 可以在这里控制LED指示灯绿色常亮，表示配网成功
            ESP_LOGI(TAG, "Provisioning completed successfully");
            // 自动停止配网服务
            bt_provision_stop();
            break;
            
        case BT_PROVISION_STATE_FAILED:
            // 可以在这里控制LED指示灯红色闪烁，表示配网失败
            ESP_LOGE(TAG, "Provisioning failed");
            break;
            
        case BT_PROVISION_STATE_TIMEOUT:
            // 可以在这里控制LED指示灯熄灭，表示配网超时
            ESP_LOGW(TAG, "Provisioning timeout");
            break;
            
        default:
            break;
    }
}

// ==================== BSP扩展接口 ====================

/**
 * @brief 扩展BSP接口，添加蓝牙配网功能
 * 
 * 这个函数可以在BSP初始化后调用，为BSP添加蓝牙配网功能
 * 
 * @param bsp BSP接口指针
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_extend_bsp(bsp_interface_t* bsp)
{
    if (!bsp) {
        return BT_PROVISION_ERR_INVALID_PARAM;
    }
    
    // 初始化蓝牙配网BSP集成
    bt_provision_err_t ret = bt_provision_bsp_init(bsp);
    if (ret != BT_PROVISION_ERR_OK) {
        return ret;
    }
    
    ESP_LOGI(TAG, "BSP extended with Bluetooth provisioning functionality");
    
    // 可以在这里添加更多的BSP扩展功能
    // 例如：添加配网状态查询命令、配网控制命令等
    
    return BT_PROVISION_ERR_OK;
}

/**
 * @brief 获取蓝牙配网功能的版本信息
 * 
 * @return const char* 版本字符串
 */
const char* bt_provision_bsp_get_version(void)
{
    return "1.0.0";
}

/**
 * @brief 检查BSP是否支持蓝牙配网功能
 * 
 * @param bsp BSP接口指针
 * @return true 支持
 * @return false 不支持
 */
bool bt_provision_bsp_is_supported(bsp_interface_t* bsp)
{
    if (!bsp) {
        return false;
    }
    
    const bsp_board_info_t* board_info = bsp->get_board_info();
    if (!board_info) {
        return false;
    }
    
    return board_info->has_bluetooth;
}