/**
 * @file main.c
 * @brief AIOT ESP32-C3 Lite 精简版主程序
 * 
 * ESP32-C3精简版IoT设备固件 - 无OTA、无LVGL、无LCD显示
 * 支持：WiFi配网、MQTT通信、DHT11传感器、LED和继电器控制
 * 
 * @author AIOT Team
 * @date 2025-12-27
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_http_server.h"

#include "mqtt_client.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "board_config.h"
#include "app_config.h"
#include "ssd1306_oled.h"
#include "dht11_driver.h"
#include "device_config.h"
#include "esp_random.h"

// ==================== DNS服务器配置 ====================
#define DNS_SERVER_PORT 53
#define DNS_MAX_LEN 512

// DNS服务器任务句柄
static TaskHandle_t dns_server_task_handle = NULL;

// ==================== 全局变量 ====================
static const char *TAG = LOG_TAG_MAIN;

// 设备标识
static char g_device_id[DEVICE_ID_MAX_LEN] = {0};
static char g_device_uuid[DEVICE_UUID_MAX_LEN] = {0};

// WiFi相关
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_wifi_retry_num = 0;
static bool g_wifi_connected = false;

// MQTT相关
static esp_mqtt_client_handle_t g_mqtt_client = NULL;
static bool g_mqtt_connected = false;
static char g_mqtt_topic_data[128] = {0};
static char g_mqtt_topic_status[128] = {0};
static char g_mqtt_topic_heartbeat[128] = {0};
static char g_mqtt_topic_control[128] = {0};

// 传感器数据（DHT11简化读取）
typedef struct {
    float temperature;
    float humidity;
    bool valid;
} sensor_data_t;
static sensor_data_t g_sensor_data = {0};

// 系统状态
static uint32_t g_system_start_time = 0;
static bool g_config_mode = false;
static char g_ip_address[16] = {0};
static bool g_device_registered = false;  // 设备是否已注册
static device_config_t g_device_config = {0};  // 设备配置

// ==================== OLED辅助函数 ====================

#if OLED_ENABLED
/**
 * @brief 安全地显示OLED内容（自动清屏避免重叠）
 */
static void oled_display_safe(void) {
    oled_clear();
    vTaskDelay(pdMS_TO_TICKS(50));
}
#endif

// ==================== GPIO控制 ====================

/**
 * @brief 红色LED控制
 */
static void led_red_control(int value) {
    gpio_set_level(LED1_GPIO_PIN, value ? LED1_ACTIVE_LEVEL : !LED1_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "红色LED %s", value ? "ON" : "OFF");
}

/**
 * @brief 蓝色LED控制
 */
static void led_blue_control(int value) {
    gpio_set_level(LED2_GPIO_PIN, value ? LED2_ACTIVE_LEVEL : !LED2_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "蓝色LED %s", value ? "ON" : "OFF");
}

/**
 * @brief 继电器控制
 */
__attribute__((unused))
static void relay_control(int value) {
    gpio_set_level(RELAY1_GPIO_PIN, value ? RELAY1_ACTIVE_LEVEL : !RELAY1_ACTIVE_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(RELAY1_SWITCH_DELAY));
    ESP_LOGI(TAG, "RELAY1 %s", value ? "ON" : "OFF");
}

// ==================== WiFi事件处理 ====================

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry_num < WIFI_MAX_RETRY_COUNT) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGW(TAG, "WiFi连接失败, 重试 %d/%d", s_wifi_retry_num, WIFI_MAX_RETRY_COUNT);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi连接失败");
        }
        g_wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "✅ WiFi连接成功！IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        g_wifi_connected = true;
    }
}

// ==================== MQTT事件处理 ====================

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ MQTT连接成功");
            g_mqtt_connected = true;
            // 订阅控制主题
            esp_mqtt_client_subscribe(g_mqtt_client, g_mqtt_topic_control, MQTT_QOS_DEFAULT);
            ESP_LOGI(TAG, "订阅主题: %s", g_mqtt_topic_control);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⚠️  MQTT断开连接");
            g_mqtt_connected = false;
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "📨 收到MQTT消息: %.*s", event->data_len, event->data);
            
            // 解析控制命令
            char *data = strndup(event->data, event->data_len);
            if (data) {
                // 新格式: {"portKey": "led_1", "action": "on"}
                // 旧格式: {"port": "LED1", "value": 1}
                
                // 检查新格式（portKey + action）
                if (strstr(data, "\"portKey\"")) {
                    bool is_on = (strstr(data, "\"action\":\"on\"") || strstr(data, "\"action\": \"on\""));
                    bool is_off = (strstr(data, "\"action\":\"off\"") || strstr(data, "\"action\": \"off\""));
                    
                    if (strstr(data, "\"led_1\"") || strstr(data, "\"LED1\"")) {
                        // LED1 = 红灯
                        led_red_control(is_on ? 1 : 0);
                        ESP_LOGI(TAG, "控制LED1(红): %s", is_on ? "ON" : "OFF");
                    } else if (strstr(data, "\"led_2\"") || strstr(data, "\"LED2\"")) {
                        // LED2 = 蓝灯
                        led_blue_control(is_on ? 1 : 0);
                        ESP_LOGI(TAG, "控制LED2(蓝): %s", is_on ? "ON" : "OFF");
                    } else if (strstr(data, "\"led_3\"")) {
                        // LED3 暂不支持
                        ESP_LOGW(TAG, "LED3 不支持");
                    } else if (strstr(data, "\"led_4\"")) {
                        // LED4 暂不支持
                        ESP_LOGW(TAG, "LED4 不支持");
                    }
                }
                // 兼容旧格式
                else if (strstr(data, "\"port\":\"LED1\"") || strstr(data, "\"port\":\"LED_RED\"")) {
                    if (strstr(data, "\"value\":1")) {
                        led_red_control(1);
                    } else if (strstr(data, "\"value\":0")) {
                        led_red_control(0);
                    }
                } else if (strstr(data, "\"port\":\"LED2\"") || strstr(data, "\"port\":\"LED_BLUE\"")) {
                    if (strstr(data, "\"value\":1")) {
                        led_blue_control(1);
                    } else if (strstr(data, "\"value\":0")) {
                        led_blue_control(0);
                    }
                }
                
                free(data);
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ MQTT错误");
            break;
            
        default:
            break;
    }
}

