#include "lcd_st7789.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_common.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LCD_ST7789";

#define LCD_LEDC_CH LEDC_CHANNEL_2  // 使用CHANNEL_2避免与舵机冲突

// 背光状态监控变量
static uint8_t current_brightness = 0;
static bool backlight_initialized = false;

// 初始化背光PWM控制
static void lcd_init_backlight(void) {
    // 配置LEDC定时器 - 提高频率到25kHz以获得更好的背光效果
    const ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_2,  // 使用TIMER_2避免与舵机冲突
        .freq_hz = 25000,  // 提高到25kHz，减少闪烁并提高效率
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));

    // 配置LEDC通道
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = LCD_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_2,  // 使用TIMER_2避免与舵机冲突
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = LCD_BACKLIGHT_OUTPUT_INVERT,
        }
    };
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
    
    backlight_initialized = true;
    ESP_LOGI(TAG, "Backlight PWM initialized on GPIO%d (Channel %d, Timer %d)", 
             LCD_BACKLIGHT_PIN, LCD_LEDC_CH, LEDC_TIMER_2);
}

// 设置背光亮度 (0-150，允许超亮模式)
static void lcd_set_backlight(uint8_t brightness) {
    if (!backlight_initialized) {
        ESP_LOGW(TAG, "Backlight not initialized, initializing now...");
        lcd_init_backlight();
    }
    
    if (brightness > 150) {
        brightness = 150;  // 最大150%，提供超亮模式
    }
    
    // 计算占空比：10位分辨率 = 1024级别
    // 100% = 1024 * 100 / 100 = 1024
    // 150% = 1024 * 150 / 100 = 1536，但限制在1023以内
    uint32_t duty = (uint32_t)((brightness * 1023) / 100);
    if (duty > 1023) {
        duty = 1023;  // 硬件限制，确保不超过10位分辨率
    }
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set backlight duty: %s, reinitializing...", esp_err_to_name(ret));
        lcd_init_backlight();  // 重新初始化
        ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty);
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update backlight duty: %s", esp_err_to_name(ret));
        return;
    }
    
    current_brightness = brightness;  // 记录当前亮度
    ESP_LOGI(TAG, "Backlight brightness set to %d%% (duty: %lu/1023)", brightness, (unsigned long)duty);
}

