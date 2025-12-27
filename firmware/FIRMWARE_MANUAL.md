# AIOT-ESP32 固件整合文档（单文件版）

本文基于当前实现的固件代码，对核心流程、配置、接口与主题进行系统梳理与整合。

重点强调：NVS 仅保存 Wi‑Fi SSID、Wi‑Fi 密码与服务器地址；所有服务器地址必须从 NVS 读取并在运行期动态拼接，禁止硬编码；

其余信息不从 NVS 读取。

## 概览

- 目标芯片：ESP32 / ESP32-S3（示例板：ESP32-S3-DevKit）
- 框架与工具：ESP-IDF 5.4.3，C/C++，CMake，FreeRTOS
- 主要模块：
  - 系统初始化：`main/system/module_init.c`
  - 网络与配网：`network/wifi_config.c`、`network/wifi_manager.c`
  - 服务器配置（统一 NVS）：`server_config.c/.h`
  - 设备查询（HTTP）：`device/device_registration.c/.h`
  - MQTT 客户端与事件：`mqtt/aiot_mqtt_client.c/.h`
  - 设备控制与预设：`device/device_control.c/.h`、`device/preset_control.c/.h`
  - 系统监控与传感器：`system/system_monitor.c`、`drivers/sensors/*`
  - OTA 管理器：`ota/ota_manager.c/.h`
  - 显示与按键：`simple_display.*`、`drivers/lcd/*`、`device/button_handler.*`

## 启动与初始化流程

1. 初始化 NVS：`nvs_flash_init()`，必要时擦除并重新初始化；确保命名空间可用。
2. 从 NVS 加载 Wi‑Fi SSID/密码：`wifi_config_load()` 仅加载 WiFi 配置；若 WiFi 配置缺失则直接进入配网模式，不进行任何联网或后端调用。默认值仅作为占位以避免空指针，禁止用于联网操作。
3. 初始化设备 ID（MAC 基础临时值）：`init_device_id_and_topics()` 仅生成临时 `g_device_id`；不构建任何主题（待获取 UUID 后再确定）。
4. 初始化并启动 WiFi 管理器：`init_wifi_and_network()` → `handle_config_mode()`，依据步骤2的 WiFi 配置连接或进入配网。连接前将执行一次环境预扫描：若未发现已保存的 SSID，系统会立即进入配网模式（不进行连接重试等待）。
5. **加载服务器配置并获取设备标识（device_id/uuid）**：在需要时（WiFi连接成功后）加载服务器配置 `server_config_load_from_nvs()`，然后通过 `init_network_services()` 调用 `fetch_uuid_by_mac()`，使用加载的 `srv_config` 动态构建 URL。若服务器配置缺失或 UUID 获取多次重试仍失败，系统进入错误停机或配网模式，不再继续。
6. 初始化设备查询模块（HTTP，MAC→UUID）：建议命名 `device_lookup_init()`（现代码为 `device_registration_init()`）。代码不要保留“正式注册”调用，建议命名 `device_lookup_start(...)`（现代码为 `device_registration_start(...)`）；注册失败不阻断系统运行，当前部署建议禁用或条件化该调用。
7. 初始化设备控制模块与预设控制模块：`device_control_init()`、`preset_control_init()`。
8. 初始化 OTA 管理器并注册事件回调：`ota_manager_init(ota_event_callback_adapter)`。
9. 初始化并连接 MQTT 客户端：Broker 地址来自 NVS 的服务器地址；端口与认证信息来自编译配置或运行期策略（不从 NVS 读取），`client_id=g_device_id`（基于已成功获取的设备标识）。仅在获取设备标识后构建并使用主题，连接成功后订阅 `devices/{device_uuid}/control`。心跳机制约定：连接成功后应启动周期心跳发布（详见"MQTT 通信"章节）；当前代码尚未启用，请根据约定补充实现。
10. 可选：自动 OTA 检测与更新：`auto_check_and_update_ota()`。

入口参考：`module_init.c` 的 `init_device_id_and_topics()`、`init_wifi_and_network()`、`handle_config_mode()`、`init_network_services()`；配置加载参考 `wifi_config.c` 与 `server_config.c`；事件回调在 `mqtt_event_callback_adapter()` 与 `ota_event_callback_adapter()`。

### 启动阻断策略（UUID获取失败）