// ==================== DNS服务器（Captive Portal支持）====================

/**
 * @brief DNS服务器任务 - 将所有DNS查询重定向到ESP32
 * 
 * 这是Captive Portal的关键：拦截所有DNS查询并返回ESP32的IP
 * 这样手机访问任何网址都会跳转到配网页面
 */
static void dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char rx_buffer[DNS_MAX_LEN];
    char tx_buffer[DNS_MAX_LEN];
    
    // 创建UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS服务器：创建socket失败");
        vTaskDelete(NULL);
        return;
    }
    
    // 设置socket为非阻塞
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // 绑定到DNS端口53
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DNS_SERVER_PORT);
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "DNS服务器：绑定端口53失败");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "✅ DNS服务器已启动（端口53）");
    
    while (1) {
        // 接收DNS查询
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (len > 0) {
            // 构建DNS响应 - 返回192.168.4.1
            memcpy(tx_buffer, rx_buffer, len);
            
            // 修改DNS响应标志
            tx_buffer[2] = 0x81;  // 标准查询响应
            tx_buffer[3] = 0x80;  // 无错误
            tx_buffer[7] = 0x01;  // 1个应答
            
            // 添加DNS应答（指向192.168.4.1）
            int response_len = len;
            tx_buffer[response_len++] = 0xC0;  // 指针到域名
            tx_buffer[response_len++] = 0x0C;
            tx_buffer[response_len++] = 0x00;  // 类型A
            tx_buffer[response_len++] = 0x01;
            tx_buffer[response_len++] = 0x00;  // 类IN
            tx_buffer[response_len++] = 0x01;
            tx_buffer[response_len++] = 0x00;  // TTL (4字节)
            tx_buffer[response_len++] = 0x00;
            tx_buffer[response_len++] = 0x00;
            tx_buffer[response_len++] = 0x3C;  // 60秒
            tx_buffer[response_len++] = 0x00;  // 数据长度
            tx_buffer[response_len++] = 0x04;  // 4字节（IPv4）
            tx_buffer[response_len++] = 192;   // IP: 192
            tx_buffer[response_len++] = 168;   // IP: 168
            tx_buffer[response_len++] = 4;     // IP: 4
            tx_buffer[response_len++] = 1;     // IP: 1
            
            // 发送DNS响应
            sendto(sock, tx_buffer, response_len, 0,
                  (struct sockaddr *)&client_addr, client_addr_len);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 短暂延迟避免CPU占用过高
    }
    
    close(sock);
    vTaskDelete(NULL);
}

/**
 * @brief 启动DNS服务器
 */
static void start_dns_server(void) {
    if (dns_server_task_handle == NULL) {
        xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_server_task_handle);
        ESP_LOGI(TAG, "DNS服务器任务已创建");
    }
}

/**
 * @brief 停止DNS服务器
 */
__attribute__((unused))
static void stop_dns_server(void) {
    if (dns_server_task_handle != NULL) {
        vTaskDelete(dns_server_task_handle);
        dns_server_task_handle = NULL;
        ESP_LOGI(TAG, "DNS服务器已停止");
    }
}

// ==================== WiFi配网Web服务器 ====================

