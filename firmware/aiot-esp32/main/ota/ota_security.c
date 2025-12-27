/**
 * @file ota_security.c
 * @brief OTA安全验证模块实现
 * @version 1.0
 * @date 2024-01-20
 */

#include "ota_security.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha1.h"
#include "mbedtls/md5.h"
#include "mbedtls/rsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "esp_random.h"
#include <string.h>

static const char* TAG = "OTA_SECURITY";

static ota_security_config_t g_security_config = {0};
static bool g_security_initialized = false;

esp_err_t ota_security_init(const ota_security_config_t *config)
{
    if (g_security_initialized) {
        ESP_LOGW(TAG, "OTA security already initialized");
        return ESP_OK;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid security config");
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&g_security_config, config, sizeof(ota_security_config_t));
    g_security_initialized = true;
    
    ESP_LOGI(TAG, "OTA security initialized with hash: %s, signature: %s", 
             ota_security_get_hash_name(config->hash_type),
             ota_security_get_sign_name(config->sign_type));
    
    return ESP_OK;
}

esp_err_t ota_security_deinit(void)
{
    if (!g_security_initialized) {
        ESP_LOGW(TAG, "OTA security not initialized");
        return ESP_OK;
    }
    
    memset(&g_security_config, 0, sizeof(ota_security_config_t));
    g_security_initialized = false;
    
    ESP_LOGI(TAG, "OTA security deinitialized");
    
    return ESP_OK;
}

esp_err_t ota_security_verify_hash(const uint8_t *data, size_t data_len, 
                                   const uint8_t *expected_hash, ota_hash_type_t hash_type)
{
    if (!data || !expected_hash || data_len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for hash verification");
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t calculated_hash[32];
    size_t hash_len = 0;
    
    esp_err_t ret = ota_security_calculate_hash(data, data_len, hash_type, calculated_hash, &hash_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate hash");
        return ret;
    }
    
    if (memcmp(calculated_hash, expected_hash, hash_len) != 0) {
        ESP_LOGE(TAG, "Hash verification failed");
        return ESP_ERR_INVALID_CRC;
    }
    
    ESP_LOGI(TAG, "Hash verification successful");
    return ESP_OK;
}

esp_err_t ota_security_calculate_hash(const uint8_t *data, size_t data_len, 
                                      ota_hash_type_t hash_type, uint8_t *hash_output, size_t *hash_len)
{
    if (!data || !hash_output || !hash_len || data_len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for hash calculation");
        return ESP_ERR_INVALID_ARG;
    }
    
    int ret = 0;
    
    switch (hash_type) {
        case OTA_HASH_SHA256: {
            mbedtls_sha256_context ctx;
            mbedtls_sha256_init(&ctx);
            ret = mbedtls_sha256_starts(&ctx, 0); // 0 for SHA256
            if (ret == 0) {
                ret = mbedtls_sha256_update(&ctx, data, data_len);
            }
            if (ret == 0) {
                ret = mbedtls_sha256_finish(&ctx, hash_output);
            }
            mbedtls_sha256_free(&ctx);
            *hash_len = 32;
            break;
        }
        case OTA_HASH_SHA1: {
            mbedtls_sha1_context ctx;
            mbedtls_sha1_init(&ctx);
            ret = mbedtls_sha1_starts(&ctx);
            if (ret == 0) {
                ret = mbedtls_sha1_update(&ctx, data, data_len);
            }
            if (ret == 0) {
                ret = mbedtls_sha1_finish(&ctx, hash_output);
            }
            mbedtls_sha1_free(&ctx);
            *hash_len = 20;
            break;
        }
        case OTA_HASH_MD5: {
            mbedtls_md5_context ctx;
            mbedtls_md5_init(&ctx);
            ret = mbedtls_md5_starts(&ctx);
            if (ret == 0) {
                ret = mbedtls_md5_update(&ctx, data, data_len);
            }
            if (ret == 0) {
                ret = mbedtls_md5_finish(&ctx, hash_output);
            }
            mbedtls_md5_free(&ctx);
            *hash_len = 16;
            break;
        }
        default:
            ESP_LOGE(TAG, "Unsupported hash type: %d", hash_type);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Hash calculation failed: %d", ret);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t ota_security_verify_signature(const uint8_t *data, size_t data_len, 
                                        const ota_signature_info_t *signature_info)
{
    if (!g_security_initialized) {
        ESP_LOGE(TAG, "OTA security not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data || !signature_info || data_len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for signature verification");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_security_config.verify_signature) {
        ESP_LOGW(TAG, "Signature verification disabled");
        return ESP_OK;
    }
    
    // 首先验证哈希
    esp_err_t ret = ota_security_verify_hash(data, data_len, signature_info->hash, signature_info->hash_type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hash verification failed in signature verification");
        return ret;
    }
    
    // TODO: 实现RSA/ECDSA签名验证
    ESP_LOGW(TAG, "Signature verification not fully implemented");
    
    return ESP_OK;
}

esp_err_t ota_security_check_rollback_protection(uint32_t new_version, uint32_t current_version)
{
    if (!g_security_initialized) {
        ESP_LOGE(TAG, "OTA security not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_security_config.check_rollback) {
        ESP_LOGW(TAG, "Rollback protection disabled");
        return ESP_OK;
    }
    
    if (new_version < current_version) {
        ESP_LOGE(TAG, "Rollback protection: new version %lu < current version %lu", new_version, current_version);
        return ESP_ERR_INVALID_VERSION;
    }
    
    ESP_LOGI(TAG, "Rollback protection passed: %lu >= %lu", new_version, current_version);
    return ESP_OK;
}

esp_err_t ota_security_set_public_key(const uint8_t *public_key, size_t key_len)
{
    if (!g_security_initialized) {
        ESP_LOGE(TAG, "OTA security not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!public_key || key_len == 0 || key_len > sizeof(g_security_config.public_key)) {
        ESP_LOGE(TAG, "Invalid public key parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(g_security_config.public_key, public_key, key_len);
    g_security_config.public_key_len = key_len;
    
    ESP_LOGI(TAG, "Public key set, length: %zu", key_len);
    return ESP_OK;
}

const char *ota_security_get_hash_name(ota_hash_type_t hash_type)
{
    switch (hash_type) {
        case OTA_HASH_SHA256: return "SHA256";
        case OTA_HASH_SHA1: return "SHA1";
        case OTA_HASH_MD5: return "MD5";
        default: return "UNKNOWN";
    }
}

const char *ota_security_get_sign_name(ota_sign_type_t sign_type)
{
    switch (sign_type) {
        case OTA_SIGN_RSA: return "RSA";
        case OTA_SIGN_ECDSA: return "ECDSA";
        case OTA_SIGN_NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

esp_err_t ota_security_generate_random(uint8_t *buffer, size_t len)
{
    if (!buffer || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for random generation");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_fill_random(buffer, len);
    return ESP_OK;
}

void ota_security_secure_memset(void *ptr, size_t len)
{
    if (ptr && len > 0) {
        volatile uint8_t *p = (volatile uint8_t *)ptr;
        for (size_t i = 0; i < len; i++) {
            p[i] = 0;
        }
    }
}

esp_err_t ota_security_verify_cert_chain(const uint8_t *cert_chain, size_t cert_len)
{
    if (!cert_chain || cert_len == 0) {
        ESP_LOGE(TAG, "Invalid certificate chain parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: 实现证书链验证
    ESP_LOGW(TAG, "Certificate chain verification not implemented");
    
    return ESP_OK;
}