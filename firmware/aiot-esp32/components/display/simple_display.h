#ifndef SIMPLE_DISPLAY_H
#define SIMPLE_DISPLAY_H

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>

#ifdef __cplusplus
extern "C" {
#endif

// 最大支持的传感器数量
#define MAX_SENSOR_LABELS 4

/**
 * @brief 传感器显示信息结构体 - 用于LCD动态UI显示
 * 注意：这是bsp_sensor_display_info_t的兼容副本，避免循环依赖
 */
typedef struct {
    const char *name;             ///< 传感器名称，如"DHT11"
    const char *unit;             ///< 单位，如"°C / %"
    int gpio_pin;                 ///< GPIO引脚号
} sensor_display_info_t;

/**
 * @brief 板级传感器显示配置
 */
typedef struct {
    const sensor_display_info_t *sensor_list;  ///< 传感器列表
    int sensor_count;                          ///< 传感器数量
} board_sensor_config_t;

typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    gpio_num_t backlight_pin;
    bool backlight_output_invert;
    int width;
    int height;
    lv_disp_t *display;
    lv_obj_t *screen;
    lv_obj_t *label_product;
    lv_obj_t *label_product_prefix;
    lv_obj_t *label_wifi_id;
    lv_obj_t *label_wifi_id_prefix;
    lv_obj_t *label_wifi_status;
    lv_obj_t *label_wifi_status_prefix;
    lv_obj_t *label_mqtt_status;
    lv_obj_t *label_mqtt_status_prefix;
    lv_obj_t *label_mqtt_address;
    lv_obj_t *label_mqtt_address_prefix;
    lv_obj_t *label_mac;
    lv_obj_t *label_mac_prefix;
    lv_obj_t *label_uuid;
    lv_obj_t *label_uuid_prefix;
    lv_obj_t *label_uptime;
    lv_obj_t *label_uptime_prefix;
    lv_obj_t *label_temp_hum;
    lv_obj_t *label_temp_hum_prefix;
    lv_obj_t *label_version_prefix;
    lv_obj_t *label_version;
    
    // 动态传感器标签（用于不同板子的不同传感器配置）
    lv_obj_t *sensor_labels[MAX_SENSOR_LABELS];        // 传感器数据标签
    lv_obj_t *sensor_label_prefixes[MAX_SENSOR_LABELS]; // 传感器名称前缀标签
    int sensor_count;                                   // 实际传感器数量
} simple_display_t;

/**
 * @brief 初始化简单显示系统
 * 
 * @param panel_io LCD面板IO句柄
 * @param panel LCD面板句柄
 * @param backlight_pin 背光控制引脚
 * @param backlight_output_invert 背光输出反转
 * @param width 显示宽度
 * @param height 显示高度
 * @param mirror_x X轴镜像
 * @param mirror_y Y轴镜像
 * @param swap_xy XY轴交换
 * @return simple_display_t* 显示句柄，失败返回NULL
 */
simple_display_t* simple_display_init(esp_lcd_panel_io_handle_t panel_io, 
                                     esp_lcd_panel_handle_t panel,
                                     gpio_num_t backlight_pin, 
                                     bool backlight_output_invert,
                                     int width, int height,
                                     bool mirror_x, bool mirror_y, bool swap_xy);

/**
 * @brief 设置背光亮度
 * 
 * @param display 显示句柄
 * @param brightness 亮度值 (0-100)
 */
void simple_display_set_backlight(simple_display_t *display, uint8_t brightness);

/**
 * @brief 显示基本信息
 * 
 * @param display 显示句柄
 * @param title 标题
 * @param mac MAC地址
 * @param status 状态信息
 */
void simple_display_show_info(simple_display_t *display, const char *title, const char *mac, const char *status);

/**
 * @brief 显示详细信息
 * 
 * @param display 显示句柄
 * @param device 设备名称
 * @param product 产品名称
 * @param wifi_id WiFi名称
 * @param wifi_status WiFi状态
 * @param mqtt_status MQTT状态
 * @param mac MAC地址
 * @param uuid 设备UUID
 * @param server_address 服务器地址
 */
void simple_display_show_detailed_info(simple_display_t *display, 
                                      const char *device,
                                      const char *product, 
                                      const char *wifi_id,
                                      const char *wifi_status,
                                      const char *mqtt_status,
                                      const char *mac,
                                      const char *uuid,
                                      const char *server_address);

/**
 * @brief 更新状态信息
 * 
 * @param display 显示句柄
 * @param status 状态信息
 */
void simple_display_update_status(simple_display_t *display, const char *status);

/**
 * @brief 更新WiFi状态（合并显示）
 * 
 * @param display 显示句柄
 * @param wifi_id WiFi名称
 * @param wifi_status WiFi状态
 */
void simple_display_update_wifi_status(simple_display_t *display, const char *wifi_id, const char *wifi_status);

/**
 * @brief 更新MQTT地址
 * 
 * @param display 显示句柄
 * @param mqtt_address MQTT服务器地址
 */
void simple_display_update_mqtt_address(simple_display_t *display, const char *mqtt_address);

/**
 * @brief 更新运行时间显示
 * 
 * @param display 显示句柄
 * @param uptime_seconds 运行时间（秒）
 */
void simple_display_update_uptime(simple_display_t *display, uint32_t uptime_seconds);

