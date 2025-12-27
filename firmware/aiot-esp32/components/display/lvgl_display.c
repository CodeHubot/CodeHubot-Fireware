/**
 * @file lvgl_display.c
 * @brief LVGL Display Driver Implementation for ST7789 LCD
 * @version 1.0
 * @date 2024-01-20
 * 
 * AIOT ESP32-S3 Project LVGL Display Driver Implementation
 * Provides integration between ST7789 LCD driver and LVGL graphics library
 */

#include "lvgl_display.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef ESP_PLATFORM
#include "esp_lvgl_port.h"
#endif

static const char *TAG = "LVGL_DISPLAY";

/* LVGL Timer Task Handle */
static TaskHandle_t lvgl_timer_task_handle = NULL;
static bool lvgl_timer_running = false;

#ifdef ESP_PLATFORM
/**
 * @brief LVGL flush callback function
 * Called by LVGL when it needs to flush the display buffer
 */
static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    lvgl_display_handle_t *lvgl_handle = (lvgl_display_handle_t *)disp_drv->user_data;
    
    if (lvgl_handle == NULL || lvgl_handle->lcd_handle == NULL) {
        ESP_LOGE(TAG, "Invalid display handle in flush callback");
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Calculate area dimensions
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    
    ESP_LOGI(TAG, "Flush area: (%d,%d) to (%d,%d), size: %dx%d", 
             area->x1, area->y1, area->x2, area->y2, width, height);

    // Draw bitmap to LCD
    esp_err_t ret = lcd_draw_bitmap(lvgl_handle->lcd_handle, 
                                   area->x1, area->y1, 
                                   width, height, 
                                   (uint16_t*)color_p);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw bitmap: %s", esp_err_to_name(ret));
    }

    // Tell LVGL that flushing is done
    lv_disp_flush_ready(disp_drv);
}

/**
 * @brief LVGL timer task
 * Handles LVGL timer and display refresh
 */
static void lvgl_timer_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL timer task started");
    
    while (lvgl_timer_running) {
        // Handle LVGL timers and refresh
        uint32_t time_till_next = lv_timer_handler();
        
        // Sleep for the calculated time or minimum 5ms
        uint32_t sleep_time = (time_till_next > 5) ? time_till_next : 5;
        vTaskDelay(pdMS_TO_TICKS(sleep_time));
    }
    
    ESP_LOGI(TAG, "LVGL timer task stopped");
    lvgl_timer_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif

