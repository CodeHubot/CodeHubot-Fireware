/**
 * @file wifi_config.c
 * @brief WiFi配网模块实现
 */

#include "wifi_config.h"
#include "server/server_config.h"  // 用于保存服务器地址
#include "captive_portal.h"  // Captive Portal功能（学习xiaozhi-esp32架构）
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "wifi_config";

// 配网状态
static wifi_config_state_t s_config_state = WIFI_CONFIG_STATE_IDLE;
static wifi_config_event_cb_t s_event_cb = NULL;
static httpd_handle_t s_server = NULL;
static esp_netif_t *s_ap_netif = NULL;

// 配网AP配置
#define CONFIG_AP_SSID_PREFIX "AIOT-Config-"
#define CONFIG_AP_PASSWORD ""  // 开放热点
#define CONFIG_AP_CHANNEL 1
#define CONFIG_AP_MAX_CONNECTIONS 4
#define CONFIG_WEB_PORT 80

// NVS存储键
#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_FORCE_CONFIG "force_config"
#define NVS_KEY_WIFI_SSID "wifi_ssid"
#define NVS_KEY_WIFI_PASS "wifi_pass"
#define NVS_KEY_CONFIGURED "configured"
// 注意：服务器地址统一使用server_config命名空间中的base_address，不再单独存储

// 全局配置数据
static char s_ap_ssid[32] = {0};

/**
 * @brief HTML属性值转义函数（转义引号、&等特殊字符）
 */
static void html_escape_attribute(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        if (output && output_size > 0) {
            output[0] = '\0';
        }
        return;
    }
    
    size_t i = 0, j = 0;
    while (input[i] != '\0' && j < output_size - 1) {
        switch (input[i]) {
            case '"':
                if (j + 6 < output_size) {  // &quot; 需要6个字符
                    strcpy(&output[j], "&quot;");
                    j += 6;
                }
                break;
            case '\'':
                if (j + 6 < output_size) {  // &#39; 需要6个字符
                    strcpy(&output[j], "&#39;");
                    j += 6;
                }
                break;
            case '&':
                if (j + 5 < output_size) {  // &amp; 需要5个字符
                    strcpy(&output[j], "&amp;");
                    j += 5;
                }
                break;
            case '<':
                if (j + 4 < output_size) {  // &lt; 需要4个字符
                    strcpy(&output[j], "&lt;");
                    j += 4;
                }
                break;
            case '>':
                if (j + 4 < output_size) {  // &gt; 需要4个字符
                    strcpy(&output[j], "&gt;");
                    j += 4;
                }
                break;
            default:
                output[j++] = input[i];
                break;
        }
        i++;
    }
    output[j] = '\0';
}

// 前向声明
static esp_err_t config_get_handler(httpd_req_t *req);
static esp_err_t config_post_handler(httpd_req_t *req);
static esp_err_t config_current_handler(httpd_req_t *req);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * @brief 去除字符串前后空格
 */
static void trim_string(char *str) {
    if (!str || *str == '\0') return;
    
    // 去除前导空格
    char *start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')) {
        start++;
    }
    
    // 去除尾部空格
    char *end = str + strlen(str) - 1;
    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    
    // 移动字符串到开始位置
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * @brief 安全的字符串复制（确保null终止）
 */
static void safe_strncpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';  // 强制null终止
}

/**
 * @brief URL解码函数（改进版，不将+转换为空格，保持原样）
 * 
 * 注意：对于WiFi密码，+号应该保持为+号，而不是转换为空格
 * 只有在表单字段值中，+才代表空格（application/x-www-form-urlencoded）
 * 但对于密码字段，我们应该保持原始值
 */
static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            // ⚠️ 对于密码字段，+应该保持为+，不转换为空格
            // 如果WiFi密码真的包含+号，用户应该输入+号
            *dst++ = ' ';  // 表单数据中+代表空格
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * @brief URL解码函数（密码专用，保持+号原样）
 */
