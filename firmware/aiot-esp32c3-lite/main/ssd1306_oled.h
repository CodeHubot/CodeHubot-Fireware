/**
 * @file ssd1306_oled.h
 * @brief SSD1306 OLED显示屏驱动 (128x64单色)
 * 
 * 支持I2C接口的SSD1306 OLED显示屏
 * 
 * @author AIOT Team
 * @date 2025-12-27
 */

#ifndef SSD1306_OLED_H
#define SSD1306_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// OLED显示区域
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} oled_area_t;

// OLED文本对齐方式
typedef enum {
    OLED_ALIGN_LEFT = 0,
    OLED_ALIGN_CENTER,
    OLED_ALIGN_RIGHT
} oled_align_t;

/**
 * @brief OLED初始化
 * @return ESP_OK 成功，其他失败
 */
esp_err_t oled_init(void);

/**
 * @brief OLED反初始化
 */
void oled_deinit(void);

/**
 * @brief 清空显示
 */
void oled_clear(void);

/**
 * @brief 显示开关
 * @param on true=开启，false=关闭
 */
void oled_display(bool on);

/**
 * @brief 设置对比度
 * @param contrast 对比度 (0-255)
 */
void oled_set_contrast(uint8_t contrast);

/**
 * @brief 在指定位置显示字符串
 * @param x X坐标 (0-127)
 * @param y Y坐标，页地址 (0-7)
 * @param str 要显示的字符串
 */
void oled_show_string(uint8_t x, uint8_t y, const char *str);

/**
 * @brief 在指定行显示字符串（自动居中）
 * @param line 行号 (0-7)
 * @param str 要显示的字符串
 * @param align 对齐方式
 */
void oled_show_line(uint8_t line, const char *str, oled_align_t align);

/**
 * @brief 显示Logo（启动画面）
 */
void oled_show_logo(void);

/**
 * @brief 显示WiFi状态
 * @param ssid WiFi SSID
 * @param status 状态字符串 ("Connected", "Connecting", "Failed")
 */
void oled_show_wifi_status(const char *ssid, const char *status);

/**
 * @brief 显示MQTT状态
 * @param status 状态字符串 ("Connected", "Connecting", "Failed")
 */
void oled_show_mqtt_status(const char *status);

/**
 * @brief 显示传感器数据
 * @param temperature 温度
 * @param humidity 湿度
 */
void oled_show_sensor_data(float temperature, float humidity);

/**
 * @brief 显示IP地址
 * @param ip IP地址字符串
 */
void oled_show_ip(const char *ip);

/**
 * @brief 显示完整状态界面
 * @param wifi_ssid WiFi SSID
 * @param wifi_connected WiFi连接状态
 * @param mqtt_connected MQTT连接状态
 * @param temperature 温度
 * @param humidity 湿度
 * @param ip_addr IP地址
 */
void oled_show_status_screen(
    const char *wifi_ssid,
    bool wifi_connected,
    bool mqtt_connected,
    float temperature,
    float humidity,
    const char *ip_addr
);

/**
 * @brief 显示启动提示
 */
void oled_show_starting(void);

/**
 * @brief 显示配网提示
 * @param ap_ssid AP热点名称
 */
void oled_show_config_mode(const char *ap_ssid);

/**
 * @brief 在指定位置画点
 * @param x X坐标 (0-127)
 * @param y Y坐标 (0-63)
 * @param on true=点亮，false=熄灭
 */
void oled_draw_pixel(uint8_t x, uint8_t y, bool on);

/**
 * @brief 画线
 * @param x1 起点X
 * @param y1 起点Y
 * @param x2 终点X
 * @param y2 终点Y
 */
void oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/**
 * @brief 画矩形
 * @param x 左上角X
 * @param y 左上角Y
 * @param w 宽度
 * @param h 高度
 * @param fill true=填充，false=空心
 */
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill);

/**
 * @brief 刷新显示（将缓冲区内容写入OLED）
 */
void oled_refresh(void);

/**
 * @brief 测试显示功能
 */
void oled_test(void);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_OLED_H

