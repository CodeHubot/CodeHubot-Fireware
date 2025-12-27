/**
 * @file captive_portal.c
 * @brief Captive Portal（强制门户）组件实现
 */

#include "captive_portal.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "captive_portal";

// DNS服务器配置
#define DNS_PORT 53
#define DNS_MAX_LEN 256
#define DNS_QUERY_A 1
#define DNS_ANSWER_ADDR 0xC0A80401  // 192.168.4.1

static int s_dns_socket = -1;
static TaskHandle_t s_dns_task_handle = NULL;
static bool s_dns_running = false;

/**
 * @brief DNS响应结构
 */
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority;
    uint16_t additional;
} dns_header_t;

/**
 * @brief 构建DNS响应包
 * 
 * @param request 请求数据
 * @param req_len 请求长度
 * @param response 响应缓冲区
 * @return 响应长度
 */
static int build_dns_response(const uint8_t *request, int req_len, uint8_t *response) {
    if (req_len < sizeof(dns_header_t)) {
        return 0;
    }

    // 复制请求头
    memcpy(response, request, sizeof(dns_header_t));
    dns_header_t *header = (dns_header_t *)response;
    
    // 设置响应标志
    header->flags = htons(0x8180);  // 标准查询响应，无错误
    header->answers = htons(1);     // 1个回答
    header->authority = 0;
    header->additional = 0;
    
    // 复制问题部分（从头部之后到请求结束）
    int pos = sizeof(dns_header_t);
    int question_len = req_len - sizeof(dns_header_t);
    memcpy(response + pos, request + sizeof(dns_header_t), question_len);
    pos += question_len;
    
    // 添加回答部分
    // NAME: 使用指针指向问题中的域名（压缩格式）
    response[pos++] = 0xC0;  // 指针标志
    response[pos++] = 0x0C;  // 指向偏移12（问题部分开始）
    
    // TYPE: A (1)
    response[pos++] = 0x00;
    response[pos++] = 0x01;
    
    // CLASS: IN (1)
    response[pos++] = 0x00;
    response[pos++] = 0x01;
    
    // TTL: 60秒
    response[pos++] = 0x00;
    response[pos++] = 0x00;
    response[pos++] = 0x00;
    response[pos++] = 0x3C;
    
    // RDLENGTH: 4字节（IPv4地址）
    response[pos++] = 0x00;
    response[pos++] = 0x04;
    
    // RDATA: 192.168.4.1
    response[pos++] = 192;
    response[pos++] = 168;
    response[pos++] = 4;
    response[pos++] = 1;
    
    return pos;
}

/**
 * @brief DNS服务器任务
 */
