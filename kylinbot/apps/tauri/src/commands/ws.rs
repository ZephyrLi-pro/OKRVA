//! WebSocket bridge for the Tauri WebView.
//!
//! The WebView's native `new WebSocket(url)` bypasses the custom URI scheme
//! handler (WKWebView / WebKitGTK only route HTTP requests through it), so when
//! the gateway listens on a Unix domain socket the page-level WebSocket cannot
//! reach it. These commands open the WS handshake on the Rust side over UDS
//! (or TCP) using `tokio-tungstenite`, then pump frames back to the WebView via
//! `app.emit("ws_msg_<handle>", ...)` and `app.emit("ws_close_<handle>", ...)`.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use futures_util::{SinkExt, StreamExt};
use http::Request;
use serde::Serialize;
use tauri::{AppHandle, Emitter, State};
use tokio::io::{AsyncRead, AsyncWrite};
use tokio::sync::mpsc;
use tokio_tungstenite::WebSocketStream;
use tokio_tungstenite::tungstenite::Message;

use crate::state::{DesktopTransport, SharedState};

pub type WsHandle = u64;

#[derive(Debug)]
enum WsCommand {
    Send(String),
    Close,
}

#[derive(Default)]
pub struct WsRegistry {
    next_id: AtomicU64,
    conns: Mutex<HashMap<u64, mpsc::UnboundedSender<WsCommand>>>,
}

impl WsRegistry {
    pub fn new() -> Self {
        Self::default()
    }

    fn next_id(&self) -> u64 {
        self.next_id.fetch_add(1, Ordering::Relaxed)
    }

    fn insert(&self, id: u64, tx: mpsc::UnboundedSender<WsCommand>) {
        self.conns
            .lock()
            .expect("ws registry mutex poisoned")
            .insert(id, tx);
    }

    fn take(&self, id: u64) -> Option<mpsc::UnboundedSender<WsCommand>> {
        self.conns
            .lock()
            .expect("ws registry mutex poisoned")
            .remove(&id)
    }

    fn get(&self, id: u64) -> Option<mpsc::UnboundedSender<WsCommand>> {
        self.conns
            .lock()
            .expect("ws registry mutex poisoned")
            .get(&id)
            .cloned()
    }
}

#[derive(Serialize, Clone)]
struct WsMsgPayload {
    data: String,
}

#[derive(Serialize, Clone)]
struct WsClosePayload {
    code: u16,
    reason: String,
}

fn build_ws_request(path: &str, protocols: &[String]) -> Result<Request<()>, String> {
    let uri = format!("ws://localhost{path}");
    let mut req = Request::builder()
        .uri(&uri)
        .method("GET")
        .body(())
        .map_err(|e| format!("build ws request: {e}"))?;
    if !protocols.is_empty() {
        let joined = protocols.join(", ");
        let value = http::HeaderValue::from_str(&joined)
            .map_err(|e| format!("invalid Sec-WebSocket-Protocol value: {e}"))?;
        req.headers_mut().insert("Sec-WebSocket-Protocol", value);
    }
    Ok(req)
}

fn spawn_pump<S>(
    ws: WebSocketStream<S>,
    mut rx: mpsc::UnboundedReceiver<WsCommand>,
    app: AppHandle,
    handle: u64,
    registry: Arc<WsRegistry>,
) where
    S: AsyncRead + AsyncWrite + Send + Unpin + 'static,
{
    let (mut sink, mut stream) = ws.split();
    let msg_event = format!("ws_msg_{handle}");
    let close_event = format!("ws_close_{handle}");

    let app_r = app.clone();
    let reg_r = registry.clone();
    let close_event_r = close_event.clone();
    tokio::spawn(async move {
        while let Some(msg) = stream.next().await {
            match msg {
                Ok(Message::Text(s)) => {
                    let _ = app_r.emit(
                        &msg_event,
                        WsMsgPayload {
                            data: s.to_string(),
                        },
                    );
                }
                Ok(Message::Close(frame)) => {
                    let (code, reason) = frame
                        .map(|f| (u16::from(f.code), f.reason.to_string()))
                        .unwrap_or((1005, String::new()));
                    let _ = app_r.emit(&close_event_r, WsClosePayload { code, reason });
                    break;
                }
                Ok(_) => {
                    // Binary / Ping / Pong / Frame — current callers only send JSON text.
                }
                Err(_) => {
                    let _ = app_r.emit(
                        &close_event_r,
                        WsClosePayload {
                            code: 1006,
                            reason: String::new(),
                        },
                    );
                    break;
                }
            }
        }
        reg_r.take(handle);
    });

    tokio::spawn(async move {
        while let Some(cmd) = rx.recv().await {
            match cmd {
                WsCommand::Send(s) => {
                    if sink.send(Message::Text(s.into())).await.is_err() {
                        break;
                    }
                }
                WsCommand::Close => {
                    let _ = sink.send(Message::Close(None)).await;
                    break;
                }
            }
        }
        let _ = sink.close().await;
    });
}

