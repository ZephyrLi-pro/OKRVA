//! Shared application state for Tauri.

use std::path::PathBuf;
use std::sync::Arc;
use tokio::sync::RwLock;

/// Agent status as reported by the gateway.
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
#[serde(rename_all = "snake_case")]
pub enum AgentStatus {
    Idle,
    Working,
    Error,
}

/// Transport mode for connecting to the gateway.
#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "lowercase", tag = "transport", content = "endpoint")]
pub enum DesktopTransport {
    Uds { socket_path: PathBuf },
    Tcp { base_url: String },
}

impl DesktopTransport {
    /// Human-readable endpoint label for logging and UI.
    pub fn display_endpoint(&self) -> String {
        match self {
            Self::Uds { socket_path } => format!("unix://{}", socket_path.display()),
            Self::Tcp { base_url } => base_url.clone(),
        }
    }
}

/// Shared application state behind an `Arc<RwLock<_>>`.
#[derive(Debug, Clone)]
pub struct AppState {
    pub transport: DesktopTransport,
    pub token: Option<String>,
    pub connected: bool,
    pub agent_status: AgentStatus,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            transport: default_transport(),
            token: None,
            connected: false,
            agent_status: AgentStatus::Idle,
        }
    }
}

/// Return the platform-appropriate default transport.
/// Unix platforms (Linux, macOS) default to UDS; others default to TCP.
fn default_transport() -> DesktopTransport {
    #[cfg(unix)]
    {
        DesktopTransport::Uds {
            socket_path: crate::gateway_client::default_socket_path(),
        }
    }
    #[cfg(not(unix))]
    {
        DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:42617".to_string(),
        }
    }
}

/// Thread-safe wrapper around `AppState`.
pub type SharedState = Arc<RwLock<AppState>>;

/// Create the default shared state.
pub fn shared_state() -> SharedState {
    Arc::new(RwLock::new(AppState::default()))
}

const TRANSPORT_STORE_KEY: &str = "transport";

/// Load a persisted transport preference from the store.
///
/// Returns `None` if the store has no transport entry or the value is malformed.
pub fn load_persisted_transport<R: tauri::Runtime>(
    app: &tauri::AppHandle<R>,
) -> Option<DesktopTransport> {
    use tauri_plugin_store::StoreExt;
    let store = app.store("transport.json").ok()?;
    let value = store.get(TRANSPORT_STORE_KEY)?;
    serde_json::from_value::<DesktopTransport>(value.clone()).ok()
}

/// Save the current transport preference to the store so it persists across restarts.
pub fn save_persisted_transport<R: tauri::Runtime>(
    app: &tauri::AppHandle<R>,
    transport: &DesktopTransport,
) {
    use tauri_plugin_store::StoreExt;
    if let Ok(store) = app.store("transport.json") {
        let value = serde_json::to_value(transport).unwrap();
        store.set(TRANSPORT_STORE_KEY.to_string(), value);
        let _ = store.save();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_state() {
        let state = AppState::default();
        match &state.transport {
            DesktopTransport::Uds { socket_path } => {
                #[cfg(unix)]
                assert!(!socket_path.as_os_str().is_empty());
            }
            DesktopTransport::Tcp { base_url } => {
                assert_eq!(base_url, "http://127.0.0.1:42617");
            }
        }
        assert!(state.token.is_none());
        assert!(!state.connected);
        assert_eq!(state.agent_status, AgentStatus::Idle);
    }

    #[test]
    fn shared_state_is_cloneable() {
        let s1 = shared_state();
        let s2 = s1.clone();
        assert!(Arc::ptr_eq(&s1, &s2));
    }

    #[tokio::test]
    async fn shared_state_concurrent_read_write() {
        let state = shared_state();

        {
            let mut s = state.write().await;
            s.connected = true;
            s.agent_status = AgentStatus::Working;
            s.token = Some("zc_test".to_string());
        }

        let state2 = state.clone();
        let s = state2.read().await;
        assert!(s.connected);
        assert_eq!(s.agent_status, AgentStatus::Working);
        assert_eq!(s.token.as_deref(), Some("zc_test"));
    }

    #[test]
    fn agent_status_serialization() {
        assert_eq!(
            serde_json::to_string(&AgentStatus::Idle).unwrap(),
            "\"idle\""
        );
        assert_eq!(
            serde_json::to_string(&AgentStatus::Working).unwrap(),
            "\"working\""
        );
        assert_eq!(
            serde_json::to_string(&AgentStatus::Error).unwrap(),
            "\"error\""
        );
    }

    #[test]
    fn transport_display_endpoint() {
        let uds = DesktopTransport::Uds {
            socket_path: PathBuf::from("/run/user/1000/kylinbot/gateway.sock"),
        };
        assert_eq!(
            uds.display_endpoint(),
            "unix:///run/user/1000/kylinbot/gateway.sock"
        );

        let tcp = DesktopTransport::Tcp {
            base_url: "http://127.0.0.1:42617".to_string(),
        };
        assert_eq!(tcp.display_endpoint(), "http://127.0.0.1:42617");
    }

    #[test]
    fn transport_serde_roundtrip() {
        let uds = DesktopTransport::Uds {
            socket_path: PathBuf::from("/tmp/test.sock"),
        };
        let json = serde_json::to_string(&uds).unwrap();
        let back: DesktopTransport = serde_json::from_str(&json).unwrap();
        assert_eq!(uds, back);

        let tcp = DesktopTransport::Tcp {
            base_url: "https://example.com".to_string(),
        };
        let json = serde_json::to_string(&tcp).unwrap();
        let back: DesktopTransport = serde_json::from_str(&json).unwrap();
        assert_eq!(tcp, back);
    }

    #[test]
    fn transport_persist_format_tcp() {
        let json = r#"{"transport":"tcp","endpoint":{"base_url":"https://example.com"}}"#;
        let transport: DesktopTransport = serde_json::from_str(json).unwrap();
        assert_eq!(
            transport,
            DesktopTransport::Tcp {
                base_url: "https://example.com".to_string()
            }
        );
    }

    #[test]
    fn transport_persist_format_uds() {
        let json = r#"{"transport":"uds","endpoint":{"socket_path":"/tmp/test.sock"}}"#;
        let transport: DesktopTransport = serde_json::from_str(json).unwrap();
        assert_eq!(
            transport,
            DesktopTransport::Uds {
                socket_path: PathBuf::from("/tmp/test.sock")
            }
        );
    }

    #[test]
    fn transport_persist_malformed_returns_none() {
        let bad_json = r#"{"transport":"invalid","endpoint":"x"}"#;
        let result: Result<DesktopTransport, _> = serde_json::from_str(bad_json);
        assert!(result.is_err());
    }

    #[test]
    fn transport_persist_empty_returns_none() {
        let result: Result<DesktopTransport, _> = serde_json::from_str("{}");
        assert!(result.is_err());
    }
}
