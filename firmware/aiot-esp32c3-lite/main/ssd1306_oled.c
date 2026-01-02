/**
 * @file ssd1306_oled.c
 * @brief SSD1306 OLED显示屏驱动实现
 * 
 * 128x64单色OLED，I2C接口
 * 
 * @author AIOT Team
 * @date 2025-12-27
 */

#include "ssd1306_oled.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "OLED";

// 显示缓冲区 (128x64 / 8 = 1024字节)
static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

// 8x8 ASCII字体（简化版，0x20-0x7E）
static const uint8_t font_8x8[][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 空格
    {0x00,0x00,0x5F,0x00,0x00,0x00,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14,0x00,0x00,0x00}, // #
    {0x24,0x2A,0x7F,0x2A,0x12,0x00,0x00,0x00}, // $
    {0x23,0x13,0x08,0x64,0x62,0x00,0x00,0x00}, // %
    {0x36,0x49,0x55,0x22,0x50,0x00,0x00,0x00}, // &
    {0x00,0x05,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00,0x00,0x00,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00,0x00,0x00,0x00}, // )
    {0x08,0x2A,0x1C,0x2A,0x08,0x00,0x00,0x00}, // *
    {0x08,0x08,0x3E,0x08,0x08,0x00,0x00,0x00}, // +
    {0x00,0x50,0x30,0x00,0x00,0x00,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00}, // -
    {0x00,0x60,0x60,0x00,0x00,0x00,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02,0x00,0x00,0x00}, // /
    {0x3E,0x51,0x49,0x45,0x3E,0x00,0x00,0x00}, // 0
    {0x00,0x42,0x7F,0x40,0x00,0x00,0x00,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46,0x00,0x00,0x00}, // 2
    {0x21,0x41,0x45,0x4B,0x31,0x00,0x00,0x00}, // 3
    {0x18,0x14,0x12,0x7F,0x10,0x00,0x00,0x00}, // 4
    {0x27,0x45,0x45,0x45,0x39,0x00,0x00,0x00}, // 5
    {0x3C,0x4A,0x49,0x49,0x30,0x00,0x00,0x00}, // 6
    {0x01,0x71,0x09,0x05,0x03,0x00,0x00,0x00}, // 7
    {0x36,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, // 8
    {0x06,0x49,0x49,0x29,0x1E,0x00,0x00,0x00}, // 9
    {0x00,0x36,0x36,0x00,0x00,0x00,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00,0x00,0x00,0x00}, // ;
    {0x00,0x08,0x14,0x22,0x41,0x00,0x00,0x00}, // <
    {0x14,0x14,0x14,0x14,0x14,0x00,0x00,0x00}, // =
    {0x41,0x22,0x14,0x08,0x00,0x00,0x00,0x00}, // >
    {0x02,0x01,0x51,0x09,0x06,0x00,0x00,0x00}, // ?
    {0x32,0x49,0x79,0x41,0x3E,0x00,0x00,0x00}, // @
    {0x7E,0x11,0x11,0x11,0x7E,0x00,0x00,0x00}, // A
    {0x7F,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, // B
    {0x3E,0x41,0x41,0x41,0x22,0x00,0x00,0x00}, // C
    {0x7F,0x41,0x41,0x22,0x1C,0x00,0x00,0x00}, // D
    {0x7F,0x49,0x49,0x49,0x41,0x00,0x00,0x00}, // E
    {0x7F,0x09,0x09,0x01,0x01,0x00,0x00,0x00}, // F
    {0x3E,0x41,0x41,0x51,0x32,0x00,0x00,0x00}, // G
    {0x7F,0x08,0x08,0x08,0x7F,0x00,0x00,0x00}, // H
    {0x00,0x41,0x7F,0x41,0x00,0x00,0x00,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01,0x00,0x00,0x00}, // J
    {0x7F,0x08,0x14,0x22,0x41,0x00,0x00,0x00}, // K
    {0x7F,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, // L
    {0x7F,0x02,0x04,0x02,0x7F,0x00,0x00,0x00}, // M
    {0x7F,0x04,0x08,0x10,0x7F,0x00,0x00,0x00}, // N
    {0x3E,0x41,0x41,0x41,0x3E,0x00,0x00,0x00}, // O
    {0x7F,0x09,0x09,0x09,0x06,0x00,0x00,0x00}, // P
    {0x3E,0x41,0x51,0x21,0x5E,0x00,0x00,0x00}, // Q
    {0x7F,0x09,0x19,0x29,0x46,0x00,0x00,0x00}, // R
    {0x46,0x49,0x49,0x49,0x31,0x00,0x00,0x00}, // S
    {0x01,0x01,0x7F,0x01,0x01,0x00,0x00,0x00}, // T
    {0x3F,0x40,0x40,0x40,0x3F,0x00,0x00,0x00}, // U
    {0x1F,0x20,0x40,0x20,0x1F,0x00,0x00,0x00}, // V
    {0x7F,0x20,0x18,0x20,0x7F,0x00,0x00,0x00}, // W
    {0x63,0x14,0x08,0x14,0x63,0x00,0x00,0x00}, // X
    {0x03,0x04,0x78,0x04,0x03,0x00,0x00,0x00}, // Y
    {0x61,0x51,0x49,0x45,0x43,0x00,0x00,0x00}, // Z
    {0x00,0x00,0x7F,0x41,0x41,0x00,0x00,0x00}, // [
    {0x02,0x04,0x08,0x10,0x20,0x00,0x00,0x00}, // back slash
    {0x41,0x41,0x7F,0x00,0x00,0x00,0x00,0x00}, // ]
    {0x04,0x02,0x01,0x02,0x04,0x00,0x00,0x00}, // ^
    {0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, // _
};

// I2C写命令
static esp_err_t oled_write_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};  // 0x00表示写命令
    return i2c_master_write_to_device(I2C_PORT, OLED_I2C_ADDRESS, 
                                     data, 2, pdMS_TO_TICKS(100));
}

// I2C写数据
static esp_err_t oled_write_data(uint8_t *data, size_t len) {
    uint8_t *buffer = malloc(len + 1);
    if (!buffer) return ESP_ERR_NO_MEM;
    
    buffer[0] = 0x40;  // 0x40表示写数据
    memcpy(buffer + 1, data, len);
    
    esp_err_t ret = i2c_master_write_to_device(I2C_PORT, OLED_I2C_ADDRESS,
                                               buffer, len + 1, pdMS_TO_TICKS(100));
    free(buffer);
    return ret;
}

// OLED初始化
esp_err_t oled_init(void) {
    // 初始化I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQUENCY,
    };
    
    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C初始化成功 (SDA=%d, SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);
    
    // 延迟等待OLED启动
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // SSD1306初始化序列
    oled_write_cmd(0xAE);  // Display OFF
    oled_write_cmd(0xD5);  // Set display clock
    oled_write_cmd(0x80);
    oled_write_cmd(0xA8);  // Set multiplex ratio
    oled_write_cmd(0x3F);  // 1/64 duty
    oled_write_cmd(0xD3);  // Set display offset
    oled_write_cmd(0x00);
    oled_write_cmd(0x40);  // Set start line
    oled_write_cmd(0x8D);  // Charge pump
    oled_write_cmd(0x14);  // Enable
    oled_write_cmd(0x20);  // Memory mode
    oled_write_cmd(0x00);  // Horizontal
    oled_write_cmd(0xA1);  // Segment remap
    oled_write_cmd(0xC8);  // COM scan direction
    oled_write_cmd(0xDA);  // COM pins
    oled_write_cmd(0x12);
    oled_write_cmd(0x81);  // Contrast
    oled_write_cmd(0xCF);
    oled_write_cmd(0xD9);  // Pre-charge
    oled_write_cmd(0xF1);
    oled_write_cmd(0xDB);  // VCOMH
    oled_write_cmd(0x40);
    oled_write_cmd(0xA4);  // Display all ON
    oled_write_cmd(0xA6);  // Normal display
    
    // 先清空显示缓冲区和屏幕（在开启显示前）
    memset(oled_buffer, 0, sizeof(oled_buffer));
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    oled_write_cmd(0xAF);  // Display ON
    
    // 再次清屏确保完全清除
    oled_clear();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "✅ OLED初始化成功 (SSD1306 128x64)");
    
    return ESP_OK;
}