- 通过 `module_init.c` 的 `fetch_uuid_by_mac()` 多次重试后端接口获取 UUID。
- 若仍失败，进入永久错误状态并打印日志：`SYSTEM HALTED: UUID fetch failed, cannot proceed`。
- 在该状态下不初始化网络服务、MQTT、设备控制与 OTA；系统不继续执行后续流程。

## 统一服务器配置（NVS）与地址构建

**重要说明**：服务器地址和 OTA 地址统一为"服务器地址"，不再单独存储。服务器地址用于 HTTP API、MQTT Broker 和 OTA 更新，统一使用 `server_config` 命名空间。

- 命名空间：`server_config`
- 键名：
  - `NVS_KEY_BASE_ADDRESS`：服务器基础地址（域名或 IP，用于 HTTP API、MQTT 和 OTA）
- 结构体：`unified_server_config_t`（`server_config.h`）统一承载 `base_address`、`http_port`、`mqtt_port` 等配置。
- 关键函数：
  - `server_config_load_from_nvs(unified_server_config_t*)`：仅读取服务器基础地址（在需要时调用，不再在第一步加载）
  - `server_config_get_default(unified_server_config_t*)`：加载默认配置结构（用于兜底）
  - `server_config_save_to_nvs(const unified_server_config_t*)`：保存服务器基础地址
  - `server_config_build_url(&srv_config, "http", "/api/...", out_buf, size)`：根据 NVS 的服务器地址构建完整 URL（示例在 `module_init.c` 的 `fetch_uuid_by_mac()` 中使用）
  - `server_config_build_http_url()`：构建 HTTP API URL
  - `server_config_build_mqtt_broker_url()`：构建 MQTT Broker 地址

重要约束：
- 服务器地址必须从 NVS 读取并动态构建 URL；端口与认证信息不从 NVS 读取。
- `server_config.h` 中的宏仅用于默认占位，不可直接硬编码使用。
- 服务器地址不再在启动第一步加载，而是在需要时（WiFi 连接成功后，获取 UUID 时）加载。
- 配网时，服务器地址和 WiFi 配置一起保存，服务器地址保存到 `server_config` 命名空间。

硬性约束：设备 UUID 必须由后端接口返回，运行期不得从 NVS 读取或持久化为 UUID 使用；如果无法获取 UUID，系统不得继续执行。

## 设备查询（HTTP，MAC→UUID）

模块（建议命名）：`device_lookup.c/.h`（现实现文件名为 `device_registration.c/.h`，建议后续统一重命名以更贴近“查询”语义）

状态与事件：
- 状态枚举：`device_registration_state_t`（IDLE、REGISTERING、REGISTERED、FAILED）
- 事件枚举：`device_registration_event_t`（STARTED、SUCCESS、FAILED、TIMEOUT），回调类型 `device_registration_callback_t`

NVS 存储：不保存设备注册信息（`device_id`、`device_uuid`、`device_secret`、`mac_address`、注册标志）。这些信息仅在内存中使用，或通过安全通道在运行期获取。

设备查询（MAC→UUID）：
- URL（动态构建）：`http://{base}/api/devices/mac/lookup`（`base` 来自 NVS；端口按部署约定或默认）
- 请求方法：`POST`
- 请求体 JSON 字段：
  - `mac_address`（形如 `AA:BB:CC:DD:EE:FF`）
  - `device_type`（示例：`ESP32-S3`）
  - `firmware_version`、`hardware_version`
- 成功响应字段（解析函数建议命名 `parse_lookup_response()`，现代码为 `parse_registration_response()`）：
  - `device_id`、`device_uuid`、`device_secret`、`mac_address`

当前部署不执行“正式注册”流程（绑定产品信息），代码中相关实现仅作为可选能力保留；如后端未来要求绑定产品信息，可启用该流程并同步更新本章描述。

流程入口：建议命名 `device_lookup_start()`（现代码为 `device_registration_start()`），会创建设备查询任务（MAC→UUID），包含重试与事件回调。查询成功后将 UUID 用于运行态标识，仅在内存使用；启动阶段必须向后端查询 UUID。

运行时约定：设备标识信息（`device_id/device_uuid/device_secret`）仅在内存使用，不从 NVS 读取或持久化。

## MQTT 通信（

