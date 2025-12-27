# Bluetooth Provisioning Module CMakeLists.txt
# 蓝牙配网模块编译配置

# 设置组件名称
set(COMPONENT_NAME "bluetooth_provisioning")

# 定义源文件
set(COMPONENT_SRCS
    "bt_provision.c"
    "bt_provision_ble.c"
    "bt_provision_cmd.c"
    "bt_provision_bsp.c"
)

# 定义头文件目录
set(COMPONENT_ADD_INCLUDEDIRS
    "."
    "../bsp"
)

# 定义依赖的组件
set(COMPONENT_REQUIRES
    "esp_common"
    "esp_system"
    "esp_wifi"
    "esp_event"
    "esp_netif"
    "esp_timer"
    "nvs_flash"
    "json"
    "esp_http_client"
    "bt"
    "esp_bt"
)

# 定义私有依赖的组件
set(COMPONENT_PRIV_REQUIRES
    "esp_wifi"
    "esp_event"
    "esp_netif"
    "esp_timer"
    "nvs_flash"
    "json"
    "esp_http_client"
    "bt"
    "esp_bt"
)

# 注册组件
register_component()

# 设置编译选项
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-function
    -Wno-unused-variable
)

# 设置预处理器定义
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CONFIG_BT_ENABLED=1
    CONFIG_BLUEDROID_ENABLED=1
    CONFIG_BT_BLUEDROID_ENABLED=1
    CONFIG_BT_BLE_ENABLED=1
)

# 添加编译条件
if(CONFIG_BT_ENABLED)
    message(STATUS "Bluetooth provisioning module enabled")
else()
    message(WARNING "Bluetooth provisioning module requires CONFIG_BT_ENABLED=y")
endif()

if(CONFIG_BLUEDROID_ENABLED)
    message(STATUS "Bluedroid stack enabled for provisioning")
else()
    message(WARNING "Bluetooth provisioning requires CONFIG_BLUEDROID_ENABLED=y")
endif()