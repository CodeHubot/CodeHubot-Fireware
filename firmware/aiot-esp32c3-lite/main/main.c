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

#include "board_config.h"
#include "app_config.h"
#include "ssd1306_oled.h"
#include "dht11_driver.h"
#include "esp_random.h"

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
            
            // 简化的JSON解析（实际应使用cJSON）
            char *data = strndup(event->data, event->data_len);
            if (data) {
                // 解析控制命令
                if (strstr(data, "\"port\":\"LED1\"") || strstr(data, "\"port\":\"LED_RED\"")) {
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
                #if RELAY_COUNT > 0
                else if (strstr(data, "\"port\":\"RELAY1\"")) {
                    if (strstr(data, "\"value\":1")) {
                        relay_control(1);
                    } else if (strstr(data, "\"value\":0")) {
                        relay_control(0);
                    }
                }
                #endif
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

// ==================== WiFi配网Web服务器 ====================

static const char* config_page_html = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>WiFi Config</title>"
    "<style>body{font-family:Arial;padding:20px;background:#f0f0f0}"
    "form{background:white;padding:20px;border-radius:8px;max-width:400px}"
    "input{width:100%;padding:8px;margin:8px 0;box-sizing:border-box}"
    "button{width:100%;padding:10px;background:#007bff;color:white;border:none;border-radius:4px;cursor:pointer}"
    "button:hover{background:#0056b3}</style></head><body>"
    "<h2>WiFi配置</h2><form action='/save' method='post'>"
    "<label>WiFi名称:</label><input name='ssid' required><br>"
    "<label>WiFi密码:</label><input name='pass' type='password' required><br>"
    "<label>MQTT服务器:</label><input name='mqtt' required placeholder='mqtt.example.com'><br>"
    "<button type='submit'>保存并重启</button></form></body></html>";

static esp_err_t config_page_handler(httpd_req_t *req) {
    httpd_resp_send(req, config_page_html, strlen(config_page_html));
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
    char ssid[33] = {0}, pass[65] = {0}, mqtt[129] = {0};
    
    // 解析POST数据 (简化版，实际应该更严谨)
    char *p = strstr(content, "ssid=");
    if (p) {
        sscanf(p, "ssid=%32[^&]", ssid);
    }
    p = strstr(content, "pass=");
    if (p) {
        sscanf(p, "pass=%64[^&]", pass);
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
        nvs_set_str(nvs_handle, NVS_KEY_MQTT_BROKER, mqtt);
        nvs_set_u8(nvs_handle, NVS_KEY_CONFIG_DONE, 1);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        ESP_LOGI(TAG, "配置已保存: SSID=%s, MQTT=%s", ssid, mqtt);
    }
    
    const char* response = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head>"
                          "<body><h2>配置成功！</h2><p>设备即将重启...</p></body></html>";
    httpd_resp_send(req, response, strlen(response));
    
    // 延迟重启
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

static httpd_handle_t start_config_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WIFI_CONFIG_WEB_PORT;
    config.lru_purge_enable = true;
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = config_page_handler,
        };
        httpd_uri_t uri_post = {
            .uri       = "/save",
            .method    = HTTP_POST,
            .handler   = config_save_handler,
        };
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
        ESP_LOGI(TAG, "✅ 配网服务器启动: http://192.168.4.1");
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
    esp_netif_create_default_wifi_ap();
    
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
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "✅ 配网AP启动: %s", ap_ssid);
    return ESP_OK;
}

// ==================== MQTT初始化 ====================

static esp_err_t mqtt_init(const char *broker, const char *client_id) {
    char uri[256];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", broker, DEFAULT_MQTT_PORT);
    
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
    
    ESP_LOGI(TAG, "MQTT客户端启动: %s (ID: %s)", uri, client_id);
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
        
        // 读取传感器
        #if DHT11_ENABLED
        {
            dht11_data_t dht_data;
            if (dht11_read(&dht_data) == ESP_OK && dht_data.valid) {
                float temp = dht_data.temperature;
                float humi = dht_data.humidity;
                g_sensor_data.temperature = temp;
                g_sensor_data.humidity = humi;
                g_sensor_data.valid = true;
                ESP_LOGI(TAG, "DHT11: 温度=%.1f°C, 湿度=%.1f%%", temp, humi);
                
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
            } else {
                ESP_LOGW(TAG, "DHT11读取失败");
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
    
    // LED2 (蓝色)
    io_conf.pin_bit_mask = (1ULL << LED2_GPIO_PIN);
    gpio_config(&io_conf);
    
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
        oled_show_logo();
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG, "✅ OLED显示已就绪");
    } else {
        ESP_LOGE(TAG, "❌ OLED初始化失败");
    }
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
    
    // 检查是否已配置或Boot按键按下
    nvs_handle_t nvs_handle;
    uint8_t config_done = 0;
    int boot_pressed = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);
    
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        nvs_get_u8(nvs_handle, NVS_KEY_CONFIG_DONE, &config_done);
        nvs_close(nvs_handle);
    }
    
    if (!config_done || boot_pressed) {
        // 进入配网模式
        ESP_LOGI(TAG, "🔧 进入配网模式");
        g_config_mode = true;
        led_blue_control(1); // 蓝色LED常亮表示配网模式
        
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
        start_config_server();
        
        ESP_LOGI(TAG, "请连接WiFi热点并访问 http://192.168.4.1 进行配置");
        
        // 配网模式下无限等待
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // 正常模式：读取配置并连接
    char ssid[33] = {0}, pass[65] = {0}, mqtt_broker[129] = {0};
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        size_t len;
        len = sizeof(ssid);
        nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, ssid, &len);
        len = sizeof(pass);
        nvs_get_str(nvs_handle, NVS_KEY_WIFI_PASS, pass, &len);
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
        
        // 启动MQTT
        mqtt_init(mqtt_broker, g_device_id);
        
        // LED闪烁表示运行正常
        for (int i = 0; i < 3; i++) {
            led_red_control(1);
            vTaskDelay(pdMS_TO_TICKS(200));
            led_red_control(0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        
        // 获取IP地址
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                snprintf(g_ip_address, sizeof(g_ip_address), IPSTR, IP2STR(&ip_info.ip));
                ESP_LOGI(TAG, "IP地址: %s", g_ip_address);
            }
        }
    } else {
        ESP_LOGE(TAG, "❌ WiFi连接失败");
    }
    
    // 启动系统监控任务
    xTaskCreate(system_monitor_task, "monitor", TASK_STACK_SIZE_MEDIUM, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "=== 系统启动完成 ===");
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

