//! Transport layer for connecting to the KylinBot gateway.
//!
//! Provides a `GatewayHttp` trait with two implementations:
//! - `UdsClient`: connects via Unix domain socket (default on Linux & macOS)
//! - `TcpClient`: connects via HTTP/TCP (explicit fallback for remote gateways)

use anyhow::{Context, Result};
use bytes::Bytes;
use std::path::PathBuf;

use crate::state::DesktopTransport;

// ---------------------------------------------------------------------------
// Default socket path resolution
// ---------------------------------------------------------------------------

/// Resolve the default Unix domain socket path for the gateway.
///
/// Linux: `$XDG_RUNTIME_DIR/kylinbot/gateway.sock` → `${workspace_dir}/run/gateway.sock`
/// macOS: `$XDG_RUNTIME_DIR/kylinbot/gateway.sock` → `~/Library/Application Support/kylinbot/gateway.sock`
#[cfg(unix)]
pub fn default_socket_path() -> PathBuf {
    if let Ok(xdg) = std::env::var("XDG_RUNTIME_DIR")
        && !xdg.is_empty()
    {
        return PathBuf::from(xdg).join("kylinbot").join("gateway.sock");
    }

    #[cfg(target_os = "macos")]
    {
        if let Some(data_dir) = dirs::data_dir() {
            return data_dir.join("kylinbot").join("gateway.sock");
        }
        // Last resort for macOS
        PathBuf::from("/tmp/kylinbot/gateway.sock")
    }

    #[cfg(not(target_os = "macos"))]
    {
        let workspace_dir = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("/tmp"));
        workspace_dir.join("run").join("gateway.sock")
    }
}

// ---------------------------------------------------------------------------
// GatewayHttp trait
// ---------------------------------------------------------------------------

/// Abstract HTTP interface to the gateway — agnostic of transport.
#[async_trait::async_trait]
pub trait GatewayHttp: Send + Sync {
    async fn request(&self, req: http::Request<Bytes>) -> Result<http::Response<Bytes>>;
}

// ---------------------------------------------------------------------------
// UdsClient (Unix domain socket)
// ---------------------------------------------------------------------------

#[cfg(unix)]
pub struct UdsClient {
    socket_path: PathBuf,
}

#[cfg(unix)]
impl UdsClient {
    pub fn new(socket_path: PathBuf) -> Self {
        Self { socket_path }
    }
}

#[cfg(unix)]
#[async_trait::async_trait]
impl GatewayHttp for UdsClient {
    async fn request(&self, req: http::Request<Bytes>) -> Result<http::Response<Bytes>> {
        use http_body_util::BodyExt;
        use hyper::client::conn::http1;
        use hyper_util::rt::TokioIo;
        use tokio::net::UnixStream;

        let stream = UnixStream::connect(&self.socket_path)
            .await
            .with_context(|| format!("UDS connect to {}", self.socket_path.display()))?;

        let io = TokioIo::new(stream);
        let (mut sender, conn) = http1::handshake(io)
            .await
            .context("UDS HTTP handshake failed")?;

        // Spawn the connection task; errors surface as broken pipe on send.
        tokio::spawn(async move {
            if let Err(e) = conn.await {
                tracing::debug!("UDS connection task error: {e}");
            }
        });

        // Convert http::Request<Bytes> → http::Request<hyper::body::Incoming>
        // (hyper 1.x uses Incoming as the response body type; for requests we use
        //  Full<Bytes> which implements Body).
        let (parts, body) = req.into_parts();
        let full_body = http_body_util::Full::new(body);
        let hyper_req = http::Request::from_parts(parts, full_body);

        let resp = sender
            .send_request(hyper_req)
            .await
            .context("UDS send_request failed")?;

        let (parts, body) = resp.into_parts();
        let body_bytes = body
            .collect()
            .await
            .context("UDS read body failed")?
            .to_bytes();

        Ok(http::Response::from_parts(parts, body_bytes))
    }
}

// ---------------------------------------------------------------------------
// TcpClient (reqwest-based)
// ---------------------------------------------------------------------------

pub struct TcpClient {
    client: reqwest::Client,
    base_url: String,
}

impl TcpClient {
    pub fn new(base_url: &str) -> Self {
        let client = reqwest::Client::builder()
            .timeout(std::time::Duration::from_secs(10))
            .build()
            .unwrap_or_default();
        Self {
            client,
            base_url: base_url.to_string(),
        }
    }
}

