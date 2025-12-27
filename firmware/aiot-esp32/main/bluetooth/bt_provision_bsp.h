#ifndef BT_PROVISION_BSP_H
#define BT_PROVISION_BSP_H

#include "bt_provision.h"
#include "../bsp/bsp_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蓝牙配网BSP
 * @param bsp BSP接口指针
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_init(bsp_interface_t* bsp);

/**
 * @brief 反初始化蓝牙配网BSP
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_deinit(void);

/**
 * @brief 启动自动配网
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_start_auto_provision(void);

/**
 * @brief 强制启动配网
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_force_provision(void);

/**
 * @brief 停止配网
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_stop_provision(void);

/**
 * @brief 检查是否已配网
 * @return bool 是否已配网
 */
bool bt_provision_bsp_is_provisioned(void);

/**
 * @brief 获取配网状态
 * @return bt_provision_state_t 配网状态
 */
bt_provision_state_t bt_provision_bsp_get_state(void);

/**
 * @brief 获取WiFi配置
 * @param config WiFi配置指针
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_get_wifi_config(bt_provision_wifi_config_t* config);

/**
 * @brief 获取服务器配置
 * @param config 服务器配置指针
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_get_server_config(bt_provision_server_config_t* config);

/**
 * @brief 重置所有配置
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_reset_all_config(void);

/**
 * @brief 重置WiFi配置
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_reset_wifi_config(void);

/**
 * @brief 重置服务器配置
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_reset_server_config(void);

/**
 * @brief 获取状态信息
 * @param status_buffer 状态缓冲区
 * @param buffer_size 缓冲区大小
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_get_status_info(char* status_buffer, size_t buffer_size);

/**
 * @brief 获取设备信息
 * @param info_buffer 信息缓冲区
 * @param buffer_size 缓冲区大小
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_bsp_get_device_info(char* info_buffer, size_t buffer_size);

/**
 * @brief 扩展BSP功能
 * @param bsp BSP接口指针
 * @return bt_provision_err_t 错误码
 */
bt_provision_err_t bt_provision_extend_bsp(bsp_interface_t* bsp);

/**
 * @brief 获取BSP版本
 * @return const char* 版本字符串
 */
const char* bt_provision_bsp_get_version(void);

/**
 * @brief 检查BSP是否支持
 * @param bsp BSP接口指针
 * @return bool 是否支持
 */
bool bt_provision_bsp_is_supported(bsp_interface_t* bsp);

#ifdef __cplusplus
}
#endif

#endif // BT_PROVISION_BSP_H