初始化：
- MQTT 配置：
  - `broker_url`：`srv_config.base_address`（来自 NVS）
  - `port`：编译配置或部署约定（不从 NVS 读取）
  - `username` / `password`：运行期生成或服务端策略（不从 NVS 读取）
  - `client_id`：设备标识（`g_device_id`）
- 连接与事件：`mqtt_client_init(..., mqtt_event_callback_adapter)`，在 `AIOT_MQTT_EVENT_CONNECTED` 事件中订阅控制主题。

运行约束：MQTT 的 `client_id` 使用 `g_device_id`（源自后端返回的 device_id）。若 UUID 获取失败，MQTT 初始化与连接流程不进行。

主题与负载：
- 控制订阅主题（当前实现）：`devices/{device_uuid}/control`
- 传感器数据发布主题（当前实现）：`devices/{device_uuid}/data`
 - 心跳发布主题（约定，待实现）：`devices/{device_uuid}/heartbeat`
   - 负载结构：`{"sequence": <uint>, "timestamp": <ms>, "status": <0/1/2>}`
   - 发布间隔：参考 `Kconfig` 中 `MQTT_HEARTBEAT_INTERVAL_MS`（默认 `30000ms`）
   - 发布条件：仅当 `UUID` 已获取且 `MQTT` 已连接；断开时暂停发布
   - QoS：建议 `1`（可在 `Kconfig` 中调整）

Kconfig 参考：
- `MQTT_TOPIC_PREFIX=aiot`（当前实现未使用该前缀；主题统一为 `devices/...`）
- 主题后缀示例：`sensor/data`、`device/status`、`alarm`、`heartbeat`、`log`、`ota`（当前实现主要使用 `control` 与 `devices/.../data`）
- QoS 默认：1；保持连接等重连与超时可在 Kconfig 调整。

示例：传感器数据发布负载（`system_publish_sensor_data()`）
```json
{
  "device_id": "<device_id>",
  "timestamp": 1730000000,
  "free_heap": 123456,
  "dht11_temp": 24.3,
  "dht11_humidity": 60.2,
  "ds18b20_temp": 23.8
}
```

示例：心跳发布负载（约定，待实现）
```json
{
  "sequence": 1024,
  "timestamp": 1730003000,
  "status": 1
}
```

示例：控制命令 JSON（`device_control_parse_json_command()` 支持）
- LED：
  - `{"cmd":"led","led_id":1,"action":"on"}`
  - `{"cmd":"led","led_id":1,"action":"brightness","brightness":128}`
- 继电器：
  - `{"cmd":"relay","relay_id":1,"action":"on"}`
- 舵机：
  - `{"cmd":"servo","servo_id":1,"angle":90}`

预设命令（`preset_control.c`，新格式）：
```json
{
  "cmd":"preset",
  "device_type":"led|servo|relay",
  "preset_type":"...",
  "device_id":0,
  "parameters":{...}
}
```

## OTA 更新（HTTP 为主，支持事件回调）

**重要说明**：OTA 地址统一使用服务器地址（`server_config.base_address`），不再单独存储 OTA URL。

自动检测与下载（`auto_check_and_update_ota()`）：
- 版本检测 URL（动态构建）：使用 `server_config_build_http_url()` 构建 `http://{base_address}:{http_port}/api/firmware/check`
- 请求方法：`POST`
- 请求体字段：`product_code`、`product_version`、`firmware_version`
- 响应含 `download_url` 时，调用：`ota_manager_start_update_simple(download_url)` 开始更新。
- OTA 下载地址通常也使用相同的服务器地址，通过动态构建 URL 实现。

事件回调（`ota_event_callback_adapter()`）：
- `OTA_EVENT_START`、`OTA_EVENT_PROGRESS`、`OTA_EVENT_COMPLETE`、`OTA_EVENT_ERROR`

管理器能力（`ota_manager.h/.c`）：
- 初始化与去初始化、检查更新、开始/停止、进度获取、版本管理、固件验证、回滚与标记有效、分区信息、增量更新支持、超时设置、状态/错误字符串等。

Kconfig 选项（`ota/Kconfig`）：
- 签名算法、回滚保护
- 启用 HTTP/HTTPS OTA
- HTTP 超时、缓冲区大小
- `OTA_MQTT_ENABLE` 与 `OTA_MQTT_TOPIC_PREFIX`（MQTT OTA 教学可选；当前实现以 HTTP OTA 为主）