// OLED反初始化
void oled_deinit(void) {
    oled_display(false);
    i2c_driver_delete(I2C_PORT);
    ESP_LOGI(TAG, "OLED已关闭");
}

// 清空显示
void oled_clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
    oled_refresh();
}

// 显示开关
void oled_display(bool on) {
    oled_write_cmd(on ? 0xAF : 0xAE);
}

// 设置对比度
void oled_set_contrast(uint8_t contrast) {
    oled_write_cmd(0x81);
    oled_write_cmd(contrast);
}

// 刷新显示
void oled_refresh(void) {
    for (int page = 0; page < 8; page++) {
        oled_write_cmd(0xB0 + page);  // 设置页地址
        oled_write_cmd(0x00);          // 设置列地址低位
        oled_write_cmd(0x10);          // 设置列地址高位
        oled_write_data(&oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
    }
}

// 画点
void oled_draw_pixel(uint8_t x, uint8_t y, bool on) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    
    uint16_t pos = x + (y / 8) * OLED_WIDTH;
    uint8_t bit = y % 8;
    
    if (on) {
        oled_buffer[pos] |= (1 << bit);
    } else {
        oled_buffer[pos] &= ~(1 << bit);
    }
}

// 显示字符
static void oled_show_char(uint8_t x, uint8_t y, char chr) {
    if (x >= OLED_WIDTH || y >= 8) return;
    if (chr < 0x20 || chr > 0x7E) chr = 0x20;  // 超出范围显示空格
    
    const uint8_t *font = font_8x8[chr - 0x20];
    for (int i = 0; i < 8; i++) {
        if (x + i < OLED_WIDTH) {
            oled_buffer[y * OLED_WIDTH + x + i] = font[i];
        }
    }
}

