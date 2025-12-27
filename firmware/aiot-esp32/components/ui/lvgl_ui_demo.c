/**
 * @file lvgl_ui_demo.c
 * @brief LVGL UI Demo Implementation for AIOT ESP32-S3 Project
 * @version 1.0
 * @date 2024-01-20
 * 
 * Simple LVGL UI demonstration with various widgets and animations
 */

#include "lvgl_ui_demo.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "lvgl.h"
#endif

static const char *TAG = "LVGL_UI_DEMO";

#ifdef ESP_PLATFORM
/**
 * @brief Timer callback for updating UI elements
 */
static void ui_update_timer_cb(lv_timer_t *timer)
{
    lvgl_ui_demo_handle_t *demo_handle = (lvgl_ui_demo_handle_t *)timer->user_data;
    
    if (demo_handle == NULL || !demo_handle->initialized) {
        return;
    }

    // Update counter
    demo_handle->demo_counter++;
    
    // Add debug log every 10 updates (every 5 seconds)
    if (demo_handle->demo_counter % 10 == 0) {
        ESP_LOGI(TAG, "UI update timer callback - counter: %lu", demo_handle->demo_counter);
    }

    // Update time display
    if (demo_handle->time_label) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        lv_label_set_text(demo_handle->time_label, time_str);
    }

    // Simulate temperature changes
    if (demo_handle->temp_arc && demo_handle->temp_label) {
        float temp = 20.0f + 10.0f * sin(demo_handle->demo_counter * 0.1f);
        int32_t temp_value = (int32_t)((temp + 10.0f) * 100.0f / 50.0f); // Scale to 0-100
        
        lv_arc_set_value(demo_handle->temp_arc, temp_value);
        
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.1f°C", temp);
        lv_label_set_text(demo_handle->temp_label, temp_str);
    }

    // Animate progress bar
    if (demo_handle->progress_bar) {
        int32_t progress = (demo_handle->demo_counter * 2) % 100;
        lv_bar_set_value(demo_handle->progress_bar, progress, LV_ANIM_ON);
    }

    // Simulate connection status changes
    if (demo_handle->demo_counter % 10 == 0) {
        bool wifi_status = (demo_handle->demo_counter / 10) % 2;
        bool mqtt_status = (demo_handle->demo_counter / 10 + 1) % 2;
        bool ble_status = (demo_handle->demo_counter / 10 + 2) % 2;
        
        lvgl_ui_demo_update_status(demo_handle, wifi_status, mqtt_status, ble_status);
    }
}

/**
 * @brief Create status LED indicator
 */
static lv_obj_t* create_status_led(lv_obj_t *parent, const char *label_text, lv_coord_t x, lv_coord_t y)
{
    // Create container
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 60, 40);
    lv_obj_set_pos(cont, x, y);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create LED
    lv_obj_t *led = lv_led_create(cont);
    lv_obj_set_size(led, 20, 20);
    lv_obj_align(led, LV_ALIGN_TOP_MID, 0, 5);
    lv_led_off(led);
    
    // Create label
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, label_text);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    
    return led;
}
#endif