// xiaozhi风格的LCD初始化
esp_err_t lcd_init(lcd_handle_t *lcd) {
    if (!lcd) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing LCD ST7789 (xiaozhi style)...");

    // 初始化背光控制
    lcd_init_backlight();

    // 初始化SPI总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = LCD_CLK_PIN,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置LCD面板IO
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_CS_PIN,
        .dc_gpio_num = LCD_DC_PIN,
        .spi_mode = 0,
        .pclk_hz = LCD_SPI_CLOCK,  // 40MHz
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ret = esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &lcd->panel_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置LCD面板
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ORDER,
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7789(lcd->panel_io, &panel_config, &lcd->panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ST7789 panel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 复位面板
    ESP_LOGI(TAG, "Resetting LCD panel...");
    esp_lcd_panel_reset(lcd->panel);

    // 初始化面板
    ESP_LOGI(TAG, "Initializing LCD panel...");
    esp_lcd_panel_init(lcd->panel);

    // xiaozhi风格的面板配置
    esp_lcd_panel_invert_color(lcd->panel, LCD_INVERT_COLOR);
    esp_lcd_panel_swap_xy(lcd->panel, LCD_SWAP_XY);
    esp_lcd_panel_mirror(lcd->panel, LCD_MIRROR_X, LCD_MIRROR_Y);

    // 开启显示
    ESP_LOGI(TAG, "Turning display on...");
    esp_lcd_panel_disp_on_off(lcd->panel, true);

    // 填充白色作为初始化测试
    ESP_LOGI(TAG, "Filling screen with white color...");
    lcd_fill_screen(lcd, COLOR_WHITE);

    // 开启背光
    lcd_backlight_on();

    lcd->width = LCD_WIDTH;
    lcd->height = LCD_HEIGHT;
    lcd->initialized = true;

    ESP_LOGI(TAG, "LCD ST7789 initialized successfully (xiaozhi style)");
    return ESP_OK;
}

// 恢复背光状态（用于在其他设备操作后恢复）
void lcd_restore_backlight(void) {
    if (current_brightness > 0) {
        ESP_LOGI(TAG, "Restoring backlight to %d%%", current_brightness);
        lcd_set_backlight(current_brightness);
    }
}

// LCD去初始化
esp_err_t lcd_deinit(lcd_handle_t *lcd) {
    if (!lcd || !lcd->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    lcd_backlight_off();
    esp_lcd_panel_disp_on_off(lcd->panel, false);
    
    if (lcd->panel) {
        esp_lcd_panel_del(lcd->panel);
        lcd->panel = NULL;
    }
    
    if (lcd->panel_io) {
        esp_lcd_panel_io_del(lcd->panel_io);
        lcd->panel_io = NULL;
    }
    
    spi_bus_free(LCD_SPI_HOST);
    
    lcd->initialized = false;
    ESP_LOGI(TAG, "LCD deinitialized");
    return ESP_OK;
}

// 背光开启
void lcd_backlight_on(void) {
    lcd_set_backlight(130);  // 默认130%亮度（超亮模式）
    ESP_LOGI(TAG, "Backlight ON - 130%% brightness (Super Bright Mode)");
}

// 背光关闭
void lcd_backlight_off(void) {
    lcd_set_backlight(0);
    ESP_LOGI(TAG, "Backlight OFF");
}

// 设置背光亮度
void lcd_set_brightness(uint8_t brightness) {
    lcd_set_backlight(brightness);
}

// 填充屏幕
esp_err_t lcd_fill_screen(lcd_handle_t *lcd, uint16_t color) {
    if (!lcd || !lcd->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // 创建颜色缓冲区
    uint16_t *color_buffer = malloc(LCD_WIDTH * sizeof(uint16_t));
    if (!color_buffer) {
        ESP_LOGE(TAG, "Failed to allocate color buffer");
        return ESP_ERR_NO_MEM;
    }

    // 填充颜色缓冲区
    for (int i = 0; i < LCD_WIDTH; i++) {
        color_buffer[i] = color;
    }

    // 逐行填充屏幕
    for (int y = 0; y < LCD_HEIGHT; y++) {
        esp_lcd_panel_draw_bitmap(lcd->panel, 0, y, LCD_WIDTH, y + 1, color_buffer);
    }

    free(color_buffer);
    return ESP_OK;
}

// 绘制位图
esp_err_t lcd_draw_bitmap(lcd_handle_t *lcd, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data) {
    if (!lcd || !lcd->initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_lcd_panel_draw_bitmap(lcd->panel, x, y, x + width, y + height, data);
}

// 8x8字体数据
static const uint8_t font_8x8[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (32)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // '!' (33)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // '"' (34)
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // '#' (35)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // '$' (36)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // '%' (37)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // '&' (38)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // ''' (39)
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // '(' (40)
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // ')' (41)
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // '*' (42)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // '+' (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ',' (44)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // '-' (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // '.' (46)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // '/' (47)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // '0' (48)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // '1' (49)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // '2' (50)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // '3' (51)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // '4' (52)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // '5' (53)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // '6' (54)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // '7' (55)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // '8' (56)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // '9' (57)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // ':' (58)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ';' (59)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // '<' (60)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // '=' (61)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // '>' (62)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // '?' (63)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // '@' (64)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // 'A' (65)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // 'B' (66)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // 'C' (67)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // 'D' (68)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // 'E' (69)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // 'F' (70)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // 'G' (71)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // 'H' (72)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // 'I' (73)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // 'J' (74)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // 'K' (75)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // 'L' (76)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // 'M' (77)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // 'N' (78)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // 'O' (79)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // 'P' (80)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // 'Q' (81)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // 'R' (82)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // 'S' (83)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // 'T' (84)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // 'U' (85)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // 'V' (86)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // 'W' (87)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // 'X' (88)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // 'Y' (89)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // 'Z' (90)
};

// 绘制矩形
esp_err_t lcd_draw_rectangle(lcd_handle_t *lcd, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!lcd || !lcd->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // 创建颜色缓冲区
    uint16_t *color_buffer = malloc(width * sizeof(uint16_t));
    if (!color_buffer) {
        ESP_LOGE(TAG, "Failed to allocate color buffer for rectangle");
        return ESP_ERR_NO_MEM;
    }

    // 填充颜色缓冲区
    for (int i = 0; i < width; i++) {
        color_buffer[i] = color;
    }

    // 逐行绘制矩形
    for (int row = 0; row < height; row++) {
        esp_lcd_panel_draw_bitmap(lcd->panel, x, y + row, x + width, y + row + 1, color_buffer);
    }

    free(color_buffer);
    return ESP_OK;
}

// 绘制字符
esp_err_t lcd_draw_char(lcd_handle_t *lcd, uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color) {
    if (!lcd || !lcd->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    if (c < 32 || c > 126) {
        return ESP_ERR_INVALID_ARG;  // 不支持的字符
    }

    // 检查边界
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return ESP_OK;  // 超出屏幕范围，直接返回
    }

    const uint8_t *font_data = font_8x8[c - 32];
    
    // 计算实际绘制区域（考虑屏幕边界）
    uint16_t draw_width = (x + 8 <= LCD_WIDTH) ? 8 : (LCD_WIDTH - x);
    uint16_t draw_height = (y + 8 <= LCD_HEIGHT) ? 8 : (LCD_HEIGHT - y);
    
    // 使用固定大小的缓冲区（最大8x8）
    uint16_t char_buffer[64];  // 8x8 = 64 pixels maximum
    
    // 填充字符缓冲区，只处理实际需要绘制的像素
    for (int row = 0; row < draw_height; row++) {
        for (int col = 0; col < draw_width; col++) {
            uint16_t pixel_color = (font_data[row] & (1 << (7 - col))) ? color : bg_color;
            char_buffer[row * draw_width + col] = pixel_color;
        }
    }
    
    // 一次性绘制字符
    esp_err_t ret = esp_lcd_panel_draw_bitmap(lcd->panel, x, y, x + draw_width, y + draw_height, char_buffer);
    
    return ret;
}

// 绘制字符串
esp_err_t lcd_draw_string(lcd_handle_t *lcd, uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color) {
    if (!lcd || !lcd->initialized || !str) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t current_x = x;
    
    while (*str && current_x < LCD_WIDTH) {
        esp_err_t ret = lcd_draw_char(lcd, current_x, y, *str, color, bg_color);
        if (ret != ESP_OK) {
            return ret;
        }
        
        current_x += 8;  // 字符宽度为8像素
        str++;
    }

    return ESP_OK;
}

// RGB转RGB565
uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ==================== 硬件诊断功能 ====================

// 检查GPIO引脚状态
static void lcd_check_gpio_status(void) {
    ESP_LOGI(TAG, "=== LCD GPIO Pin Status Check ===");
    
    // 检查所有LCD相关的GPIO引脚
    gpio_num_t lcd_pins[] = {
        LCD_MOSI_PIN,     // GPIO20 - SDA
        LCD_CLK_PIN,      // GPIO19 - SCL  
        LCD_RST_PIN,      // GPIO21 - RES
        LCD_DC_PIN,       // GPIO47 - DC
        LCD_CS_PIN,       // GPIO45 - CS
        LCD_BACKLIGHT_PIN // GPIO38 - BLK
    };
    
    const char* pin_names[] = {
        "MOSI/SDA", "CLK/SCL", "RST/RES", "DC", "CS", "BACKLIGHT/BLK"
    };
    
    for (int i = 0; i < sizeof(lcd_pins)/sizeof(lcd_pins[0]); i++) {
        gpio_num_t pin = lcd_pins[i];
        
        // 检查引脚是否有效
        if (!GPIO_IS_VALID_GPIO(pin)) {
            ESP_LOGE(TAG, "GPIO%d (%s): INVALID PIN NUMBER", pin, pin_names[i]);
            continue;
        }
        
        // 检查引脚是否可以作为输出
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
            ESP_LOGW(TAG, "GPIO%d (%s): Cannot be used as output", pin, pin_names[i]);
        }
        
        // 读取引脚当前状态
        int level = gpio_get_level(pin);
        ESP_LOGI(TAG, "GPIO%d (%s): Level=%d", pin, pin_names[i], level);
        
        // 检查引脚配置
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        
        esp_err_t ret = gpio_config(&io_conf);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "GPIO%d (%s): Configuration test PASSED", pin, pin_names[i]);
        } else {
            ESP_LOGE(TAG, "GPIO%d (%s): Configuration test FAILED - %s", pin, pin_names[i], esp_err_to_name(ret));
        }
    }
}

// 测试GPIO引脚输出功能
static void lcd_test_gpio_output(void) {
    ESP_LOGI(TAG, "=== LCD GPIO Output Test ===");
    
    // 测试背光引脚
    ESP_LOGI(TAG, "Testing backlight pin GPIO%d...", LCD_BACKLIGHT_PIN);
    gpio_set_level(LCD_BACKLIGHT_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Backlight HIGH for 500ms");
    
    gpio_set_level(LCD_BACKLIGHT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Backlight LOW for 500ms");
    
    // 测试复位引脚
    ESP_LOGI(TAG, "Testing reset pin GPIO%d...", LCD_RST_PIN);
    gpio_set_level(LCD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Reset LOW for 100ms");
    
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Reset HIGH for 100ms");
    
    // 测试CS引脚
    ESP_LOGI(TAG, "Testing CS pin GPIO%d...", LCD_CS_PIN);
    gpio_set_level(LCD_CS_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "CS LOW for 50ms");
    
    gpio_set_level(LCD_CS_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "CS HIGH for 50ms");
    
    // 测试DC引脚
    ESP_LOGI(TAG, "Testing DC pin GPIO%d...", LCD_DC_PIN);
    gpio_set_level(LCD_DC_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "DC LOW for 50ms");
    
    gpio_set_level(LCD_DC_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "DC HIGH for 50ms");
}

// 检查SPI总线状态
static void lcd_check_spi_status(void) {
    ESP_LOGI(TAG, "=== SPI Bus Status Check ===");
    
    // 检查SPI总线是否已初始化
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = LCD_CLK_PIN,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4096,
    };
    
    // 尝试初始化SPI总线（如果已初始化会返回错误）
    esp_err_t ret = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "SPI bus already initialized - GOOD");
        // 总线已初始化，这是正常的
    } else if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPI bus initialized successfully");
        // 释放刚才初始化的总线
        spi_bus_free(LCD_SPI_HOST);
    } else {
        ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "SPI Host: %d", LCD_SPI_HOST);
    ESP_LOGI(TAG, "MOSI Pin: GPIO%d", LCD_MOSI_PIN);
    ESP_LOGI(TAG, "SCLK Pin: GPIO%d", LCD_CLK_PIN);
    ESP_LOGI(TAG, "SPI Clock: %d Hz", LCD_SPI_CLOCK);
}