static void url_decode_password(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else {
            // 密码字段：所有字符保持原样（包括+）
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * @brief 从URL编码的表单数据中提取参数
 * 
 * @param data 表单数据
 * @param param_name 参数名
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @param is_password 是否为密码字段（密码字段保持+号原样）
 */
static char* get_form_param_ex(const char *data, const char *param_name, char *output, size_t output_size, bool is_password) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "%s=", param_name);
    
    const char *start = strstr(data, search_key);
    if (!start) {
        output[0] = '\0';
        return NULL;
    }
    
    start += strlen(search_key);
    const char *end = strchr(start, '&');
    
    size_t len;
    if (end) {
        len = end - start;
    } else {
        len = strlen(start);
    }
    
    if (len >= output_size) {
        len = output_size - 1;
    }
    
    char encoded[256];
    if (len >= sizeof(encoded)) {
        len = sizeof(encoded) - 1;
    }
    strncpy(encoded, start, len);
    encoded[len] = '\0';
    
    // 根据字段类型选择解码方式
    if (is_password) {
        url_decode_password(output, encoded);  // 密码：保持+号原样
    } else {
        url_decode(output, encoded);  // 普通字段：+转空格
    }
    
    return output;
}

/**
 * @brief 从URL编码的表单数据中提取参数（普通字段）
 */
static char* get_form_param(const char *data, const char *param_name, char *output, size_t output_size) {
    return get_form_param_ex(data, param_name, output, output_size, false);
}

// HTTP服务器配置
static const httpd_uri_t config_get = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = config_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t config_post = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t config_current = {
    .uri       = "/config/current",
    .method    = HTTP_GET,
    .handler   = config_current_handler,
    .user_ctx  = NULL
};

/**
 * @brief 生成AP模式SSID
 * 
 * 使用Station MAC地址（设备真实MAC地址）而不是AP MAC地址
 * 因为AP MAC = STA MAC + 1，会导致热点名称与真实MAC不一致
 */
static void generate_ap_ssid(void) {
    uint8_t mac[6];
    // 使用Station MAC地址，确保热点名称与设备真实MAC地址一致
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s%02X%02X%02X", 
             CONFIG_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
}

/**
 * @brief 触发事件回调
 */
static void trigger_event(wifi_config_event_t event, void *data) {
    if (s_event_cb) {
        s_event_cb(event, data);
    }
}

/**
 * @brief WiFi事件处理器
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP模式启动成功");
                s_config_state = WIFI_CONFIG_STATE_AP_STARTED;
                trigger_event(WIFI_CONFIG_EVENT_AP_STARTED, NULL);
                break;
                
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "客户端连接到AP");
                trigger_event(WIFI_CONFIG_EVENT_CLIENT_CONNECTED, NULL);
                break;
                
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA模式启动");
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi连接成功");
                s_config_state = WIFI_CONFIG_STATE_CONNECTED;
                trigger_event(WIFI_CONFIG_EVENT_WIFI_CONNECTED, NULL);
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi连接断开");
                s_config_state = WIFI_CONFIG_STATE_FAILED;
                trigger_event(WIFI_CONFIG_EVENT_WIFI_FAILED, NULL);
                break;
                
            default:
                break;
        }
    }
}

/**
 * @brief 启动HTTP服务器
 */
static esp_err_t start_webserver(void) {
    // 如果HTTP服务器已经在运行，直接返回成功
    if (s_server != NULL) {
        ESP_LOGI(TAG, "HTTP服务器已在运行，跳过启动");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_WEB_PORT;
    config.max_uri_handlers = 16;  // 增加处理器数量以支持Captive Portal
    
    // 优化连接管理，防止文件描述符耗尽
    config.max_open_sockets = 7;      // 最大同时打开的socket数
    config.lru_purge_enable = true;   // 启用LRU清除，自动关闭最久未使用的连接
    config.close_fn = NULL;           // 使用默认关闭函数
    config.recv_wait_timeout = 5;     // 接收超时5秒
    config.send_wait_timeout = 5;     // 发送超时5秒
    
    ESP_LOGI(TAG, "启动HTTP服务器，端口: %d (max_sockets: %d, lru_purge: %s)", 
             config.server_port, config.max_open_sockets, 
             config.lru_purge_enable ? "enabled" : "disabled");
    
    if (httpd_start(&s_server, &config) == ESP_OK) {
        // 注册配网页面处理器
        httpd_register_uri_handler(s_server, &config_get);
        httpd_register_uri_handler(s_server, &config_post);
        httpd_register_uri_handler(s_server, &config_current);
        
        // 注册Captive Portal处理器（必须在配网处理器之后，学习xiaozhi-esp32架构）
        esp_err_t ret = captive_portal_register_handlers(s_server);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "注册Captive Portal处理器失败: %s", esp_err_to_name(ret));
        }
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "启动HTTP服务器失败");
    return ESP_FAIL;
}

/**
 * @brief 停止HTTP服务器
 */
