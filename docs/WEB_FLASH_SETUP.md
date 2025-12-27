# Web烧录功能配置说明

## 概述

`firmware-flash.html` 页面已集成 **ESP Web Tools**，支持用户直接在浏览器中烧录ESP32固件，无需安装任何软件。

## 工作原理

ESP Web Tools 使用浏览器的 **Web Serial API**，可以直接与USB串口设备通信，实现固件烧录。

## 配置步骤

### 1. 准备固件文件

将编译好的合并版固件放在文档站点可访问的位置：

```bash
docs/
├── firmware/
│   └── aiot-esp32s3-firmware-merged.bin  # 合并版固件
├── manifest.json                          # Web Tools配置文件
└── firmware-flash.html                    # 烧录页面
```

### 2. 配置 manifest.json

复制示例文件并修改：

```bash
cp manifest.json.example manifest.json
```

编辑 `manifest.json`，配置固件路径：

```json
{
  "name": "AIOT ESP32-S3 Firmware",
  "version": "2.1.3",
  "home_assistant_domain": "esphome",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "firmware/aiot-esp32s3-firmware-merged.bin",
          "offset": 0
        }
      ]
    }
  ]
}
```

**参数说明：**
- `name`: 固件显示名称
- `version`: 当前版本号
- `new_install_prompt_erase`: 是否提示擦除设备（建议true）
- `chipFamily`: 芯片系列（ESP32-S3）
- `path`: 固件文件相对路径
- `offset`: 烧录地址（合并版固件使用0x0）

### 3. 更新 HTML 配置

编辑 `firmware-flash.html`，找到这行：

```html
<esp-web-install-button id="installButton">
```

添加 manifest 属性：

```html
<esp-web-install-button 
    id="installButton"
    manifest="./manifest.json">
```

### 4. 测试Web烧录

1. 启动本地HTTP服务器：
   ```bash
   cd docs
   python3 -m http.server 8080
   ```

2. 使用Chrome浏览器访问：
   ```
   http://localhost:8080/firmware-flash.html
   ```

3. 连接ESP32设备，点击"连接设备并烧录"按钮

4. 选择串口设备，开始烧录

## 部署到生产环境

### GitHub Pages

1. 将 `docs/` 目录推送到GitHub仓库
2. 在仓库设置中启用GitHub Pages
3. 选择 `main` 分支的 `/docs` 目录

### 自定义服务器

确保Web服务器：
- ✅ 支持HTTPS（Web Serial API要求）
- ✅ 正确设置MIME类型（.bin文件）
- ✅ 允许跨域访问manifest.json

Nginx配置示例：

```nginx
server {
    listen 443 ssl;
    server_name docs.aiot.example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    root /var/www/aiot-docs;
    
    location / {
        try_files $uri $uri/ =404;
    }
    
    # 确保.bin文件正确类型
    location ~* \.bin$ {
        types { application/octet-stream bin; }
        add_header Content-Disposition 'attachment';
    }
    
    # 允许manifest.json跨域
    location = /manifest.json {
        add_header Access-Control-Allow-Origin *;
    }
}
```

## 浏览器支持

### ✅ 支持的浏览器
- Chrome 89+ (推荐)
- Edge 89+
- Opera 75+

### ❌ 不支持的浏览器
- Firefox (Mozilla未实现Web Serial API)
- Safari (Apple政策限制)
- 移动端浏览器（大多数不支持）

对于不支持的浏览器，页面会自动显示"方式2：工具烧录"的指引。

## 故障排查

### 问题1：按钮无响应
- 检查是否使用Chrome/Edge浏览器
- 确认manifest.json路径正确
- 查看浏览器控制台错误信息

### 问题2：找不到串口设备
- Windows：安装CP2102/CH340驱动
- Mac：允许浏览器访问USB设备权限
- Linux：添加用户到dialout组

### 问题3：烧录失败
- 检查固件文件是否完整
- 确认是合并版固件（merged）
- 尝试降低波特率（在manifest中配置）

## 高级配置

### 多版本固件支持

可以在manifest中配置多个固件版本：

```json
{
  "name": "AIOT ESP32-S3 Firmware",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": false,
      "parts": [
        {
          "path": "firmware/v2.1.3/merged.bin",
          "offset": 0
        }
      ]
    }
  ]
}
```

### 自定义波特率

```json
{
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "flashFreq": "80m",
      "flashMode": "dio",
      "flashSize": "8MB",
      "parts": [...]
    }
  ]
}
```

## 相关资源

- [ESP Web Tools 官方文档](https://esphome.github.io/esp-web-tools/)
- [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [Espressif 固件烧录指南](https://docs.espressif.com/projects/esptool/en/latest/)

## 安全说明

⚠️ **重要**：
1. 固件文件应该通过HTTPS提供（防止中间人攻击）
2. 验证固件来源和完整性
3. 用户烧录前应了解风险（数据清除）
4. 建议提供固件的SHA256校验值

## 维护清单

定期检查：
- ✅ 固件文件可访问性
- ✅ manifest.json配置正确
- ✅ ESP Web Tools CDN可用
- ✅ 浏览器兼容性
- ✅ SSL证书有效期

---

**问题反馈**: 如遇到问题，请查看浏览器控制台日志并提供详细信息。

