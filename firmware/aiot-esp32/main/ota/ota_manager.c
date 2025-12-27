/**
 * @file ota_manager.c
 * @brief OTA固件更新管理器实现
 * 
 * 参考xiaozhi-esp32项目的OTA实现
 * https://github.com/78/xiaozhi-esp32
 */

#include "ota_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#define TAG "OTA_MANAGER"
#define OTA_BUFFER_SIZE 1024
#define MAX_HTTP_RECV_BUFFER 4096

static char http_response_buffer[MAX_HTTP_RECV_BUFFER];
static int http_response_len = 0;
static ota_progress_callback_t s_progress_callback = NULL;

/**
 * @brief HTTP事件处理器（用于响应接收）
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (http_response_len + evt->data_len < MAX_HTTP_RECV_BUFFER) {
                memcpy(http_response_buffer + http_response_len, evt->data, evt->data_len);
                http_response_len += evt->data_len;
                http_response_buffer[http_response_len] = '\0';
            } else {
                ESP_LOGW(TAG, "HTTP响应缓冲区已满");
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t ota_manager_init(void) {
    ESP_LOGI(TAG, "OTA管理器初始化");
    return ESP_OK;
}

/**
 * @brief 解析版本号字符串
 * 
 * 参考xiaozhi的ParseVersion()
 * 将"1.2.3"解析为[1, 2, 3]
 */
static void parse_version(const char *version, int *major, int *minor, int *patch) {
    *major = 0;
    *minor = 0;
    *patch = 0;
    
    if (sscanf(version, "%d.%d.%d", major, minor, patch) < 1) {
        ESP_LOGW(TAG, "无法解析版本号: %s", version);
    }
}

bool ota_manager_is_new_version(const char *current_version, const char *new_version) {
    int curr_major, curr_minor, curr_patch;
    int new_major, new_minor, new_patch;
    
    parse_version(current_version, &curr_major, &curr_minor, &curr_patch);
    parse_version(new_version, &new_major, &new_minor, &new_patch);
    
    ESP_LOGI(TAG, "版本对比: 当前=%d.%d.%d, 新版=%d.%d.%d", 
             curr_major, curr_minor, curr_patch,
             new_major, new_minor, new_patch);
    
    // 参考xiaozhi的逻辑：逐级比较
    if (new_major > curr_major) return true;
    if (new_major < curr_major) return false;
    
    if (new_minor > curr_minor) return true;
    if (new_minor < curr_minor) return false;
    
    if (new_patch > curr_patch) return true;
    
    return false;
}

const char* ota_manager_get_current_version(void) {
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc->version;
}