分区与回滚：
- 分区信息可通过管理器接口读取；更新完成后需正确标记有效，失败时回滚。

## WiFi 配网与存储（NVS）

命名空间：`wifi_config`

键名（见 `wifi_config.c`）：
- `NVS_KEY_WIFI_SSID`、`NVS_KEY_WIFI_PASS`：Wi‑Fi SSID 与密码
- `NVS_KEY_CONFIGURED`：WiFi 配置标志
- **注意**：服务器地址不再保存在 `wifi_config` 命名空间中，而是统一保存在 `server_config` 命名空间中的 `NVS_KEY_BASE_ADDRESS`。

配网 Web 界面：
- 提供 WiFi SSID、WiFi 密码和服务器地址三个输入字段
- 服务器地址用于 HTTP API、MQTT 和 OTA，统一使用同一个地址
- 配网成功后，WiFi 配置保存到 `wifi_config` 命名空间，服务器地址保存到 `server_config` 命名空间

流程：
- 初始化：`wifi_manager_init()`（重试次数、间隔、连接超时、是否自动启动配网等）
- 启动：`wifi_manager_start()`，随后 `wifi_manager_wait_for_connection_or_provisioning(0)` 等待连接或进入配网模式。
- 配网完成后设备重启；正常连接继续后续流程。配网成功后保存 SSID/密码到 `wifi_config` 命名空间，保存服务器地址到 `server_config` 命名空间。

## 系统监控与显示

- 周期性任务记录堆内存、运行时间、连接状态等（`system_monitor.c`）。
- LCD 更新与简易显示支持（`simple_display.*`、`drivers/lcd/*`）。
- 关键日志用于故障定位：MQTT 连接、设备注册状态、OTA 进度与错误等。

## 传感器与硬件

- 传感器：`drivers/sensors/dht11.*`、`drivers/sensors/ds18b20.*`
- 采集数据字段在传感器发布负载中体现（参考上文 JSON）。
- 设备控制：LED、继电器、舵机；预设控制支持组合动作与新旧两种格式。

## 重要约束与最佳实践

- 服务器地址与 Wi‑Fi SSID/密码必须从 NVS 读取；端口与认证信息不从 NVS 读取；禁止硬编码服务器地址。
- **服务器地址和 OTA 地址统一为"服务器地址"**：不再单独存储 OTA URL，统一使用 `server_config.base_address` 用于 HTTP API、MQTT 和 OTA。
- URL 构建统一通过 `server_config_build_url()` 或 `server_config_build_http_url()`；其中 `base_address` 来自 NVS，端口按编译或部署约定。
- 服务器地址在需要时加载（WiFi 连接成功后），不再在启动第一步加载。
- 安全：`device_secret` 等敏感信息仅用于认证，不在日志中明文输出（当前代码在调试日志中做了长度与掩码处理）。
- 重试与超时：设备注册、MQTT 连接、OTA 检测均带重试机制；失败情况下打印清晰日志便于定位。

## 故障排查（常见场景）

- 设备注册失败：检查 HTTP 服务是否可达、NVS 中的服务器地址是否正确、后端接口是否返回 200。
- UUID 获取失败（`fetch_uuid_by_mac()`）：系统会在多次重试后进入停机循环，不继续执行后续流程；请检查网络与后端服务，确认 `/api/devices/mac/lookup` 正常。
- MQTT 连接失败：确认 MQTT 端口与凭据配置正确（不从 NVS 读取）、Broker 正常运行；查看 `AIOT_MQTT_EVENT_ERROR` 日志。
- OTA 检测失败或更新失败：确认 `/api/firmware/check` 返回内容、`download_url` 可访问、设备可用的闪存与分区配置正确。

## 版本与构建

- ESP-IDF：5.4.3
- 版本号：`CONFIG_AIOT_FIRMWARE_VERSION`（在构建配置中设置）
- 分区表与 OTA：请确保分区表支持 OTA，且启用回滚保护以提升可靠性。
- Kconfig：根据教学或实际部署需要开启/关闭 MQTT、OTA、缓存、压缩、QoS 等选项。

---

维护建议：本文件为当前实现的权威单一说明文档。新增/变更模块时，请同步更新本文件中对应章节，尤其是 NVS 键名、HTTP 端点与 MQTT 主题的约定，确保与实际代码一致。