// 现代化配网页面HTML
static const char* config_page_html = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>AIOT设备配网</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
    "background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px;"
    "display:flex;align-items:center;justify-content:center}"
    ".container{background:white;border-radius:16px;box-shadow:0 20px 60px rgba(0,0,0,0.3);"
    "max-width:420px;width:100%;padding:32px;animation:slideIn 0.3s ease}"
    "@keyframes slideIn{from{opacity:0;transform:translateY(-20px)}to{opacity:1;transform:translateY(0)}}"
    ".header{text-align:center;margin-bottom:24px}"
    ".header h1{font-size:24px;color:#333;margin-bottom:8px}"
    ".header p{color:#666;font-size:14px}"
    ".device-info{background:#f8f9fa;border-radius:8px;padding:12px;margin-bottom:20px;font-size:12px;color:#666}"
    ".form-group{margin-bottom:20px}"
    ".form-group label{display:block;margin-bottom:8px;color:#333;font-weight:500;font-size:14px}"
    ".form-group select,.form-group input{width:100%;padding:12px;border:2px solid #e0e0e0;"
    "border-radius:8px;font-size:14px;transition:all 0.3s}"
    ".form-group select:focus,.form-group input:focus{outline:none;border-color:#667eea}"
    ".btn-primary{width:100%;padding:14px;background:linear-gradient(135deg,#667eea,#764ba2);"
    "color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;"
    "cursor:pointer;transition:transform 0.2s}"
    ".btn-primary:hover{transform:translateY(-2px)}"
    ".btn-secondary{width:100%;padding:12px;background:#f0f0f0;color:#333;border:none;"
    "border-radius:8px;margin-top:10px;cursor:pointer;font-size:14px}"
    ".loading{display:none;text-align:center;margin-top:16px;color:#666}"
    ".spinner{border:3px solid #f3f3f3;border-top:3px solid #667eea;border-radius:50%;"
    "width:32px;height:32px;animation:spin 1s linear infinite;margin:0 auto}"
    "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
    "</style></head><body>"
    "<div class='container'>"
    "<div class='header'>"
    "<h1>🔧 AIOT设备配网</h1>"
    "<p>连接您的WiFi网络</p>"
    "</div>"
    "<div class='device-info' id='deviceInfo'>设备ID: <span id='devId'>加载中...</span></div>"
    "<form id='configForm' action='/save' method='post'>"
    "<div class='form-group'>"
    "<label>📶 WiFi网络</label>"
    "<select id='ssid' name='ssid' required>"
    "<option value=''>正在扫描WiFi...</option>"
    "</select>"
    "</div>"
    "<div class='form-group'>"
    "<label>🔑 WiFi密码</label>"
    "<input type='password' name='pass' placeholder='请输入WiFi密码' required>"
    "</div>"
    "<div class='form-group'>"
    "<label>⚙️ 配置服务器</label>"
    "<input type='text' name='config_srv' value='http://conf.aiot.powertechhub.com:8001' required>"
    "</div>"
    "<div class='form-group'>"
    "<label>🌐 MQTT服务器</label>"
    "<input type='text' name='mqtt' value='conf.aiot.powertechhub.com' required>"
    "</div>"
    "<button type='submit' class='btn-primary'>💾 保存配置</button>"
    "<button type='button' class='btn-secondary' onclick='scanWifi()'>🔄 重新扫描</button>"
    "</form>"
    "<div class='loading' id='loading'>"
    "<div class='spinner'></div>"
    "<p style='margin-top:12px'>正在保存配置...</p>"
    "</div>"
    "</div>"
    "<script>"
    "function scanWifi(){"
    "document.getElementById('ssid').innerHTML='<option>正在扫描...</option>';"
    "fetch('/scan').then(r=>r.json()).then(d=>{"
    "let html='<option value=\"\">请选择WiFi网络</option>';"
    "d.forEach(w=>html+=`<option value=\"${w.ssid}\">${w.ssid} (${w.rssi}dBm)</option>`);"
    "document.getElementById('ssid').innerHTML=html;"
    "}).catch(()=>{"
    "document.getElementById('ssid').innerHTML='<option>扫描失败，请手动输入</option>';"
    "})}"
    "function getDeviceId(){"
    "fetch('/info').then(r=>r.json()).then(d=>{"
    "document.getElementById('devId').textContent=d.device_id;"
    "}).catch(()=>{"
    "document.getElementById('devId').textContent='未知';"
    "})}"
    "document.getElementById('configForm').onsubmit=function(){"
    "document.getElementById('loading').style.display='block';"
    "document.getElementById('configForm').style.display='none';"
    "};"
    "window.onload=function(){scanWifi();getDeviceId()};"
    "</script></body></html>";

// WiFi扫描API
static esp_err_t scan_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    ESP_LOGI(TAG, "开始扫描WiFi网络...");
    
    // 启动WiFi扫描（阻塞模式）
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi扫描失败: %s", esp_err_to_name(ret));
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "扫描到 %d 个WiFi网络", ap_count);
    
    if (ap_count == 0) {
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    // 限制最多20个
    if (ap_count > 20) {
        ESP_LOGI(TAG, "WiFi数量过多，限制为20个");
        ap_count = 20;
    }
    
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        ESP_LOGE(TAG, "分配内存失败");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    // 按信号强度排序（冒泡排序）
    for (int i = 0; i < ap_count - 1; i++) {
        for (int j = 0; j < ap_count - i - 1; j++) {
            if (ap_list[j].rssi < ap_list[j + 1].rssi) {
                wifi_ap_record_t temp = ap_list[j];
                ap_list[j] = ap_list[j + 1];
                ap_list[j + 1] = temp;
            }
        }
    }
    
    // 构建JSON响应
    char *json = malloc(4096);
    if (!json) {
        ESP_LOGE(TAG, "分配JSON缓冲区失败");
        free(ap_list);
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    int len = snprintf(json, 4096, "[");
    for (int i = 0; i < ap_count; i++) {
        // 跳过空SSID
        if (strlen((char *)ap_list[i].ssid) == 0) {
            continue;
        }
        
        len += snprintf(json + len, 4096 - len, 
                       "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                       (len > 1) ? "," : "",  // 第一个不加逗号
                       ap_list[i].ssid,
                       ap_list[i].rssi);
        
        ESP_LOGI(TAG, "  WiFi: %s (RSSI: %d dBm)", ap_list[i].ssid, ap_list[i].rssi);
    }
    len += snprintf(json + len, 4096 - len, "]");
    
    httpd_resp_sendstr(req, json);
    ESP_LOGI(TAG, "WiFi扫描结果已发送");
    
    free(json);
    free(ap_list);
    
    return ESP_OK;
}

// 设备信息API
static esp_err_t info_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    char json[256];
    snprintf(json, sizeof(json), 
             "{\"device_id\":\"%s\",\"chip\":\"ESP32-C3\",\"version\":\"%s\"}",
             g_device_id, FIRMWARE_VERSION);
    
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

// 配网页面处理（主页面和Captive Portal）
static esp_err_t config_page_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, config_page_html, strlen(config_page_html));
    return ESP_OK;
}