static void dns_server_task(void *pvParameters) {
    char rx_buffer[DNS_MAX_LEN];
    char tx_buffer[DNS_MAX_LEN];
    struct sockaddr_in client_addr;
    socklen_t socklen = sizeof(client_addr);
    
    ESP_LOGI(TAG, "DNS服务器任务启动");
    
    while (s_dns_running) {
        // 接收DNS查询
        int len = recvfrom(s_dns_socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&client_addr, &socklen);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            ESP_LOGE(TAG, "DNS接收错误: errno %d", errno);
            break;
        }
        
        if (len > 0) {
            // 提取域名用于日志（简单解析）
            char domain_name[128] = {0};
            int pos = sizeof(dns_header_t);
            int domain_pos = 0;
            
            while (pos < len && rx_buffer[pos] != 0 && domain_pos < sizeof(domain_name) - 1) {
                int label_len = rx_buffer[pos++];
                if (label_len > 63 || pos + label_len > len) break;  // 防止越界
                
                if (domain_pos > 0) {
                    domain_name[domain_pos++] = '.';
                }
                
                for (int i = 0; i < label_len && domain_pos < sizeof(domain_name) - 1; i++) {
                    domain_name[domain_pos++] = rx_buffer[pos++];
                }
            }
            domain_name[domain_pos] = '\0';
            
            ESP_LOGI(TAG, "📡 DNS查询: %s -> 192.168.4.1", domain_name[0] ? domain_name : "(解析失败)");
            
            // 构建DNS响应（所有域名都解析到192.168.4.1）
            int response_len = build_dns_response((uint8_t *)rx_buffer, len, (uint8_t *)tx_buffer);
            
            if (response_len > 0) {
                // 发送响应
                int sent = sendto(s_dns_socket, tx_buffer, response_len, 0,
                                 (struct sockaddr *)&client_addr, sizeof(client_addr));
                
                if (sent < 0) {
                    ESP_LOGE(TAG, "DNS发送响应失败: errno %d", errno);
                } else {
                    ESP_LOGI(TAG, "✅ DNS响应已发送: %d字节", sent);
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "DNS服务器任务退出");
    s_dns_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t captive_portal_dns_start(void) {
    if (s_dns_running) {
        ESP_LOGW(TAG, "DNS服务器已在运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动Captive Portal DNS服务器...");
    
    // 创建UDP socket
    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_socket < 0) {
        ESP_LOGE(TAG, "创建DNS socket失败: errno %d", errno);
        return ESP_FAIL;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(s_dns_socket, F_GETFL, 0);
    fcntl(s_dns_socket, F_SETFL, flags | O_NONBLOCK);
    
    // 绑定到DNS端口（53）
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);
    
    if (bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "绑定DNS端口失败: errno %d", errno);
        close(s_dns_socket);
        s_dns_socket = -1;
        return ESP_FAIL;
    }
    
    // 启动DNS服务器任务
    s_dns_running = true;
    if (xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &s_dns_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "创建DNS服务器任务失败");
        close(s_dns_socket);
        s_dns_socket = -1;
        s_dns_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Captive Portal DNS服务器启动成功，端口: %d", DNS_PORT);
    return ESP_OK;
}

void captive_portal_dns_stop(void) {
    if (!s_dns_running) {
        return;
    }
    
    ESP_LOGI(TAG, "停止DNS服务器...");
    s_dns_running = false;
    
    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }
    
    // 等待任务退出
    if (s_dns_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "DNS服务器已停止");
}

/**
 * @brief 处理Captive Portal检测请求
 * 
 * iOS: http://captive.apple.com/hotspot-detect.html
 * Android: http://connectivitycheck.gstatic.com/generate_204
 * Windows: http://www.msftconnecttest.com/connecttest.txt
 */
static esp_err_t captive_portal_detect_handler(httpd_req_t *req) {
    const char *uri = req->uri;
    
    ESP_LOGI(TAG, "收到Captive Portal检测请求: %s", uri);
    
    // 所有Captive Portal检测请求都返回302重定向
    // 这样iOS/Android/Windows会认为存在Captive Portal，触发弹窗
    ESP_LOGI(TAG, "   🔄 重定向到配网页面（触发Captive Portal弹窗）");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}

/**
 * @brief 常见路径重定向处理器（用于替代通配符）
 */
static esp_err_t redirect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "重定向请求: %s", req->uri);
    
    // 重定向到配网页面
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}

esp_err_t captive_portal_register_handlers(httpd_handle_t server) {
    if (server == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "注册Captive Portal HTTP处理器...");
    
    // 注册常见的Captive Portal检测路径
    const char *detect_paths[] = {
        "/hotspot-detect.html",
        "/library/test/success.html",
        "/generate_204",
        "/gen_204",
        "/connecttest.txt",
        "/ncsi.txt",
        "/success.txt",
        NULL
    };
    
    for (int i = 0; detect_paths[i] != NULL; i++) {
        httpd_uri_t detect_uri = {
            .uri = detect_paths[i],
            .method = HTTP_GET,
            .handler = captive_portal_detect_handler,
            .user_ctx = NULL
        };
        
        esp_err_t ret = httpd_register_uri_handler(server, &detect_uri);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "注册 %s 失败: %s", detect_paths[i], esp_err_to_name(ret));
        }
    }
    
    // 注册常见的重定向路径（不使用通配符，避免冲突）
    const char *redirect_paths[] = {
        "/favicon.ico",
        "/apple-touch-icon.png",
        "/apple-touch-icon-precomposed.png",
        "/robots.txt",
        "/sitemap.xml",
        NULL
    };
    
    for (int i = 0; redirect_paths[i] != NULL; i++) {
        httpd_uri_t redirect_uri = {
            .uri = redirect_paths[i],
            .method = HTTP_GET,
            .handler = redirect_handler,
            .user_ctx = NULL
        };
        
        esp_err_t ret = httpd_register_uri_handler(server, &redirect_uri);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "注册 %s 失败: %s", redirect_paths[i], esp_err_to_name(ret));
        }
    }
    
    ESP_LOGI(TAG, "✅ Captive Portal HTTP处理器注册成功");
    return ESP_OK;
}