static void stop_webserver(void) {
    // 停止Captive Portal DNS服务器（学习xiaozhi-esp32架构）
    captive_portal_dns_stop();
    
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP服务器已停止");
    }
}

/**
 * @brief 获取当前配置（简化版，返回空值）
 */
static esp_err_t config_current_handler(httpd_req_t *req) {
    // 简化版，不读取NVS配置，直接返回空值
    // 这样可以避免在HTTP请求处理中访问NVS导致的内存访问问题
    const char *json_response = "{\"ssid\":\"\",\"password\":\"\",\"server_address\":\"\"}";
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief 配置页面GET处理器（简化版，不读取NVS配置）
 */
static esp_err_t config_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "📱 收到配网页面请求: %s", req->uri);
    
    // 获取Host头
    char host_header[100] = {0};
    size_t host_len = sizeof(host_header);
    if (httpd_req_get_hdr_value_str(req, "Host", host_header, host_len) == ESP_OK) {
        ESP_LOGI(TAG, "   Host: %s", host_header);
        
        // 如果Host不是192.168.4.1，重定向到配网页面（Captive Portal标准行为）
        // 这会让iOS/Android/Windows自动弹出配网页面
        if (strcmp(host_header, "192.168.4.1") != 0) {
            ESP_LOGI(TAG, "   🔄 重定向到配网页面（Host: %s）", host_header);
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }
    
    ESP_LOGI(TAG, "   ✅ 显示配网页面");
    
    // 尝试读取已保存的服务器地址（如果存在则自动填充，否则使用默认值）
    char saved_server_address[128] = "";  // 默认配网服务器地址（用户需在配网时输入）
    unified_server_config_t srv_config = {0};
    if (server_config_load_from_nvs(&srv_config) == ESP_OK && strlen(srv_config.base_address) > 0) {
        strncpy(saved_server_address, srv_config.base_address, sizeof(saved_server_address) - 1);
        ESP_LOGI(TAG, "   📋 读取到已保存的服务器地址: %s", saved_server_address);
    } else {
        ESP_LOGI(TAG, "   ℹ️  未找到已保存的服务器地址，使用默认值: %s", saved_server_address);
    }
    
    // HTML页面模板（将在后面动态插入服务器地址）
    const char *html_template =
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>AIOT设备配网</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5}"
        ".container{max-width:500px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
        "h1{text-align:center;color:#333;margin-bottom:30px}"
        ".form-group{margin-bottom:20px}"
        "label{display:block;margin-bottom:5px;color:#555;font-weight:bold}"
        "input[type=text],input[type=password]{width:100%%;padding:10px;border:1px solid #ddd;border-radius:5px;font-size:16px;box-sizing:border-box}"
        "input[type=text]:focus,input[type=password]:focus{border-color:#007bff;outline:none}"
        "button{width:100%%;padding:12px;background:#007bff;color:white;border:none;border-radius:5px;font-size:16px;cursor:pointer}"
        "button:hover{background:#0056b3}"
        ".status{margin-top:20px;padding:10px;border-radius:5px;text-align:center}"
        ".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}"
        ".info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb}"
        "</style>"
        "</head><body>"
        "<div class='container'>"
        "<h1>AIOT Device Configuration</h1>"
        "<form id='configForm' method='POST' action='/config'>"
        "<div class='form-group'>"
        "<label for='ssid'>WiFi Name (SSID):</label>"
        "<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name'>"
        "</div>"
        "<div class='form-group'>"
        "<label for='password'>WiFi Password:</label>"
        "<input type='password' id='password' name='password' placeholder='Enter WiFi password (optional)'>"
        "</div>"
        "<div class='form-group'>"
        "<label for='server_address'>Server Address:</label>"
        "<input type='text' id='server_address' name='server_address' value='%s' placeholder='http://192.168.1.100 or https://demo.aiot.com' required>"
        "</div>"
        "<button type='submit'>Save Configuration</button>"
        "</form>"
        "<div id='status'></div>"
        "</div>"
        "<script>"
        "console.log('🔧 配网页面JavaScript开始执行');"
        ""
        "// 页面加载完成后执行"
        "window.addEventListener('DOMContentLoaded', function() {"
        "console.log('📄 DOM加载完成');"
        ""
        "// 自动填充已保存的服务器地址"
        "var savedServerAddress = '%s';"
        "console.log('📋 准备填充服务器地址:', savedServerAddress);"
        "var serverInput = document.getElementById('server_address');"
        "if (serverInput && savedServerAddress && savedServerAddress.length > 0) {"
        "serverInput.value = savedServerAddress;"
        "console.log('✅ 服务器地址已自动填充');"
        "} else {"
        "console.log('⚠️ 服务器地址为空或输入框未找到');"
        "}"
        ""
        "// 绑定表单提交事件"
        "var form = document.getElementById('configForm');"
        "if (!form) {"
        "console.error('❌ 表单元素未找到！');"
        "return;"
        "}"
        "console.log('✅ 表单元素已找到，绑定提交事件');"
        ""
        "form.addEventListener('submit', function(e) {"
        "console.log('📝 表单提交事件触发');"
        "e.preventDefault();"
        "console.log('✅ 已阻止默认提交行为');"
        ""
        "var formData = new FormData(e.target);"
        "var data = {};"
        "formData.forEach(function(value, key) {"
        "data[key] = value;"
        "console.log('  - ' + key + ':', value);"
        "});"
        ""
        "document.getElementById('status').innerHTML = '<div class=\"info\">Saving configuration...</div>';"
        "console.log('📤 发送POST请求到 /config');"
        ""
        "fetch('/config', {"
        "method: 'POST',"
        "headers: {'Content-Type': 'application/json'},"
        "body: JSON.stringify(data)"
        "})"
        ".then(function(response) {"
        "console.log('📥 收到响应，状态:', response.status);"
        "return response.json();"
        "})"
        ".then(function(data) {"
        "console.log('📦 响应数据:', data);"
        "if(data.success) {"
        "console.log('✅ 配置保存成功');"
        "document.getElementById('status').innerHTML = '<div class=\"success\">Configuration saved! Device will restart and connect to WiFi...</div>';"
        "} else {"
        "console.error('❌ 配置保存失败:', data.message);"
        "document.getElementById('status').innerHTML = '<div class=\"error\">Failed to save: ' + (data.message || 'Unknown error') + '</div>';"
        "}"
        "})"
        ".catch(function(error) {"
        "console.error('❌ 网络错误:', error);"
        "document.getElementById('status').innerHTML = '<div class=\"error\">Network error: ' + error.message + '</div>';"
        "});"
        ""
        "return false;"
        "}, false);"
        ""
        "console.log('✅ 表单提交事件已绑定');"
        "});"
        ""
        "console.log('🔧 配网页面JavaScript执行完成');"
        "</script>"
        "</body></html>";
    
    // 分配缓冲区并生成完整HTML（插入服务器地址）
    // 注意：HTML模板中有2个%s占位符（HTML value属性和JavaScript变量）
    size_t html_size = strlen(html_template) + strlen(saved_server_address) * 2 + 200;
    char *html_response = malloc(html_size);
    if (!html_response) {
        ESP_LOGE(TAG, "分配HTML响应缓冲区失败");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 使用snprintf插入服务器地址（两个%s都替换成相同的地址）
    int written = snprintf(html_response, html_size, html_template, 
                          saved_server_address,  // 第1个%s: HTML value属性
                          saved_server_address); // 第2个%s: JavaScript变量
    
    ESP_LOGI(TAG, "   📤 发送配网页面，已插入服务器地址: '%s' (写入%d字节)", 
             saved_server_address, written);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    
    free(html_response);
    return ESP_OK;
}

