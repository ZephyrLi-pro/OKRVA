// Tauri detection utilities for KylinBot Desktop.

declare global {
  interface Window {
    __TAURI__?: unknown;
    __TAURI_INTERNALS__?: unknown;
    __KYLINBOT_GATEWAY__?: string;
  }
}

/**
 * Returns true when running inside a Tauri WebView.
 *
 * Tauri 2 only exposes `window.__TAURI__` when `app.withGlobalTauri: true` is
 * set in `tauri.conf.json`; otherwise that global is undefined. The reliable
 * v2-era marker is `window.__TAURI_INTERNALS__`, which the runtime always
 * injects (it's what `@tauri-apps/api`'s `core.isTauri` checks too). The
 * legacy `__TAURI__` check is kept for forward/backward-compat with v1 builds.
 */
export const isTauri = (): boolean =>
  '__TAURI_INTERNALS__' in window || '__TAURI__' in window;

/**
 * Gateway base URL when running inside Tauri. Empty string means same-origin —
 * we're loaded from `kylinbot://localhost/_app/` and all HTTP requests use
 * relative paths which the custom URI scheme handler proxies to UDS. The
 * legacy hardcoded `http://127.0.0.1:42617` fallback was removed because the
 * gateway no longer listens on TCP by default.
 */
export const tauriGatewayUrl = (): string => window.__KYLINBOT_GATEWAY__ ?? '';
