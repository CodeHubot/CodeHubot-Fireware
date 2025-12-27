/**
 * @file ota_security.h
 * @brief OTA安全验证模块
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef OTA_SECURITY_H
#define OTA_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 哈希算法类型 */
typedef enum {
    OTA_HASH_SHA256,
    OTA_HASH_SHA1,
    OTA_HASH_MD5
} ota_hash_type_t;

/* 签名算法类型 */
typedef enum {
    OTA_SIGN_RSA,
    OTA_SIGN_ECDSA,
    OTA_SIGN_NONE
} ota_sign_type_t;

/* 安全配置 */
typedef struct {
    ota_hash_type_t hash_type;
    ota_sign_type_t sign_type;
    bool verify_signature;
    bool verify_hash;
    bool check_rollback;
    uint8_t public_key[256];
    size_t public_key_len;
} ota_security_config_t;

/* 固件签名信息 */
typedef struct {
    uint8_t signature[256];
    size_t signature_len;
    uint8_t hash[32];
    size_t hash_len;
    ota_hash_type_t hash_type;
    ota_sign_type_t sign_type;
} ota_signature_info_t;

/**
 * @brief 初始化安全验证模块
 * 
 * @param config 安全配置
 * @return esp_err_t 
 */
esp_err_t ota_security_init(const ota_security_config_t *config);

/**
 * @brief 反初始化安全验证模块
 * 
 * @return esp_err_t 
 */
esp_err_t ota_security_deinit(void);

/**
 * @brief 验证固件哈希
 * 
 * @param data 固件数据
 * @param data_len 数据长度
 * @param expected_hash 期望的哈希值
 * @param hash_type 哈希算法类型
 * @return esp_err_t 
 */
esp_err_t ota_security_verify_hash(const uint8_t *data, size_t data_len, 
                                   const uint8_t *expected_hash, ota_hash_type_t hash_type);

/**
 * @brief 验证固件签名
 * 
 * @param data 固件数据
 * @param data_len 数据长度
 * @param signature_info 签名信息
 * @return esp_err_t 
 */
esp_err_t ota_security_verify_signature(const uint8_t *data, size_t data_len, 
                                        const ota_signature_info_t *signature_info);

/**
 * @brief 计算数据哈希
 * 
 * @param data 数据
 * @param data_len 数据长度
 * @param hash_type 哈希算法类型
 * @param hash_output 哈希输出缓冲区
 * @param hash_len 哈希长度
 * @return esp_err_t 
 */
esp_err_t ota_security_calculate_hash(const uint8_t *data, size_t data_len, 
                                      ota_hash_type_t hash_type, uint8_t *hash_output, size_t *hash_len);

/**
 * @brief 验证证书链
 * 
 * @param cert_chain 证书链
 * @param cert_len 证书长度
 * @return esp_err_t 
 */
esp_err_t ota_security_verify_cert_chain(const uint8_t *cert_chain, size_t cert_len);

/**
 * @brief 检查回滚保护
 * 
 * @param new_version 新版本号
 * @param current_version 当前版本号
 * @return esp_err_t 
 */
esp_err_t ota_security_check_rollback_protection(uint32_t new_version, uint32_t current_version);

/**
 * @brief 设置公钥
 * 
 * @param public_key 公钥数据
 * @param key_len 公钥长度
 * @return esp_err_t 
 */
esp_err_t ota_security_set_public_key(const uint8_t *public_key, size_t key_len);

/**
 * @brief 获取哈希算法名称
 * 
 * @param hash_type 哈希类型
 * @return const char* 算法名称
 */
const char *ota_security_get_hash_name(ota_hash_type_t hash_type);

/**
 * @brief 获取签名算法名称
 * 
 * @param sign_type 签名类型
 * @return const char* 算法名称
 */
const char *ota_security_get_sign_name(ota_sign_type_t sign_type);

/**
 * @brief 生成随机数
 * 
 * @param buffer 输出缓冲区
 * @param len 随机数长度
 * @return esp_err_t 
 */
esp_err_t ota_security_generate_random(uint8_t *buffer, size_t len);

/**
 * @brief 安全擦除内存
 * 
 * @param ptr 内存指针
 * @param len 内存长度
 */
void ota_security_secure_memset(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* OTA_SECURITY_H */