/**
 * @brief 更新MQTT状态显示
 * 
 * @param display 显示句柄
 * @param mqtt_status MQTT状态字符串
 */
void simple_display_update_mqtt_status(simple_display_t *display, const char *mqtt_status);

/**
 * @brief 更新Device ID显示（支持长文本自动换行）
 * 
 * @param display 显示句柄
 * @param device_id 设备ID字符串
 */
void simple_display_update_device_id(simple_display_t *display, const char *device_id);

/**
 * @brief 更新温湿度数据
 * 
 * @param display 显示句柄
 * @param temperature 温度
 * @param humidity 湿度
 */
void simple_display_update_temp_hum(simple_display_t *display, float temperature, float humidity);

/**
 * @brief 显示传感器数据
 * 
 * @param display 显示句柄
 * @param sensor_data 传感器数据字符串
 */
void simple_display_show_sensor_data(simple_display_t *display, const char *sensor_data);

/**
 * @brief 显示配网引导信息
 * 
 * 在LCD上显示配网模式引导信息，包括AP热点名称和配网地址
 * 
 * @param display 显示句柄
 * @param ap_ssid AP热点名称
 * @param config_url 配网地址（如 http://192.168.4.1）
 */
void simple_display_show_provisioning_info(simple_display_t *display, const char *ap_ssid, const char *config_url);

/**
 * @brief 显示启动步骤信息（独立UI模板，5行详细布局，不复用其他标签）
 * 
 * 在LCD上显示当前启动步骤的详细英文提示信息，使用专门的启动UI布局
 * 
 * 布局 (5行):
 * - Line 1: === DEVICE STARTUP === (亮蓝色标题)
 * - Line 2: Stage: [步骤名称] (浅蓝色，当前阶段)
 * - Line 3: [详细描述] (天蓝色，解释当前操作)
 * - Line 4: Status: [状态消息] (蓝色/绿色/红色，颜色编码状态)
 * - Line 5: [进度提示] (浅蓝色/淡红色，如 "Please wait...", ">>> Continue >>>")
 * 
 * 颜色编码: 蓝色(标题/阶段/描述/进行中/提示) 绿色(成功) 红色(错误/警告)
 * 
 * @param display 显示句柄
 * @param step_name 步骤名称（如 "Initializing", "WiFi Connect", "Get Config"）
 * @param status 状态信息（如 "System Starting...", "Connecting...", "Config OK"）
 */
void simple_display_show_startup_step(simple_display_t *display, const char *step_name, const char *status);

/**
 * @brief 清除屏幕并准备显示新的启动UI
 * 
 * @param display 显示句柄
 */
void simple_display_clear_for_startup(simple_display_t *display);

/**
 * @brief 测试彩色显示
 * 
 * @param display 显示句柄
 */
void simple_display_test_colors(simple_display_t *display);

/**
 * @brief 显示设备注册信息（使用大字体，便于用户记录）
 * 
 * 专门用于在启动时显示Product ID和MAC地址，使用较大的字体便于用户看清并记录
 * 
 * @param display 显示句柄
 * @param product_id 产品ID
 * @param mac_address MAC地址
 */
void simple_display_show_registration_info(simple_display_t *display, 
                                          const char *product_id, 
                                          const char *mac_address);

/**
 * @brief 显示设备未注册提示信息
 * 
 * WiFi已连接，但设备未在后端注册。显示提示信息，告知用户需要先注册设备。
 * 
 * @param display 显示句柄
 * @param mac_address MAC地址（可选，可以为NULL）
 */
void simple_display_show_not_registered_info(simple_display_t *display, const char *mac_address);

/**
 * @brief 显示运行时主界面
 * 
 * 启动完成后的主界面，显示设备关键信息和运行状态
 * 
 * @param display 显示句柄
 * @param product_id 产品标识符
 * @param wifi_status WiFi状态 ("Connected" / "Disconnected")
 * @param mqtt_status MQTT状态 ("Connected" / "Disconnected")
 * @param device_uuid 设备UUID
 * @param temperature 温度值
 * @param humidity 湿度值
 * @param uptime_seconds 运行时间（秒）
 */
void simple_display_show_runtime_main(simple_display_t *display,
                                     const char *product_id,
                                     const char *wifi_status,
                                     const char *mqtt_status,
                                     const char *device_uuid,
                                     float temperature,
                                     float humidity,
                                     uint32_t uptime_seconds);

/**
 * @brief 根据板子配置初始化传感器UI显示
 * 
 * 根据BSP提供的传感器配置信息，动态创建传感器显示标签
 * 不同的板子会显示不同的传感器列表
 * 
 * @param display 显示句柄
 * @param sensor_config 传感器配置（包含传感器列表和数量）
 */
void simple_display_init_sensor_ui(simple_display_t *display, const board_sensor_config_t *sensor_config);

/**
 * @brief 更新特定传感器的显示值
 * 
 * @param display 显示句柄
 * @param sensor_index 传感器索引（0-3）
 * @param value 传感器值字符串（如 "24.5°C / 60%"）
 */
void simple_display_update_sensor_value(simple_display_t *display, int sensor_index, const char *value);

/**
 * @brief 销毁显示系统
 * 
 * @param display 显示句柄
 */
void simple_display_destroy(simple_display_t *display);

#ifdef __cplusplus
}
#endif

#endif // SIMPLE_DISPLAY_H