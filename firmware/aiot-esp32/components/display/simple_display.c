#include "simple_display.h"
#include <esp_log.h>
#include <esp_err.h>
#include <esp_mac.h>
#include <driver/ledc.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define TAG "SimpleDisplay"
#define LCD_LEDC_CH LEDC_CHANNEL_3  // 使用CHANNEL_3避免与舵机和LCD背光冲突

// 使用 LVGL 内置字体
LV_FONT_DECLARE(lv_font_montserrat_14);

static void init_backlight(gpio_num_t backlight_pin) {
    if (backlight_pin == GPIO_NUM_NC) {
        return;
    }

    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_3,  // 使用TIMER_3避免与舵机和LCD背光冲突
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置LEDC通道
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .timer_sel = LEDC_TIMER_3,  // 使用TIMER_3避免与舵机和LCD背光冲突
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = backlight_pin,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

simple_display_t* simple_display_init(esp_lcd_panel_io_handle_t panel_io, 
                                     esp_lcd_panel_handle_t panel,
                                     gpio_num_t backlight_pin, 
                                     bool backlight_output_invert,
                                     int width, int height,
                                     bool mirror_x, bool mirror_y, bool swap_xy) {
    
    simple_display_t *display = malloc(sizeof(simple_display_t));
    if (!display) {
        ESP_LOGE(TAG, "Failed to allocate memory for display");
        return NULL;
    }

    memset(display, 0, sizeof(simple_display_t));
    display->panel_io = panel_io;
    display->panel = panel;
    display->backlight_pin = backlight_pin;
    display->backlight_output_invert = backlight_output_invert;
    display->width = width;
    display->height = height;

    // 初始化背光
    init_backlight(backlight_pin);

    // 初始化LVGL
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    // 添加LCD显示
    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io,
        .panel_handle = panel,
        .buffer_size = width * height * sizeof(uint16_t) / 10,  // 使用1/10屏幕大小的缓冲区
        .double_buffer = false,  // 禁用双缓冲以节省内存
        .hres = width,
        .vres = height,
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
        },
    };

    display->display = lvgl_port_add_disp(&display_cfg);
    if (display->display == NULL) {
        ESP_LOGE(TAG, "Failed to add display");
        free(display);
        return NULL;
    }

    // 设置背光
    simple_display_set_backlight(display, 100);

    // 创建UI
    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        free(display);
        return NULL;
    }

    // 获取活动屏幕
    display->screen = lv_disp_get_scr_act(display->display);
    
    // 设置屏幕背景色为白色
    lv_obj_set_style_bg_color(display->screen, lv_color_white(), LV_PART_MAIN);
    
    // 禁用屏幕滚动条，避免出现滚动条占用显示空间
    lv_obj_set_scrollbar_mode(display->screen, LV_SCROLLBAR_MODE_OFF);

    // 创建产品前缀标签（蓝色）
    display->label_product_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_product_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_product_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_product_prefix, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_label_set_text(display->label_product_prefix, "ProductID:");
    
    // 创建产品内容标签（黑色）
    display->label_product = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_product, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_product, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_product, LV_ALIGN_TOP_LEFT, 85, 5);
    lv_label_set_text(display->label_product, "AIOT ESP32-S3");

    // 创建WiFi信息标签（合并WiFi ID和状态，黑色）
    display->label_wifi_id = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_wifi_id, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_wifi_id, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_wifi_id, LV_ALIGN_TOP_LEFT, 5, 25);
    lv_label_set_text(display->label_wifi_id, "Loading... : Starting...");

    // 隐藏不使用的WiFi标签（保持兼容性）
    display->label_wifi_id_prefix = NULL;
    display->label_wifi_status_prefix = NULL;
    display->label_wifi_status = NULL;

    // 创建MQTT状态前缀标签（蓝色）
    display->label_mqtt_status_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_mqtt_status_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_mqtt_status_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_mqtt_status_prefix, LV_ALIGN_TOP_LEFT, 5, 65);
    lv_label_set_text(display->label_mqtt_status_prefix, "MQTT:");
    
    // 创建MQTT状态内容标签（黑色）
    display->label_mqtt_status = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_mqtt_status, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_mqtt_status, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_mqtt_status, LV_ALIGN_TOP_LEFT, 55, 65);
    lv_label_set_text(display->label_mqtt_status, "Starting...");

    // 创建MAC地址前缀标签（蓝色）
    display->label_mac_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_mac_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_mac_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_mac_prefix, LV_ALIGN_TOP_LEFT, 5, 85);
    lv_label_set_text(display->label_mac_prefix, "MAC:");
    
    // 创建MAC地址内容标签（黑色）
    display->label_mac = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_mac, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_mac, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_mac, LV_ALIGN_TOP_LEFT, 45, 85);
    lv_label_set_text(display->label_mac, "Loading...");

    // 创建UUID前缀标签（蓝色）
    display->label_uuid_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_uuid_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_uuid_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_uuid_prefix, LV_ALIGN_TOP_LEFT, 5, 105);
    lv_label_set_text(display->label_uuid_prefix, "Device UUID:");
    
    // 创建UUID内容标签（黑色）
    display->label_uuid = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_uuid, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_uuid, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_uuid, LV_ALIGN_TOP_LEFT, 5, 125);
    lv_label_set_text(display->label_uuid, "Loading...");
    // 设置支持换行和宽度限制，为长Device ID做准备
    lv_label_set_long_mode(display->label_uuid, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(display->label_uuid, 230);

    // 创建服务器地址前缀标签（蓝色）
    display->label_mqtt_address_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_mqtt_address_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_mqtt_address_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_mqtt_address_prefix, LV_ALIGN_TOP_LEFT, 5, 45);
    lv_label_set_text(display->label_mqtt_address_prefix, "Server:");
    
    // 创建服务器地址内容标签（黑色）
    display->label_mqtt_address = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_mqtt_address, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_mqtt_address, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_mqtt_address, LV_ALIGN_TOP_LEFT, 60, 45);
    lv_label_set_text(display->label_mqtt_address, "Loading...");

    // 创建运行时间前缀标签（蓝色）
    display->label_uptime_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_uptime_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_uptime_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_uptime_prefix, LV_ALIGN_TOP_LEFT, 5, 185);
    lv_label_set_text(display->label_uptime_prefix, "Uptime:");
    
    // 创建运行时间内容标签（黑色）
    display->label_uptime = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_uptime, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_uptime, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_uptime, LV_ALIGN_TOP_LEFT, 70, 185);
    lv_label_set_text(display->label_uptime, "0 minutes");

    // 创建温湿度前缀标签（蓝色）
    display->label_temp_hum_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_temp_hum_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_temp_hum_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_temp_hum_prefix, LV_ALIGN_TOP_LEFT, 5, 165);
    lv_label_set_text(display->label_temp_hum_prefix, "T&H:");
    
    // 创建温湿度内容标签（黑色）
    display->label_temp_hum = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_temp_hum, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_temp_hum, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_temp_hum, LV_ALIGN_TOP_LEFT, 45, 165);
    lv_label_set_text(display->label_temp_hum, "-- °C / -- %");
    
    // 创建版本号前缀标签（蓝色）
    display->label_version_prefix = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_version_prefix, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(display->label_version_prefix, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_version_prefix, LV_ALIGN_TOP_LEFT, 5, 205);
    lv_label_set_text(display->label_version_prefix, "Version:");
    
    // 创建版本号内容标签（黑色）
    display->label_version = lv_label_create(display->screen);
    lv_obj_set_style_text_color(display->label_version, lv_color_black(), 0);
    lv_obj_set_style_text_font(display->label_version, &lv_font_montserrat_14, 0);
    lv_obj_align(display->label_version, LV_ALIGN_TOP_LEFT, 75, 205);
    lv_label_set_text(display->label_version, "v1.0.0");

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Simple display initialized successfully");
    return display;
}

