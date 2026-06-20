# 合并固件发布指南

> 本文说明如何编译、生成合并固件，并发布到 `docs/firmware/` 供烧录工具 / Web 烧录使用。

---

## 1. 什么是合并固件

合并固件将 **Bootloader + 分区表 + 应用程序**（S3 还包含 OTA 初始数据）打包成**单个 `.bin` 文件**，烧录地址统一为 **`0x0`**。

| 优势 | 说明 |
|------|------|
| 单文件分发 | 适合 Git 归档、网盘、Web 烧录 |
| 不易出错 | 不需要记住多个烧录地址 |
| 批量生产 | 配合 esptool / Flash Download Tools |

---

## 2. 发布目录与命名规范

**发布目录：** `docs/firmware/`

**命名格式：** `{产品代码}-v{版本号}.bin`

| 项目 | 产品代码来源 | 版本号来源 | 示例 |
|------|-------------|-----------|------|
| ESP32-S3 | `sdkconfig` 板型 → Kconfig | `DEVICE_CONFIG.h` | `ESP32-S3-Rain-01-v1.7.bin` |
| ESP32-C3 | `board_config.h` | `app_config.h` | `ESP32-C3-OLED-01-v1.0.0.bin` |

---

## 3. 发布前必做：更新版本号

### ESP32-S3（Rain / Lite / Dev）

编辑 `firmware/aiot-esp32/DEVICE_CONFIG.h`：

```c
#define DEVICE_FIRMWARE_VERSION     "1.8"   // 每次发布必须递增
```

确认板型（`firmware/aiot-esp32/sdkconfig.defaults`）：

```ini
CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y
```

### ESP32-C3 Lite

编辑 `firmware/aiot-esp32c3-lite/main/app_config.h`：

```c
#define FIRMWARE_VERSION        "1.0.1"
```

---

## 4. 一键发布（推荐）

在仓库根目录，先激活 ESP-IDF：

```bash
. ~/esp/esp-idf/export.sh
```

### ESP32-S3 Rain 板

```bash
# 方式 A：统一入口
chmod +x firmware/tools/publish_merged_firmware.sh
./firmware/tools/publish_merged_firmware.sh s3 --build

# 方式 B：项目内脚本
cd firmware/aiot-esp32
./build_esp_idf.sh publish
```

### ESP32-C3 Lite

```bash
./firmware/tools/publish_merged_firmware.sh c3 --build

# 或
cd firmware/aiot-esp32c3-lite
./build.sh publish
```

**输出：**

```
docs/firmware/ESP32-S3-Rain-01-v1.7.bin
docs/firmware/ESP32-S3-Rain-01-v1.7.bin.md5
```

---

## 5. 分步操作

### ESP32-S3

```bash
cd firmware/aiot-esp32
. ~/esp/esp-idf/export.sh

idf.py set-target esp32s3
idf.py build

# 仅合并（输出到 build/merged/）
./merge_firmware.sh

# 合并并发布到 docs/firmware/
./merge_firmware.sh --publish
```

**S3 合并分区：**

| 组件 | 地址 |
|------|------|
| Bootloader | 0x0 |
| 分区表 | 0x8000 |
| 应用程序 | 0x10000 |
| OTA 初始数据 | 0x610000 |

**Flash 参数：** 16MB, DIO, 80MHz

### ESP32-C3

```bash
cd firmware/aiot-esp32c3-lite
. ~/esp/esp-idf/export.sh

./build.sh build
./merge_firmware.sh --publish
```

**C3 合并方式：** 4MB 完整镜像（0xFF 填充 + 分段写入）

**Flash 参数：** 4MB, DIO, 80MHz

---

## 6. 烧录合并固件

### ESP32-S3

```bash
python -m esptool --chip esp32s3 --port /dev/cu.usbserial-* --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 docs/firmware/ESP32-S3-Rain-01-v1.7.bin
```

### ESP32-C3

```bash
python -m esptool --chip esp32c3 --port /dev/cu.usbserial-* --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x0 docs/firmware/ESP32-C3-OLED-01-v1.0.0.bin
```

---

## 7. 提交到 Git 远程仓库

```bash
# 1. 确认发布文件
ls -lh docs/firmware/

# 2. 验证 MD5（可选）
md5 docs/firmware/ESP32-S3-Rain-01-v1.7.bin

# 3. 提交
git add docs/firmware/*.bin docs/firmware/*.md5
git add firmware/aiot-esp32/merge_firmware.sh
git add firmware/tools/publish_merged_firmware.sh
git commit -m "release(firmware): 发布 ESP32-S3-Rain-01 v1.7 合并固件"
git push origin main
```

---

## 8. Web 烧录配置（可选）

若使用 `docs/firmware-flash.html` 在线烧录，需同步更新 `docs/manifest.json`：

```json
{
  "name": "ESP32-S3 Rain Firmware",
  "version": "1.7",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "firmware/ESP32-S3-Rain-01-v1.7.bin",
          "offset": 0
        }
      ]
    }
  ]
}
```

详见：`docs/WEB_FLASH_SETUP.md`

---

## 9. 发布检查清单

- [ ] 版本号已在源码中更新并递增
- [ ] 板型配置正确（S3 Rain / Lite / Dev）
- [ ] `idf.py build` 编译无错误
- [ ] 合并固件已生成到 `docs/firmware/`
- [ ] MD5 文件已生成
- [ ] 实机烧录验证通过（建议先 `erase_flash`）
- [ ] 配网、MQTT、传感器功能正常
- [ ] Git 提交并推送到远程

---

## 10. 给 AI 的发布指令（可复制）

```
请在 CodeHubot-Fireware 仓库中发布 ESP32-S3 Rain 合并固件：

1. 确认 firmware/aiot-esp32/DEVICE_CONFIG.h 中的 DEVICE_FIRMWARE_VERSION
2. 确认 sdkconfig.defaults 中 CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y
3. 激活 ESP-IDF: . ~/esp/esp-idf/export.sh
4. 执行: ./firmware/tools/publish_merged_firmware.sh s3 --build
5. 列出 docs/firmware/ 下生成的 .bin 和 .md5
6. 不要自动 git commit，等我确认后再提交

参考: docs/MERGED_FIRMWARE_RELEASE.md
```

---

## 相关文档

- [macOS 开发环境安装](MACOS_DEV_ENV_SETUP.md)
- [Web 烧录配置](WEB_FLASH_SETUP.md)
- [S3 板型选择](../firmware/aiot-esp32/BOARD_SELECTION_GUIDE.md)
- [C3 精简固件说明](../firmware/aiot-esp32c3-lite/README.md)

---

**最后更新：** 2026-03-21
