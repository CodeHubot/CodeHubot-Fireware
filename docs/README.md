# AIOT系统使用指南文档

这是一个完整的AIOT智能设备管理系统使用指南网站，提供从入门到精通的完整文档。

## 📚 文档结构

```
docs/
├── index.html           # 首页 - 系统简介和快速导航
├── quick-start.html     # 快速开始 - 10分钟上手指南
├── admin-panel.html     # 管理后台使用指南（待创建）
├── device-config.html   # 设备配置说明（待创建）
├── ai-control.html      # AI智能控制指南（待创建）
├── troubleshooting.html # 故障排查手册（待创建）
├── faq.html            # 常见问题解答（待创建）
├── css/
│   └── style.css       # 全局样式文件
├── js/
│   └── main.js         # JavaScript交互功能
└── images/             # 图片资源目录
```

## 🚀 使用方法

### 本地预览

1. **直接打开**：
   ```bash
   # 在浏览器中打开 index.html
   open index.html  # macOS
   start index.html # Windows
   ```

2. **使用本地服务器**（推荐）：
   ```bash
   # Python 3
   cd docs
   python3 -m http.server 8080
   
   # 然后访问 http://localhost:8080
   ```

3. **使用Node.js**：
   ```bash
   npx http-server docs -p 8080
   ```

### 部署到服务器

#### 方法1：使用Nginx

```nginx
server {
    listen 80;
    server_name docs.aiot.example.com;
    
    root /path/to/docs;
    index index.html;
    
    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

#### 方法2：使用GitHub Pages

1. 将`docs`目录推送到GitHub仓库
2. 在仓库设置中启用GitHub Pages
3. 选择`docs`目录作为源

#### 方法3：使用Vercel/Netlify

直接连接GitHub仓库，选择`docs`目录自动部署。

## ✨ 功能特性

### 已实现功能

- ✅ 响应式设计（支持手机/平板/桌面）
- ✅ 现代化UI（使用CSS3和渐变动画）
- ✅ 代码高亮和复制功能
- ✅ 移动端导航菜单
- ✅ 返回顶部按钮
- ✅ 平滑滚动
- ✅ 面包屑导航
- ✅ 搜索功能（关键词高亮）
- ✅ 页面目录（右侧边栏）
- ✅ 进度指示器

### 待实现功能

- ⏳ 全文搜索
- ⏳ 多语言支持（中文/英文）
- ⏳ 暗黑模式
- ⏳ 版本切换
- ⏳ 在线Demo
- ⏳ 反馈系统

## 📖 页面说明

### 1. 首页 (`index.html`)

**内容**：
- 系统简介和核心功能
- 快速导航卡片
- 适用场景展示
- 系统架构图
- 核心特性说明
- 版本信息

**目标用户**：所有用户

### 2. 快速开始 (`quick-start.html`)

**内容**：
- 准备工作清单
- 设备开箱指南
- 三步配网流程
- 连接验证方法
- 常见问题解答

**目标用户**：新手用户

### 3. 管理后台（待创建）

**计划内容**：
- 登录系统
- 设备管理
- 实时控制
- 传感器数据
- 预设指令

### 4. 设备配置（待创建）

**计划内容**：
- WiFi配置
- 服务器配置
- 设备注册
- 外设扩展

### 5. AI控制（待创建）

**计划内容**：
- Coze插件介绍
- 配置方法
- 语音控制示例
- 调试模式

### 6. 故障排查（待创建）

**计划内容**：
- 配网问题
- 连接问题
- 控制问题
- 错误码说明

### 7. FAQ（待创建）

**计划内容**：
- 设备相关
- 配网相关
- 控制相关
- AI相关

## 🎨 样式定制

### 颜色变量

在`css/style.css`中修改CSS变量：

```css
:root {
    --primary-color: #2563eb;      /* 主题色 */
    --secondary-color: #10b981;    /* 辅助色 */
    --danger-color: #ef4444;       /* 危险色 */
    --warning-color: #f59e0b;      /* 警告色 */
    --bg-color: #f8fafc;           /* 背景色 */
    --text-color: #1e293b;         /* 文字色 */
    /* ... */
}
```

### 响应式断点

- 桌面端：> 768px
- 移动端：≤ 768px

## 🛠️ 开发指南

### 添加新页面

1. 复制现有页面模板（如`quick-start.html`）
2. 修改页面标题和描述
3. 更新导航栏激活状态
4. 添加页面内容
5. 更新README

### 代码规范

- HTML使用语义化标签
- CSS使用BEM命名规范
- JavaScript使用ES6+语法
- 注释使用中文

### 组件使用

#### 卡片组件

```html
<div class="card">
    <h2 class="card-title">标题</h2>
    <div class="card-content">
        <!-- 内容 -->
    </div>
</div>
```

#### 提示框

```html
<div class="alert alert-info">
    <div class="alert-icon">💡</div>
    <div class="alert-content">
        <div class="alert-title">提示</div>
        <p>提示内容</p>
    </div>
</div>
```

类型：`alert-info`, `alert-success`, `alert-warning`, `alert-danger`

#### 步骤指示器

```html
<div class="steps">
    <div class="step">
        <div class="step-number">1</div>
        <div class="step-content">
            <div class="step-title">步骤标题</div>
            <div class="step-description">步骤说明</div>
        </div>
    </div>
</div>
```

#### 代码块

```html
<div class="code-block">
    <div class="code-header">
        <span class="code-language">Bash</span>
        <button class="copy-btn">复制</button>
    </div>
    <pre>code here</pre>
</div>
```

## 📝 内容编写建议

### 文案风格

- 使用简洁明了的语言
- 多用列表和表格
- 添加视觉图标（emoji）
- 突出重点信息
- 提供实际示例

### 结构建议

- 每页不超过5个主要章节
- 每章节不超过3个子章节
- 使用渐进式信息披露
- 添加"下一步"引导

### 图片使用

- 放在`images/`目录
- 使用描述性文件名
- 提供alt文本
- 压缩图片大小

## 🔧 维护指南

### 定期更新

- 固件版本更新后同步文档
- 功能变更后更新说明
- 收集用户反馈改进

### 质量检查

- 检查链接有效性
- 测试所有交互功能
- 验证不同浏览器兼容性
- 确保移动端显示正常

## 📞 贡献指南

欢迎提交改进建议和内容补充！

### 提交方式

1. Fork本仓库
2. 创建特性分支
3. 提交更改
4. 发起Pull Request

### 内容要求

- 准确无误
- 格式规范
- 易于理解
- 包含示例

## 📄 许可证

本文档采用MIT许可证。

## 🙏 致谢

- ESP-IDF文档项目
- LVGL文档项目
- 各开源UI框架

---

**最后更新**：2025-11-11  
**文档版本**：v1.0.0  
**对应固件版本**：v2.1.3+