void simple_display_set_backlight(simple_display_t *display, uint8_t brightness) {
    if (!display || display->backlight_pin == GPIO_NUM_NC) {
        return;
    }

    // LEDC分辨率设置为10位，因此：100% = 1023
    uint32_t duty_cycle = (1023 * brightness) / 100;
    if (display->backlight_output_invert) {
        duty_cycle = 1023 - duty_cycle;
    }
    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
}

void simple_display_show_info(simple_display_t *display, const char *title, const char *mac, const char *status) {
    if (!display) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    if (mac && display->label_mac) {
        // 应用28字符限制
        char limited_mac[32];
        if (strlen(mac) > 28) {
            strncpy(limited_mac, mac, 25);
            limited_mac[25] = '.';
            limited_mac[26] = '.';
            limited_mac[27] = '.';
            limited_mac[28] = '\0';
        } else {
            strcpy(limited_mac, mac);
        }
        
        lv_label_set_text(display->label_mac, limited_mac);
    }

    lvgl_port_unlock();
    ESP_LOGI(TAG, "显示信息: %s | %s | %s", title ? title : "N/A", mac ? mac : "N/A", status ? status : "N/A");
}

void simple_display_show_detailed_info(simple_display_t *display, 
                                      const char *device,
                                      const char *product, 
                                      const char *wifi_id,
                                      const char *wifi_status,
                                      const char *mqtt_status,
                                      const char *mac,
                                      const char *uuid,
                                      const char *server_address) {
    if (!display) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    // 显示所有prefix标签（恢复正常显示模式）
    if (display->label_product_prefix) {
        lv_obj_clear_flag(display->label_product_prefix, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_wifi_id_prefix) {
        lv_obj_clear_flag(display->label_wifi_id_prefix, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_mqtt_status_prefix) {
        lv_obj_clear_flag(display->label_mqtt_status_prefix, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_mac_prefix) {
        lv_obj_clear_flag(display->label_mac_prefix, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_uuid_prefix) {
        lv_obj_clear_flag(display->label_uuid_prefix, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_mqtt_address_prefix) {
        lv_obj_clear_flag(display->label_mqtt_address_prefix, LV_OBJ_FLAG_HIDDEN);
    }

    if (product && display->label_product) {
            // 限制产品名称显示为28个字符
            char truncated_product[32];
            strncpy(truncated_product, product, sizeof(truncated_product) - 1);
            truncated_product[sizeof(truncated_product) - 1] = '\0';
            
            // 如果超过28个字符，截断并添加省略号
            if (strlen(truncated_product) > 28) {
                truncated_product[25] = '.';
                truncated_product[26] = '.';
                truncated_product[27] = '.';
                truncated_product[28] = '\0';
            }
            
            lv_label_set_text(display->label_product, truncated_product);
        lv_obj_clear_flag(display->label_product, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 合并WiFi ID和状态显示，限制为28个字符
    if ((wifi_id || wifi_status) && display->label_wifi_id) {
            char wifi_combined[64];
            const char *id = wifi_id ? wifi_id : "Unknown";
            const char *status = wifi_status ? wifi_status : "Unknown";
            snprintf(wifi_combined, sizeof(wifi_combined), "%s : %s", id, status);
            
            // 如果超过28个字符，截断并添加省略号
            if (strlen(wifi_combined) > 28) {
                wifi_combined[25] = '.';
                wifi_combined[26] = '.';
                wifi_combined[27] = '.';
                wifi_combined[28] = '\0';
            }
            
            lv_label_set_text(display->label_wifi_id, wifi_combined);
        lv_obj_clear_flag(display->label_wifi_id, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (mqtt_status && display->label_mqtt_status) {
            // 限制MQTT状态显示为28个字符
            char truncated_mqtt_status[32];
            strncpy(truncated_mqtt_status, mqtt_status, sizeof(truncated_mqtt_status) - 1);
            truncated_mqtt_status[sizeof(truncated_mqtt_status) - 1] = '\0';
            
            // 如果超过28个字符，截断并添加省略号
            if (strlen(truncated_mqtt_status) > 28) {
                truncated_mqtt_status[25] = '.';
                truncated_mqtt_status[26] = '.';
                truncated_mqtt_status[27] = '.';
                truncated_mqtt_status[28] = '\0';
            }
            
            lv_label_set_text(display->label_mqtt_status, truncated_mqtt_status);
        lv_obj_clear_flag(display->label_mqtt_status, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (mac && display->label_mac) {
            // 限制MAC地址显示为28个字符
            char truncated_mac[32];
            strncpy(truncated_mac, mac, sizeof(truncated_mac) - 1);
            truncated_mac[sizeof(truncated_mac) - 1] = '\0';
            
            // 如果超过28个字符，截断并添加省略号
            if (strlen(truncated_mac) > 28) {
                truncated_mac[25] = '.';
                truncated_mac[26] = '.';
                truncated_mac[27] = '.';
                truncated_mac[28] = '\0';
            }
            
            lv_label_set_text(display->label_mac, truncated_mac);
        lv_obj_clear_flag(display->label_mac, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (uuid) {
        // 使用新的Device UUID更新函数，支持长文本处理
        lvgl_port_unlock(); // 先解锁，因为update_device_id会重新加锁
        simple_display_update_device_id(display, uuid);
        if (!lvgl_port_lock(3000)) {
            ESP_LOGE(TAG, "Failed to re-lock LVGL");
            return;
        }
        lv_obj_clear_flag(display->label_uuid, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 显示服务器地址
    if (server_address && display->label_mqtt_address) {
        char server_text[64];
        // 去除http://或https://前缀以便显示
        const char *addr = server_address;
        if (strncmp(addr, "http://", 7) == 0) {
            addr = addr + 7;
        } else if (strncmp(addr, "https://", 8) == 0) {
            addr = addr + 8;
        }
        snprintf(server_text, sizeof(server_text), "%s", addr);
        
        // 限制为28字符
        if (strlen(server_text) > 28) {
            server_text[25] = '.';
            server_text[26] = '.';
            server_text[27] = '.';
            server_text[28] = '\0';
            }
            
        lv_label_set_text(display->label_mqtt_address, server_text);
        lv_obj_clear_flag(display->label_mqtt_address, LV_OBJ_FLAG_HIDDEN);
    }

    lvgl_port_unlock();
}

void simple_display_update_status(simple_display_t *display, const char *status) {
    if (!display || !status) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    // 由于WiFi信息现在是合并显示的，这个函数主要用于MQTT状态更新
    if (display->label_mqtt_status) {
        // 检查是否是WiFi状态更新，如果是则忽略（由show_detailed_info处理）
        if (strstr(status, "WiFi:") == NULL) {
            // 限制状态显示为28个字符
            char truncated_status[32];
            strncpy(truncated_status, status, sizeof(truncated_status) - 1);
            truncated_status[sizeof(truncated_status) - 1] = '\0';
            
            // 如果超过28个字符，截断并添加省略号
            if (strlen(truncated_status) > 28) {
                truncated_status[25] = '.';
                truncated_status[26] = '.';
                truncated_status[27] = '.';
                truncated_status[28] = '\0';
            }
            
            lv_label_set_text(display->label_mqtt_status, truncated_status);
        }
    }

    lvgl_port_unlock();
}

void simple_display_update_wifi_status(simple_display_t *display, const char *wifi_id, const char *wifi_status) {
    if (!display || !wifi_id || !wifi_status) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    if (display->label_wifi_id) {
        // 创建合并的WiFi信息字符串，限制为28个字符
        char combined_wifi[128];
        snprintf(combined_wifi, sizeof(combined_wifi), "%s : %s", wifi_id, wifi_status);
        
        // 如果超过28个字符，截断并添加省略号
        if (strlen(combined_wifi) > 28) {
            combined_wifi[25] = '.';
            combined_wifi[26] = '.';
            combined_wifi[27] = '.';
            combined_wifi[28] = '\0';
        }
        
        lv_label_set_text(display->label_wifi_id, combined_wifi);
    }

    lvgl_port_unlock();
}

void simple_display_update_mqtt_address(simple_display_t *display, const char *mqtt_address) {
    if (!display || !mqtt_address) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    if (display->label_mqtt_address) {
        // 应用28字符限制
        char limited_address[32];
        if (strlen(mqtt_address) > 28) {
            strncpy(limited_address, mqtt_address, 25);
            limited_address[25] = '.';
            limited_address[26] = '.';
            limited_address[27] = '.';
            limited_address[28] = '\0';
        } else {
            strcpy(limited_address, mqtt_address);
        }
        
        lv_label_set_text(display->label_mqtt_address, limited_address);
    }

    lvgl_port_unlock();
}

void simple_display_update_uptime(simple_display_t *display, uint32_t uptime_seconds) {
    if (!display || !display->label_uptime) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    // 计算天、小时、分钟、秒
    uint32_t days = uptime_seconds / 86400;        // 86400秒 = 1天
    uint32_t hours = (uptime_seconds % 86400) / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;

    char uptime_str[48];
    if (days > 0) {
        // 显示天和小时：例如 "3d 12h"
        snprintf(uptime_str, sizeof(uptime_str), "%" PRIu32 "d %" PRIu32 "h", days, hours);
    } else if (hours > 0) {
        // 显示小时和分钟：例如 "12h 30m"
        snprintf(uptime_str, sizeof(uptime_str), "%" PRIu32 "h %" PRIu32 "m", hours, minutes);
    } else if (minutes > 0) {
        // 显示分钟和秒：例如 "30m 45s"
        snprintf(uptime_str, sizeof(uptime_str), "%" PRIu32 "m %" PRIu32 "s", minutes, seconds);
    } else {
        // 只显示秒：例如 "45s"
        snprintf(uptime_str, sizeof(uptime_str), "%" PRIu32 "s", seconds);
    }

    lv_label_set_text(display->label_uptime, uptime_str);

    lvgl_port_unlock();
}

void simple_display_update_mqtt_status(simple_display_t *display, const char *mqtt_status) {
    if (!display || !display->label_mqtt_status || !mqtt_status) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    // 限制MQTT状态显示为28个字符
    char truncated_mqtt_status[32];
    strncpy(truncated_mqtt_status, mqtt_status, sizeof(truncated_mqtt_status) - 1);
    truncated_mqtt_status[sizeof(truncated_mqtt_status) - 1] = '\0';
    
    // 如果超过28个字符，截断并添加省略号
    if (strlen(truncated_mqtt_status) > 28) {
        truncated_mqtt_status[25] = '.';
        truncated_mqtt_status[26] = '.';
        truncated_mqtt_status[27] = '.';
        truncated_mqtt_status[28] = '\0';
    }
    
    lv_label_set_text(display->label_mqtt_status, truncated_mqtt_status);

    lvgl_port_unlock();
}

void simple_display_update_device_id(simple_display_t *display, const char *device_id) {
    if (!display || !display->label_uuid || !device_id) {
        ESP_LOGE(TAG, "Device ID update failed: invalid parameters");
        return;
    }

    ESP_LOGI(TAG, "Updating Device ID: %s (length: %d)", device_id, strlen(device_id));

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    // 严格限制：每行最大28个字符，总共两行
    const int max_chars_per_line = 28;
    const int max_chars_line2 = 25;  // 第二行预留3个字符给省略号
    int device_id_len = strlen(device_id);
    
    char formatted_text[128] = {0};
    
    if (device_id_len <= max_chars_per_line) {
        // 短Device ID，单行显示
        strcpy(formatted_text, device_id);
        ESP_LOGI(TAG, "Short Device ID, single line: %s", formatted_text);
    } else {
        // 长Device ID，严格限制为两行
        char line1[32] = {0};
        char line2[32] = {0};
        
        // 第一行：前28个字符
        strncpy(line1, device_id, max_chars_per_line);
        line1[max_chars_per_line] = '\0';
        
        // 第二行：最多25个字符 + "..."
        int remaining_start = max_chars_per_line;
        int remaining_len = device_id_len - remaining_start;
        
        if (remaining_len > max_chars_line2) {
            // 超出两行限制，截断并添加省略号
            strncpy(line2, device_id + remaining_start, max_chars_line2);
            line2[max_chars_line2] = '\0';
            strcat(line2, "...");
            ESP_LOGI(TAG, "Long Device ID truncated - Line1: %s, Line2: %s", line1, line2);
        } else {
            // 剩余内容可以完整显示在第二行
            strcpy(line2, device_id + remaining_start);
            ESP_LOGI(TAG, "Long Device ID fits in two lines - Line1: %s, Line2: %s", line1, line2);
        }
        
        // 组合两行文本
        snprintf(formatted_text, sizeof(formatted_text), "%s\n%s", line1, line2);
    }
    
    lv_label_set_text(display->label_uuid, formatted_text);
    
    // 禁用自动换行，使用我们手动控制的换行
    lv_label_set_long_mode(display->label_uuid, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(display->label_uuid, 240); // 设置足够的宽度避免自动换行

    lvgl_port_unlock();
}

void simple_display_update_temp_hum(simple_display_t *display, float temperature, float humidity) {
    if (!display) {
        return;
    }

    char temp_hum_str[32];
    snprintf(temp_hum_str, sizeof(temp_hum_str), "%.1f°C / %.1f%%", temperature, humidity);

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    if (display->label_temp_hum) {
        lv_label_set_text(display->label_temp_hum, temp_hum_str);
    }

    lvgl_port_unlock();
}

void simple_display_show_sensor_data(simple_display_t *display, const char *sensor_data) {
    if (!display || !sensor_data) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    if (display->label_temp_hum) {
        // 应用28字符限制
        char limited_data[32];
        if (strlen(sensor_data) > 28) {
            strncpy(limited_data, sensor_data, 25);
            limited_data[25] = '.';
            limited_data[26] = '.';
            limited_data[27] = '.';
            limited_data[28] = '\0';
        } else {
            strcpy(limited_data, sensor_data);
        }
        
        lv_label_set_text(display->label_temp_hum, limited_data);
    }

    lvgl_port_unlock();
}

void simple_display_show_provisioning_info(simple_display_t *display, const char *ap_ssid, const char *config_url) {
    if (!display) {
        return;
    }
    
    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    ESP_LOGI(TAG, "显示配网引导信息: AP=%s, URL=%s", ap_ssid ? ap_ssid : "N/A", config_url ? config_url : "N/A");

    // ============================================================
    // 清空所有标签（确保没有残留内容）
    // ============================================================
    // 隐藏所有prefix标签
    if (display->label_product_prefix) lv_obj_add_flag(display->label_product_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_wifi_id_prefix) lv_obj_add_flag(display->label_wifi_id_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mqtt_status_prefix) lv_obj_add_flag(display->label_mqtt_status_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mac_prefix) lv_obj_add_flag(display->label_mac_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_uuid_prefix) lv_obj_add_flag(display->label_uuid_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mqtt_address_prefix) lv_obj_add_flag(display->label_mqtt_address_prefix, LV_OBJ_FLAG_HIDDEN);

    // 清空所有content标签（避免显示旧内容）
    if (display->label_product) lv_label_set_text(display->label_product, "");
    if (display->label_wifi_id) lv_label_set_text(display->label_wifi_id, "");
    if (display->label_mqtt_status) lv_label_set_text(display->label_mqtt_status, "");
    if (display->label_mac) lv_label_set_text(display->label_mac, "");
    if (display->label_uuid) lv_label_set_text(display->label_uuid, "");
    if (display->label_mqtt_address) lv_label_set_text(display->label_mqtt_address, "");

    // ============================================================
    // 配网UI - 清晰简洁的5行布局
    // ============================================================
    int y_pos = 10;
    const int line_height = 48; // 24pt字体 + 间距
    
    // 第1行：标题 "WiFi Setup"
    if (display->label_product) {
        lv_label_set_text(display->label_product, "WiFi Setup");
        lv_obj_set_style_text_color(display->label_product, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_align(display->label_product, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_product, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(display->label_product, display->width - 10);
        lv_obj_align(display->label_product, LV_ALIGN_TOP_LEFT, 5, y_pos);
        lv_obj_clear_flag(display->label_product, LV_OBJ_FLAG_HIDDEN);
    }
    y_pos += line_height;

    // 第2行：AP SSID
    if (ap_ssid && display->label_wifi_id) {
        char wifi_text[80];
        snprintf(wifi_text, sizeof(wifi_text), "AP:%s", ap_ssid);
        lv_label_set_text(display->label_wifi_id, wifi_text);
        lv_obj_set_style_text_color(display->label_wifi_id, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_align(display->label_wifi_id, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_wifi_id, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(display->label_wifi_id, display->width - 10);
        lv_obj_align(display->label_wifi_id, LV_ALIGN_TOP_LEFT, 5, y_pos);
        lv_obj_clear_flag(display->label_wifi_id, LV_OBJ_FLAG_HIDDEN);
    }
    y_pos += line_height;

    // 第3行：配网地址（简化显示，只显示IP）
    // 增加额外间距，让第三行往下移
    y_pos += 10; // 额外的间距
    
    if (config_url && display->label_mqtt_address) {
        // 提取IP地址（去掉http://前缀）
        const char *ip_addr = config_url;
        if (strncmp(config_url, "http://", 7) == 0) {
            ip_addr = config_url + 7;
        }
        
        char url_text[80];
        snprintf(url_text, sizeof(url_text), "URL:%s", ip_addr);
        lv_label_set_text(display->label_mqtt_address, url_text);
        lv_obj_set_style_text_color(display->label_mqtt_address, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_align(display->label_mqtt_address, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_set_style_text_font(display->label_mqtt_address, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_mqtt_address, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(display->label_mqtt_address, display->width - 10);
        lv_obj_align(display->label_mqtt_address, LV_ALIGN_TOP_LEFT, 5, y_pos);
        lv_obj_clear_flag(display->label_mqtt_address, LV_OBJ_FLAG_HIDDEN);
    }
    y_pos += line_height;
    
    // 第4行：MAC地址（显示在URL下方）
    // 获取MAC地址并显示
    static char g_mac_str[32] = {0};
    static bool g_mac_initialized = false;
    
    if (!g_mac_initialized) {
        uint8_t mac[6];
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
            snprintf(g_mac_str, sizeof(g_mac_str), "MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            g_mac_initialized = true;
        } else {
            snprintf(g_mac_str, sizeof(g_mac_str), "MAC: N/A");
        }
    }
    
    if (display->label_mac) {
        lv_label_set_text(display->label_mac, g_mac_str);
        lv_obj_set_style_text_color(display->label_mac, lv_color_hex(0x0000CC), LV_PART_MAIN);  // 深蓝色
        lv_obj_set_style_text_align(display->label_mac, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_set_style_text_font(display->label_mac, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_mac, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(display->label_mac, display->width - 10);
        lv_obj_align(display->label_mac, LV_ALIGN_TOP_LEFT, 5, y_pos);
        lv_obj_clear_flag(display->label_mac, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 隐藏步骤说明标签（不需要显示）
    if (display->label_mqtt_status) {
        lv_obj_add_flag(display->label_mqtt_status, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_uuid) {
        lv_obj_add_flag(display->label_uuid, LV_OBJ_FLAG_HIDDEN);
    }
    
    lvgl_port_unlock();
    ESP_LOGI(TAG, "配网引导信息已显示在LCD上");
}

/**
 * @brief 清除屏幕并准备显示启动UI
 */
void simple_display_clear_for_startup(simple_display_t *display) {
    if (!display) {
        return;
    }

    // 隐藏所有标签（包括prefix和content）
    if (display->label_product_prefix) lv_obj_add_flag(display->label_product_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_product) lv_obj_add_flag(display->label_product, LV_OBJ_FLAG_HIDDEN);
    if (display->label_wifi_id_prefix) lv_obj_add_flag(display->label_wifi_id_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_wifi_id) lv_obj_add_flag(display->label_wifi_id, LV_OBJ_FLAG_HIDDEN);
    if (display->label_wifi_status_prefix) lv_obj_add_flag(display->label_wifi_status_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_wifi_status) lv_obj_add_flag(display->label_wifi_status, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mqtt_status_prefix) lv_obj_add_flag(display->label_mqtt_status_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mqtt_status) lv_obj_add_flag(display->label_mqtt_status, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mqtt_address_prefix) lv_obj_add_flag(display->label_mqtt_address_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mqtt_address) lv_obj_add_flag(display->label_mqtt_address, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mac_prefix) lv_obj_add_flag(display->label_mac_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_mac) lv_obj_add_flag(display->label_mac, LV_OBJ_FLAG_HIDDEN);
    if (display->label_uuid_prefix) lv_obj_add_flag(display->label_uuid_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_uuid) lv_obj_add_flag(display->label_uuid, LV_OBJ_FLAG_HIDDEN);
    if (display->label_uptime_prefix) lv_obj_add_flag(display->label_uptime_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_uptime) lv_obj_add_flag(display->label_uptime, LV_OBJ_FLAG_HIDDEN);
    if (display->label_temp_hum_prefix) lv_obj_add_flag(display->label_temp_hum_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_temp_hum) lv_obj_add_flag(display->label_temp_hum, LV_OBJ_FLAG_HIDDEN);
    if (display->label_version_prefix) lv_obj_add_flag(display->label_version_prefix, LV_OBJ_FLAG_HIDDEN);
    if (display->label_version) lv_obj_add_flag(display->label_version, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 显示启动步骤信息（独立UI模板，全英文，清晰布局）
 * 
 * Layout:
 * Line 1: [STARTUP] (Blue title)
 * Line 2: Step Name (Cyan, prominent)
 * Line 3: Status (Yellow/Green/Red based on state)
 * Line 4-8: Reserved for future use
 */
void simple_display_show_startup_step(simple_display_t *display, const char *step_name, const char *status) {
    if (!display) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for startup step");
        return;
    }

    // 清除所有标签
    simple_display_clear_for_startup(display);

    // Line 1: Stage Name (24号字体, 黑色, 左对齐, X=5, Y=10)
    if (step_name && display->label_product) {
        lv_label_set_text(display->label_product, step_name);
        lv_obj_set_style_text_color(display->label_product, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_align(display->label_product, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_set_style_text_font(display->label_product, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_product, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(display->label_product, display->width - 10);
        lv_obj_align(display->label_product, LV_ALIGN_TOP_LEFT, 5, 10);
        lv_obj_clear_flag(display->label_product, LV_OBJ_FLAG_HIDDEN);
    }

    // Line 2: Status Message (24号字体, 黑色, 左对齐, X=5, Y=45)
    if (status && display->label_wifi_id) {
        lv_label_set_text(display->label_wifi_id, status);
        lv_obj_set_style_text_color(display->label_wifi_id, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_align(display->label_wifi_id, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_set_style_text_font(display->label_wifi_id, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_wifi_id, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(display->label_wifi_id, display->width - 10);
        lv_obj_align(display->label_wifi_id, LV_ALIGN_TOP_LEFT, 5, 45);
        lv_obj_clear_flag(display->label_wifi_id, LV_OBJ_FLAG_HIDDEN);
    }

    // Line 3: MAC Address (24号字体, 蓝色, 左对齐, X=5, Y=176 - 往下移动两行, 带冒号分隔, 自动换行)
    // 获取MAC地址并显示（使用全局静态变量避免重复读取）
    static char g_mac_str[32] = {0};
    static bool g_mac_initialized = false;
    
    if (!g_mac_initialized) {
        uint8_t mac[6];
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
            snprintf(g_mac_str, sizeof(g_mac_str), "MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            g_mac_initialized = true;
        } else {
            snprintf(g_mac_str, sizeof(g_mac_str), "MAC: N/A");
        }
    }
    
    if (display->label_mac) {
        lv_label_set_text(display->label_mac, g_mac_str);
        lv_obj_set_style_text_color(display->label_mac, lv_color_hex(0x0000CC), LV_PART_MAIN);  // 深蓝色
        lv_obj_set_style_text_align(display->label_mac, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_set_style_text_font(display->label_mac, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_long_mode(display->label_mac, LV_LABEL_LONG_WRAP);  // 自动换行
        lv_obj_set_width(display->label_mac, display->width - 10);
        lv_obj_align(display->label_mac, LV_ALIGN_TOP_LEFT, 5, 176);
        lv_obj_clear_flag(display->label_mac, LV_OBJ_FLAG_HIDDEN);
    }

    // 隐藏其他不使用的标签
    if (display->label_wifi_status) {
        lv_obj_add_flag(display->label_wifi_status, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_mqtt_status) {
        lv_obj_add_flag(display->label_mqtt_status, LV_OBJ_FLAG_HIDDEN);
    }
    if (display->label_mqtt_address) {
        lv_obj_add_flag(display->label_mqtt_address, LV_OBJ_FLAG_HIDDEN);
    }

    lvgl_port_unlock();
    ESP_LOGI(TAG, "Startup UI: [%s] %s", 
             step_name ? step_name : "N/A", 
             status ? status : "N/A");
}

void simple_display_test_colors(simple_display_t *display) {
    if (!display) {
        return;
    }

    ESP_LOGI(TAG, "开始LCD彩色测试...");

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    // 创建临时的全屏对象用于颜色测试
    lv_obj_t *test_obj = lv_obj_create(display->screen);
    lv_obj_set_size(test_obj, display->width, display->height);
    lv_obj_align(test_obj, LV_ALIGN_CENTER, 0, 0);

    // 测试红色
    lv_obj_set_style_bg_color(test_obj, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (!lvgl_port_lock(3000)) return;
    // 测试绿色
    lv_obj_set_style_bg_color(test_obj, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (!lvgl_port_lock(3000)) return;
    // 测试蓝色
    lv_obj_set_style_bg_color(test_obj, lv_color_hex(0x0000FF), LV_PART_MAIN);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (!lvgl_port_lock(3000)) return;
    // 删除测试对象，恢复正常显示
    lv_obj_del(test_obj);
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "LCD彩色测试完成");
}

void simple_display_show_registration_info(simple_display_t *display, 
                                          const char *product_id, 
                                          const char *mac_address) {
    if (!display || !product_id || !mac_address) {
        ESP_LOGE(TAG, "Invalid parameters for registration info display");
        return;
    }

    ESP_LOGI(TAG, "显示设备注册信息 - Product ID: %s, MAC: %s", product_id, mac_address);

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for registration info");
        return;
    }

    // 清空屏幕，创建新的布局
    lv_obj_clean(display->screen);
    lv_obj_set_style_bg_color(display->screen, lv_color_hex(0x000000), LV_PART_MAIN);

    // 标题：Device Registration （使用14号字体）
    lv_obj_t *label_title = lv_label_create(display->screen);
    lv_label_set_text(label_title, "Device Registration");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_title, display->width - 20);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 5);

    // 分隔线
    lv_obj_t *line1 = lv_obj_create(display->screen);
    lv_obj_set_size(line1, display->width - 40, 2);
    lv_obj_set_style_bg_color(line1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(line1, 0, LV_PART_MAIN);
    lv_obj_align(line1, LV_ALIGN_TOP_MID, 0, 28);

    // Product ID 标签（使用14号字体，青色）
    lv_obj_t *label_product_prefix = lv_label_create(display->screen);
    lv_label_set_text(label_product_prefix, "Product ID:");
    lv_obj_set_style_text_font(label_product_prefix, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_product_prefix, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_align(label_product_prefix, LV_ALIGN_TOP_LEFT, 10, 40);

    // Product ID 值（使用14号字体，放大2倍缩放，黄色醒目）
    lv_obj_t *label_product_value = lv_label_create(display->screen);
    lv_label_set_text(label_product_value, product_id);
    lv_obj_set_style_text_font(label_product_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_product_value, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // 黄色，醒目
    lv_obj_set_style_text_align(label_product_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    // 使用变换放大文字（1.8倍）
    lv_obj_set_style_transform_zoom(label_product_value, 280, LV_PART_MAIN);  // 280 = 1.8x * 256
    lv_obj_set_width(label_product_value, display->width - 20);
    lv_obj_align(label_product_value, LV_ALIGN_TOP_MID, 0, 65);

    // MAC Address 标签（使用14号字体，青色）
    lv_obj_t *label_mac_prefix = lv_label_create(display->screen);
    lv_label_set_text(label_mac_prefix, "MAC Address:");
    lv_obj_set_style_text_font(label_mac_prefix, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_mac_prefix, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_align(label_mac_prefix, LV_ALIGN_TOP_LEFT, 10, 115);

    // MAC Address 值（使用14号字体，放大2倍缩放，黄色醒目）
    lv_obj_t *label_mac_value = lv_label_create(display->screen);
    lv_label_set_text(label_mac_value, mac_address);
    lv_obj_set_style_text_font(label_mac_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_mac_value, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // 黄色，醒目
    lv_obj_set_style_text_align(label_mac_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    // 使用变换放大文字（1.8倍）
    lv_obj_set_style_transform_zoom(label_mac_value, 280, LV_PART_MAIN);  // 280 = 1.8x * 256
    lv_obj_set_width(label_mac_value, display->width - 20);
    lv_obj_align(label_mac_value, LV_ALIGN_TOP_MID, 0, 140);

    // 底部提示信息（使用14号字体，灰色）
    lv_obj_t *label_hint = lv_label_create(display->screen);
    lv_label_set_text(label_hint, "Please register\nusing the info above");
    lv_obj_set_style_text_font(label_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0xAAAAAA), LV_PART_MAIN);  // 灰色提示
    lv_obj_set_style_text_align(label_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_hint, display->width - 20);
    lv_obj_align(label_hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "设备注册信息已显示在LCD上");
}

void simple_display_show_not_registered_info(simple_display_t *display, const char *mac_address) {
    if (!display) {
        ESP_LOGE(TAG, "Invalid display parameter");
        return;
    }

    ESP_LOGI(TAG, "显示设备未注册提示信息 - MAC: %s", mac_address ? mac_address : "N/A");

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for not registered info");
        return;
    }

    // 清空屏幕，创建新的布局
    lv_obj_clean(display->screen);
    lv_obj_set_style_bg_color(display->screen, lv_color_hex(0x000000), LV_PART_MAIN);

    // 标题：Device Not Registered （使用14号字体，红色警告）
    lv_obj_t *label_title = lv_label_create(display->screen);
    lv_label_set_text(label_title, "Device Not Registered");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFF0000), LV_PART_MAIN);  // 红色警告
    lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_title, display->width - 20);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 5);

    // 分隔线
    lv_obj_t *line1 = lv_obj_create(display->screen);
    lv_obj_set_size(line1, display->width - 40, 2);
    lv_obj_set_style_bg_color(line1, lv_color_hex(0xFF0000), LV_PART_MAIN);  // 红色分隔线
    lv_obj_set_style_border_width(line1, 0, LV_PART_MAIN);
    lv_obj_align(line1, LV_ALIGN_TOP_MID, 0, 28);

    // 提示信息（使用14号字体，黄色）
    lv_obj_t *label_hint1 = lv_label_create(display->screen);
    lv_label_set_text(label_hint1, "WiFi Connected");
    lv_obj_set_style_text_font(label_hint1, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_hint1, lv_color_hex(0x00FF00), LV_PART_MAIN);  // 绿色，表示WiFi已连接
    lv_obj_set_style_text_align(label_hint1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_hint1, display->width - 20);
    lv_obj_align(label_hint1, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *label_hint2 = lv_label_create(display->screen);
    lv_label_set_text(label_hint2, "Please register");
    lv_obj_set_style_text_font(label_hint2, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_hint2, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // 黄色提示
    lv_obj_set_style_text_align(label_hint2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_hint2, display->width - 20);
    lv_obj_align(label_hint2, LV_ALIGN_TOP_MID, 0, 65);

    lv_obj_t *label_hint3 = lv_label_create(display->screen);
    lv_label_set_text(label_hint3, "device in backend");
    lv_obj_set_style_text_font(label_hint3, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_hint3, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // 黄色提示
    lv_obj_set_style_text_align(label_hint3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_hint3, display->width - 20);
    lv_obj_align(label_hint3, LV_ALIGN_TOP_MID, 0, 90);

    // MAC地址显示（如果提供）
    if (mac_address && strlen(mac_address) > 0) {
        lv_obj_t *label_mac_prefix = lv_label_create(display->screen);
        lv_label_set_text(label_mac_prefix, "MAC Address:");
        lv_obj_set_style_text_font(label_mac_prefix, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(label_mac_prefix, lv_color_hex(0x00FFFF), LV_PART_MAIN);  // 青色
        lv_obj_align(label_mac_prefix, LV_ALIGN_TOP_LEFT, 10, 125);

        lv_obj_t *label_mac_value = lv_label_create(display->screen);
        lv_label_set_text(label_mac_value, mac_address);
        lv_obj_set_style_text_font(label_mac_value, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(label_mac_value, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // 黄色，醒目
        lv_obj_set_style_text_align(label_mac_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(label_mac_value, display->width - 20);
        lv_obj_align(label_mac_value, LV_ALIGN_TOP_MID, 0, 150);
    }

    // 底部提示信息（使用14号字体，灰色）
    lv_obj_t *label_bottom_hint = lv_label_create(display->screen);
    lv_label_set_text(label_bottom_hint, "Long press Boot\nfor provisioning");
    lv_obj_set_style_text_font(label_bottom_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_bottom_hint, lv_color_hex(0xAAAAAA), LV_PART_MAIN);  // 灰色提示
    lv_obj_set_style_text_align(label_bottom_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label_bottom_hint, display->width - 20);
    lv_obj_align(label_bottom_hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "设备未注册提示信息已显示在LCD上");
}

void simple_display_show_runtime_main(simple_display_t *display,
                                     const char *product_id,
                                     const char *wifi_status,
                                     const char *mqtt_status,
                                     const char *device_uuid,
                                     float temperature,
                                     float humidity,
                                     uint32_t uptime_seconds) {
    if (!display || !display->screen) {
        ESP_LOGW(TAG, "Display not initialized");
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGW(TAG, "Failed to lock LVGL");
        return;
    }

    // 清空屏幕（这会删除所有子对象）
    lv_obj_clean(display->screen);
    
    // ⚠️ 重要：清空传感器UI指针，因为对象已被删除
    for (int i = 0; i < MAX_SENSOR_LABELS; i++) {
        display->sensor_labels[i] = NULL;
        display->sensor_label_prefixes[i] = NULL;
    }
    display->sensor_count = 0;

    // 设置屏幕背景色为白色
    lv_obj_set_style_bg_color(display->screen, lv_color_white(), LV_PART_MAIN);

    // 定义颜色
    lv_color_t color_title = lv_color_hex(0x0066CC);     // 深蓝色 - 标题
    lv_color_t color_value = lv_color_black();           // 黑色 - 值
    lv_color_t color_green = lv_color_hex(0x00AA00);     // 绿色 - 连接状态
    lv_color_t color_red = lv_color_hex(0xCC0000);       // 红色 - 断开状态
    lv_color_t color_uuid = lv_color_hex(0x0066CC);      // 深蓝色 - UUID

    int y_offset = 5;   // 起始Y坐标
    int line_height = 19;  // 行高
    int small_gap = 3;  // 小间距

    // ========== 第1行：Product ID ==========
    lv_obj_t *label_product_title = lv_label_create(display->screen);
    lv_label_set_text(label_product_title, "Product:");
    lv_obj_set_style_text_font(label_product_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_product_title, color_title, LV_PART_MAIN);
    lv_obj_align(label_product_title, LV_ALIGN_TOP_LEFT, 5, y_offset);

    lv_obj_t *label_product_value = lv_label_create(display->screen);
    lv_label_set_text(label_product_value, product_id ? product_id : "Unknown");
    lv_obj_set_style_text_font(label_product_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_product_value, color_value, LV_PART_MAIN);
    lv_obj_align(label_product_value, LV_ALIGN_TOP_LEFT, 75, y_offset);

    y_offset += line_height;

    // ========== 第2行：WiFi Status ==========
    lv_obj_t *label_wifi_title = lv_label_create(display->screen);
    lv_label_set_text(label_wifi_title, "WiFi:");
    lv_obj_set_style_text_font(label_wifi_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_wifi_title, color_title, LV_PART_MAIN);
    lv_obj_align(label_wifi_title, LV_ALIGN_TOP_LEFT, 5, y_offset);

    lv_obj_t *label_wifi_value = lv_label_create(display->screen);
    lv_label_set_text(label_wifi_value, wifi_status ? wifi_status : "Unknown");
    lv_obj_set_style_text_font(label_wifi_value, &lv_font_montserrat_14, LV_PART_MAIN);
    // 根据状态显示颜色
    bool wifi_connected = (wifi_status && strcmp(wifi_status, "Connected") == 0);
    lv_obj_set_style_text_color(label_wifi_value, wifi_connected ? color_green : color_red, LV_PART_MAIN);
    lv_obj_align(label_wifi_value, LV_ALIGN_TOP_LEFT, 75, y_offset);

    y_offset += line_height;

    // ========== 第3行：MQTT Status ==========
    lv_obj_t *label_mqtt_title = lv_label_create(display->screen);
    lv_label_set_text(label_mqtt_title, "MQTT:");
    lv_obj_set_style_text_font(label_mqtt_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_mqtt_title, color_title, LV_PART_MAIN);
    lv_obj_align(label_mqtt_title, LV_ALIGN_TOP_LEFT, 5, y_offset);

    lv_obj_t *label_mqtt_value = lv_label_create(display->screen);
    lv_label_set_text(label_mqtt_value, mqtt_status ? mqtt_status : "Unknown");
    lv_obj_set_style_text_font(label_mqtt_value, &lv_font_montserrat_14, LV_PART_MAIN);
    // 根据状态显示颜色
    bool mqtt_connected = (mqtt_status && strcmp(mqtt_status, "Connected") == 0);
    lv_obj_set_style_text_color(label_mqtt_value, mqtt_connected ? color_green : color_red, LV_PART_MAIN);
    lv_obj_align(label_mqtt_value, LV_ALIGN_TOP_LEFT, 75, y_offset);

    y_offset += line_height + small_gap;

    // ========== 第4-5行：UUID（可能换行） ==========
    lv_obj_t *label_uuid_title = lv_label_create(display->screen);
    lv_label_set_text(label_uuid_title, "UUID:");
    lv_obj_set_style_text_font(label_uuid_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_uuid_title, color_title, LV_PART_MAIN);
    lv_obj_align(label_uuid_title, LV_ALIGN_TOP_LEFT, 5, y_offset);

    lv_obj_t *label_uuid_value = lv_label_create(display->screen);
    lv_label_set_text(label_uuid_value, device_uuid ? device_uuid : "Unknown");
    lv_obj_set_style_text_font(label_uuid_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_uuid_value, color_uuid, LV_PART_MAIN);
    lv_label_set_long_mode(label_uuid_value, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_uuid_value, display->width - 55);  // 留出左边距
    lv_obj_align(label_uuid_value, LV_ALIGN_TOP_LEFT, 50, y_offset);

    y_offset += line_height * 2 + small_gap;  // UUID可能占2行

    // ========== Sensor Data行已删除，由下方动态传感器UI显示 ==========
    // 不再显示汇总的Sensor行，直接显示Uptime

    // ========== 第6行：Uptime ==========
    lv_obj_t *label_uptime_title = lv_label_create(display->screen);
    lv_label_set_text(label_uptime_title, "Uptime:");
    lv_obj_set_style_text_font(label_uptime_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_uptime_title, color_title, LV_PART_MAIN);
    lv_obj_align(label_uptime_title, LV_ALIGN_TOP_LEFT, 5, y_offset);

    char uptime_text[32];
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;
    
    if (hours > 0) {
        snprintf(uptime_text, sizeof(uptime_text), "%luh%lum%lus", 
                 (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    } else if (minutes > 0) {
        snprintf(uptime_text, sizeof(uptime_text), "%lum%lus", 
                 (unsigned long)minutes, (unsigned long)seconds);
    } else {
        snprintf(uptime_text, sizeof(uptime_text), "%lus", (unsigned long)seconds);
    }

    lv_obj_t *label_uptime_value = lv_label_create(display->screen);
    lv_label_set_text(label_uptime_value, uptime_text);
    lv_obj_set_style_text_font(label_uptime_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_uptime_value, color_value, LV_PART_MAIN);
    lv_obj_align(label_uptime_value, LV_ALIGN_TOP_LEFT, 75, y_offset);

    // 保存引用以便后续更新（可选）
    display->label_wifi_status = label_wifi_value;
    display->label_mqtt_status = label_mqtt_value;
    // label_temp_hum不再需要，已由动态传感器UI显示
    display->label_uptime = label_uptime_value;

    lvgl_port_unlock();

    ESP_LOGI(TAG, "运行时主界面已显示");
}

void simple_display_destroy(simple_display_t *display) {
    if (!display) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for cleanup");
    } else {
        // 删除所有LVGL对象
        if (display->label_product_prefix) {
            lv_obj_del(display->label_product_prefix);
        }
        if (display->label_product) {
            lv_obj_del(display->label_product);
        }
        if (display->label_wifi_id_prefix) {
            lv_obj_del(display->label_wifi_id_prefix);
        }
        if (display->label_wifi_id) {
            lv_obj_del(display->label_wifi_id);
        }
        if (display->label_wifi_status_prefix) {
            lv_obj_del(display->label_wifi_status_prefix);
        }
        if (display->label_wifi_status) {
            lv_obj_del(display->label_wifi_status);
        }
        if (display->label_mqtt_status_prefix) {
            lv_obj_del(display->label_mqtt_status_prefix);
        }
        if (display->label_mqtt_status) {
            lv_obj_del(display->label_mqtt_status);
        }
        if (display->label_mac_prefix) {
            lv_obj_del(display->label_mac_prefix);
        }
        if (display->label_mac) {
            lv_obj_del(display->label_mac);
        }
        if (display->label_uuid_prefix) {
            lv_obj_del(display->label_uuid_prefix);
        }
        if (display->label_uuid) {
            lv_obj_del(display->label_uuid);
        }
        if (display->label_mqtt_address_prefix) {
            lv_obj_del(display->label_mqtt_address_prefix);
        }
        if (display->label_mqtt_address) {
            lv_obj_del(display->label_mqtt_address);
        }
        if (display->label_uptime_prefix) {
            lv_obj_del(display->label_uptime_prefix);
        }
        if (display->label_uptime) {
            lv_obj_del(display->label_uptime);
        }
        if (display->label_temp_hum_prefix) {
            lv_obj_del(display->label_temp_hum_prefix);
        }
        if (display->label_temp_hum) {
            lv_obj_del(display->label_temp_hum);
        }
        if (display->label_version_prefix) {
            lv_obj_del(display->label_version_prefix);
        }
        if (display->label_version) {
            lv_obj_del(display->label_version);
        }
        lvgl_port_unlock();
    }
    
    lvgl_port_deinit();
    free(display);
}

// ==================== 动态传感器UI功能 ====================

void simple_display_init_sensor_ui(simple_display_t *display, const board_sensor_config_t *sensor_config) {
    if (!display || !sensor_config) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for sensor UI init");
        return;
    }

    // 清除旧的传感器标签
    for (int i = 0; i < MAX_SENSOR_LABELS; i++) {
        if (display->sensor_labels[i]) {
            lv_obj_del(display->sensor_labels[i]);
            display->sensor_labels[i] = NULL;
        }
        if (display->sensor_label_prefixes[i]) {
            lv_obj_del(display->sensor_label_prefixes[i]);
            display->sensor_label_prefixes[i] = NULL;
        }
    }

    // 根据板子配置创建传感器标签
    display->sensor_count = sensor_config->sensor_count;
    if (display->sensor_count > MAX_SENSOR_LABELS) {
        display->sensor_count = MAX_SENSOR_LABELS;
    }

    int y_pos = 140;  // 传感器显示区域起始Y坐标
    const int line_height = 24;  // 每个传感器占用的高度

            for (int i = 0; i < display->sensor_count; i++) {
        const sensor_display_info_t *sensor = &sensor_config->sensor_list[i];
        
        // 创建传感器名称标签（前缀）
        display->sensor_label_prefixes[i] = lv_label_create(display->screen);
        if (display->sensor_label_prefixes[i]) {
            char prefix_text[32];
            snprintf(prefix_text, sizeof(prefix_text), "%s:", sensor->name);
            lv_label_set_text(display->sensor_label_prefixes[i], prefix_text);
            lv_obj_set_style_text_color(display->sensor_label_prefixes[i], lv_color_hex(0x000080), LV_PART_MAIN);  // 深蓝色
            lv_obj_set_style_text_font(display->sensor_label_prefixes[i], &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_align(display->sensor_label_prefixes[i], LV_ALIGN_TOP_LEFT, 5, y_pos);
        }

        // 创建传感器数据标签
        display->sensor_labels[i] = lv_label_create(display->screen);
        if (display->sensor_labels[i]) {
            lv_label_set_text(display->sensor_labels[i], "-- --");
            lv_obj_set_style_text_color(display->sensor_labels[i], lv_color_black(), LV_PART_MAIN);
            lv_obj_set_style_text_font(display->sensor_labels[i], &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_align(display->sensor_labels[i], LV_ALIGN_TOP_LEFT, 90, y_pos);
        }

        y_pos += line_height;
    }

    ESP_LOGI(TAG, "初始化传感器UI完成: %d个传感器", display->sensor_count);
    
    lvgl_port_unlock();
}

void simple_display_update_sensor_value(simple_display_t *display, int sensor_index, const char *value) {
    if (!display || sensor_index < 0 || sensor_index >= display->sensor_count || !value) {
        return;
    }

    if (!lvgl_port_lock(3000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for sensor update");
        return;
    }

    if (display->sensor_labels[sensor_index]) {
        lv_label_set_text(display->sensor_labels[sensor_index], value);
    }

    lvgl_port_unlock();
}