/**
 * @file captive_portal.h
 * @brief Captive Portal（强制门户）组件
 * 
 * 实现DNS服务器和HTTP重定向，让手机连接热点后自动弹出配网页面
 */

#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 Captive Portal DNS 服务器
 * 
 * 将所有域名解析到 192.168.4.1
 * 
 * @return ESP_OK 成功
 *         其他值 失败
 */
esp_err_t captive_portal_dns_start(void);

/**
 * @brief 停止 Captive Portal DNS 服务器
 */
void captive_portal_dns_stop(void);

/**
 * @brief 注册 Captive Portal HTTP 处理器
 * 
 * 捕获所有未匹配的HTTP请求，重定向到配网页面
 * 
 * @param server HTTP服务器句柄
 * @return ESP_OK 成功
 *         其他值 失败
 */
esp_err_t captive_portal_register_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // CAPTIVE_PORTAL_H

