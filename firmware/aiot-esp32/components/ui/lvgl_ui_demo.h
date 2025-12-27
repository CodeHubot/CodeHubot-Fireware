/**
 * @file lvgl_ui_demo.h
 * @brief LVGL UI Demo for AIOT ESP32-S3 Project
 * @version 1.0
 * @date 2024-01-20
 * 
 * Simple LVGL UI demonstration with various widgets and animations
 */

#ifndef LVGL_UI_DEMO_H
#define LVGL_UI_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "lvgl_display.h"

#ifdef ESP_PLATFORM
#include "lvgl.h"
#endif

/* UI Demo Configuration */
#define UI_DEMO_UPDATE_PERIOD_MS    1000    // Update period for dynamic content
#define UI_DEMO_ANIMATION_TIME_MS   500     // Animation duration

/* UI Demo Handle */
typedef struct {
    lvgl_display_handle_t *display_handle;  // Display handle
#ifdef ESP_PLATFORM
    lv_obj_t *main_screen;                  // Main screen object
    lv_obj_t *status_label;                 // Status label
    lv_obj_t *time_label;                   // Time label
    lv_obj_t *temp_arc;                     // Temperature arc
    lv_obj_t *temp_label;                   // Temperature label
    lv_obj_t *humidity_arc;                 // Humidity arc
    lv_obj_t *humidity_label;               // Humidity label
    lv_obj_t *temp_hum_label;               // Combined temperature and humidity label
    lv_obj_t *wifi_led;                     // WiFi status LED
    lv_obj_t *mqtt_led;                     // MQTT status LED
    lv_obj_t *ble_led;                      // BLE status LED
    lv_obj_t *progress_bar;                 // Progress bar
    lv_timer_t *update_timer;               // Update timer
#endif
    bool initialized;                       // Initialization status
    uint32_t demo_counter;                  // Demo counter for animations
} lvgl_ui_demo_handle_t;

/**
 * @brief Initialize LVGL UI demo
 * 
 * @param display_handle Pointer to initialized LVGL display handle
 * @param demo_handle Pointer to UI demo handle structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_init(lvgl_display_handle_t *display_handle, lvgl_ui_demo_handle_t *demo_handle);

/**
 * @brief Deinitialize LVGL UI demo
 * 
 * @param demo_handle Pointer to UI demo handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_deinit(lvgl_ui_demo_handle_t *demo_handle);

/**
 * @brief Update system status display
 * 
 * @param demo_handle Pointer to UI demo handle
 * @param wifi_connected WiFi connection status
 * @param mqtt_connected MQTT connection status
 * @param ble_connected BLE connection status
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_update_status(lvgl_ui_demo_handle_t *demo_handle, 
                                    bool wifi_connected, 
                                    bool mqtt_connected, 
                                    bool ble_connected);

/**
 * @brief Update temperature display
 * 
 * @param demo_handle Pointer to UI demo handle
 * @param temperature Temperature value in Celsius
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_update_temperature(lvgl_ui_demo_handle_t *demo_handle, float temperature);

/**
 * @brief Update humidity display
 * 
 * @param demo_handle Pointer to UI demo handle
 * @param humidity Humidity value in percentage
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_update_humidity(lvgl_ui_demo_handle_t *demo_handle, float humidity);

/**
 * @brief Update temperature and humidity display
 * 
 * @param demo_handle Pointer to UI demo handle
 * @param temperature Temperature value in Celsius
 * @param humidity Humidity value in percentage
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_update_temp_humidity(lvgl_ui_demo_handle_t *demo_handle, float temperature, float humidity);

/**
 * @brief Update progress bar
 * 
 * @param demo_handle Pointer to UI demo handle
 * @param progress Progress value (0-100)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_update_progress(lvgl_ui_demo_handle_t *demo_handle, uint8_t progress);

/**
 * @brief Show message on display
 * 
 * @param demo_handle Pointer to UI demo handle
 * @param message Message text to display
 * @param duration_ms Display duration in milliseconds (0 = permanent)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_ui_demo_show_message(lvgl_ui_demo_handle_t *demo_handle, 
                                   const char *message, 
                                   uint32_t duration_ms);

/**
 * @brief Get main screen object
 * 
 * @param demo_handle Pointer to UI demo handle
 * @return lv_obj_t* Main screen object or NULL
 */
#ifdef ESP_PLATFORM
lv_obj_t* lvgl_ui_demo_get_screen(lvgl_ui_demo_handle_t *demo_handle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LVGL_UI_DEMO_H */