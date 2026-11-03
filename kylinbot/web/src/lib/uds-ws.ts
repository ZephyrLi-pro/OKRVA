// WebSocket-API-compatible shim for Tauri+UDS mode.
//
// The WebView's native `new WebSocket(...)` cannot reach the gateway when the
// gateway listens on a Unix domain socket — the custom URI scheme handler is
// HTTP-only and the browser opens raw TCP connections that go nowhere. This
// class routes the WS lifecycle through Tauri commands (`ws_open`, `ws_send`,
// `ws_close`) and per-handle events (`ws_msg_<id>`, `ws_close_<id>`) emitted by
// the Rust side, which performs the actual handshake over UDS via
// `tokio-tungstenite`.
//
// The public surface matches the parts of WebSocket that this codebase uses
// (see `web/src/lib/ws.ts` and `web/src/pages/Canvas.tsx`).

import { invoke } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';

type ListenerArg<T> = (this: UdsWebSocket, ev: T) => unknown;

interface WsMsgPayload {
  data: string;
}
interface WsClosePayload {
  code: number;
  reason: string;
}

export class UdsWebSocket {
  static readonly CONNECTING = 0;
  static readonly OPEN = 1;
  static readonly CLOSING = 2;
  static readonly CLOSED = 3;

  readonly CONNECTING = UdsWebSocket.CONNECTING;
  readonly OPEN = UdsWebSocket.OPEN;
  readonly CLOSING = UdsWebSocket.CLOSING;
  readonly CLOSED = UdsWebSocket.CLOSED;

  readyState: number = UdsWebSocket.CONNECTING;
  binaryType: 'blob' | 'arraybuffer' = 'blob';
  readonly url: string;
  readonly protocol: string;

  onopen: ListenerArg<Event> | null = null;
  onmessage: ListenerArg<MessageEvent<string>> | null = null;
  onclose: ListenerArg<CloseEvent> | null = null;
  onerror: ListenerArg<Event> | null = null;

  private handle: number | null = null;
  private unlistenMsg: UnlistenFn | null = null;
  private unlistenClose: UnlistenFn | null = null;
  private pendingClose = false;

  constructor(url: string, protocols?: string | string[]) {
    this.url = url;
    const protoArr = protocols === undefined
      ? []
      : Array.isArray(protocols)
        ? protocols
        : [protocols];
    this.protocol = protoArr[0] ?? '';

    // Parse path+query out of the URL. The scheme/host don't matter — Rust
    // picks the transport based on the desktop's configured `SharedState`.
    let path = '/';
    try {
      const parsed = new URL(url, 'ws://placeholder');
      path = parsed.pathname + parsed.search;
    } catch {
      // Fall through with path='/' — will likely fail at ws_open and surface a
      // proper onerror+onclose.
    }

    void this.openAsync(path, protoArr);
  }

  private async openAsync(path: string, protocols: string[]): Promise<void> {
    try {
      const handle: number = await invoke('ws_open', { path, protocols });
      this.handle = handle;

      this.unlistenMsg = await listen<WsMsgPayload>(`ws_msg_${handle}`, (ev) => {
        if (this.readyState === UdsWebSocket.OPEN) {
          const message = new MessageEvent('message', { data: ev.payload.data });
          this.onmessage?.call(this, message);
        }
      });

      this.unlistenClose = await listen<WsClosePayload>(`ws_close_${handle}`, (ev) => {
        this.finishClose(ev.payload.code, ev.payload.reason, true);
      });

      if (this.pendingClose) {
        // close() was called while the open was still in flight.
        await this.invokeClose();
        return;
      }

      this.readyState = UdsWebSocket.OPEN;
      this.onopen?.call(this, new Event('open'));
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      // Mimic the native WebSocket failure flow: fire 'error' then 'close' 1006.
      this.onerror?.call(this, new Event('error'));
      this.finishClose(1006, message, false);
    }
  }

  send(data: string | ArrayBufferLike | Blob | ArrayBufferView): void {
    if (this.readyState !== UdsWebSocket.OPEN) {
      throw new DOMException(
        `Failed to execute 'send' on 'UdsWebSocket': state is ${this.readyState}`,
        'InvalidStateError',
      );
    }
    if (typeof data !== 'string') {
      throw new DOMException(
        "Failed to execute 'send' on 'UdsWebSocket': binary frames are not supported",
        'NotSupportedError',
      );
    }
    if (this.handle === null) {
      throw new DOMException(
        "Failed to execute 'send' on 'UdsWebSocket': handle is missing",
        'InvalidStateError',
      );
    }
    void invoke('ws_send', { handle: this.handle, data });
  }

  close(_code?: number, _reason?: string): void {
    if (
      this.readyState === UdsWebSocket.CLOSING ||
      this.readyState === UdsWebSocket.CLOSED
    ) {
      return;
    }
    this.readyState = UdsWebSocket.CLOSING;

    if (this.handle === null) {
      // Open hasn't returned yet — flag and let openAsync handle it.
      this.pendingClose = true;
      return;
    }
    void this.invokeClose();
  }

  private async invokeClose(): Promise<void> {
    if (this.handle === null) return;
    try {
      await invoke('ws_close', { handle: this.handle });
    } catch {
      // Surface as a synthetic close so React state machines unwind.
      this.finishClose(1006, 'invoke ws_close failed', false);
    }
  }

  private finishClose(code: number, reason: string, wasClean: boolean): void {
    if (this.readyState === UdsWebSocket.CLOSED) return;
    this.readyState = UdsWebSocket.CLOSED;
    this.unlistenMsg?.();
    this.unlistenClose?.();
    this.unlistenMsg = null;
    this.unlistenClose = null;

    const closeEv = new CloseEvent('close', { code, reason, wasClean });
    this.onclose?.call(this, closeEv);
  }
}
