// ====================================
// AIOT系统使用指南 - 主JavaScript文件
// ====================================

document.addEventListener('DOMContentLoaded', function() {
    // 初始化所有功能
    initMobileMenu();
    initCodeCopy();
    initSearch();
    initScrollSpy();
    initSmoothScroll();
    highlightCurrentPage();
    initBackToTop();
});

// 移动端菜单
function initMobileMenu() {
    const menuToggle = document.querySelector('.menu-toggle');
    const navMenu = document.querySelector('.nav-menu');
    
    if (menuToggle && navMenu) {
        menuToggle.addEventListener('click', function() {
            navMenu.classList.toggle('active');
        });

        // 点击菜单项后关闭菜单
        const menuItems = navMenu.querySelectorAll('a');
        menuItems.forEach(item => {
            item.addEventListener('click', function() {
                navMenu.classList.remove('active');
            });
        });
    }
}

// 代码复制功能
function initCodeCopy() {
    const codeBlocks = document.querySelectorAll('.code-block');
    
    codeBlocks.forEach(block => {
        const copyBtn = block.querySelector('.copy-btn');
        const code = block.querySelector('pre').textContent;
        
        if (copyBtn) {
            copyBtn.addEventListener('click', function() {
                copyToClipboard(code);
                copyBtn.textContent = '✓ 已复制';
                copyBtn.style.background = '#10b981';
                
                setTimeout(() => {
                    copyBtn.textContent = '复制';
                    copyBtn.style.background = '';
                }, 2000);
            });
        }
    });
}

// 复制到剪贴板
function copyToClipboard(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text);
    } else {
        // 降级方案
        const textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        textarea.select();
        document.execCommand('copy');
        document.body.removeChild(textarea);
    }
}

// 搜索功能
function initSearch() {
    const searchInput = document.querySelector('.search-input');
    
    if (searchInput) {
        searchInput.addEventListener('input', debounce(function(e) {
            const query = e.target.value.toLowerCase();
            searchContent(query);
        }, 300));
    }
}

// 搜索内容
function searchContent(query) {
    if (!query) {
        // 清除高亮
        removeHighlights();
        return;
    }
    
    const content = document.querySelector('.main-container');
    if (!content) return;
    
    removeHighlights();
    
    // 简单的关键词高亮
    const walker = document.createTreeWalker(
        content,
        NodeFilter.SHOW_TEXT,
        null,
        false
    );
    
    const textNodes = [];
    let node;
    while (node = walker.nextNode()) {
        if (node.nodeValue.toLowerCase().includes(query)) {
            textNodes.push(node);
        }
    }
    
    textNodes.forEach(textNode => {
        const parent = textNode.parentNode;
        if (parent.tagName !== 'SCRIPT' && parent.tagName !== 'STYLE') {
            const text = textNode.nodeValue;
            const regex = new RegExp(`(${query})`, 'gi');
            const highlightedText = text.replace(regex, '<mark>$1</mark>');
            
            const span = document.createElement('span');
            span.innerHTML = highlightedText;
            parent.replaceChild(span, textNode);
        }
    });
}

// 移除高亮
function removeHighlights() {
    const marks = document.querySelectorAll('mark');
    marks.forEach(mark => {
        const parent = mark.parentNode;
        parent.replaceChild(document.createTextNode(mark.textContent), mark);
    });
}

// 防抖函数
function debounce(func, wait) {
    let timeout;
    return function(...args) {
        clearTimeout(timeout);
        timeout = setTimeout(() => func.apply(this, args), wait);
    };
}

// 滚动监听（高亮当前章节）
function initScrollSpy() {
    const tocLinks = document.querySelectorAll('.toc-list a');
    if (tocLinks.length === 0) return;
    
    const sections = Array.from(tocLinks).map(link => {
        const id = link.getAttribute('href').substring(1);
        return document.getElementById(id);
    }).filter(section => section !== null);
    
    if (sections.length === 0) return;
    
    window.addEventListener('scroll', debounce(function() {
        let current = '';
        
        sections.forEach(section => {
            const sectionTop = section.offsetTop;
            if (window.pageYOffset >= sectionTop - 100) {
                current = section.getAttribute('id');
            }
        });
        
        tocLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('href') === `#${current}`) {
                link.classList.add('active');
            }
        });
    }, 100));
}

// 平滑滚动
function initSmoothScroll() {
    const links = document.querySelectorAll('a[href^="#"]');
    
    links.forEach(link => {
        link.addEventListener('click', function(e) {
            const href = this.getAttribute('href');
            if (href === '#') return;
            
            const target = document.querySelector(href);
            if (target) {
                e.preventDefault();
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });
}

// 高亮当前页面
function highlightCurrentPage() {
    const currentPath = window.location.pathname;
    const navLinks = document.querySelectorAll('.nav-menu a');
    
    navLinks.forEach(link => {
        const linkPath = new URL(link.href).pathname;
        if (currentPath === linkPath || 
            (currentPath.endsWith('/') && linkPath === '/index.html')) {
            link.classList.add('active');
        }
    });
}

// 返回顶部按钮
function initBackToTop() {
    // 创建返回顶部按钮
    const backToTop = document.createElement('button');
    backToTop.className = 'back-to-top';
    backToTop.innerHTML = '↑';
    backToTop.style.cssText = `
        position: fixed;
        bottom: 30px;
        right: 30px;
        width: 50px;
        height: 50px;
        background: var(--primary-color);
        color: white;
        border: none;
        border-radius: 50%;
        font-size: 1.5rem;
        cursor: pointer;
        display: none;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        transition: all 0.3s;
        z-index: 999;
    `;
    
    document.body.appendChild(backToTop);
    
    // 滚动时显示/隐藏按钮
    window.addEventListener('scroll', function() {
        if (window.pageYOffset > 300) {
            backToTop.style.display = 'block';
        } else {
            backToTop.style.display = 'none';
        }
    });
    
    // 点击返回顶部
    backToTop.addEventListener('click', function() {
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
    });
    
    // 鼠标悬停效果
    backToTop.addEventListener('mouseenter', function() {
        this.style.transform = 'scale(1.1)';
    });
    
    backToTop.addEventListener('mouseleave', function() {
        this.style.transform = 'scale(1)';
    });
}

// 工具函数：生成目录
function generateTOC() {
    const headings = document.querySelectorAll('h2, h3');
    const tocList = document.querySelector('.toc-list');
    
    if (!tocList || headings.length === 0) return;
    
    headings.forEach((heading, index) => {
        const id = heading.id || `heading-${index}`;
        heading.id = id;
        
        const li = document.createElement('li');
        const a = document.createElement('a');
        a.href = `#${id}`;
        a.textContent = heading.textContent;
        
        if (heading.tagName === 'H3') {
            a.style.paddingLeft = '15px';
            a.style.fontSize = '0.8rem';
        }
        
        li.appendChild(a);
        tocList.appendChild(li);
    });
}

// 页面加载完成后生成目录
window.addEventListener('load', function() {
    generateTOC();
});

// 打印功能
function printPage() {
    window.print();
}

// 分享功能
function shareUrl() {
    const url = window.location.href;
    
    if (navigator.share) {
        navigator.share({
            title: document.title,
            url: url
        }).catch(err => console.log('分享失败:', err));
    } else {
        copyToClipboard(url);
        alert('链接已复制到剪贴板！');
    }
}

// 反馈功能
function submitFeedback(helpful) {
    // 这里可以连接到后端API记录反馈
    console.log('用户反馈:', helpful ? '有帮助' : '无帮助');
    alert(helpful ? '感谢您的反馈！' : '我们会继续改进文档！');
}

