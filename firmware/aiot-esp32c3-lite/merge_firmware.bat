@echo off
REM ========================================
REM ESP32-C3 固件合并脚本 (Windows版)
REM 功能：将bootloader、分区表、应用程序合并成单一固件文件
REM ========================================

setlocal enabledelayedexpansion

echo ========================================
echo   ESP32-C3 固件合并工具 (Windows)
echo ========================================
echo.

REM 检查Python环境
python --version >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到Python，请先安装Python
    pause
    exit /b 1
)

REM 检查编译文件
echo [检查] 检查编译文件...
if not exist "build\bootloader\bootloader.bin" (
    echo [错误] bootloader.bin 不存在
    echo 请先编译固件: build.bat 或 idf.py build
    pause
    exit /b 1
)

if not exist "build\partition_table\partition-table.bin" (
    echo [错误] partition-table.bin 不存在
    echo 请先编译固件: build.bat 或 idf.py build
    pause
    exit /b 1
)

if not exist "build\aiot-esp32c3-lite.bin" (
    echo [错误] aiot-esp32c3-lite.bin 不存在
    echo 请先编译固件: build.bat 或 idf.py build
    pause
    exit /b 1
)

echo [完成] 所有编译文件就绪
for %%f in (build\bootloader\bootloader.bin) do echo   Bootloader:     %%~zf bytes
for %%f in (build\partition_table\partition-table.bin) do echo   分区表:         %%~zf bytes
for %%f in (build\aiot-esp32c3-lite.bin) do echo   应用程序:       %%~zf bytes
echo.

REM 创建输出目录
if not exist "build\merged" mkdir "build\merged"

REM 定义输出文件
set OUTPUT_DIR=build\merged
set MERGED_BIN=%OUTPUT_DIR%\aiot-esp32c3-lite_merged.bin

REM 获取日期 (YYYYMMDD格式)
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set mydate=%%c%%a%%b)
set MERGED_WITH_VERSION=%OUTPUT_DIR%\aiot-esp32c3-lite_v1.0.0_%mydate%.bin

echo [开始] 合并固件...
echo.

REM Flash大小 (4MB)
set FLASH_SIZE=4194304

REM 分区地址
set BOOTLOADER_OFFSET=0
set PARTITION_TABLE_OFFSET=32768
set APP_OFFSET=65536

echo [布局] 分区布局:
echo   Bootloader:     0x0      (%BOOTLOADER_OFFSET%)
echo   分区表:         0x8000   (%PARTITION_TABLE_OFFSET%)
echo   应用程序:       0x10000  (%APP_OFFSET%)
echo.

REM 使用Python脚本合并
echo [步骤1] 创建空白Flash镜像 (4MB)...
python -c "with open('%MERGED_BIN%', 'wb') as f: f.write(b'\xFF' * %FLASH_SIZE%); print('[完成] 已创建 4MB 空白镜像')"

echo [步骤2] 写入Bootloader (0x0)...
python -c "with open('%MERGED_BIN%', 'r+b') as merged: merged.seek(%BOOTLOADER_OFFSET%); data = open('build/bootloader/bootloader.bin', 'rb').read(); merged.write(data); print(f'[完成] 已写入 Bootloader ({len(data)} bytes)')"

echo [步骤3] 写入分区表 (0x8000)...
python -c "with open('%MERGED_BIN%', 'r+b') as merged: merged.seek(%PARTITION_TABLE_OFFSET%); data = open('build/partition_table/partition-table.bin', 'rb').read(); merged.write(data); print(f'[完成] 已写入分区表 ({len(data)} bytes)')"

echo [步骤4] 写入应用程序 (0x10000)...
python -c "with open('%MERGED_BIN%', 'r+b') as merged: merged.seek(%APP_OFFSET%); data = open('build/aiot-esp32c3-lite.bin', 'rb').read(); merged.write(data); print(f'[完成] 已写入应用程序 ({len(data)} bytes)')"

REM 复制一份带版本号的文件
copy "%MERGED_BIN%" "%MERGED_WITH_VERSION%" >nul

REM 生成烧录说明文件
echo [步骤5] 生成烧录说明...
(
echo ========================================
echo ESP32-C3 合并固件烧录说明 ^(Windows^)
echo ========================================
echo.
echo 固件文件：
echo   - aiot-esp32c3-lite_merged.bin
echo   - aiot-esp32c3-lite_vX.X.X_YYYYMMDD.bin
echo.
echo 烧录方法1：使用esptool.py
echo ----------------------------------------
echo python -m esptool --chip esp32c3 --port COM3 --baud 460800 ^
echo     --before default_reset --after hard_reset write_flash ^
echo     --flash_mode dio --flash_size 4MB --flash_freq 80m ^
echo     0x0 aiot-esp32c3-lite_merged.bin
echo.
echo 烧录方法2：使用乐鑫Flash下载工具
echo ----------------------------------------
echo 1. 下载工具：https://www.espressif.com/zh-hans/support/download/other-tools
echo 2. 打开 Flash Download Tools
echo 3. 选择芯片：ESP32-C3
echo 4. 添加文件：地址 0x0, 文件 aiot-esp32c3-lite_merged.bin
echo 5. SPI配置：DIO, 80MHz, 4MB
echo 6. 选择串口^(如: COM3^)和波特率^(460800^)
echo 7. 点击 START 开始烧录
echo.
echo Flash配置参数：
echo ----------------------------------------
echo 芯片型号：     ESP32-C3
echo Flash大小：    4MB
echo Flash模式：    DIO
echo Flash频率：    80MHz
echo 烧录地址：     0x0 ^(重要！^)
echo 波特率：       460800
echo.
echo 注意事项：
echo ----------------------------------------
echo 1. 合并固件必须烧录到地址 0x0
echo 2. 烧录前建议先擦除Flash
echo 3. 确保USB驱动已正确安装
echo 4. 使用支持数据传输的USB线
echo.
) > "%OUTPUT_DIR%\FLASH_INSTRUCTIONS.txt"

echo.
echo ========================================
echo   [完成] 固件合并完成！
echo ========================================
echo.
echo 输出文件：
dir /b "%OUTPUT_DIR%\*.bin"
echo.
echo 烧录说明：
echo   %OUTPUT_DIR%\FLASH_INSTRUCTIONS.txt
echo.
echo 快速烧录命令：
echo   python -m esptool --chip esp32c3 --port COM3 --baud 460800 ^
echo       --before default_reset --after hard_reset write_flash ^
echo       --flash_mode dio --flash_size 4MB --flash_freq 80m ^
echo       0x0 %MERGED_BIN%
echo.
echo 注意事项：
echo   1. 合并固件必须烧录到地址 0x0
echo   2. 烧录前建议先擦除Flash
echo   3. Flash配置: DIO, 80MHz, 4MB
echo.
for %%f in (%MERGED_BIN%) do echo 文件大小: %%~zf bytes
echo.

pause