#[tauri::command]
pub async fn ws_open(
    app: AppHandle,
    state: State<'_, SharedState>,
    registry: State<'_, Arc<WsRegistry>>,
    path: String,
    protocols: Vec<String>,
) -> Result<WsHandle, String> {
    let transport = { state.read().await.transport.clone() };
    let request = build_ws_request(&path, &protocols)?;
    let (tx, rx) = mpsc::unbounded_channel();
    let id = registry.next_id();
    let reg_arc: Arc<WsRegistry> = registry.inner().clone();
    reg_arc.insert(id, tx);

    let handshake = match transport {
        DesktopTransport::Uds { socket_path } => {
            let stream = match tokio::net::UnixStream::connect(&socket_path).await {
                Ok(s) => s,
                Err(e) => {
                    reg_arc.take(id);
                    return Err(format!("uds connect to {}: {e}", socket_path.display()));
                }
            };
            tokio_tungstenite::client_async(request, stream)
                .await
                .map(|(ws, _resp)| spawn_pump(ws, rx, app.clone(), id, reg_arc.clone()))
        }
        DesktopTransport::Tcp { base_url } => {
            let host_port = base_url
                .trim_start_matches("https://")
                .trim_start_matches("http://")
                .trim_end_matches('/');
            let stream = match tokio::net::TcpStream::connect(host_port).await {
                Ok(s) => s,
                Err(e) => {
                    reg_arc.take(id);
                    return Err(format!("tcp connect to {host_port}: {e}"));
                }
            };
            tokio_tungstenite::client_async(request, stream)
                .await
                .map(|(ws, _resp)| spawn_pump(ws, rx, app.clone(), id, reg_arc.clone()))
        }
    };

    if let Err(e) = handshake {
        reg_arc.take(id);
        return Err(format!("ws handshake: {e}"));
    }
    Ok(id)
}

#[tauri::command]
pub async fn ws_send(
    registry: State<'_, Arc<WsRegistry>>,
    handle: WsHandle,
    data: String,
) -> Result<(), String> {
    let tx = registry
        .get(handle)
        .ok_or_else(|| format!("ws handle {handle} not found"))?;
    tx.send(WsCommand::Send(data))
        .map_err(|_| format!("ws handle {handle} closed"))?;
    Ok(())
}

#[tauri::command]
pub async fn ws_close(
    registry: State<'_, Arc<WsRegistry>>,
    handle: WsHandle,
) -> Result<(), String> {
    if let Some(tx) = registry.get(handle) {
        let _ = tx.send(WsCommand::Close);
    }
    registry.take(handle);
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registry_next_id_monotonic() {
        let reg = WsRegistry::new();
        assert_eq!(reg.next_id(), 0);
        assert_eq!(reg.next_id(), 1);
        assert_eq!(reg.next_id(), 2);
    }

    #[test]
    fn registry_insert_get_take() {
        let reg = WsRegistry::new();
        let (tx, _rx) = mpsc::unbounded_channel::<WsCommand>();
        reg.insert(42, tx);
        assert!(reg.get(42).is_some());
        assert!(reg.take(42).is_some());
        assert!(reg.get(42).is_none());
    }

    #[test]
    fn build_ws_request_no_protocols() {
        let req = build_ws_request("/ws/chat?session_id=abc", &[]).unwrap();
        assert_eq!(req.uri().path(), "/ws/chat");
        assert_eq!(req.uri().query(), Some("session_id=abc"));
        assert!(req.headers().get("Sec-WebSocket-Protocol").is_none());
    }

    #[test]
    fn build_ws_request_with_protocols() {
        let req = build_ws_request(
            "/ws/chat",
            &["kylinbot.v1".to_string(), "bearer.zc_xyz".to_string()],
        )
        .unwrap();
        let proto = req
            .headers()
            .get("Sec-WebSocket-Protocol")
            .and_then(|v| v.to_str().ok())
            .unwrap();
        assert_eq!(proto, "kylinbot.v1, bearer.zc_xyz");
    }
}
