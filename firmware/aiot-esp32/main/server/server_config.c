/**
 * @file server_config.c
 * @brief 统一服务器配置模块实现
 */

#include "server_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "SERVER_CONFIG";

/**
 * @brief 从NVS加载服务器配置
 */
esp_err_t server_config_load_from_nvs(unified_server_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "[NVS DEBUG] server_config_load_from_nvs: 参数错误，config为NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "[NVS DEBUG] ========== 开始从Flash读取服务器配置 ==========");
    ESP_LOGI(TAG, "[NVS DEBUG] 命名空间: %s", SERVER_CONFIG_NAMESPACE);

    memset(config, 0, sizeof(unified_server_config_t));

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SERVER_CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS DEBUG] ❌ 打开NVS命名空间失败: %s (错误码: %d)", 
                 esp_err_to_name(err), err);
        ESP_LOGE(TAG, "[NVS DEBUG] 可能原因：NVS未初始化或命名空间不存在");
        return err;
    }
    ESP_LOGI(TAG, "[NVS DEBUG] ✅ NVS命名空间打开成功");

    // 读取服务器基础地址
    ESP_LOGI(TAG, "[NVS DEBUG] --- 读取服务器基础地址 (键名: %s) ---", NVS_KEY_BASE_ADDRESS);
    size_t required_size = sizeof(config->base_address);
    err = nvs_get_str(nvs_handle, NVS_KEY_BASE_ADDRESS, config->base_address, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[NVS DEBUG] ⚠️ 服务器基础地址未找到 (键名不存在)");
        ESP_LOGW(TAG, "[NVS DEBUG] 键名: %s, 命名空间: %s", NVS_KEY_BASE_ADDRESS, SERVER_CONFIG_NAMESPACE);
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS DEBUG] ❌ 读取服务器基础地址失败: %s (错误码: %d)", 
                 esp_err_to_name(err), err);
        nvs_close(nvs_handle);
        return err;
    }

    ESP_LOGI(TAG, "[NVS DEBUG] ✅ 服务器基础地址读取成功: '%s' (长度: %zu)", 
             config->base_address, required_size);

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "[NVS DEBUG] NVS句柄已关闭");

    // 验证base_address格式：应该包含http://或https://前缀
    // 如果缺少协议前缀，自动添加http://（兼容旧数据）
    // 先验证字符串是否有效（以'\0'结尾）
    if (required_size > 0 && required_size <= sizeof(config->base_address) && 
        config->base_address[required_size - 1] == '\0') {
        // 使用required_size而不是strlen，避免在中断上下文中访问无效内存
        size_t addr_len = required_size - 1;  // 减去'\0'
        if (addr_len > 0) {
            if (strncmp(config->base_address, "http://", 7) != 0 && 
                strncmp(config->base_address, "https://", 8) != 0) {
                ESP_LOGW(TAG, "[NVS DEBUG] ⚠️ 服务器地址缺少协议前缀，自动添加http://（兼容旧数据）");
                // 检查长度，确保添加"http://"后不会溢出
                if (addr_len + 7 < sizeof(config->base_address)) {
                    char temp_address[128] = {0};  // 增大缓冲区避免截断警告
                    int snprintf_ret = snprintf(temp_address, sizeof(temp_address), "http://%s", config->base_address);
                    if (snprintf_ret > 0 && snprintf_ret < sizeof(temp_address)) {
                        strncpy(config->base_address, temp_address, sizeof(config->base_address) - 1);
                        config->base_address[sizeof(config->base_address) - 1] = '\0';
                        addr_len = snprintf_ret;  // 使用snprintf返回值更新长度，避免调用strlen
                        ESP_LOGI(TAG, "[NVS DEBUG]    修正后地址: '%s'", config->base_address);
                    } else {
                        ESP_LOGE(TAG, "[NVS DEBUG] ❌ 构建修正后地址失败");
                        return ESP_ERR_INVALID_SIZE;
                    }
                } else {
                    ESP_LOGE(TAG, "[NVS DEBUG] ❌ 服务器地址过长，无法添加协议前缀");
                    return ESP_ERR_INVALID_SIZE;
                }
            }
            
            // 确保结尾不包含斜杠（使用已知长度，避免再次调用strlen）
            if (addr_len > 0 && config->base_address[addr_len - 1] == '/') {
                config->base_address[addr_len - 1] = '\0';
                ESP_LOGW(TAG, "[NVS DEBUG] ⚠️ 检测到服务器地址结尾包含/，已自动去除");
                ESP_LOGI(TAG, "[NVS DEBUG]    修正后地址: '%s'", config->base_address);
            }
        }
    } else {
        ESP_LOGE(TAG, "[NVS DEBUG] ❌ 服务器地址格式无效（required_size=%zu）", required_size);
        return ESP_ERR_INVALID_ARG;
    }

    // 设置默认端口（不从NVS读取）
    config->http_port = DEFAULT_HTTP_PORT;
    config->mqtt_port = DEFAULT_MQTT_PORT;

    ESP_LOGI(TAG, "[NVS DEBUG] ========== 服务器配置读取完成 ==========");
    ESP_LOGI(TAG, "[NVS DEBUG] 📋 完整配置信息:");
    ESP_LOGI(TAG, "[NVS DEBUG]    服务器地址: '%s'", config->base_address);
    ESP_LOGI(TAG, "[NVS DEBUG]    HTTP端口: %d (默认值，不从NVS读取)", config->http_port);
    ESP_LOGI(TAG, "[NVS DEBUG]    MQTT端口: %d (默认值，不从NVS读取)", config->mqtt_port);
    ESP_LOGI(TAG, "[NVS DEBUG]    配置有效性: %s", 
             strlen(config->base_address) > 0 ? "✅ 有效" : "❌ 无效 (地址为空)");
    ESP_LOGI(TAG, "[NVS DEBUG] ========================================");

    return ESP_OK;
}

