//! Localhost TCP listener that transparently relays HTTP and WebSocket traffic
//! to the gateway. The WebView is configured to load
//! `http://127.0.0.1:<port>/_app/...`, so all `fetch()` / `new WebSocket()`
//! calls from the React app land here as same-origin requests, and Tauri's
//! IPC (which is sensitive to page origin in Tauri 2 — see tauri-apps/tauri#5632)
//! sees a standard `http://` page and works normally.
//!
//! The proxy is intentionally protocol-agnostic: it does not parse HTTP. Each
//! accepted TCP connection is paired with a connection to the gateway (UDS or
//! TCP) and bytes are pumped in both directions until either side closes. This
//! covers plain HTTP, HTTP/1.1 keep-alive (the gateway will choose to close
//! after each response if needed), and WebSocket upgrades — the upgrade
//! response and subsequent frames are just more bytes on the wire.
//!
//! A fixed loopback port keeps `frontendDist` static in `tauri.conf.json`.
//! If something else is already on the port, `start_proxy` fails with a clear
//! error rather than silently picking another port.

use std::path::PathBuf;

use tokio::io::AsyncWriteExt;
use tokio::net::{TcpListener, TcpStream, UnixStream};

use crate::state::DesktopTransport;

/// Fixed loopback port the proxy listens on.
///
/// Picked to be distinctive (not the 42617 historical default, to avoid
/// colliding with a gateway that someone explicitly bound to TCP on the old
/// port) and stable so `frontendDist` can reference it at config time.
pub const PROXY_PORT: u16 = 42619;

/// Bind the proxy listener. Returns the listener so the caller can spawn the
/// accept loop on Tauri's async runtime.
pub async fn bind() -> std::io::Result<TcpListener> {
    TcpListener::bind(("127.0.0.1", PROXY_PORT)).await
}

/// Accept loop. Each connection is relayed to the gateway on the configured
/// transport (UDS or TCP).
pub async fn run(listener: TcpListener, transport: DesktopTransport) {
    loop {
        let (tcp, _peer) = match listener.accept().await {
            Ok(pair) => pair,
            Err(e) => {
                tracing::warn!("proxy accept error: {e}");
                continue;
            }
        };
        let transport = transport.clone();
        tauri::async_runtime::spawn(async move {
            if let Err(e) = handle_one(tcp, transport).await {
                tracing::debug!("proxy connection ended: {e}");
            }
        });
    }
}

async fn handle_one(client: TcpStream, transport: DesktopTransport) -> std::io::Result<()> {
    let _ = client.set_nodelay(true);
    match transport {
        DesktopTransport::Uds { socket_path } => {
            let upstream = UnixStream::connect(&socket_path).await.map_err(|e| {
                std::io::Error::new(
                    e.kind(),
                    format!("uds connect to {}: {e}", socket_path.display()),
                )
            })?;
            relay(client, upstream).await
        }
        DesktopTransport::Tcp { base_url } => {
            let host_port = parse_host_port(&base_url);
            let upstream = TcpStream::connect(&host_port).await.map_err(|e| {
                std::io::Error::new(e.kind(), format!("tcp connect to {host_port}: {e}"))
            })?;
            let _ = upstream.set_nodelay(true);
            relay(client, upstream).await
        }
    }
}

/// Bidirectional byte pump using `tokio::io::copy_bidirectional`.
async fn relay<U>(client: TcpStream, upstream: U) -> std::io::Result<()>
where
    U: tokio::io::AsyncRead + tokio::io::AsyncWrite + Unpin,
{
    let (mut client, mut upstream) = (client, upstream);
    let result = tokio::io::copy_bidirectional(&mut client, &mut upstream).await;
    // Best-effort half-close so both sides see EOF.
    let _ = client.shutdown().await;
    let _ = upstream.shutdown().await;
    result.map(|_| ())
}

fn parse_host_port(base_url: &str) -> String {
    base_url
        .trim_start_matches("https://")
        .trim_start_matches("http://")
        .trim_end_matches('/')
        .to_string()
}

/// Helper used at startup to surface a meaningful error if the port is taken.
pub fn explain_bind_error(e: &std::io::Error, socket_path_hint: Option<&PathBuf>) -> String {
    let port = PROXY_PORT;
    let mut msg = format!(
        "failed to bind Tauri↔gateway proxy on 127.0.0.1:{port}: {e}. \
         This is usually a port conflict — check `lsof -nP -iTCP:{port}` and \
         stop the conflicting process."
    );
    if let Some(p) = socket_path_hint {
        msg.push_str(&format!(
            " The gateway socket is expected at {}.",
            p.display()
        ));
    }
    msg
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn host_port_strips_scheme_and_slash() {
        assert_eq!(parse_host_port("http://127.0.0.1:42617"), "127.0.0.1:42617");
        assert_eq!(
            parse_host_port("https://example.com:8443/"),
            "example.com:8443"
        );
        assert_eq!(parse_host_port("127.0.0.1:9000"), "127.0.0.1:9000");
    }
}
