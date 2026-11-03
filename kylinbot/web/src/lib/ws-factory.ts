// WebSocket factory.
//
// Originally created to route around the WKWebView/WebKitGTK limitation that
// `new WebSocket()` cannot use a custom URI scheme handler. That route turned
// out to be unworkable in Tauri 2 because IPC itself doesn't function reliably
// when the page loads from a user-registered custom protocol (tauri#5632), so
// the desktop client now loads from `http://127.0.0.1:<proxy-port>/_app/` and
// the Tauri Rust side runs a localhost TCP→UDS relay. With that proxy in
// place, native `new WebSocket()` works against the same loopback origin in
// every environment, so the factory is just a passthrough.
//
// The factory remains as the single import point so that if we ever need to
// branch again (e.g. for a different transport), only this file changes.

export type WebSocketLike = WebSocket;

export function createWebSocket(
  url: string,
  protocols?: string | string[],
): WebSocketLike {
  return new WebSocket(url, protocols);
}