esp_err_t ota_manager_check_version(
    const char *provision_server,
    const char *mac_address,
    const char *current_version,
    firmware_info_t *fw_info)
{
    if (!provision_server || !mac_address || !fw_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(fw_info, 0, sizeof(firmware_info_t));
    
    // 构建URL（使用GET请求）
    char url[512];
    snprintf(url, sizeof(url), "%s/device/info?mac=%s&firmware_version=%s",
             provision_server, mac_address, current_version);
    
    ESP_LOGI(TAG, "🔍 检查固件版本: %s", url);
    
    // 重置响应缓冲区
    http_response_len = 0;
    memset(http_response_buffer, 0, sizeof(http_response_buffer));
    
    // 配置HTTP客户端
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = MAX_HTTP_RECV_BUFFER,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        return ESP_FAIL;
    }
    
    // 发送请求
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP状态码: %d", status_code);
        
        if (status_code == 200) {
            // 解析JSON响应（参考xiaozhi的CheckVersion）
            cJSON *root = cJSON_Parse(http_response_buffer);
            if (root) {
                cJSON *firmware_update = cJSON_GetObjectItem(root, "firmware_update");
                if (firmware_update && !cJSON_IsNull(firmware_update)) {
                    cJSON *available = cJSON_GetObjectItem(firmware_update, "available");
                    cJSON *version = cJSON_GetObjectItem(firmware_update, "version");
                    cJSON *url = cJSON_GetObjectItem(firmware_update, "download_url");
                    cJSON *size = cJSON_GetObjectItem(firmware_update, "file_size");
                    cJSON *checksum = cJSON_GetObjectItem(firmware_update, "checksum");
                    cJSON *changelog = cJSON_GetObjectItem(firmware_update, "changelog");
                    
                    // 提取固件信息
                    if (available && cJSON_IsTrue(available)) {
                        fw_info->available = true;
                        
                        if (version && cJSON_IsString(version)) {
                            strncpy(fw_info->version, version->valuestring, sizeof(fw_info->version) - 1);
                        }
                        if (url && cJSON_IsString(url)) {
                            strncpy(fw_info->download_url, url->valuestring, sizeof(fw_info->download_url) - 1);
                        }
                        if (size && cJSON_IsNumber(size)) {
                            fw_info->file_size = size->valueint;
                        }
                        if (checksum && cJSON_IsString(checksum)) {
                            strncpy(fw_info->checksum, checksum->valuestring, sizeof(fw_info->checksum) - 1);
                        }
                        if (changelog && cJSON_IsString(changelog)) {
                            strncpy(fw_info->changelog, changelog->valuestring, sizeof(fw_info->changelog) - 1);
                        }
                        
                        ESP_LOGI(TAG, "⚠️ 发现固件更新:");
                        ESP_LOGI(TAG, "   版本: %s", fw_info->version);
                        ESP_LOGI(TAG, "   大小: %lu 字节", (unsigned long)fw_info->file_size);
                        ESP_LOGI(TAG, "   URL: %s", fw_info->download_url);
                        ESP_LOGI(TAG, "   更新日志: %s", fw_info->changelog);
                        
                        err = ESP_OK;
                    } else {
                        ESP_LOGI(TAG, "✅ 已是最新版本");
                        fw_info->available = false;
                        err = ESP_OK;
                    }
                } else {
                    ESP_LOGI(TAG, "✅ 响应中无固件更新信息");
                    fw_info->available = false;
                    err = ESP_OK;
                }
                
                cJSON_Delete(root);
            } else {
                ESP_LOGE(TAG, "❌ JSON解析失败");
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "❌ HTTP请求失败: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP请求失败: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t ota_manager_start_upgrade(
    const char *firmware_url,
    ota_progress_callback_t callback)
{
    if (!firmware_url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_progress_callback = callback;
    
    ESP_LOGI(TAG, "🚀 开始OTA升级");
    ESP_LOGI(TAG, "📥 固件URL: %s", firmware_url);
    
    // 参考xiaozhi的Upgrade()实现
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "❌ 获取OTA分区失败");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "写入分区: %s (地址: 0x%lx)", 
             update_partition->label, update_partition->address);
    
    // 打开HTTP连接
    esp_http_client_config_t config = {
        .url = firmware_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 30000,
        .buffer_size = OTA_BUFFER_SIZE,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "❌ HTTP客户端初始化失败");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTP连接失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "❌ HTTP状态码错误: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "❌ 无法获取内容长度");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "📦 固件大小: %d 字节", content_length);
    
    // 流式下载和写入（参考xiaozhi的逐块读取）
    char buffer[OTA_BUFFER_SIZE];
    size_t total_read = 0;
    size_t recent_read = 0;
    int64_t last_calc_time = esp_timer_get_time();
    bool image_header_checked = false;
    
    while (true) {
        int ret = esp_http_client_read(client, buffer, sizeof(buffer));
        
        if (ret < 0) {
            ESP_LOGE(TAG, "❌ 读取数据失败: %s", esp_err_to_name(ret));
            if (update_handle) {
                esp_ota_abort(update_handle);
            }
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        
        if (ret == 0) {
            // 读取完成
            break;
        }
        
        total_read += ret;
        recent_read += ret;
        
        // 参考xiaozhi：检查固件头（首次）
        if (!image_header_checked && total_read >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
            ESP_LOGI(TAG, "开始OTA写入...");
            err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "❌ OTA开始失败: %s", esp_err_to_name(err));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
            image_header_checked = true;
        }
        
        // 写入OTA数据
        if (image_header_checked) {
            err = esp_ota_write(update_handle, buffer, ret);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "❌ OTA写入失败: %s", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
        }
        
        // 参考xiaozhi：每秒计算一次进度和速度
        int64_t current_time = esp_timer_get_time();
        if (current_time - last_calc_time >= 1000000 || ret == 0) {
            int progress = (total_read * 100) / content_length;
            ESP_LOGI(TAG, "📥 进度: %d%% (%u/%d), 速度: %uB/s", 
                     progress, total_read, content_length, recent_read);
            
            // 调用回调函数
            if (s_progress_callback) {
                s_progress_callback(progress, recent_read);
            }
            
            last_calc_time = current_time;
            recent_read = 0;
        }
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    ESP_LOGI(TAG, "📥 下载完成，总共: %u 字节", total_read);
    
    // 参考xiaozhi：结束OTA并验证
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "❌ 固件验证失败，文件损坏");
        } else {
            ESP_LOGE(TAG, "❌ OTA结束失败: %s", esp_err_to_name(err));
        }
        return ESP_FAIL;
    }
    
    // 参考xiaozhi：设置启动分区
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ 设置启动分区失败: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ OTA升级成功！");
    return ESP_OK;
}

esp_err_t ota_manager_mark_valid(void) {
    // 参考xiaozhi的MarkCurrentVersionValid()
    const esp_partition_t *partition = esp_ota_get_running_partition();
    
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "运行在factory分区，跳过");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "当前运行分区: %s", partition->label);
    
    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(partition, &state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取分区状态失败: %s", esp_err_to_name(err));
        return err;
    }
    
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "✅ 标记新固件为有效");
        esp_ota_mark_app_valid_cancel_rollback();
    } else {
        ESP_LOGI(TAG, "固件状态: %d (无需标记)", state);
    }
    
    return ESP_OK;
}