// 执行完整的LCD硬件诊断
void lcd_hardware_diagnosis(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "##########################################");
    ESP_LOGI(TAG, "#     LCD ST7789 Hardware Diagnosis     #");
    ESP_LOGI(TAG, "##########################################");
    ESP_LOGI(TAG, "");
    
    // 1. 检查GPIO引脚状态
    lcd_check_gpio_status();
    ESP_LOGI(TAG, "");
    
    // 2. 测试GPIO输出功能
    lcd_test_gpio_output();
    ESP_LOGI(TAG, "");
    
    // 3. 检查SPI总线状态
    lcd_check_spi_status();
    ESP_LOGI(TAG, "");
    
    // 4. 检查背光PWM状态
    ESP_LOGI(TAG, "=== Backlight PWM Status ===");
    ESP_LOGI(TAG, "Backlight initialized: %s", backlight_initialized ? "YES" : "NO");
    ESP_LOGI(TAG, "Current brightness: %d%%", current_brightness);
    ESP_LOGI(TAG, "PWM Channel: %d", LCD_LEDC_CH);
    ESP_LOGI(TAG, "PWM Timer: %d", LEDC_TIMER_2);
    ESP_LOGI(TAG, "");
    
    // 5. 显示引脚配置摘要
    ESP_LOGI(TAG, "=== Pin Configuration Summary ===");
    ESP_LOGI(TAG, "MOSI/SDA: GPIO%d", LCD_MOSI_PIN);
    ESP_LOGI(TAG, "CLK/SCL:  GPIO%d", LCD_CLK_PIN);
    ESP_LOGI(TAG, "RST/RES:  GPIO%d", LCD_RST_PIN);
    ESP_LOGI(TAG, "DC:       GPIO%d", LCD_DC_PIN);
    ESP_LOGI(TAG, "CS:       GPIO%d", LCD_CS_PIN);
    ESP_LOGI(TAG, "BLK:      GPIO%d", LCD_BACKLIGHT_PIN);
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "##########################################");
    ESP_LOGI(TAG, "#      Hardware Diagnosis Complete      #");
    ESP_LOGI(TAG, "##########################################");
    ESP_LOGI(TAG, "");
}