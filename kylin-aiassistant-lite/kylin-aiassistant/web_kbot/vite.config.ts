import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';
import fs from 'fs';
import path from 'path';

const DEFAULT_DEV_BACKEND = 'http://127.0.0.1:42617';

/**
 * Qt WebEngine 从 qrc:// 加载资源时不附带 MIME 类型，Chromium 对 type="module"
 * 脚本会拒绝执行（日志: non-JavaScript MIME type of ""）。
 * qrc 构建改为经典 script（IIFE），并在产物 index.html 中去掉 module/crossorigin。
 */
function qrcEmbedHtmlFix(): Plugin {
  return {
    name: 'qrc-embed-html-fix',
    apply: 'build',
    closeBundle() {
      const indexPath = path.resolve(__dirname, 'dist/index.html');
      if (!fs.existsSync(indexPath)) return;
      let html = fs.readFileSync(indexPath, 'utf8');
      html = html.replace(
        /<script\s+type="module"\s+crossorigin\s+src="([^"]+)"><\/script>/,
        '<script src="$1"></script>',
      );
      html = html.replace(
        /<link\s+rel="stylesheet"\s+crossorigin\s+href="\.\/assets\/style\.css">/,
        '<link rel="stylesheet" href="./assets/index.css">',
      );
      html = html.replace(
        /<link\s+rel="stylesheet"\s+crossorigin\s+href="\.\/assets\/index\.css">/,
        '<link rel="stylesheet" href="./assets/index.css">',
      );
      // IIFE 在 <head> 中会同步执行，此时 #root 尚未解析，createRoot 抛 React #299。
      // 移到 body 末尾：先 qwebchannel，再应用脚本（与原先 module 的 deferred 行为一致）。
      const appScript =
        /<script\s+src="\.\/assets\/index\.js"><\/script>\s*/;
      if (appScript.test(html)) {
        html = html.replace(appScript, '');
        html = html.replace(
          /<script\s+src="\.\/qwebchannel\.js"><\/script>/,
          '<script src="./qwebchannel.js"></script>\n  <script src="./assets/index.js"></script>',
        );
      }
      fs.writeFileSync(indexPath, html);
    },
  };
}

// Production assets are served by the Rust gateway via rust-embed under /_app/.
// During local development, Vite serves the frontend directly and proxies API/ws
// traffic to a live KylinBot backend so edits are visible immediately.
export default defineConfig(({ command }) => {
  // 允许通过环境变量切换开发态后端地址；未配置时回退到本地默认端口。
  const backendTarget = process.env.KYLINBOT_DEV_BACKEND ?? DEFAULT_DEV_BACKEND;
  // serve=本地开发；build=生产打包。用于控制 base 与 dev proxy。
  const isDev = command === 'serve';
  // 本目录 web_kbot 供 kylin-aiassistant 的 qrc 嵌入；生产构建默认相对路径（./）。
  // 仅当需要 Rust gateway 的 /_app/ 部署时显式设置 KYLINBOT_GATEWAY=1。
  const isQrc = !isDev && process.env.KYLINBOT_GATEWAY !== '1';

  return {
    base: isDev ? '/' : isQrc ? './' : '/_app/',
    // 关闭 Tailwind v4 内置 optimize（其内部目标浏览器偏现代），
    // 改由 postcss.config.js + build.cssTarget 统一做旧内核兼容处理。
    plugins: [
      react(),
      tailwindcss({ optimize: false }),
      ...(isQrc ? [qrcEmbedHtmlFix()] : []),
    ],
    resolve: {
      alias: {
        '@': path.resolve(__dirname, './src'),
      },
    },
    server: isDev
      ? {
          host: '127.0.0.1',
          port: 5173,
          strictPort: true,
          proxy: {
            '/api': {
              target: backendTarget,
              changeOrigin: true,
            },
            '/ws': {
              target: backendTarget.replace(/^http/, 'ws'),
              ws: true,
              changeOrigin: true,
            },
            '/pair': {
              target: backendTarget,
              changeOrigin: true,
            },
            '/health': {
              target: backendTarget,
              changeOrigin: true,
            },
            '/admin': {
              target: backendTarget,
              changeOrigin: true,
            },
          },
        }
      : undefined,
    optimizeDeps: {
      // Qt 5.12(.12) WebEngine ~ Chromium 69：smol-toml 等会使用 ?. / Object.hasOwn，
      // 须在依赖预构建阶段降级至 chrome69；否则 dev（及旧内核）无法运行。
      include: ['smol-toml'],
      esbuildOptions: {
        target: 'chrome69',
      },
    },
    build: {
      outDir: 'dist',
      // Qt 5.12 WebEngine ~ Chromium 69，JS/CSS 按该基线输出。
      target: ['chrome69', 'edge79', 'firefox78', 'safari13'],
      cssTarget: 'chrome69',
      // 继续使用 LightningCSS 压缩；配合 cssTarget 产出兼容 CSS。
      cssMinify: 'lightningcss',
      // qrc：单文件 IIFE，避免 qrc:// 下额外 chunk 也触发 module MIME 校验失败。
      ...(isQrc
        ? {
            modulePreload: false,
            cssCodeSplit: false,
          }
        : {}),
      // qrc 嵌入模式下文件名哈希无意义，使用固定名称避免每次构建都要更新 webkbot.qrc。
      rollupOptions: {
        output: {
          format: isQrc ? 'iife' : 'es',
          ...(isQrc ? { inlineDynamicImports: true, name: 'KylinBotWeb' } : {}),
          entryFileNames: 'assets/index.js',
          chunkFileNames: 'assets/[name].js',
          assetFileNames: (info) => {
            const base = info.names?.[0] ?? '';
            if (base.endsWith('.css')) {
              return 'assets/index.css';
            }
            return 'assets/[name][extname]';
          },
        },
      },
    },
  };
});