// Captive Portal重定向处理
static esp_err_t captive_portal_handler(httpd_req_t *req) {
    // 重定向到配网页面
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t config_save_handler(httpd_req_t *req) {
    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // 简化的参数解析
    char ssid[33] = {0}, pass[65] = {0}, config_srv[256] = {0}, mqtt[129] = {0};
    
    // 解析POST数据 (简化版，实际应该更严谨)
    char *p = strstr(content, "ssid=");
    if (p) {
        sscanf(p, "ssid=%32[^&]", ssid);
    }
    p = strstr(content, "pass=");
    if (p) {
        sscanf(p, "pass=%64[^&]", pass);
    }
    p = strstr(content, "config_srv=");
    if (p) {
        sscanf(p, "config_srv=%255[^&]", config_srv);
    }
    p = strstr(content, "mqtt=");
    if (p) {
        sscanf(p, "mqtt=%128s", mqtt);
    }
    
    // 保存到NVS
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_str(nvs_handle, NVS_KEY_WIFI_SSID, ssid);
        nvs_set_str(nvs_handle, NVS_KEY_WIFI_PASS, pass);
        nvs_set_str(nvs_handle, NVS_KEY_CONFIG_SERVER, config_srv);
        nvs_set_str(nvs_handle, NVS_KEY_MQTT_BROKER, mqtt);
        nvs_set_u8(nvs_handle, NVS_KEY_CONFIG_DONE, 1);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        ESP_LOGI(TAG, "配置已保存: SSID=%s, ConfigServer=%s, MQTT=%s", ssid, config_srv, mqtt);
    }
    
    // 简洁的配置成功页面
    const char* response = 
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>配置成功</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;text-align:center;padding:50px;background:#f5f5f5}"
        ".container{background:white;padding:40px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:400px;margin:0 auto}"
        "h1{color:#4CAF50;font-size:32px;margin-bottom:20px}"
        "p{color:#666;font-size:16px;line-height:1.6}"
        ".info{background:#f0f0f0;padding:15px;border-radius:5px;margin:20px 0;text-align:left;font-size:14px}"
        ".info div{margin:8px 0}"
        "</style></head><body>"
        "<div class='container'>"
        "<h1>✅ 配置成功</h1>"
        "<p>您的设备配置已保存</p>"
        "<div class='info'>"
        "<div>WiFi: <strong>%s</strong></div>"
        "<div>配置服务器: <strong>%s</strong></div>"
        "<div>MQTT: <strong>%s</strong></div>"
        "</div>"
        "<p>设备将在3秒后重启...</p>"
        "</div></body></html>";
    
    // 构建响应
    char final_response[1536];  // 增加缓冲区大小以容纳更长的配置服务器地址
    snprintf(final_response, sizeof(final_response), response, ssid, config_srv, mqtt);
    
    httpd_resp_send(req, final_response, strlen(final_response));
    
    // 延迟重启
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

static httpd_handle_t start_config_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WIFI_CONFIG_WEB_PORT;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 16;  // 增加URI处理器数量
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // 主配网页面
        httpd_uri_t uri_root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = config_page_handler,
        };
        httpd_register_uri_handler(server, &uri_root);
        
        // 配置保存
        httpd_uri_t uri_save = {
            .uri       = "/save",
            .method    = HTTP_POST,
            .handler   = config_save_handler,
        };
        httpd_register_uri_handler(server, &uri_save);
        
        // WiFi扫描API
        httpd_uri_t uri_scan = {
            .uri       = "/scan",
            .method    = HTTP_GET,
            .handler   = scan_handler,
        };
        httpd_register_uri_handler(server, &uri_scan);
        
        // 设备信息API
        httpd_uri_t uri_info = {
            .uri       = "/info",
            .method    = HTTP_GET,
            .handler   = info_handler,
        };
        httpd_register_uri_handler(server, &uri_info);
        
        // Captive Portal - 捕获所有常见的检测URL
        const char* captive_urls[] = {
            "/generate_204",           // Android
            "/gen_204",                // Android
            "/hotspot-detect.html",    // iOS
            "/library/test/success.html", // iOS
            "/connecttest.txt",        // Windows
            "/redirect",               // 通用
            "/success.txt"            // 通用
        };
        
        for (int i = 0; i < sizeof(captive_urls) / sizeof(captive_urls[0]); i++) {
            httpd_uri_t uri_captive = {
                .uri       = captive_urls[i],
                .method    = HTTP_GET,
                .handler   = captive_portal_handler,
            };
            httpd_register_uri_handler(server, &uri_captive);
        }
        
        ESP_LOGI(TAG, "✅ 配网服务器启动: http://192.168.4.1");
        ESP_LOGI(TAG, "   支持自动跳转配网页面（Captive Portal）");
    }
    return server;
}

// ==================== WiFi初始化 ====================

