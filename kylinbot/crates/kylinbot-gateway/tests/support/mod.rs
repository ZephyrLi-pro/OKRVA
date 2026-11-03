use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::time::Duration;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::UnixStream;
use kylinbot_config::schema::{Config, ModelProviderConfig, Transport};

pub fn uds_test_config(temp_dir: &Path) -> Config {
    let workspace_dir = temp_dir.join("workspace");
    std::fs::create_dir_all(&workspace_dir).expect("workspace dir");

    let socket_path = temp_dir.join("run").join("gateway.sock");
    let mut config = Config {
        workspace_dir: workspace_dir.clone(),
        config_path: temp_dir.join("config.toml"),
        ..Config::default()
    };
    config.gateway.transport = Transport::Uds;
    config.gateway.socket_path = Some(socket_path);
    config.gateway.require_pairing = false;
    config.gateway.session_persistence = false;
    config.providers.fallback = Some("openrouter".into());
    config.providers.models.insert(
        "openrouter".into(),
        ModelProviderConfig {
            model: Some("test-model".into()),
            ..ModelProviderConfig::default()
        },
    );
    config
}

pub async fn wait_for_socket(socket_path: &Path) {
    let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
    while tokio::time::Instant::now() < deadline {
        if std::fs::metadata(socket_path)
            .map(|metadata| metadata.permissions().mode() & 0o777 == 0o600)
            .unwrap_or(false)
        {
            return;
        }
        tokio::time::sleep(Duration::from_millis(25)).await;
    }
    panic!(
        "gateway socket was not created with 0600 permissions at {}",
        socket_path.display()
    );
}

pub async fn uds_request(socket_path: &Path, request: &str) -> String {
    let mut stream = UnixStream::connect(socket_path)
        .await
        .expect("connect uds gateway");
    stream
        .write_all(request.as_bytes())
        .await
        .expect("write uds request");

    let mut response = String::new();
    tokio::time::timeout(Duration::from_secs(5), stream.read_to_string(&mut response))
        .await
        .expect("read uds response timeout")
        .expect("read uds response");
    response
}

pub async fn shutdown_gateway(socket_path: &Path) {
    let shutdown_response = uds_request(
        socket_path,
        "POST /admin/shutdown HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
    )
    .await;
    assert!(
        shutdown_response.starts_with("HTTP/1.1 200 OK"),
        "{shutdown_response}"
    );
}