esp_err_t lvgl_ui_demo_init(lvgl_display_handle_t *display_handle, lvgl_ui_demo_handle_t *demo_handle)
{
    if (display_handle == NULL || demo_handle == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (!display_handle->initialized) {
        ESP_LOGE(TAG, "Display not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing LVGL UI demo");

    // Clear handle structure
    memset(demo_handle, 0, sizeof(lvgl_ui_demo_handle_t));
    demo_handle->display_handle = display_handle;

#ifdef ESP_PLATFORM
    // Create main screen
    demo_handle->main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(demo_handle->main_screen, lv_color_hex(0x000000), 0);
    
    // Create title label
    lv_obj_t *title_label = lv_label_create(demo_handle->main_screen);
    lv_label_set_text(title_label, "AIOT ESP32-S3");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // Create status label
    demo_handle->status_label = lv_label_create(demo_handle->main_screen);
    lv_label_set_text(demo_handle->status_label, "System Ready");
    lv_obj_set_style_text_color(demo_handle->status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(demo_handle->status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(demo_handle->status_label, LV_ALIGN_TOP_MID, 0, 35);

    // Create time label
    demo_handle->time_label = lv_label_create(demo_handle->main_screen);
    lv_label_set_text(demo_handle->time_label, "00:00:00");
    lv_obj_set_style_text_color(demo_handle->time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(demo_handle->time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(demo_handle->time_label, LV_ALIGN_TOP_MID, 0, 55);

    // Create temperature arc
    demo_handle->temp_arc = lv_arc_create(demo_handle->main_screen);
    lv_obj_set_size(demo_handle->temp_arc, 80, 80);
    lv_obj_align(demo_handle->temp_arc, LV_ALIGN_CENTER, -60, -10);
    lv_arc_set_range(demo_handle->temp_arc, 0, 100);
    lv_arc_set_value(demo_handle->temp_arc, 50);
    lv_obj_set_style_arc_color(demo_handle->temp_arc, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(demo_handle->temp_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(demo_handle->temp_arc, 8, LV_PART_MAIN);

    // Create temperature label
    demo_handle->temp_label = lv_label_create(demo_handle->temp_arc);
    lv_label_set_text(demo_handle->temp_label, "25.0°C");
    lv_obj_set_style_text_color(demo_handle->temp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(demo_handle->temp_label, &lv_font_montserrat_14, 0);
    lv_obj_center(demo_handle->temp_label);

    // Create humidity arc
    demo_handle->humidity_arc = lv_arc_create(demo_handle->main_screen);
    lv_obj_set_size(demo_handle->humidity_arc, 80, 80);
    lv_obj_align(demo_handle->humidity_arc, LV_ALIGN_CENTER, 60, -10);
    lv_arc_set_range(demo_handle->humidity_arc, 0, 100);
    lv_arc_set_value(demo_handle->humidity_arc, 60);
    lv_obj_set_style_arc_color(demo_handle->humidity_arc, lv_color_hex(0x0066FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(demo_handle->humidity_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(demo_handle->humidity_arc, 8, LV_PART_MAIN);

    // Create humidity label
    demo_handle->humidity_label = lv_label_create(demo_handle->humidity_arc);
    lv_label_set_text(demo_handle->humidity_label, "60.0%");
    lv_obj_set_style_text_color(demo_handle->humidity_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(demo_handle->humidity_label, &lv_font_montserrat_14, 0);
    lv_obj_center(demo_handle->humidity_label);

    // Create combined temperature and humidity label (similar to reference project)
    demo_handle->temp_hum_label = lv_label_create(demo_handle->main_screen);
    lv_label_set_text(demo_handle->temp_hum_label, "25.0 °C / 60.0 %");
    lv_obj_set_style_text_color(demo_handle->temp_hum_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(demo_handle->temp_hum_label, &lv_font_montserrat_14, 0);
    lv_obj_align(demo_handle->temp_hum_label, LV_ALIGN_CENTER, 0, 50);

    // Create status LEDs
    demo_handle->wifi_led = create_status_led(demo_handle->main_screen, "WiFi", 20, 80);
    demo_handle->mqtt_led = create_status_led(demo_handle->main_screen, "MQTT", 90, 80);
    demo_handle->ble_led = create_status_led(demo_handle->main_screen, "BLE", 160, 80);

    // Create progress bar
    demo_handle->progress_bar = lv_bar_create(demo_handle->main_screen);
    lv_obj_set_size(demo_handle->progress_bar, 200, 20);
    lv_obj_align(demo_handle->progress_bar, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_bar_set_range(demo_handle->progress_bar, 0, 100);
    lv_bar_set_value(demo_handle->progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(demo_handle->progress_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(demo_handle->progress_bar, lv_color_hex(0x00AA00), LV_PART_INDICATOR);

    // Create progress label
    lv_obj_t *progress_label = lv_label_create(demo_handle->main_screen);
    lv_label_set_text(progress_label, "Progress");
    lv_obj_set_style_text_color(progress_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_14, 0);
    lv_obj_align(progress_label, LV_ALIGN_BOTTOM_MID, 0, -65);

    // Load the screen
    lv_scr_load(demo_handle->main_screen);

    // Create update timer
    demo_handle->update_timer = lv_timer_create(ui_update_timer_cb, UI_DEMO_UPDATE_PERIOD_MS, demo_handle);
    if (demo_handle->update_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create update timer");
        goto cleanup;
    }

    ESP_LOGI(TAG, "LVGL UI demo screen created and loaded");
#endif

    demo_handle->initialized = true;
    ESP_LOGI(TAG, "LVGL UI demo initialized successfully");

    return ESP_OK;

#ifdef ESP_PLATFORM
cleanup:
    if (demo_handle->main_screen) {
        lv_obj_del(demo_handle->main_screen);
        demo_handle->main_screen = NULL;
    }
    return ESP_FAIL;
#endif
}

esp_err_t lvgl_ui_demo_deinit(lvgl_ui_demo_handle_t *demo_handle)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        ESP_LOGE(TAG, "Demo not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing LVGL UI demo");

#ifdef ESP_PLATFORM
    // Delete update timer
    if (demo_handle->update_timer) {
        lv_timer_del(demo_handle->update_timer);
        demo_handle->update_timer = NULL;
    }

    // Delete main screen
    if (demo_handle->main_screen) {
        lv_obj_del(demo_handle->main_screen);
        demo_handle->main_screen = NULL;
    }
#endif

    demo_handle->initialized = false;
    ESP_LOGI(TAG, "LVGL UI demo deinitialized");

    return ESP_OK;
}

esp_err_t lvgl_ui_demo_update_status(lvgl_ui_demo_handle_t *demo_handle, 
                                    bool wifi_connected, 
                                    bool mqtt_connected, 
                                    bool ble_connected)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#ifdef ESP_PLATFORM
    // Update WiFi LED
    if (demo_handle->wifi_led) {
        if (wifi_connected) {
            lv_led_on(demo_handle->wifi_led);
            lv_obj_set_style_bg_color(demo_handle->wifi_led, lv_color_hex(0x00FF00), LV_PART_MAIN);
        } else {
            lv_led_off(demo_handle->wifi_led);
            lv_obj_set_style_bg_color(demo_handle->wifi_led, lv_color_hex(0xFF0000), LV_PART_MAIN);
        }
    }

    // Update MQTT LED
    if (demo_handle->mqtt_led) {
        if (mqtt_connected) {
            lv_led_on(demo_handle->mqtt_led);
            lv_obj_set_style_bg_color(demo_handle->mqtt_led, lv_color_hex(0x00FF00), LV_PART_MAIN);
        } else {
            lv_led_off(demo_handle->mqtt_led);
            lv_obj_set_style_bg_color(demo_handle->mqtt_led, lv_color_hex(0xFF0000), LV_PART_MAIN);
        }
    }

    // Update BLE LED
    if (demo_handle->ble_led) {
        if (ble_connected) {
            lv_led_on(demo_handle->ble_led);
            lv_obj_set_style_bg_color(demo_handle->ble_led, lv_color_hex(0x0000FF), LV_PART_MAIN);
        } else {
            lv_led_off(demo_handle->ble_led);
            lv_obj_set_style_bg_color(demo_handle->ble_led, lv_color_hex(0xFF0000), LV_PART_MAIN);
        }
    }
#endif

    return ESP_OK;
}

esp_err_t lvgl_ui_demo_update_temperature(lvgl_ui_demo_handle_t *demo_handle, float temperature)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#ifdef ESP_PLATFORM
    if (demo_handle->temp_arc && demo_handle->temp_label) {
        // Scale temperature to arc range (assuming -10°C to 40°C range)
        int32_t temp_value = (int32_t)((temperature + 10.0f) * 100.0f / 50.0f);
        temp_value = (temp_value < 0) ? 0 : (temp_value > 100) ? 100 : temp_value;
        
        lv_arc_set_value(demo_handle->temp_arc, temp_value);
        
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.1f°C", temperature);
        lv_label_set_text(demo_handle->temp_label, temp_str);
    }
#endif

    return ESP_OK;
}

esp_err_t lvgl_ui_demo_update_humidity(lvgl_ui_demo_handle_t *demo_handle, float humidity)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#ifdef ESP_PLATFORM
    if (demo_handle->humidity_arc && demo_handle->humidity_label) {
        // Scale humidity to arc range (0% to 100%)
        int32_t hum_value = (int32_t)humidity;
        hum_value = (hum_value < 0) ? 0 : (hum_value > 100) ? 100 : hum_value;
        
        lv_arc_set_value(demo_handle->humidity_arc, hum_value);
        
        char hum_str[16];
        snprintf(hum_str, sizeof(hum_str), "%.1f%%", humidity);
        lv_label_set_text(demo_handle->humidity_label, hum_str);
    }
#endif

    return ESP_OK;
}

esp_err_t lvgl_ui_demo_update_temp_humidity(lvgl_ui_demo_handle_t *demo_handle, float temperature, float humidity)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Update individual temperature and humidity displays
    lvgl_ui_demo_update_temperature(demo_handle, temperature);
    lvgl_ui_demo_update_humidity(demo_handle, humidity);

#ifdef ESP_PLATFORM
    // Update combined temperature and humidity label (similar to reference project)
    if (demo_handle->temp_hum_label) {
        char temp_hum_str[32];
        snprintf(temp_hum_str, sizeof(temp_hum_str), "%.1f °C / %.1f %%", temperature, humidity);
        lv_label_set_text(demo_handle->temp_hum_label, temp_hum_str);
    }
#endif

    return ESP_OK;
}

esp_err_t lvgl_ui_demo_update_progress(lvgl_ui_demo_handle_t *demo_handle, uint8_t progress)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#ifdef ESP_PLATFORM
    if (demo_handle->progress_bar) {
        lv_bar_set_value(demo_handle->progress_bar, progress, LV_ANIM_ON);
    }
#endif

    return ESP_OK;
}

esp_err_t lvgl_ui_demo_show_message(lvgl_ui_demo_handle_t *demo_handle, 
                                   const char *message, 
                                   uint32_t duration_ms)
{
    if (demo_handle == NULL || !demo_handle->initialized || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    if (demo_handle->status_label) {
        lv_label_set_text(demo_handle->status_label, message);
        
        // If duration is specified, create a timer to restore original text
        if (duration_ms > 0) {
            // Note: In a real implementation, you might want to store the original text
            // and restore it after the duration. For simplicity, we'll just update the text.
            ESP_LOGI(TAG, "Message displayed: %s (duration: %lu ms)", message, duration_ms);
        }
    }
#endif

    return ESP_OK;
}

#ifdef ESP_PLATFORM
lv_obj_t* lvgl_ui_demo_get_screen(lvgl_ui_demo_handle_t *demo_handle)
{
    if (demo_handle == NULL || !demo_handle->initialized) {
        return NULL;
    }
    return demo_handle->main_screen;
}
#endif