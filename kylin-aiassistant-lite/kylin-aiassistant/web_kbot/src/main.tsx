import './polyfills';
import React from 'react';
import ReactDOM from 'react-dom/client';
import { BrowserRouter, MemoryRouter, useNavigate } from 'react-router-dom';
import App from './App';
import { basePath, isQrcMode } from './lib/basePath';
import './index.css';

// Global disable right-click context menu (applies to all pages including dialogs)
document.addEventListener('contextmenu', (e) => e.preventDefault());

// qrc:// 协议下 new URL(window.location.href) 会抛异常（React Router 内部调用），
// 使用 MemoryRouter 完全在内存中管理路由，不触碰 window.location。
// QML 首次加载通过 hash 片段传入初始路由，后续切换通过 window.__kylinbotNavigate() 导航。
function getQrcInitialRoute(): string {
  try {
    const hash = window.location.hash;
    // hash 格式: #/channel, #/cron, #/model, #/skills
    return hash.startsWith('#') ? hash.slice(1) : '/';
  } catch {
    return '/';
  }
}

// MemoryRouter 的 navigate 引用，供 QML 通过 runJavaScript 调用
let memoryNavigate: ((path: string) => void) | null = null;

/** 在 MemoryRouter 内部捕获 navigate 函数 */
function NavigateSetter() {
  const navigate = useNavigate();
  React.useEffect(() => {
    memoryNavigate = navigate;
    setKylinbotNavigate((path: string) => {
      if (memoryNavigate) memoryNavigate(path);
    });
  }, [navigate]);
  return null;
}

/** QML 通过 runJavaScript("window.__kylinbotNavigate('/channel')") 切换路由 */
function setKylinbotNavigate(handler: (path: string) => void) {
  const win = window as any;
  win.__kylinbotNavigateImpl = handler;
  win.__kylinbotNavigate = handler;
  if (typeof win.__kylinbotFlushNavigate === 'function') {
    win.__kylinbotFlushNavigate();
  }
}

setKylinbotNavigate((path: string) => {
  if (memoryNavigate) {
    memoryNavigate(path);
  }
});

function mountApp(): void {
  const rootEl = document.getElementById('root');
  if (!rootEl) {
    console.error('[KylinBot] #root not found — check index.html script order for qrc embed');
    return;
  }

  if (isQrcMode) {
    ReactDOM.createRoot(rootEl).render(
      <React.StrictMode>
        <MemoryRouter initialEntries={[getQrcInitialRoute()]}>
          <NavigateSetter />
          <App />
        </MemoryRouter>
      </React.StrictMode>,
    );
  } else {
    ReactDOM.createRoot(rootEl).render(
      <React.StrictMode>
        <BrowserRouter basename={basePath || '/'}>
          <App />
        </BrowserRouter>
      </React.StrictMode>,
    );
  }
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', mountApp);
} else {
  mountApp();
}