esp_err_t lvgl_display_init(lcd_handle_t *lcd_handle, lvgl_display_handle_t *lvgl_handle)
{
    if (lcd_handle == NULL || lvgl_handle == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing LVGL display system");

    // Clear handle structure
    memset(lvgl_handle, 0, sizeof(lvgl_display_handle_t));
    lvgl_handle->lcd_handle = lcd_handle;

#ifdef ESP_PLATFORM
    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Allocate display buffers
    size_t buffer_size = LVGL_BUFFER_SIZE * sizeof(lv_color_t);
    
    lvgl_handle->draw_buf1 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (lvgl_handle->draw_buf1 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate display buffer 1");
        return ESP_ERR_NO_MEM;
    }
    
    // Optional second buffer for double buffering
    lvgl_handle->draw_buf2 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (lvgl_handle->draw_buf2 == NULL) {
        ESP_LOGW(TAG, "Failed to allocate display buffer 2, using single buffer");
    }

    ESP_LOGI(TAG, "Display buffers allocated: %zu bytes each", buffer_size);

    // 创建LVGL显示缓冲区
    lv_disp_draw_buf_t *draw_buf = malloc(sizeof(lv_disp_draw_buf_t));
    if (!draw_buf) {
        ESP_LOGE(TAG, "Failed to allocate display draw buffer");
        goto cleanup;
    }
    
    lv_disp_draw_buf_init(draw_buf, lvgl_handle->draw_buf1, lvgl_handle->draw_buf2, LVGL_BUFFER_SIZE);
    
    // 创建LVGL显示驱动
    lv_disp_drv_t *disp_drv = malloc(sizeof(lv_disp_drv_t));
    if (!disp_drv) {
        ESP_LOGE(TAG, "Failed to allocate display driver");
        free(draw_buf);
        goto cleanup;
    }
    
    lv_disp_drv_init(disp_drv);
    disp_drv->hor_res = LVGL_DISPLAY_WIDTH;
    disp_drv->ver_res = LVGL_DISPLAY_HEIGHT;
    disp_drv->flush_cb = lvgl_flush_cb;
    disp_drv->draw_buf = draw_buf;
    disp_drv->user_data = lvgl_handle;
    
    // 注册显示驱动
    lvgl_handle->lv_display = lv_disp_drv_register(disp_drv);
    if (!lvgl_handle->lv_display) {
        ESP_LOGE(TAG, "Failed to register LVGL display driver");
        free(disp_drv);
        free(draw_buf);
        goto cleanup;
    }

    ESP_LOGI(TAG, "LVGL display created and configured");

    // Apply default theme
    lv_theme_t *theme = lv_theme_default_init(lvgl_handle->lv_display, 
                                             lv_palette_main(LV_PALETTE_BLUE), 
                                             lv_palette_main(LV_PALETTE_RED), 
                                             true, 
                                             LV_FONT_DEFAULT);
    lv_disp_set_theme(lvgl_handle->lv_display, theme);

    ESP_LOGI(TAG, "LVGL theme applied");
#endif

    lvgl_handle->initialized = true;
    ESP_LOGI(TAG, "LVGL display system initialized successfully");

    return ESP_OK;

#ifdef ESP_PLATFORM
cleanup:
    if (lvgl_handle->draw_buf1) {
        heap_caps_free(lvgl_handle->draw_buf1);
        lvgl_handle->draw_buf1 = NULL;
    }
    if (lvgl_handle->draw_buf2) {
        heap_caps_free(lvgl_handle->draw_buf2);
        lvgl_handle->draw_buf2 = NULL;
    }
    return ESP_FAIL;
#endif
}

esp_err_t lvgl_display_deinit(lvgl_display_handle_t *lvgl_handle)
{
    if (lvgl_handle == NULL || !lvgl_handle->initialized) {
        ESP_LOGE(TAG, "Display not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing LVGL display system");

    // Stop timer task first
    lvgl_timer_stop();

#ifdef ESP_PLATFORM
    // 删除LVGL显示对象
    if (lvgl_handle->lv_display) {
        // 在LVGL 8.x中，显示对象会在lv_deinit时自动清理
        lvgl_handle->lv_display = NULL;
    }
    
    // Free display buffers
    if (lvgl_handle->draw_buf1) {
        heap_caps_free(lvgl_handle->draw_buf1);
        lvgl_handle->draw_buf1 = NULL;
    }
    if (lvgl_handle->draw_buf2) {
        heap_caps_free(lvgl_handle->draw_buf2);
        lvgl_handle->draw_buf2 = NULL;
    }

    // Deinitialize LVGL
    lv_deinit();
#endif

    lvgl_handle->initialized = false;
    ESP_LOGI(TAG, "LVGL display system deinitialized");

    return ESP_OK;
}

esp_err_t lvgl_timer_start(void)
{
    if (lvgl_timer_running) {
        ESP_LOGW(TAG, "LVGL timer already running");
        return ESP_OK;
    }

#ifdef ESP_PLATFORM
    lvgl_timer_running = true;
    
    BaseType_t ret = xTaskCreate(lvgl_timer_task, 
                                "lvgl_timer", 
                                LVGL_TASK_STACK_SIZE, 
                                NULL, 
                                LVGL_TASK_PRIORITY, 
                                &lvgl_timer_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL timer task");
        lvgl_timer_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL timer task started");
#endif
    return ESP_OK;
}

esp_err_t lvgl_timer_stop(void)
{
    if (!lvgl_timer_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping LVGL timer task");
    lvgl_timer_running = false;

    // Wait for task to finish
    if (lvgl_timer_task_handle) {
        // Give task time to exit gracefully
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Force delete if still running
        if (lvgl_timer_task_handle) {
            vTaskDelete(lvgl_timer_task_handle);
            lvgl_timer_task_handle = NULL;
        }
    }

    ESP_LOGI(TAG, "LVGL timer task stopped");
    return ESP_OK;
}

#ifdef ESP_PLATFORM
lv_disp_t* lvgl_get_display(lvgl_display_handle_t *lvgl_handle)
{
    if (lvgl_handle == NULL || !lvgl_handle->initialized) {
        return NULL;
    }
    return lvgl_handle->lv_display;
}
#endif

esp_err_t lvgl_set_brightness(lvgl_display_handle_t *lvgl_handle, uint8_t brightness)
{
    if (lvgl_handle == NULL || !lvgl_handle->initialized) {
        ESP_LOGE(TAG, "Display not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 调用LCD背光设置函数
    lcd_set_brightness(brightness);
    return ESP_OK;
}

esp_err_t lvgl_display_enable(lvgl_display_handle_t *lvgl_handle, bool enable)
{
    if (!lvgl_handle || !lvgl_handle->lcd_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 根据enable参数控制背光
    if (enable) {
        lcd_backlight_on();
    } else {
        lcd_backlight_off();
    }
    return ESP_OK;
}