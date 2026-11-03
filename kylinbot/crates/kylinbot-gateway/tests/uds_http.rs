mod support;

use std::os::unix::fs::PermissionsExt;
use std::time::Duration;

use support::{shutdown_gateway, uds_request, uds_test_config, wait_for_socket};
use tempfile::TempDir;
use kylinbot_gateway::{GatewayBindOptions, run_gateway};

#[tokio::test]
async fn uds_health_matches_tcp_shape_and_socket_lifecycle() {
    let temp = TempDir::new().expect("temp dir");
    let config = uds_test_config(temp.path());
    let socket_path = config.gateway.socket_path.clone().expect("socket path");
    let parent = socket_path.parent().expect("socket parent");
    std::fs::create_dir_all(parent).expect("socket parent dir");
    std::fs::write(&socket_path, b"stale").expect("stale socket placeholder");

    let bind = GatewayBindOptions::from_config(&config);
    let gateway = tokio::spawn(async move { run_gateway(bind, config, None).await });

    wait_for_socket(&socket_path).await;

    let socket_mode = std::fs::metadata(&socket_path)
        .expect("socket metadata")
        .permissions()
        .mode()
        & 0o777;
    assert_eq!(socket_mode, 0o600);

    let response = uds_request(
        &socket_path,
        "GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
    )
    .await;
    assert!(response.starts_with("HTTP/1.1 200 OK"), "{response}");
    let body = response.split("\r\n\r\n").nth(1).expect("response body");
    let json: serde_json::Value = serde_json::from_str(body).expect("health json");
    assert_eq!(json["status"], "ok");
    assert_eq!(json["require_pairing"], false);

    shutdown_gateway(&socket_path).await;

    tokio::time::timeout(Duration::from_secs(5), gateway)
        .await
        .expect("gateway shutdown timeout")
        .expect("gateway task join")
        .expect("gateway result");

    assert!(
        !socket_path.exists(),
        "socket should be removed on graceful shutdown"
    );
}