// 显示字符串
void oled_show_string(uint8_t x, uint8_t y, const char *str) {
    while (*str) {
        oled_show_char(x, y, *str);
        x += 8;
        if (x >= OLED_WIDTH) break;
        str++;
    }
}

// 按行显示字符串
void oled_show_line(uint8_t line, const char *str, oled_align_t align) {
    if (line >= 8) return;
    
    // 先清空该行的buffer（填充空格）
    memset(&oled_buffer[line * OLED_WIDTH], 0, OLED_WIDTH);
    
    int len = strlen(str);
    int x = 0;
    
    switch (align) {
        case OLED_ALIGN_CENTER:
            x = (OLED_WIDTH - len * 8) / 2;
            if (x < 0) x = 0;
            break;
        case OLED_ALIGN_RIGHT:
            x = OLED_WIDTH - len * 8;
            if (x < 0) x = 0;
            break;
        case OLED_ALIGN_LEFT:
        default:
            x = 0;
            break;
    }
    
    oled_show_string(x, line, str);
}

// 显示Logo（精简版）
void oled_show_logo(void) {
    // 清空所有内容
    memset(oled_buffer, 0, sizeof(oled_buffer));
    
    // 显示Logo内容
    oled_show_line(2, "ESP32-C3", OLED_ALIGN_CENTER);
    oled_show_line(4, "Starting...", OLED_ALIGN_CENTER);
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(50));  // 刷新后延迟
}

// 显示WiFi状态（精简版）
void oled_show_wifi_status(const char *ssid, const char *status) {
    char buf[32];
    // 精简显示：W:OK 或 W:NO
    snprintf(buf, sizeof(buf), "W:%s M:", status);
    oled_show_line(0, buf, OLED_ALIGN_LEFT);
}

// 显示MQTT状态（精简版）
void oled_show_mqtt_status(const char *status) {
    char buf[32];
    // MQTT状态显示在WiFi同一行
    snprintf(buf, sizeof(buf), "%s", status);
    oled_show_string(48, 0, buf);  // 从第48像素开始显示
}

// 显示传感器数据（精简版）
void oled_show_sensor_data(float temperature, float humidity) {
    char buf[32];
    snprintf(buf, sizeof(buf), "T:%.1fC", temperature);
    oled_show_line(2, buf, OLED_ALIGN_LEFT);
    snprintf(buf, sizeof(buf), "H:%.1f%%", humidity);
    oled_show_line(3, buf, OLED_ALIGN_LEFT);
}