/**
 * @brief 获取默认服务器配置（用于兜底）
 */
esp_err_t server_config_get_default(unified_server_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(unified_server_config_t));
    strncpy(config->base_address, DEFAULT_SERVER_BASE_ADDRESS, sizeof(config->base_address) - 1);
    config->base_address[sizeof(config->base_address) - 1] = '\0';
    config->http_port = DEFAULT_HTTP_PORT;
    config->mqtt_port = DEFAULT_MQTT_PORT;

    ESP_LOGW(TAG, "Using default server config: base_address=%s (THIS IS FOR PLACEHOLDER ONLY, NOT FOR ACTUAL CONNECTION)",
             config->base_address);

    return ESP_OK;
}

/**
 * @brief 保存服务器配置到NVS
 */
esp_err_t server_config_save_to_nvs(const unified_server_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SERVER_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", 
                 SERVER_CONFIG_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    // 保存服务器基础地址
    err = nvs_set_str(nvs_handle, NVS_KEY_BASE_ADDRESS, config->base_address);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save base_address to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Server config saved to NVS: base_address=%s", config->base_address);

    return ESP_OK;
}

/**
 * @brief 根据服务器配置构建完整URL
 * 
 * base_address已经包含协议前缀（http://或https://），直接拼接端口和路径
 * 格式：base_address:port/path
 */
esp_err_t server_config_build_url(
    const unified_server_config_t *config,
    const char *protocol,
    const char *path,
    char *out_buf,
    size_t out_buf_size)
{
    if (!config || !path || !out_buf) {
        return ESP_ERR_INVALID_ARG;
    }

    // base_address已经包含协议前缀（http://或https://），直接拼接端口和路径
    // 格式：base_address:port/path
    int ret = snprintf(out_buf, out_buf_size, "%s:%d%s",
                       config->base_address, config->http_port, path);
    
    if (ret < 0 || ret >= out_buf_size) {
        ESP_LOGE(TAG, "URL buffer too small: need %d bytes, have %zu", ret, out_buf_size);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "[URL DEBUG] 构建URL: %s", out_buf);
    return ESP_OK;
}

/**
 * @brief 构建HTTP API URL（使用默认HTTP端口）
 * 
 * base_address已经包含协议前缀，直接调用build_url（protocol参数已废弃）
 */
esp_err_t server_config_build_http_url(
    const unified_server_config_t *config,
    const char *path,
    char *out_buf,
    size_t out_buf_size)
{
    return server_config_build_url(config, NULL, path, out_buf, out_buf_size);
}

/**
 * @brief 构建MQTT Broker地址（用于MQTT客户端）
 * 
 * MQTT客户端会自动添加mqtt://前缀和端口，所以这里只返回主机地址（IP或域名）
 * 从base_address中提取主机地址（去除http://或https://前缀）
 */
esp_err_t server_config_build_mqtt_broker_url(
    const unified_server_config_t *config,
    char *out_buf,
    size_t out_buf_size)
{
    if (!config || !out_buf) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *base_addr = config->base_address;
    const char *host_start = base_addr;
    
    // 去除http://或https://前缀
    if (strncmp(base_addr, "http://", 7) == 0) {
        host_start = base_addr + 7;
    } else if (strncmp(base_addr, "https://", 8) == 0) {
        host_start = base_addr + 8;
    }
    
    // 只返回主机地址（不包含端口，MQTT客户端会自动添加端口）
    int ret = snprintf(out_buf, out_buf_size, "%s", host_start);
    
    if (ret < 0 || ret >= out_buf_size) {
        ESP_LOGE(TAG, "MQTT broker URL buffer too small: need %d bytes, have %zu", ret, out_buf_size);
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

