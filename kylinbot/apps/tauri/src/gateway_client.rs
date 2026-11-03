//! HTTP client for communicating with the KylinBot gateway.

use anyhow::{Context, Result};
use bytes::Bytes;

use crate::state::DesktopTransport;
use crate::transport::{GatewayHttp, build_client};

pub struct GatewayClient {
    http: Box<dyn GatewayHttp>,
    token: Option<String>,
}

impl GatewayClient {
    pub fn new(transport: &DesktopTransport, token: Option<&str>) -> Self {
        Self {
            http: build_client(transport),
            token: token.map(String::from),
        }
    }

    fn auth_header(&self) -> Option<String> {
        self.token.as_ref().map(|t| format!("Bearer {t}"))
    }

    fn build_request(&self, method: http::Method, path: &str) -> http::Request<Bytes> {
        let uri = path.parse().unwrap_or_else(|_| http::Uri::from_static("/"));
        let mut builder = http::Request::builder().method(method).uri(uri);
        if let Some(auth) = self.auth_header() {
            builder = builder.header(http::header::AUTHORIZATION, auth);
        }
        builder.body(Bytes::new()).unwrap()
    }

    fn build_request_with_body(
        &self,
        method: http::Method,
        path: &str,
        body: Bytes,
    ) -> http::Request<Bytes> {
        let uri = path.parse().unwrap_or_else(|_| http::Uri::from_static("/"));
        let mut builder = http::Request::builder().method(method).uri(uri);
        if let Some(auth) = self.auth_header() {
            builder = builder.header(http::header::AUTHORIZATION, auth);
        }
        builder.body(body).unwrap()
    }

    pub async fn get_status(&self) -> Result<serde_json::Value> {
        let req = self.build_request(http::Method::GET, "/api/status");
        let resp = self
            .http
            .request(req)
            .await
            .context("status request failed")?;
        Ok(serde_json::from_slice(resp.body())?)
    }

    pub async fn get_health(&self) -> Result<bool> {
        let req = self.build_request(http::Method::GET, "/health");
        match self.http.request(req).await {
            Ok(resp) => Ok(resp.status().is_success()),
            Err(_) => Ok(false),
        }
    }

    pub async fn get_devices(&self) -> Result<serde_json::Value> {
        let req = self.build_request(http::Method::GET, "/api/devices");
        let resp = self
            .http
            .request(req)
            .await
            .context("devices request failed")?;
        Ok(serde_json::from_slice(resp.body())?)
    }

    pub async fn initiate_pairing(&self) -> Result<serde_json::Value> {
        let req = self.build_request(http::Method::POST, "/api/pairing/initiate");
        let resp = self
            .http
            .request(req)
            .await
            .context("pairing request failed")?;
        Ok(serde_json::from_slice(resp.body())?)
    }

    /// Check whether the gateway requires pairing.
    pub async fn requires_pairing(&self) -> Result<bool> {
        let req = self.build_request(http::Method::GET, "/health");
        let resp = self
            .http
            .request(req)
            .await
            .context("health request failed")?;
        let body: serde_json::Value = serde_json::from_slice(resp.body())?;
        Ok(body["require_pairing"].as_bool().unwrap_or(false))
    }

    /// Request a new pairing code from the gateway (localhost-only admin endpoint).
    pub async fn request_new_paircode(&self) -> Result<String> {
        let req = self.build_request(http::Method::POST, "/admin/paircode/new");
        let resp = self
            .http
            .request(req)
            .await
            .context("paircode request failed")?;
        let body: serde_json::Value = serde_json::from_slice(resp.body())?;
        body["pairing_code"]
            .as_str()
            .map(String::from)
            .context("no pairing_code in response")
    }

    /// Exchange a pairing code for a bearer token.
    pub async fn pair_with_code(&self, code: &str) -> Result<String> {
        let uri = "/pair"
            .parse()
            .unwrap_or_else(|_| http::Uri::from_static("/"));
        let req = http::Request::builder()
            .method(http::Method::POST)
            .uri(uri)
            .header("X-Pairing-Code", code)
            .body(Bytes::new())
            .unwrap();
        let resp = self
            .http
            .request(req)
            .await
            .context("pair request failed")?;
        if !resp.status().is_success() {
            anyhow::bail!("pair request returned {}", resp.status());
        }
        let body: serde_json::Value = serde_json::from_slice(resp.body())?;
        body["token"]
            .as_str()
            .map(String::from)
            .context("no token in pair response")
    }

    /// Validate an existing token by calling a protected endpoint.
    pub async fn validate_token(&self) -> Result<bool> {
        let req = self.build_request(http::Method::GET, "/api/status");
        match self.http.request(req).await {
            Ok(resp) => Ok(resp.status().is_success()),
            Err(_) => Ok(false),
        }
    }

    /// Auto-pair with the gateway: request a new code and exchange it for a token.
    pub async fn auto_pair(&self) -> Result<String> {
        let code = self.request_new_paircode().await?;
        self.pair_with_code(&code).await
    }

    pub async fn send_webhook_message(&self, message: &str) -> Result<serde_json::Value> {
        let body = serde_json::json!({ "message": message });
        let body_bytes = serde_json::to_vec(&body)?.into();
        let req = self.build_request_with_body(http::Method::POST, "/webhook", body_bytes);
        let resp = self
            .http
            .request(req)
            .await
            .context("webhook request failed")?;
        Ok(serde_json::from_slice(resp.body())?)
    }
}

/// Re-export `default_socket_path` so `state.rs` can reference it.
#[cfg(unix)]
pub use crate::transport::default_socket_path;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn client_creation_no_token() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:42617".to_string(),
        };
        let client = GatewayClient::new(&transport, None);
        assert!(client.token.is_none());
        assert!(client.auth_header().is_none());
    }

    #[test]
    fn client_creation_with_token() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://localhost:8080".to_string(),
        };
        let client = GatewayClient::new(&transport, Some("test-token"));
        assert_eq!(client.token.as_deref(), Some("test-token"));
        assert_eq!(client.auth_header().unwrap(), "Bearer test-token");
    }

    #[test]
    fn auth_header_format() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://localhost".to_string(),
        };
        let client = GatewayClient::new(&transport, Some("zc_abc123"));
        assert_eq!(client.auth_header().unwrap(), "Bearer zc_abc123");
    }

    #[tokio::test]
    async fn health_returns_false_for_unreachable_host() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:1".to_string(),
        };
        let client = GatewayClient::new(&transport, None);
        let result = client.get_health().await.unwrap();
        assert!(!result, "health should be false for unreachable host");
    }

    #[tokio::test]
    async fn status_fails_for_unreachable_host() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:1".to_string(),
        };
        let client = GatewayClient::new(&transport, None);
        let result = client.get_status().await;
        assert!(result.is_err(), "status should fail for unreachable host");
    }

    #[tokio::test]
    async fn devices_fails_for_unreachable_host() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:1".to_string(),
        };
        let client = GatewayClient::new(&transport, None);
        let result = client.get_devices().await;
        assert!(result.is_err(), "devices should fail for unreachable host");
    }

    #[tokio::test]
    async fn pairing_fails_for_unreachable_host() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:1".to_string(),
        };
        let client = GatewayClient::new(&transport, None);
        let result = client.initiate_pairing().await;
        assert!(result.is_err(), "pairing should fail for unreachable host");
    }

    #[tokio::test]
    async fn webhook_fails_for_unreachable_host() {
        let transport = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:1".to_string(),
        };
        let client = GatewayClient::new(&transport, None);
        let result = client.send_webhook_message("hello").await;
        assert!(result.is_err(), "webhook should fail for unreachable host");
    }
}