// 显示IP地址（精简版）
void oled_show_ip(const char *ip) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.16s", ip);  // 最多16字符
    oled_show_line(5, buf, OLED_ALIGN_LEFT);
}

// 显示完整状态界面（精简版，适配0.96寸屏幕）
void oled_show_status_screen(
    const char *wifi_ssid,
    bool wifi_connected,
    bool mqtt_connected,
    float temperature,
    float humidity,
    uint32_t uptime_seconds
) {
    // 完全清空buffer
    memset(oled_buffer, 0, sizeof(oled_buffer));
    
    // 第0行：WiFi和MQTT状态（合并显示）
    char buf[32];
    snprintf(buf, sizeof(buf), "W:%s M:%s", 
             wifi_connected ? "OK" : "NO",
             mqtt_connected ? "OK" : "NO");
    oled_show_line(0, buf, OLED_ALIGN_LEFT);
    
    // 第2-3行：传感器数据
    oled_show_sensor_data(temperature, humidity);
    
    // 第5行：显示系统运行时间（小时:分钟）
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    snprintf(buf, sizeof(buf), "Run:%uh%02um", hours, minutes);
    oled_show_line(5, buf, OLED_ALIGN_LEFT);
    
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(50));  // 刷新后延迟
}

// 显示倒计时
void oled_show_countdown(int seconds) {
    // 完全清空buffer
    memset(oled_buffer, 0, sizeof(oled_buffer));
    
    // 显示倒计时
    oled_show_line(2, "[BOOT]", OLED_ALIGN_CENTER);
    
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", seconds);
    oled_show_line(5, buf, OLED_ALIGN_CENTER);
    
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(50));  // 刷新后延迟
}

// 显示启动提示
void oled_show_starting(void) {
    // 完全清空buffer
    memset(oled_buffer, 0, sizeof(oled_buffer));
    
    // 显示SETUP
    oled_show_line(3, "SETUP", OLED_ALIGN_CENTER);
    
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(50));  // 刷新后延迟
}

// 显示配网提示（优化版）
void oled_show_config_mode(const char *ap_ssid) {
    // 完全清空buffer
    memset(oled_buffer, 0, sizeof(oled_buffer));
    
    // 精简显示，增加行间距避免重叠
    oled_show_line(1, "SETUP MODE", OLED_ALIGN_CENTER);
    oled_show_line(4, ap_ssid, OLED_ALIGN_CENTER);
    oled_show_line(6, "192.168.4.1", OLED_ALIGN_CENTER);
    
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(50));  // 刷新后延迟
}

// 画线
void oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        oled_draw_pixel(x1, y1, true);
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// 画矩形
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill) {
    if (fill) {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                oled_draw_pixel(x + j, y + i, true);
            }
        }
    } else {
        oled_draw_line(x, y, x + w - 1, y);
        oled_draw_line(x + w - 1, y, x + w - 1, y + h - 1);
        oled_draw_line(x + w - 1, y + h - 1, x, y + h - 1);
        oled_draw_line(x, y + h - 1, x, y);
    }
}

// 测试显示
void oled_test(void) {
    ESP_LOGI(TAG, "开始OLED测试...");
    
    // 测试1: Logo
    oled_show_logo();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试2: 文本显示
    oled_clear();
    oled_show_line(0, "Line 0: TEST", OLED_ALIGN_LEFT);
    oled_show_line(2, "Center Text", OLED_ALIGN_CENTER);
    oled_show_line(4, "Right", OLED_ALIGN_RIGHT);
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试3: 图形
    oled_clear();
    oled_draw_rect(10, 10, 30, 20, false);
    oled_draw_rect(50, 10, 30, 20, true);
    oled_draw_line(0, 40, 127, 63);
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试4: 状态显示（运行时间：1小时1分5秒 = 3665秒）
    oled_show_status_screen("TestWiFi", true, true, 25.5, 60.2, 3665);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "✅ OLED测试完成");
}