/**
 * @brief 配置POST处理器
 */
static esp_err_t config_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "📝 收到配网信息提交请求");
    ESP_LOGI(TAG, "   Content-Length: %d", req->content_len);
    
    char buf[512];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        ESP_LOGE(TAG, "❌ 内容过长: %d字节 (最大: %d字节)", remaining, sizeof(buf));
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    // 读取POST数据
    ESP_LOGI(TAG, "   正在读取POST数据...");
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        ESP_LOGE(TAG, "❌ 读取POST数据失败: %d", ret);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "   ✅ 读取到%d字节数据", ret);
    ESP_LOGI(TAG, "   收到配置数据: %s", buf);
    
    // 定义配置变量
    char ssid_str[64] = {0};
    char password_str[64] = {0};
    char server_address_str[128] = {0};
    
    // 尝试判断数据格式（JSON或URL编码的表单数据）
    bool is_json = (buf[0] == '{');
    cJSON *json = NULL;
    
    if (is_json) {
        // 解析JSON数据
        ESP_LOGI(TAG, "   检测到JSON格式，正在解析...");
        json = cJSON_Parse(buf);
        if (!json) {
            ESP_LOGE(TAG, "❌ JSON解析失败");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "   ✅ JSON解析成功");
        
        // 从JSON提取参数
        cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
        cJSON *password = cJSON_GetObjectItem(json, "password");
        cJSON *server_address = cJSON_GetObjectItem(json, "server_address");
        
        if (!ssid || !cJSON_IsString(ssid) || strlen(ssid->valuestring) == 0) {
            ESP_LOGE(TAG, "❌ SSID缺失或无效");
            cJSON_Delete(json);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
            return ESP_FAIL;
        }
        
        if (!server_address || !cJSON_IsString(server_address) || strlen(server_address->valuestring) == 0) {
            ESP_LOGE(TAG, "❌ 服务器地址缺失或无效");
            cJSON_Delete(json);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Server address is required");
            return ESP_FAIL;
        }
        
        // 使用安全的字符串复制
        safe_strncpy(ssid_str, ssid->valuestring, sizeof(ssid_str));
        if (password && cJSON_IsString(password)) {
            safe_strncpy(password_str, password->valuestring, sizeof(password_str));
        }
        safe_strncpy(server_address_str, server_address->valuestring, sizeof(server_address_str));
        
    } else {
        // 解析URL编码的表单数据
        ESP_LOGI(TAG, "   检测到表单数据格式，正在解析...");
        
        if (!get_form_param(buf, "ssid", ssid_str, sizeof(ssid_str)) || strlen(ssid_str) == 0) {
            ESP_LOGE(TAG, "❌ SSID缺失或无效");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
            return ESP_FAIL;
        }
        
        // ⚠️ 密码字段使用特殊解码（保持+号原样）
        get_form_param_ex(buf, "password", password_str, sizeof(password_str), true);
        
        if (!get_form_param(buf, "server_address", server_address_str, sizeof(server_address_str)) || 
            strlen(server_address_str) == 0) {
            ESP_LOGE(TAG, "❌ 服务器地址缺失或无效");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Server address is required");
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "   ✅ 表单数据解析成功");
    }
    
    // 去除SSID和密码前后空格
    trim_string(ssid_str);
    trim_string(password_str);
    trim_string(server_address_str);
    
    // 检查长度限制
    if (strlen(ssid_str) > 31) {
        ESP_LOGW(TAG, "⚠️ SSID过长 (%zu字节)，将被截断为31字节！原始: '%s'", strlen(ssid_str), ssid_str);
    }
    if (strlen(password_str) > 63) {
        ESP_LOGW(TAG, "⚠️ 密码过长 (%zu字节)，将被截断为63字节！", strlen(password_str));
    }
    
    ESP_LOGI(TAG, "   ✅ 参数验证通过:");
    ESP_LOGI(TAG, "      SSID: '%s' (长度: %zu字节)", ssid_str, strlen(ssid_str));
    ESP_LOGI(TAG, "      密码: %s (长度: %zu字节)", strlen(password_str) > 0 ? "***" : "(空)", strlen(password_str));
    ESP_LOGI(TAG, "      服务器: '%s'", server_address_str);
    
    // 保存WiFi配置
    ESP_LOGI(TAG, "   正在保存WiFi配置...");
    wifi_config_data_t config = {0};
    safe_strncpy(config.ssid, ssid_str, sizeof(config.ssid));
    
    if (strlen(password_str) > 0) {
        safe_strncpy(config.password, password_str, sizeof(config.password));
    }
    
    config.configured = true;
    
    esp_err_t err = wifi_config_save(&config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "   ✅ WiFi配置保存成功");
    } else {
        ESP_LOGE(TAG, "   ❌ WiFi配置保存失败: %s", esp_err_to_name(err));
    }
    
    // 清理JSON对象（如果使用了JSON解析）
    if (json) {
        cJSON_Delete(json);
    }
    
    // 保存服务器地址到server_config命名空间
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "   正在保存服务器配置...");
        unified_server_config_t srv_config = {0};
        const char *server_addr = server_address_str;
        char cleaned_address[256] = {0};
        
        // 处理用户输入的服务器地址
        // 如果用户输入了http://或https://前缀，保留；如果没有，默认添加http://
        // 确保结尾不包含斜杠
        if (strncmp(server_addr, "http://", 7) == 0 || strncmp(server_addr, "https://", 8) == 0) {
            // 用户已输入协议前缀，直接使用
            strncpy(cleaned_address, server_addr, sizeof(cleaned_address) - 1);
            cleaned_address[sizeof(cleaned_address) - 1] = '\0';
            ESP_LOGI(TAG, "检测到用户输入包含协议前缀，保留");
        } else {
            // 用户未输入协议前缀，默认添加http://
            snprintf(cleaned_address, sizeof(cleaned_address), "http://%s", server_addr);
            ESP_LOGI(TAG, "用户输入未包含协议前缀，自动添加http://");
        }
        
        // 去除结尾的斜杠（如果有）
        size_t len = strlen(cleaned_address);
        if (len > 0 && cleaned_address[len - 1] == '/') {
            cleaned_address[len - 1] = '\0';
            ESP_LOGI(TAG, "去除服务器地址结尾的斜杠");
        }
        
        strncpy(srv_config.base_address, cleaned_address, sizeof(srv_config.base_address) - 1);
        srv_config.base_address[sizeof(srv_config.base_address) - 1] = '\0';
        srv_config.http_port = DEFAULT_HTTP_PORT;
        srv_config.mqtt_port = DEFAULT_MQTT_PORT;
        
        esp_err_t srv_err = server_config_save_to_nvs(&srv_config);
        if (srv_err != ESP_OK) {
            ESP_LOGE(TAG, "   ❌ 服务器地址保存失败: %s", esp_err_to_name(srv_err));
            err = srv_err;
        } else {
            ESP_LOGI(TAG, "   ✅ 服务器地址保存成功: %s (原始输入: %s)", 
                     srv_config.base_address, server_address_str);
        }
    }
    
    // 发送响应
    ESP_LOGI(TAG, "   正在发送HTTP响应...");
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "   📤 发送成功响应");
        httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Configuration saved successfully\"}");
        
        // 触发配置接收事件
        ESP_LOGI(TAG, "   触发配置接收事件...");
        trigger_event(WIFI_CONFIG_EVENT_CONFIG_RECEIVED, &config);
        
        // 清除强制配网标志（重要！避免重启后再次进入配网模式）
        ESP_LOGI(TAG, "   清除强制配网标志...");
        wifi_config_clear_force_flag();
        
        // 延迟重启以便响应发送完成
        ESP_LOGI(TAG, "   等待1秒以确保响应发送完成...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "✅ 配置保存完成，设备即将重启...");
        ESP_LOGI(TAG, "========================================");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "   ❌ 发送失败响应");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Failed to save configuration\"}");
    }
    
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

