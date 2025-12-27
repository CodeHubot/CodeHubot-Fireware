#ifndef LCD_ST7789_H
#define LCD_ST7789_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

// LCD 尺寸定义 (240x240)
#define LCD_WIDTH  240
#define LCD_HEIGHT 240

// SPI配置
#define LCD_SPI_HOST    SPI3_HOST
#define LCD_SPI_CLOCK   40000000   // 40MHz

// 引脚定义 (根据AIOT ESP32-S3硬件配置)
#define LCD_MOSI_PIN     GPIO_NUM_20  // 数据信号
#define LCD_CLK_PIN      GPIO_NUM_19  // 时钟线
#define LCD_RST_PIN      GPIO_NUM_21  // 复位
#define LCD_DC_PIN       GPIO_NUM_47  // 数据选择
#define LCD_CS_PIN       GPIO_NUM_45  // 片选
#define LCD_BACKLIGHT_PIN GPIO_NUM_38  // 背光

// 显示配置
#define LCD_INVERT_COLOR    true
#define LCD_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define LCD_MIRROR_X        true
#define LCD_MIRROR_Y        false
#define LCD_SWAP_XY         true
#define LCD_OFFSET_X        0
#define LCD_OFFSET_Y        0
#define LCD_BACKLIGHT_OUTPUT_INVERT false

// 颜色定义 (RGB565格式)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

// LCD结构体
typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    uint16_t width;
    uint16_t height;
    bool initialized;
} lcd_handle_t;

// 函数声明
esp_err_t lcd_init(lcd_handle_t *lcd);
esp_err_t lcd_deinit(lcd_handle_t *lcd);
void lcd_backlight_on(void);
void lcd_backlight_off(void);
void lcd_set_brightness(uint8_t brightness);  // 设置背光亮度 (0-150)
void lcd_restore_backlight(void);  // 恢复背光状态
esp_err_t lcd_fill_screen(lcd_handle_t *lcd, uint16_t color);
esp_err_t lcd_draw_bitmap(lcd_handle_t *lcd, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data);
esp_err_t lcd_draw_rectangle(lcd_handle_t *lcd, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
esp_err_t lcd_draw_char(lcd_handle_t *lcd, uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color);
esp_err_t lcd_draw_string(lcd_handle_t *lcd, uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color);
uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

// 硬件诊断功能
void lcd_hardware_diagnosis(void);

#endif // LCD_ST7789_H