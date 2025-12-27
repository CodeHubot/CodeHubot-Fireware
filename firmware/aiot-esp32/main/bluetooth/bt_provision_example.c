/**
 * @file bt_provision_example.c
 * @brief 蓝牙配网功能使用示例
 * 
 * 展示如何在main.c中集成和使用蓝牙配网功能
 * 
 * @author AIOT Team
 * @date 2024-01-01
 */

#include "bt_provision.h"
#include "bt_provision_bsp.h"
#include "../bsp/bsp_interface.h"
#include <stdio.h>
#include <string.h>

// ==================== 私有常量 ====================

static const char* TAG = "BT_PROVISION_EXAMPLE";

// ==================== 示例函数 ====================

/**
 * @brief 蓝牙配网功能集成示例
 * 
 * 这个函数展示了如何在main.c的app_main函数中集成蓝牙配网功能
 */
void bt_provision_integration_example(void)
{
    ESP_LOGI(TAG, "=== Bluetooth Provisioning Integration Example ===");
    
    // 1. 获取当前BSP接口
    const bsp_interface_t* bsp = bsp_get_interface();
    if (!bsp) {
        ESP_LOGE(TAG, "Failed to get BSP interface");
        return;
    }
    
    // 2. 检查BSP是否支持蓝牙配网
    if (!bt_provision_bsp_is_supported((bsp_interface_t*)bsp)) {
        ESP_LOGW(TAG, "Current BSP does not support Bluetooth provisioning");
        return;
    }
    
    // 3. 初始化蓝牙配网BSP集成
    bt_provision_err_t ret = bt_provision_bsp_init((bsp_interface_t*)bsp);
    if (ret != BT_PROVISION_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bluetooth provisioning: %s", 
                bt_provision_get_error_string(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Bluetooth provisioning initialized successfully");
    
    // 4. 检查设备是否已经配网
    if (bt_provision_bsp_is_provisioned()) {
        ESP_LOGI(TAG, "Device is already provisioned");
        
        // 显示当前配置信息
        char status_info[256];
        ret = bt_provision_bsp_get_status_info(status_info, sizeof(status_info));
        if (ret == BT_PROVISION_ERR_OK) {
            ESP_LOGI(TAG, "Current status: %s", status_info);
        }
        
        char device_info[256];
        ret = bt_provision_bsp_get_device_info(device_info, sizeof(device_info));
        if (ret == BT_PROVISION_ERR_OK) {
            ESP_LOGI(TAG, "Device info: %s", device_info);
        }
        
        // 可以选择是否强制重新配网
        // bt_provision_bsp_force_provision();
    } else {
        ESP_LOGI(TAG, "Device not provisioned, starting auto provisioning");
        
        // 5. 启动自动配网模式
        ret = bt_provision_bsp_start_auto_provision();
        if (ret != BT_PROVISION_ERR_OK) {
            ESP_LOGE(TAG, "Failed to start auto provisioning: %s", 
                    bt_provision_get_error_string(ret));
            return;
        }
        
        ESP_LOGI(TAG, "Auto provisioning started, device is now discoverable");
    }
}

/**
 * @brief 蓝牙配网状态监控示例
 * 
 * 这个函数展示了如何监控配网状态
 */
void bt_provision_status_monitor_example(void)
{
    ESP_LOGI(TAG, "=== Bluetooth Provisioning Status Monitor Example ===");
    
    // 获取当前配网状态
    bt_provision_state_t state = bt_provision_bsp_get_state();
    ESP_LOGI(TAG, "Current provisioning state: %s", bt_provision_get_state_string(state));
    
    // 获取详细状态信息
    char status_info[256];
    bt_provision_err_t ret = bt_provision_bsp_get_status_info(status_info, sizeof(status_info));
    if (ret == BT_PROVISION_ERR_OK) {
        ESP_LOGI(TAG, "Detailed status: %s", status_info);
    }
    
    // 获取WiFi配置信息
    bt_provision_wifi_config_t wifi_config;
    ret = bt_provision_bsp_get_wifi_config(&wifi_config);
    if (ret == BT_PROVISION_ERR_OK && wifi_config.configured) {
        ESP_LOGI(TAG, "WiFi configured - SSID: %s, Security: %d", 
                wifi_config.ssid, wifi_config.security);
    } else {
        ESP_LOGI(TAG, "WiFi not configured");
    }
    
    // 获取服务器配置信息
    bt_provision_server_config_t server_config;
    ret = bt_provision_bsp_get_server_config(&server_config);
    if (ret == BT_PROVISION_ERR_OK && server_config.configured) {
        ESP_LOGI(TAG, "Server configured - URL: %s:%d", 
                server_config.server_url, server_config.server_port);
    } else {
        ESP_LOGI(TAG, "Server not configured");
    }
}

/**
 * @brief 蓝牙配网配置管理示例
 * 
 * 这个函数展示了如何管理配网配置
 */
void bt_provision_config_management_example(void)
{
    ESP_LOGI(TAG, "=== Bluetooth Provisioning Config Management Example ===");
    
    // 重置WiFi配置
    ESP_LOGI(TAG, "Resetting WiFi configuration...");
    bt_provision_err_t ret = bt_provision_bsp_reset_wifi_config();
    if (ret == BT_PROVISION_ERR_OK) {
        ESP_LOGI(TAG, "WiFi configuration reset successfully");
    } else {
        ESP_LOGE(TAG, "Failed to reset WiFi configuration: %s", 
                bt_provision_get_error_string(ret));
    }
    
    // 重置服务器配置
    ESP_LOGI(TAG, "Resetting server configuration...");
    ret = bt_provision_bsp_reset_server_config();
    if (ret == BT_PROVISION_ERR_OK) {
        ESP_LOGI(TAG, "Server configuration reset successfully");
    } else {
        ESP_LOGE(TAG, "Failed to reset server configuration: %s", 
                bt_provision_get_error_string(ret));
    }
    
    // 重置所有配置
    ESP_LOGI(TAG, "Resetting all configurations...");
    ret = bt_provision_bsp_reset_all_config();
    if (ret == BT_PROVISION_ERR_OK) {
        ESP_LOGI(TAG, "All configurations reset successfully");
    } else {
        ESP_LOGE(TAG, "Failed to reset all configurations: %s", 
                bt_provision_get_error_string(ret));
    }
}

/**
 * @brief 完整的main.c集成示例
 * 
 * 这个函数展示了如何在app_main中完整集成蓝牙配网功能
 */
void app_main_integration_example(void)
{
    ESP_LOGI(TAG, "=== Complete app_main Integration Example ===");
    
    // 这是一个完整的app_main函数示例，展示如何集成蓝牙配网功能
    
    /*
    void app_main(void)
    {
        // 1. 初始化NVS
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        
        // 2. 注册并初始化BSP
        bsp_esp32_s3_devkit_register();
        const bsp_interface_t* bsp = bsp_get_interface();
        if (bsp && bsp->init) {
            hal_err_t hal_ret = bsp->init();
            if (hal_ret != HAL_OK) {
                ESP_LOGE("MAIN", "BSP initialization failed");
                return;
            }
        }
        
        // 3. 打印板级信息
        bsp_print_board_info();
        
        // 4. 初始化蓝牙配网功能
        if (bt_provision_bsp_is_supported((bsp_interface_t*)bsp)) {
            bt_provision_err_t provision_ret = bt_provision_bsp_init((bsp_interface_t*)bsp);
            if (provision_ret == BT_PROVISION_ERR_OK) {
                ESP_LOGI("MAIN", "Bluetooth provisioning initialized");
                
                // 检查是否需要配网
                if (!bt_provision_bsp_is_provisioned()) {
                    ESP_LOGI("MAIN", "Starting provisioning mode");
                    bt_provision_bsp_start_auto_provision();
                } else {
                    ESP_LOGI("MAIN", "Device already provisioned");
                    
                    // 获取并显示配置信息
                    char status_info[256];
                    if (bt_provision_bsp_get_status_info(status_info, sizeof(status_info)) == BT_PROVISION_ERR_OK) {
                        ESP_LOGI("MAIN", "Status: %s", status_info);
                    }
                }
            } else {
                ESP_LOGE("MAIN", "Failed to initialize Bluetooth provisioning: %s", 
                        bt_provision_get_error_string(provision_ret));
            }
        } else {
            ESP_LOGW("MAIN", "Bluetooth provisioning not supported on this board");
        }
        
        // 5. 执行其他初始化和主循环
        // ... 其他应用逻辑 ...
        
        // 主循环
        while (1) {
            // 定期检查配网状态
            bt_provision_state_t state = bt_provision_bsp_get_state();
            if (state == BT_PROVISION_STATE_SUCCESS) {
                // 配网成功，可以开始正常的IoT功能
                ESP_LOGI("MAIN", "Provisioning completed, starting IoT functions");
                break;
            } else if (state == BT_PROVISION_STATE_FAILED || state == BT_PROVISION_STATE_TIMEOUT) {
                // 配网失败，可以选择重试或进入错误处理
                ESP_LOGW("MAIN", "Provisioning failed, retrying...");
                bt_provision_bsp_start_auto_provision();
            }
            
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        // 6. 清理资源（如果需要）
        // bt_provision_bsp_deinit();
    }
    */
    
    ESP_LOGI(TAG, "See the commented code above for complete integration example");
}

/**
 * @brief 运行所有示例
 */
void bt_provision_run_all_examples(void)
{
    ESP_LOGI(TAG, "Running all Bluetooth provisioning examples...");
    
    bt_provision_integration_example();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    bt_provision_status_monitor_example();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    bt_provision_config_management_example();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    app_main_integration_example();
    
    ESP_LOGI(TAG, "All examples completed");
}