#[async_trait::async_trait]
impl GatewayHttp for TcpClient {
    async fn request(&self, req: http::Request<Bytes>) -> Result<http::Response<Bytes>> {
        let method = req.method().clone();
        let uri = req.uri().clone();
        let path_and_query = uri.path_and_query().map(|pq| pq.as_str()).unwrap_or("/");

        let url = format!("{}{path_and_query}", self.base_url);

        let mut builder = self.client.request(method, &url);

        // Copy headers (skip Host — reqwest sets it from the URL).
        for (name, value) in req.headers().iter() {
            if name != http::header::HOST {
                builder = builder.header(name, value);
            }
        }

        let body = req.into_body();
        if !body.is_empty() {
            builder = builder.body(body);
        }

        let resp = builder.send().await.context("TCP request failed")?;

        let status = http::StatusCode::from_u16(resp.status().as_u16())?;
        let mut builder = http::Response::builder().status(status);

        for (name, value) in resp.headers().iter() {
            if let Ok(name) = http::HeaderName::from_bytes(name.as_str().as_bytes()) {
                builder = builder.header(name, value);
            }
        }

        let body_bytes = resp.bytes().await.context("TCP read body failed")?;

        Ok(builder.body(body_bytes)?)
    }
}

// ---------------------------------------------------------------------------
// Client factory
// ---------------------------------------------------------------------------

/// Build a `GatewayHttp` client from the given transport config.
pub fn build_client(transport: &DesktopTransport) -> Box<dyn GatewayHttp> {
    match transport {
        #[cfg(unix)]
        DesktopTransport::Uds { socket_path } => Box::new(UdsClient::new(socket_path.clone())),
        #[cfg(not(unix))]
        DesktopTransport::Uds { .. } => {
            // UDS not available on non-Unix; fall back to TCP.
            // This arm only compiles on non-Unix where UDS shouldn't be the default.
            Box::new(TcpClient::new("http://127.0.0.1:42617"))
        }
        DesktopTransport::Tcp { base_url } => Box::new(TcpClient::new(base_url)),
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    // SAFETY: These tests run single-threaded and no other code in this test
    // module reads XDG_RUNTIME_DIR concurrently.
    #[cfg(unix)]
    #[test]
    fn default_socket_path_xdg_set() {
        unsafe { std::env::set_var("XDG_RUNTIME_DIR", "/run/user/1000") };
        let path = default_socket_path();
        assert_eq!(path, PathBuf::from("/run/user/1000/kylinbot/gateway.sock"));
        unsafe { std::env::remove_var("XDG_RUNTIME_DIR") };
    }

    #[cfg(unix)]
    #[test]
    fn default_socket_path_xdg_empty() {
        unsafe { std::env::set_var("XDG_RUNTIME_DIR", "") };
        let path = default_socket_path();
        #[cfg(target_os = "macos")]
        assert!(path.to_string_lossy().contains("Application Support"));
        #[cfg(not(target_os = "macos"))]
        assert!(path.to_string_lossy().contains("run/gateway.sock"));
        unsafe { std::env::remove_var("XDG_RUNTIME_DIR") };
    }

    #[cfg(unix)]
    #[test]
    fn default_socket_path_xdg_unset() {
        unsafe { std::env::remove_var("XDG_RUNTIME_DIR") };
        let path = default_socket_path();
        #[cfg(target_os = "macos")]
        assert!(path.to_string_lossy().contains("Application Support"));
        #[cfg(not(target_os = "macos"))]
        assert!(path.to_string_lossy().contains("run/gateway.sock"));
    }

    #[test]
    fn tcp_client_build() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:42617".to_string(),
        };
        let _client = build_client(&transport);
    }

    #[cfg(unix)]
    #[test]
    fn uds_client_build() {
        let transport = DesktopTransport::Uds {
            socket_path: PathBuf::from("/tmp/test.sock"),
        };
        let _client = build_client(&transport);
    }

    #[tokio::test]
    async fn tcp_client_unreachable() {
        let client = TcpClient::new("http://127.0.0.1:1");
        let req = http::Request::builder()
            .uri("/health")
            .body(Bytes::new())
            .unwrap();
        let result = client.request(req).await;
        assert!(result.is_err());
    }

    #[cfg(unix)]
    #[tokio::test]
    async fn uds_client_connect_failure() {
        let client = UdsClient::new(PathBuf::from("/tmp/nonexistent_uds_test.sock"));
        let req = http::Request::builder()
            .uri("/health")
            .body(Bytes::new())
            .unwrap();
        let result = client.request(req).await;
        assert!(result.is_err());
    }
}
