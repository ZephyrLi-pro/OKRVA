mod support;

use std::time::Duration;

use futures_util::{SinkExt, StreamExt};
use support::{shutdown_gateway, uds_test_config, wait_for_socket};
use tempfile::TempDir;
use tokio::net::UnixStream;
use tokio_tungstenite::tungstenite::Message;
use kylinbot_gateway::{GatewayBindOptions, run_gateway};

#[tokio::test]
async fn uds_websocket_connects_and_round_trips_frames() {
    let temp = TempDir::new().expect("temp dir");
    let config = uds_test_config(temp.path());
    let socket_path = config.gateway.socket_path.clone().expect("socket path");

    let bind = GatewayBindOptions::from_config(&config);
    let gateway = tokio::spawn(async move { run_gateway(bind, config, None).await });

    wait_for_socket(&socket_path).await;

    let stream = UnixStream::connect(&socket_path)
        .await
        .expect("connect uds gateway");
    let (mut ws, _response) = tokio_tungstenite::client_async("ws://localhost/ws/chat", stream)
        .await
        .expect("websocket handshake over uds");

    let first = tokio::time::timeout(Duration::from_secs(5), ws.next())
        .await
        .expect("session_start timeout")
        .expect("session_start frame")
        .expect("session_start result");
    let first_json: serde_json::Value =
        serde_json::from_str(first.to_text().expect("session_start must be a text frame"))
            .expect("session_start json");
    assert_eq!(first_json["type"], "session_start");

    ws.send(Message::Text(
        serde_json::json!({
            "type": "connect",
            "session_id": "uds-ws-test",
            "device_name": "uds-test-client",
            "capabilities": ["test"]
        })
        .to_string()
        .into(),
    ))
    .await
    .expect("send connect frame");

    let ack = tokio::time::timeout(Duration::from_secs(5), ws.next())
        .await
        .expect("connect ack timeout")
        .expect("connect ack frame")
        .expect("connect ack result");
    let ack_json: serde_json::Value =
        serde_json::from_str(ack.to_text().expect("connect ack must be text"))
            .expect("connect ack json");
    assert_eq!(ack_json["type"], "connected");

    ws.close(None).await.expect("close websocket");
    drop(ws);

    shutdown_gateway(&socket_path).await;
    tokio::time::timeout(Duration::from_secs(5), gateway)
        .await
        .expect("gateway shutdown timeout")
        .expect("gateway task join")
        .expect("gateway result");
}