static esp_err_t wifi_init_sta(const char *ssid, const char *password) {
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi初始化完成，等待连接...");
    
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t wifi_init_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建AP和STA网络接口（支持WiFi扫描）
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 生成AP SSID (使用MAC地址后3字节)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "%s%02X%02X%02X", 
             WIFI_CONFIG_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
    
    wifi_config_t wifi_config = {
        .ap = {
            .channel = WIFI_CONFIG_AP_CHANNEL,
            .max_connection = WIFI_CONFIG_AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    
    // 使用AP+STA模式（允许在AP模式下扫描WiFi）
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "✅ 配网AP启动: %s (AP+STA模式，支持WiFi扫描)", ap_ssid);
    return ESP_OK;
}

// ==================== MQTT初始化 ====================

static esp_err_t mqtt_init(const char *broker, const char *client_id) {
    char uri[256];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", broker, DEFAULT_MQTT_PORT);
    
    // 如果MQTT主题为空，使用UUID构建默认主题
    if (strlen(g_mqtt_topic_control) == 0) {
        snprintf(g_mqtt_topic_data, sizeof(g_mqtt_topic_data), 
                 "devices/%s/data", client_id);
        snprintf(g_mqtt_topic_control, sizeof(g_mqtt_topic_control), 
                 "devices/%s/control", client_id);
        snprintf(g_mqtt_topic_status, sizeof(g_mqtt_topic_status), 
                 "devices/%s/status", client_id);
        snprintf(g_mqtt_topic_heartbeat, sizeof(g_mqtt_topic_heartbeat), 
                 "devices/%s/heartbeat", client_id);
        
        ESP_LOGI(TAG, "使用默认MQTT主题格式");
        ESP_LOGI(TAG, "  数据: %s", g_mqtt_topic_data);
        ESP_LOGI(TAG, "  控制: %s", g_mqtt_topic_control);
        ESP_LOGI(TAG, "  状态: %s", g_mqtt_topic_status);
        ESP_LOGI(TAG, "  心跳: %s", g_mqtt_topic_heartbeat);
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = client_id,
        .credentials.username = DEFAULT_MQTT_USERNAME,
        .credentials.set_null_client_id = false,
        .session.keepalive = MQTT_KEEPALIVE_S,
        .network.reconnect_timeout_ms = MQTT_RETRY_INTERVAL_MS,
    };
    
    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_mqtt_client);
    
    ESP_LOGI(TAG, "MQTT客户端启动: %s (ClientID: %s)", uri, client_id);
    return ESP_OK;
}

// ==================== 系统监控任务 ====================

static void system_monitor_task(void *pvParameters) {
    uint32_t heartbeat_count = 0;
    
    while (1) {
        uint32_t uptime = (esp_timer_get_time() / 1000000) - g_system_start_time;
        uint32_t free_heap = esp_get_free_heap_size();
        
        ESP_LOGI(TAG, "=== 系统状态 ===");
        ESP_LOGI(TAG, "运行时间: %lu秒, 空闲内存: %lu字节", uptime, free_heap);
        ESP_LOGI(TAG, "WiFi: %s, MQTT: %s", 
                 g_wifi_connected ? "已连接" : "未连接",
                 g_mqtt_connected ? "已连接" : "未连接");
        
        // 读取传感器（尝试3次）
        #if DHT11_ENABLED
        {
            dht11_data_t dht_data;
            bool read_success = false;
            
            // 尝试读取3次
            for (int retry = 0; retry < 3; retry++) {
                if (dht11_read(&dht_data) == ESP_OK && dht_data.valid) {
                    float temp = dht_data.temperature;
                    float humi = dht_data.humidity;
                    g_sensor_data.temperature = temp;
                    g_sensor_data.humidity = humi;
                    g_sensor_data.valid = true;
                    read_success = true;
                    ESP_LOGI(TAG, "DHT11: 温度=%.1f°C, 湿度=%.1f%%", temp, humi);
                    
                    // 温度异常提示
                    if (temp > 40.0f) {
                        ESP_LOGW(TAG, "⚠️  温度异常高(%.1f°C)！可能原因：传感器靠近发热源、读取错误或传感器故障", temp);
                    } else if (temp < 0.0f || temp > 80.0f) {
                        ESP_LOGW(TAG, "⚠️  温度超出正常范围(%.1f°C)", temp);
                    }
                    
                    // 更新OLED显示
                    #if OLED_ENABLED
                    if (g_wifi_connected) {
                        oled_show_status_screen(
                            "MyWiFi",  // TODO: 从NVS读取实际SSID
                            g_wifi_connected,
                            g_mqtt_connected,
                            temp,
                            humi,
                            g_ip_address
                        );
                    }
                    #endif
                    
                    // 上报传感器数据
                    if (g_mqtt_connected) {
                        char payload[256];
                        snprintf(payload, sizeof(payload),
                            "{\"device_id\":\"%s\",\"sensor\":\"DHT11\","
                            "\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":%lu}",
                            g_device_id, temp, humi, uptime);
                        esp_mqtt_client_publish(g_mqtt_client, g_mqtt_topic_data, 
                                              payload, 0, MQTT_QOS_DEFAULT, 0);
                    }
                    break;  // 读取成功，跳出重试循环
                } else {
                    if (retry < 2) {
                        ESP_LOGW(TAG, "⚠️  DHT11读取失败（第%d次尝试），2秒后重试...", retry + 1);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                }
            }
            
            if (!read_success) {
                ESP_LOGE(TAG, "❌ DHT11连续3次读取失败");
                g_sensor_data.valid = false;
            }
        }
        #endif
        
        // 发送心跳
        if (g_mqtt_connected && (uptime % MQTT_HEARTBEAT_INTERVAL_S == 0)) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                "{\"sequence\":%lu,\"timestamp\":%llu,\"status\":1}",
                ++heartbeat_count, esp_timer_get_time() / 1000);
            esp_mqtt_client_publish(g_mqtt_client, g_mqtt_topic_heartbeat, 
                                  payload, 0, 1, 0);
            ESP_LOGI(TAG, "💓 心跳 #%lu", heartbeat_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(SYSTEM_MONITOR_INTERVAL_MS));
    }
}

// ==================== 主函数 ====================

