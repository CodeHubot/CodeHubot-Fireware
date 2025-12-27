# ESP32-S3 板子选择指南

## 📋 概述

本固件支持三种 ESP32-S3 板子配置，您可以通过修改配置文件来选择需要的板子类型。

## 🎯 支持的板子类型

### 1. ESP32-S3 DevKit (Standard) - 标准版 ⭐ 默认
- **产品代码**: `ESP32-S3-Dev-01`
- **传感器**: 
  - DHT11 温湿度传感器 (GPIO35)
  - DS18B20 防水温度传感器 (GPIO39)
- **控制设备**:
  - 4个LED (GPIO42, 41, 37, 36)
  - 2个继电器 (GPIO1, 2)
  - 2个舵机 (GPIO48, 40)

### 2. ESP32-S3 DevKit Rain - 雨水传感器版
- **产品代码**: `ESP32-S3-Rain-01`
- **传感器**: 
  - DHT11 温湿度传感器 (GPIO35)
  - 雨水传感器 (GPIO39) ⭐ 特色
- **控制设备**: 同标准版

### 3. ESP32-S3 DevKit Lite - 精简版
- **产品代码**: `ESP32-S3-Lite-01`
- **传感器**: 
  - DHT11 温湿度传感器 (GPIO35)
- **控制设备**: 
  - 4个LED (GPIO42, 41, 37, 36)
  - 2个继电器 (GPIO1, 2)
  - **不支持舵机**

## 🔧 切换板子的方法

### 方法一：修改 sdkconfig.defaults（推荐，影响全局默认）

编辑文件：`/Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32/sdkconfig.defaults`

找到以下部分（第 61-65 行）：

```ini
# Application Configuration - Board Selection
# 板子选择：取消注释其中一个，注释掉其他的
CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y        # 标准版（当前默认）
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y  # 雨水传感器版
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y  # 精简版
```

**切换到 Rain 版本**：
```ini
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y
CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y
```

**切换到 Lite 版本**：
```ini
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y
CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y
```

修改后执行：
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32
rm -f sdkconfig  # 删除旧配置
idf.py reconfigure  # 重新生成配置
idf.py build  # 编译
```

### 方法二：使用 menuconfig（适合临时切换）

```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32
. $HOME/esp/esp-idf/export.sh
idf.py menuconfig
```

在菜单中导航：
```
AIOT Device Configuration → Board Selection → Select Board Type
```

使用方向键选择，空格键确定，然后：
- 按 `S` 保存
- 按 `Q` 退出

最后编译：
```bash
idf.py build
```

## 📝 注意事项

### 1. 配置文件优先级
- `sdkconfig.defaults` - 默认配置（推荐修改此文件）
- `sdkconfig` - 生成的配置文件（每次编译时自动生成）

### 2. 切换板子后需要做什么
1. 删除旧的 `sdkconfig` 文件
2. 重新配置：`idf.py reconfigure`
3. 清理构建：`idf.py fullclean`（可选，但推荐）
4. 重新编译：`idf.py build`

### 3. 不同板子的产品代码
系统会根据板子类型自动设置产品代码：
- 标准版 → `ESP32-S3-Dev-01`
- Rain版 → `ESP32-S3-Rain-01`
- Lite版 → `ESP32-S3-Lite-01`

这些产品代码必须与后端数据库中的产品配置匹配。

## 🔍 验证当前板子配置

### 方法一：查看配置文件
```bash
grep "CONFIG_AIOT_BOARD" sdkconfig.defaults
```

### 方法二：查看编译日志
编译时会在日志中显示：
```
BSP初始化完成 (标准板子)
或
BSP初始化完成 (Rain板子)
或
BSP初始化完成 (Lite板子)
```

### 方法三：查看设备串口输出
烧录后在串口监控中可以看到：
```
[AIOT_MAIN] 📋 板子类型: ESP32-S3-DevKit
[AIOT_MAIN] 🏷️ 产品代码: ESP32-S3-Dev-01
```

## 🚀 快速切换流程（完整示例）

假设要从 Lite 切换到标准版：

```bash
# 1. 进入固件目录
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32

# 2. 激活 ESP-IDF 环境
. $HOME/esp/esp-idf/export.sh

# 3. 修改配置文件（使用 sed 或手动编辑）
# 注释掉 Lite，启用标准版
sed -i.bak 's/^CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y/# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y/' sdkconfig.defaults
sed -i.bak 's/^# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y/CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y/' sdkconfig.defaults

# 4. 清理旧配置
rm -f sdkconfig

# 5. 完全清理并重新编译
idf.py fullclean
idf.py build

# 6. 烧录
idf.py flash monitor
```

## 📦 为不同板子构建固件

如果需要为不同板子构建多个固件版本：

```bash
# 构建标准版
sed -i.bak 's/^# \(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y\)/\1/' sdkconfig.defaults
sed -i.bak 's/^\(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y\)/# \1/' sdkconfig.defaults
sed -i.bak 's/^\(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y\)/# \1/' sdkconfig.defaults
rm -f sdkconfig && idf.py build
mv build/aiot-esp32s3-firmware.bin build/firmware-standard.bin

# 构建 Rain 版
sed -i.bak 's/^\(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y\)/# \1/' sdkconfig.defaults
sed -i.bak 's/^# \(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y\)/\1/' sdkconfig.defaults
rm -f sdkconfig && idf.py build
mv build/aiot-esp32s3-firmware.bin build/firmware-rain.bin

# 构建 Lite 版
sed -i.bak 's/^\(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y\)/# \1/' sdkconfig.defaults
sed -i.bak 's/^# \(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y\)/\1/' sdkconfig.defaults
rm -f sdkconfig && idf.py build
mv build/aiot-esp32s3-firmware.bin build/firmware-lite.bin
```

## ❓ 常见问题

### Q: 切换板子后编译出错？
**A**: 执行完全清理后重新编译：
```bash
idf.py fullclean
rm -f sdkconfig
idf.py build
```

### Q: 如何确认板子类型是否正确？
**A**: 查看编译输出或串口日志，搜索 "板子类型" 或 "产品代码"。

### Q: 可以同时启用多个板子配置吗？
**A**: 不可以。同一时间只能启用一个板子配置，其他的必须注释掉。

### Q: 修改 sdkconfig.defaults 后没有生效？
**A**: 需要删除 `sdkconfig` 文件，然后重新编译。`sdkconfig` 是生成的配置文件，它的优先级高于 `sdkconfig.defaults`。

## 📚 相关文档

- [固件编译指南](README.md)
- [Rain 板子配置说明](boards/esp32-s3-devkit-rain/README.md)
- [固件手册](../FIRMWARE_MANUAL.md)