/**
 * @brief 初始化WiFi配网模块
 */
esp_err_t wifi_config_init(wifi_config_event_cb_t event_cb) {
    s_event_cb = event_cb;
    
    ESP_LOGI(TAG, "WiFi配网模块初始化完成");
    return ESP_OK;
}

/**
 * @brief 启动配网模式
 */
esp_err_t wifi_config_start(void) {
    esp_err_t ret;
    
    // 如果HTTP服务器已经在运行，说明配网模式已经在运行，直接返回成功
    if (s_server != NULL) {
        ESP_LOGI(TAG, "配网模式已在运行，跳过启动");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动WiFi配网模式");
    s_config_state = WIFI_CONFIG_STATE_AP_STARTING;
    
    // 确保netif和事件循环已初始化（全局资源，只初始化一次）
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "事件循环创建失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 确保WiFi已初始化（如果之前被清理了需要重新初始化）
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi重新初始化成功");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        // WiFi已经初始化，需要清理STA模式资源
        ESP_LOGI(TAG, "WiFi已初始化，清理STA模式资源...");
        
        // 1. 停止WiFi
        esp_wifi_stop();
        
        // 2. 清理STA netif及其默认处理器（在WiFi去初始化之前）
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta_netif) {
            ESP_LOGI(TAG, "清理STA默认处理器和netif...");
            esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
            esp_netif_destroy(sta_netif);
        }
        
        // 3. 去初始化WiFi
        esp_wifi_deinit();
        
        // 4. 重新初始化WiFi
        ret = esp_wifi_init(&cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "重新初始化WiFi失败: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "WiFi重新初始化成功");
    } else {
        ESP_LOGE(TAG, "WiFi初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 创建AP网络接口
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }
    
    // 注册WiFi事件处理器
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    
    // 生成AP SSID（必须在WiFi初始化之后调用，才能获取到真实的MAC地址）
    generate_ap_ssid();
    ESP_LOGI(TAG, "生成配网AP SSID: %s", s_ap_ssid);
    
    // 配置AP模式
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(s_ap_ssid),
            .channel = CONFIG_AP_CHANNEL,
            .password = CONFIG_AP_PASSWORD,
            .max_connection = CONFIG_AP_MAX_CONNECTIONS,
            .authmode = strlen(CONFIG_AP_PASSWORD) ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN,
        },
    };
    strcpy((char*)wifi_config.ap.ssid, s_ap_ssid);
    
    // 设置WiFi模式为AP
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置WiFi AP模式失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 设置AP配置
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置WiFi AP配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动WiFi失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动Web服务器
    ret = start_webserver();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动Web服务器失败");
        return ret;
    }
    
    // 启动Captive Portal DNS服务器（学习xiaozhi-esp32架构）
    ret = captive_portal_dns_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "启动Captive Portal DNS服务器失败: %s", esp_err_to_name(ret));
        // DNS启动失败不影响配网，继续
    }
    
    ESP_LOGI(TAG, "配网模式启动成功");
    ESP_LOGI(TAG, "请连接WiFi热点: %s", s_ap_ssid);
    ESP_LOGI(TAG, "📱 手机连接热点后会自动弹出配网页面");
    ESP_LOGI(TAG, "如果没有自动弹出，请手动访问: http://192.168.4.1");
    
    return ESP_OK;
}

