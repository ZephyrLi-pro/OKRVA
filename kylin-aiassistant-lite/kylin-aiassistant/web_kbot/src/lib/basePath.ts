// Runtime base path injected by the Rust gateway into index.html.
// Allows the SPA to work under a reverse-proxy path prefix.
// When running inside Tauri, the frontend is served from disk so basePath is
// empty and API calls target the gateway URL directly.
// When loaded from qrc://, API calls go through QWebChannel bridge (Unix Socket),
// no HTTP origin is needed.

import { isTauri, tauriGatewayUrl } from './tauri';

declare global {
  interface Window {
    __KYLINBOT_BASE__?: string;
  }
}

/** True when the SPA is loaded from a Qt resource (qrc://) URL. */
export const isQrcMode: boolean =
  typeof window !== 'undefined' && window.location.protocol === 'qrc:';

/** Gateway path prefix (e.g. "/kylinbot"), or empty string when served at root. */
export const basePath: string = isQrcMode
  ? ''
  : isTauri()
    ? ''
    : (window.__KYLINBOT_BASE__ ?? '').replace(/\/+$/, '');

/** Static asset base (e.g. "/_app" or "/kylinbot/_app"). */
const viteBasePath = import.meta.env.BASE_URL === '/'
  ? ''
  : import.meta.env.BASE_URL.replace(/\/+$/, '');

export const assetBasePath: string = isQrcMode
  ? '.'
  : isTauri()
    ? `${tauriGatewayUrl().replace(/\/+$/, '')}/_app`
    : `${basePath}${viteBasePath}`;

/** Full origin for API requests. Empty when using same-origin or QWebChannel bridge. */
export const apiOrigin: string = isQrcMode
  ? ''
  : isTauri()
    ? tauriGatewayUrl()
    : '';