void app_main(void) {
    ESP_LOGI(TAG, "=== AIOT ESP32-C3 Lite v%s ===", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "芯片: %s, Flash: %dMB", CHIP_MODEL, FLASH_SIZE_MB);
    
    g_system_start_time = esp_timer_get_time() / 1000000;
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化GPIO
    gpio_config_t io_conf = {};
    
    // LED1 (红色)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED1_GPIO_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    led_red_control(0);  // 默认熄灭
    
    // LED2 (蓝色)
    io_conf.pin_bit_mask = (1ULL << LED2_GPIO_PIN);
    gpio_config(&io_conf);
    led_blue_control(0);  // 默认熄灭
    
    // 继电器（如果有）
    #if RELAY_COUNT > 0 && RELAY1_GPIO_PIN >= 0
    io_conf.pin_bit_mask = (1ULL << RELAY1_GPIO_PIN);
    gpio_config(&io_conf);
    #endif
    
    // Boot按键
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    // 生成设备ID (使用MAC地址)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(g_device_id, sizeof(g_device_id), "C3-LITE-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(g_device_uuid, sizeof(g_device_uuid), "%s", g_device_id);
    
    // 构建MQTT主题
    snprintf(g_mqtt_topic_data, sizeof(g_mqtt_topic_data), 
             "%s/%s/%s", MQTT_TOPIC_PREFIX, g_device_uuid, MQTT_TOPIC_DATA);
    snprintf(g_mqtt_topic_status, sizeof(g_mqtt_topic_status), 
             "%s/%s/%s", MQTT_TOPIC_PREFIX, g_device_uuid, MQTT_TOPIC_STATUS);
    snprintf(g_mqtt_topic_heartbeat, sizeof(g_mqtt_topic_heartbeat), 
             "%s/%s/%s", MQTT_TOPIC_PREFIX, g_device_uuid, MQTT_TOPIC_HEARTBEAT);
    snprintf(g_mqtt_topic_control, sizeof(g_mqtt_topic_control), 
             "%s/%s/%s", MQTT_TOPIC_PREFIX, g_device_uuid, MQTT_TOPIC_CONTROL);
    
    ESP_LOGI(TAG, "设备ID: %s", g_device_id);
    
    // 初始化OLED
    #if OLED_ENABLED
    ESP_LOGI(TAG, "初始化OLED显示...");
    if (oled_init() == ESP_OK) {
        oled_clear();  // 初始化后清屏
        ESP_LOGI(TAG, "✅ OLED显示已就绪");
    } else {
        ESP_LOGE(TAG, "❌ OLED初始化失败");
    }
    #endif
    
    // ==================== 启动倒计时（Boot键检测）====================
    ESP_LOGI(TAG, "=== 启动倒计时：3秒内按Boot键可进入配网模式 ===");
    bool enter_config_mode = false;
    
    for (int countdown = 3; countdown > 0; countdown--) {
        // OLED显示倒计时
        #if OLED_ENABLED
        oled_clear();
        vTaskDelay(pdMS_TO_TICKS(50));
        
        char buf[16];
        oled_show_line(2, "[BOOT]", OLED_ALIGN_CENTER);
        snprintf(buf, sizeof(buf), "%d", countdown);
        oled_show_line(5, buf, OLED_ALIGN_CENTER);
        oled_refresh();
        #endif
        
        ESP_LOGI(TAG, "倒计时: %d 秒...", countdown);
        
        // 检测Boot按键（每100ms检测一次）
        for (int i = 0; i < 10; i++) {
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                // Boot键被按下
                ESP_LOGI(TAG, "🔧 检测到Boot键按下！");
                enter_config_mode = true;
                
                // OLED显示确认信息
                #if OLED_ENABLED
                oled_clear();
                vTaskDelay(pdMS_TO_TICKS(50));
                oled_show_line(2, "Config Mode", OLED_ALIGN_CENTER);
                oled_show_line(4, "Wait...", OLED_ALIGN_CENTER);
                oled_refresh();
                vTaskDelay(pdMS_TO_TICKS(1000));
                #endif
                
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        if (enter_config_mode) {
            break;
        }
    }
    
    // 如果按下了Boot键，清除配置
    if (enter_config_mode) {
        ESP_LOGI(TAG, "清除现有配置，准备进入配网模式...");
        
        // 清除NVS配置
        nvs_handle_t nvs_handle;
        if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_erase_key(nvs_handle, NVS_KEY_CONFIG_DONE);
            nvs_erase_key(nvs_handle, NVS_KEY_WIFI_SSID);
            nvs_erase_key(nvs_handle, NVS_KEY_WIFI_PASS);
            nvs_erase_key(nvs_handle, NVS_KEY_CONFIG_SERVER);
            nvs_erase_key(nvs_handle, NVS_KEY_MQTT_BROKER);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "✅ 配置已清除");
        }
        
        // LED闪烁提示（快速闪烁3次）
        for (int i = 0; i < 3; i++) {
            led_blue_control(1);
            vTaskDelay(pdMS_TO_TICKS(200));
            led_blue_control(0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    } else {
        ESP_LOGI(TAG, "未按Boot键，继续正常启动");
        
        // OLED显示启动中
        #if OLED_ENABLED
        oled_clear();
        oled_show_line(3, "Starting", OLED_ALIGN_CENTER);
        oled_refresh();
        vTaskDelay(pdMS_TO_TICKS(500));
        #endif
    }
    
    // 清屏准备后续显示
    #if OLED_ENABLED
    oled_clear();
    vTaskDelay(pdMS_TO_TICKS(100));
    #endif
    
    // 初始化DHT11
    #if DHT11_ENABLED
    ESP_LOGI(TAG, "初始化DHT11传感器...");
    if (dht11_init(DHT11_GPIO_PIN) == ESP_OK) {
        ESP_LOGI(TAG, "✅ DHT11传感器已就绪");
    } else {
        ESP_LOGE(TAG, "❌ DHT11初始化失败");
    }
    #endif
    
    // ==================== 开机启动测试流程 ====================
    ESP_LOGI(TAG, "=== 开机测试开始 ===");
    
    // GPIO测试（帮助诊断DHT11问题）
    #if DHT11_ENABLED
    ESP_LOGI(TAG, "--- GPIO6电平测试 ---");
    dht11_gpio_test();
    ESP_LOGI(TAG, "--- GPIO6测试完成 ---");
    vTaskDelay(pdMS_TO_TICKS(1000));
    #endif
    
    // 读取并显示温湿度
    #if DHT11_ENABLED && OLED_ENABLED
    {
        dht11_data_t dht_data;
        float temp = 0.0, humi = 0.0;
        bool read_success = false;
        
        // 等待传感器稳定（DHT11需要等待）
        ESP_LOGI(TAG, "等待DHT11传感器稳定（2秒）...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 尝试读取3次
        ESP_LOGI(TAG, "读取DHT11传感器数据...");
        for (int retry = 0; retry < 3; retry++) {
            if (dht11_read(&dht_data) == ESP_OK && dht_data.valid) {
                temp = dht_data.temperature;
                humi = dht_data.humidity;
                read_success = true;
                ESP_LOGI(TAG, "✅ DHT11读取成功（第%d次尝试）: 温度=%.1f°C, 湿度=%.1f%%", 
                         retry + 1, temp, humi);
                break;
            } else {
                ESP_LOGW(TAG, "⚠️  DHT11读取失败（第%d次尝试）", retry + 1);
                if (retry < 2) {
                    vTaskDelay(pdMS_TO_TICKS(2000));  // 等待2秒后重试
                }
            }
        }
        
        if (!read_success) {
            ESP_LOGE(TAG, "❌ DHT11连续3次读取失败，使用默认值");
            temp = 25.0;
            humi = 60.0;
        }
        
        // 点亮LED（显示温湿度期间）
        ESP_LOGI(TAG, "点亮LED并显示温湿度...");
        led_red_control(1);
        led_blue_control(1);
        
        // OLED显示温湿度
        #if OLED_ENABLED
        oled_display_safe();
        char buf[20];
        
        snprintf(buf, sizeof(buf), "T:%.1fC", temp);
        oled_show_line(3, buf, OLED_ALIGN_CENTER);
        
        snprintf(buf, sizeof(buf), "H:%.1f%%", humi);
        oled_show_line(4, buf, OLED_ALIGN_CENTER);
        
        oled_refresh();
        #endif
        
        // 显示温湿度3秒
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // 熄灭LED
        led_red_control(0);
        led_blue_control(0);
        ESP_LOGI(TAG, "✅ 温湿度显示完成，LED已熄灭");
    }
    #endif
    
    ESP_LOGI(TAG, "=== 开机自检完成 ===");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 检查是否已配置或需要进入配网模式
    nvs_handle_t nvs_handle;
    uint8_t config_done = 0;
    
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        nvs_get_u8(nvs_handle, NVS_KEY_CONFIG_DONE, &config_done);
        nvs_close(nvs_handle);
    }
    
    // 如果倒计时时按了Boot键或首次启动，进入配网模式
    if (!config_done || enter_config_mode) {
        // 进入配网模式
        ESP_LOGI(TAG, "🔧 进入配网模式");
        g_config_mode = true;
        // led_blue_control(1); // 已禁用：配网模式不自动点亮LED
        
        // OLED显示配网提示
        #if OLED_ENABLED
        char ap_ssid[32];
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(ap_ssid, sizeof(ap_ssid), "AIOT-C3-%02X%02X%02X",
                 mac[3], mac[4], mac[5]);
        oled_show_config_mode(ap_ssid);
        #endif
        
        wifi_init_ap();
        start_dns_server();     // 启动DNS服务器（Captive Portal）
        start_config_server();  // 启动Web配网服务器
        
        ESP_LOGI(TAG, "📱 请连接WiFi热点，系统会自动弹出配网页面");
        ESP_LOGI(TAG, "   或手动访问: http://192.168.4.1");
        
        // 配网模式下无限等待
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // 正常模式：读取配置并连接
    char ssid[33] = {0}, pass[65] = {0}, config_server[256] = {0}, mqtt_broker[129] = {0};
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        size_t len;
        len = sizeof(ssid);
        nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, ssid, &len);
        len = sizeof(pass);
        nvs_get_str(nvs_handle, NVS_KEY_WIFI_PASS, pass, &len);
        len = sizeof(config_server);
        if (nvs_get_str(nvs_handle, NVS_KEY_CONFIG_SERVER, config_server, &len) != ESP_OK) {
            strcpy(config_server, DEFAULT_CONFIG_SERVER);
        }
        len = sizeof(mqtt_broker);
        if (nvs_get_str(nvs_handle, NVS_KEY_MQTT_BROKER, mqtt_broker, &len) != ESP_OK) {
            strcpy(mqtt_broker, DEFAULT_MQTT_BROKER);
        }
        nvs_close(nvs_handle);
    }
    
    // 连接WiFi
    ESP_LOGI(TAG, "连接WiFi: %s", ssid);
    if (wifi_init_sta(ssid, pass) == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi连接成功");
        
        // 获取IP地址
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                snprintf(g_ip_address, sizeof(g_ip_address), IPSTR, IP2STR(&ip_info.ip));
                ESP_LOGI(TAG, "IP地址: %s", g_ip_address);
            }
        }
        
        // WiFi初始化后重新配置DHT11 GPIO（WiFi可能改变GPIO配置）
        #if DHT11_ENABLED
        ESP_LOGI(TAG, "WiFi连接成功，重新配置DHT11 GPIO...");
        esp_err_t dht11_ret = dht11_reinit_after_wifi();
        if (dht11_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ DHT11 GPIO重新配置成功");
        } else {
            ESP_LOGW(TAG, "⚠️ DHT11 GPIO重新配置失败: %s", esp_err_to_name(dht11_ret));
        }
        #endif
        
        // ==================== 获取设备配置 ====================
        ESP_LOGI(TAG, "=== 获取设备配置 ===");
        
        #if OLED_ENABLED
        oled_display_safe();
        oled_show_line(2, "Get Config", OLED_ALIGN_CENTER);
        oled_show_line(4, "Wait...", OLED_ALIGN_CENTER);
        oled_refresh();
        #endif
        
        esp_err_t config_ret = device_config_get_from_server(
            config_server,  // 使用配置服务器地址（端口8001）
            PRODUCT_ID,
            FIRMWARE_VERSION,
            &g_device_config
        );
        
        if (config_ret == ESP_OK) {
            // 设备已注册，使用服务器返回的配置
            ESP_LOGI(TAG, "✅ 设备已注册");
            g_device_registered = true;
            
            // 更新设备ID和UUID
            strncpy(g_device_id, g_device_config.device_id, sizeof(g_device_id) - 1);
            strncpy(g_device_uuid, g_device_config.device_uuid, sizeof(g_device_uuid) - 1);
            
            // 使用服务器返回的MQTT主题，如果服务器没提供则用UUID构建
            if (strlen(g_device_config.mqtt_topic_data) > 0) {
                strncpy(g_mqtt_topic_data, g_device_config.mqtt_topic_data, sizeof(g_mqtt_topic_data) - 1);
            } else {
                snprintf(g_mqtt_topic_data, sizeof(g_mqtt_topic_data), "devices/%s/data", g_device_uuid);
            }
            
            if (strlen(g_device_config.mqtt_topic_control) > 0) {
                strncpy(g_mqtt_topic_control, g_device_config.mqtt_topic_control, sizeof(g_mqtt_topic_control) - 1);
            } else {
                snprintf(g_mqtt_topic_control, sizeof(g_mqtt_topic_control), "devices/%s/control", g_device_uuid);
            }
            
            if (strlen(g_device_config.mqtt_topic_status) > 0) {
                strncpy(g_mqtt_topic_status, g_device_config.mqtt_topic_status, sizeof(g_mqtt_topic_status) - 1);
            } else {
                snprintf(g_mqtt_topic_status, sizeof(g_mqtt_topic_status), "devices/%s/status", g_device_uuid);
            }
            
            if (strlen(g_device_config.mqtt_topic_heartbeat) > 0) {
                strncpy(g_mqtt_topic_heartbeat, g_device_config.mqtt_topic_heartbeat, sizeof(g_mqtt_topic_heartbeat) - 1);
            } else {
                snprintf(g_mqtt_topic_heartbeat, sizeof(g_mqtt_topic_heartbeat), "devices/%s/heartbeat", g_device_uuid);
            }
            
            ESP_LOGI(TAG, "Device UUID: %s", g_device_uuid);
            ESP_LOGI(TAG, "MQTT主题:");
            ESP_LOGI(TAG, "  数据: %s", g_mqtt_topic_data);
            ESP_LOGI(TAG, "  控制: %s", g_mqtt_topic_control);
            ESP_LOGI(TAG, "  状态: %s", g_mqtt_topic_status);
            ESP_LOGI(TAG, "  心跳: %s", g_mqtt_topic_heartbeat);
            
            // 启动MQTT（使用device_uuid作为client_id）
            mqtt_init(mqtt_broker, g_device_uuid);
            
            // LED闪烁表示运行正常
            for (int i = 0; i < 3; i++) {
                led_red_control(1);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_red_control(0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            
        } else if (config_ret == ESP_ERR_NOT_FOUND) {
            // 设备未注册（404）
            ESP_LOGW(TAG, "⚠️  设备未注册");
            ESP_LOGW(TAG, "   请先在管理页面注册设备");
            
            // 获取MAC地址用于显示
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            
            ESP_LOGW(TAG, "   MAC地址: %s", mac_str);
            
            // OLED显示未注册提示
            #if OLED_ENABLED
            oled_display_safe();
            oled_show_line(1, "Not Register", OLED_ALIGN_CENTER);
            oled_show_line(3, mac_str, OLED_ALIGN_CENTER);
            oled_show_line(5, "Pls Register", OLED_ALIGN_CENTER);
            oled_refresh();
            #endif
            
            // LED蓝灯慢速闪烁表示未注册
            g_device_registered = false;
            
        } else {
            // 配置获取失败
            ESP_LOGE(TAG, "❌ 配置获取失败");
            
            #if OLED_ENABLED
            oled_display_safe();
            oled_show_line(3, "Config Fail", OLED_ALIGN_CENTER);
            oled_show_line(5, "Chk Server", OLED_ALIGN_CENTER);
            oled_refresh();
            #endif
            
            g_device_registered = false;
        }
        
    } else {
        ESP_LOGE(TAG, "❌ WiFi连接失败");
        g_device_registered = false;
    }
    
    // 启动系统监控任务
    xTaskCreate(system_monitor_task, "monitor", TASK_STACK_SIZE_MEDIUM, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "=== 系统启动完成 ===");
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