/**
 * @brief 停止配网模式
 */
esp_err_t wifi_config_stop(void) {
    ESP_LOGI(TAG, "停止WiFi配网模式");
    
    // 停止Web服务器
    stop_webserver();
    
    // 注销事件处理器
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    
    // 停止WiFi
    esp_wifi_stop();
    
    s_config_state = WIFI_CONFIG_STATE_IDLE;
    
    return ESP_OK;
}

/**
 * @brief 获取当前配网状态
 */
wifi_config_state_t wifi_config_get_state(void) {
    return s_config_state;
}

/**
 * @brief 检查是否需要进入配网模式
 */
bool wifi_config_should_start(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }
    
    uint8_t force_config = 0;
    size_t required_size = sizeof(force_config);
    err = nvs_get_blob(nvs_handle, NVS_KEY_FORCE_CONFIG, &force_config, &required_size);
    nvs_close(nvs_handle);
    
    return (err == ESP_OK && force_config == 1);
}

/**
 * @brief 设置强制配网标志
 */
esp_err_t wifi_config_set_force_flag(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    uint8_t force_config = 1;
    err = nvs_set_blob(nvs_handle, NVS_KEY_FORCE_CONFIG, &force_config, sizeof(force_config));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 清除强制配网标志
 */
esp_err_t wifi_config_clear_force_flag(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_erase_key(nvs_handle, NVS_KEY_FORCE_CONFIG);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 保存WiFi配置到NVS
 */
esp_err_t wifi_config_save(const wifi_config_data_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    do {
        err = nvs_set_str(nvs_handle, NVS_KEY_WIFI_SSID, config->ssid);
        if (err != ESP_OK) break;
        
        err = nvs_set_str(nvs_handle, NVS_KEY_WIFI_PASS, config->password);
        if (err != ESP_OK) break;
        
        // 注意：服务器地址不再保存在wifi_config命名空间中，而是保存在server_config命名空间
        uint8_t configured = config->configured ? 1 : 0;
        err = nvs_set_blob(nvs_handle, NVS_KEY_CONFIGURED, &configured, sizeof(configured));
        if (err != ESP_OK) break;
        
        err = nvs_commit(nvs_handle);
    } while (0);
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置保存成功: SSID=%s", config->ssid);
    } else {
        ESP_LOGE(TAG, "WiFi配置保存失败: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 从NVS加载WiFi配置
 */
esp_err_t wifi_config_load(wifi_config_data_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "[NVS DEBUG] wifi_config_load: 参数错误，config为NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "[NVS DEBUG] ========== 开始从Flash读取WiFi配置 ==========");
    ESP_LOGI(TAG, "[NVS DEBUG] 命名空间: %s", NVS_NAMESPACE);
    
    memset(config, 0, sizeof(wifi_config_data_t));
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS DEBUG] 打开NVS命名空间失败: %s (错误码: %d)", esp_err_to_name(err), err);
        ESP_LOGE(TAG, "[NVS DEBUG] 可能原因：NVS未初始化或命名空间不存在");
        return err;
    }
    ESP_LOGI(TAG, "[NVS DEBUG] ✅ NVS命名空间打开成功");
    
    size_t required_size;
    esp_err_t read_err;
    
    // 读取SSID
    ESP_LOGI(TAG, "[NVS DEBUG] --- 读取WiFi SSID (键名: %s) ---", NVS_KEY_WIFI_SSID);
    required_size = sizeof(config->ssid);
    read_err = nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, config->ssid, &required_size);
    if (read_err == ESP_OK) {
        ESP_LOGI(TAG, "[NVS DEBUG] ✅ SSID读取成功: '%s' (长度: %zu)", config->ssid, required_size);
    } else if (read_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[NVS DEBUG] ⚠️ SSID未找到 (键名不存在)");
        config->ssid[0] = '\0';
    } else {
        ESP_LOGE(TAG, "[NVS DEBUG] ❌ SSID读取失败: %s (错误码: %d)", esp_err_to_name(read_err), read_err);
        config->ssid[0] = '\0';
    }
    
    // 读取密码
    ESP_LOGI(TAG, "[NVS DEBUG] --- 读取WiFi密码 (键名: %s) ---", NVS_KEY_WIFI_PASS);
    required_size = sizeof(config->password);
    read_err = nvs_get_str(nvs_handle, NVS_KEY_WIFI_PASS, config->password, &required_size);
    if (read_err == ESP_OK) {
        // 为了安全，只显示密码长度和前几个字符
        size_t pwd_len = strlen(config->password);
        char pwd_preview[8] = {0};
        if (pwd_len > 0) {
            strncpy(pwd_preview, config->password, 3);
            if (pwd_len > 3) {
                strcat(pwd_preview, "...");
            }
        }
        ESP_LOGI(TAG, "[NVS DEBUG] ✅ 密码读取成功: '%s' (长度: %zu, 预览: %s)", 
                 pwd_len > 0 ? "***" : "(空)", required_size, pwd_preview);
    } else if (read_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[NVS DEBUG] ⚠️ 密码未找到 (键名不存在)");
        config->password[0] = '\0';
    } else {
        ESP_LOGE(TAG, "[NVS DEBUG] ❌ 密码读取失败: %s (错误码: %d)", esp_err_to_name(read_err), read_err);
        config->password[0] = '\0';
    }
    
    // 注意：服务器地址不再从wifi_config命名空间读取，而是从server_config命名空间读取
    // 读取配置状态
    ESP_LOGI(TAG, "[NVS DEBUG] --- 读取配置状态 (键名: %s) ---", NVS_KEY_CONFIGURED);
    uint8_t configured = 0;
    required_size = sizeof(configured);
    read_err = nvs_get_blob(nvs_handle, NVS_KEY_CONFIGURED, &configured, &required_size);
    if (read_err == ESP_OK) {
        config->configured = (configured == 1);
        ESP_LOGI(TAG, "[NVS DEBUG] ✅ 配置状态读取成功: configured=%d (原始值: %d, 大小: %zu)", 
                 config->configured, configured, required_size);
    } else if (read_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[NVS DEBUG] ⚠️ 配置状态未找到 (键名不存在)，使用默认值: false");
        config->configured = false;
    } else {
        ESP_LOGE(TAG, "[NVS DEBUG] ❌ 配置状态读取失败: %s (错误码: %d)，使用默认值: false", 
                 esp_err_to_name(read_err), read_err);
        config->configured = false;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "[NVS DEBUG] NVS句柄已关闭");
    
    // 打印完整的配置信息
    ESP_LOGI(TAG, "[NVS DEBUG] ========== WiFi配置读取完成 ==========");
    ESP_LOGI(TAG, "[NVS DEBUG] 📋 完整配置信息:");
    ESP_LOGI(TAG, "[NVS DEBUG]    SSID: '%s'", strlen(config->ssid) > 0 ? config->ssid : "(空)");
    ESP_LOGI(TAG, "[NVS DEBUG]    密码: %s", strlen(config->password) > 0 ? "*** (已设置)" : "(空)");
    ESP_LOGI(TAG, "[NVS DEBUG]    注意: 服务器地址统一从server_config命名空间读取，不再单独存储");
    ESP_LOGI(TAG, "[NVS DEBUG]    已配置标志: %s", config->configured ? "是 (true)" : "否 (false)");
    ESP_LOGI(TAG, "[NVS DEBUG]    配置有效性: %s", 
             (config->configured && strlen(config->ssid) > 0) ? "✅ 有效" : "❌ 无效");
    ESP_LOGI(TAG, "[NVS DEBUG] ========================================");
    
    return ESP_OK;
}

/**
 * @brief 获取AP模式的SSID
 */
const char* wifi_config_get_ap_ssid(void) {
    return s_ap_ssid;
}

/**
 * @brief 获取Web服务器URL
 */
const char* wifi_config_get_web_url(void) {
    return "http://192.168.4.1";
}