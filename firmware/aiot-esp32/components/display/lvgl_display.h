/**
 * @file lvgl_display.h
 * @brief LVGL Display Driver for ST7789 LCD
 * @version 1.0
 * @date 2024-01-20
 * 
 * AIOT ESP32-S3 Project LVGL Display Driver
 * Integrates ST7789 LCD driver with LVGL graphics library
 */

#ifndef LVGL_DISPLAY_H
#define LVGL_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "lcd_st7789.h"

#ifdef ESP_PLATFORM
#include "lvgl.h"
#include "esp_lvgl_port.h"
#endif

/* LVGL Display Configuration */
#define LVGL_DISPLAY_WIDTH      240
#define LVGL_DISPLAY_HEIGHT     240
#define LVGL_BUFFER_SIZE        (LVGL_DISPLAY_WIDTH * LVGL_DISPLAY_HEIGHT / 10)  // 1/10 screen buffer
#define LVGL_TASK_PRIORITY      4
#define LVGL_TASK_STACK_SIZE    4096

/* LVGL Display Handle */
typedef struct {
    lcd_handle_t *lcd_handle;          // LCD hardware handle
#ifdef ESP_PLATFORM
    lv_disp_t *lv_display;              // LVGL display object
    lv_color_t *draw_buf1;             // Drawing buffer 1
    lv_color_t *draw_buf2;             // Drawing buffer 2 (optional)
#endif
    bool initialized;                   // Initialization status
} lvgl_display_handle_t;

/**
 * @brief Initialize LVGL display system
 * 
 * @param lcd_handle Pointer to initialized LCD handle
 * @param lvgl_handle Pointer to LVGL display handle structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_display_init(lcd_handle_t *lcd_handle, lvgl_display_handle_t *lvgl_handle);

/**
 * @brief Deinitialize LVGL display system
 * 
 * @param lvgl_handle Pointer to LVGL display handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_display_deinit(lvgl_display_handle_t *lvgl_handle);

/**
 * @brief Start LVGL timer task
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_timer_start(void);

/**
 * @brief Stop LVGL timer task
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_timer_stop(void);

/**
 * @brief Get LVGL display object
 * 
 * @param lvgl_handle Pointer to LVGL display handle
 * @return lv_display_t* LVGL display object or NULL
 */
#ifdef ESP_PLATFORM
lv_disp_t* lvgl_get_display(lvgl_display_handle_t *lvgl_handle);
#endif

/**
 * @brief Set display brightness
 * 
 * @param lvgl_handle Pointer to LVGL display handle
 * @param brightness Brightness level (0-100)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_set_brightness(lvgl_display_handle_t *lvgl_handle, uint8_t brightness);

/**
 * @brief Enable/disable display
 * 
 * @param lvgl_handle Pointer to LVGL display handle
 * @param enable true to enable, false to disable
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_display_enable(lvgl_display_handle_t *lvgl_handle, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_DISPLAY_H */