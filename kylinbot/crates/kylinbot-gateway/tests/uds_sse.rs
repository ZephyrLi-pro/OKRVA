mod support;

use std::time::Duration;

use support::{shutdown_gateway, uds_test_config, wait_for_socket};
use tempfile::TempDir;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::UnixStream;
use kylinbot_gateway::{GatewayBindOptions, run_gateway};

#[tokio::test]
async fn uds_sse_receives_event_and_disconnects_cleanly() {
    let temp = TempDir::new().expect("temp dir");
    let config = uds_test_config(temp.path());
    let socket_path = config.gateway.socket_path.clone().expect("socket path");
    let (event_tx, _rx) = tokio::sync::broadcast::channel::<serde_json::Value>(16);

    let bind = GatewayBindOptions::from_config(&config);
    let gateway_event_tx = event_tx.clone();
    let gateway =
        tokio::spawn(async move { run_gateway(bind, config, Some(gateway_event_tx)).await });

    wait_for_socket(&socket_path).await;

    let mut stream = UnixStream::connect(&socket_path)
        .await
        .expect("connect uds gateway");
    stream
        .write_all(
            b"GET /api/events HTTP/1.1\r\nHost: localhost\r\nAccept: text/event-stream\r\n\r\n",
        )
        .await
        .expect("write sse request");

    let mut buf = Vec::new();
    read_until_contains(&mut stream, &mut buf, "\r\n\r\n").await;
    let headers = String::from_utf8_lossy(&buf);
    assert!(headers.starts_with("HTTP/1.1 200 OK"), "{headers}");
    assert!(
        headers.contains("content-type: text/event-stream")
            || headers.contains("Content-Type: text/event-stream"),
        "{headers}"
    );

    let expected = serde_json::json!({
        "type": "uds_sse_test",
        "message": "event over unix socket"
    });
    event_tx.send(expected.clone()).expect("send sse event");

    read_until_contains(&mut stream, &mut buf, "\"uds_sse_test\"").await;
    let body = String::from_utf8_lossy(&buf);
    assert!(body.contains("data:"), "{body}");
    assert!(
        body.contains("\"message\":\"event over unix socket\""),
        "{body}"
    );

    drop(stream);
    shutdown_gateway(&socket_path).await;
    tokio::time::timeout(Duration::from_secs(5), gateway)
        .await
        .expect("gateway shutdown timeout")
        .expect("gateway task join")
        .expect("gateway result");
}

async fn read_until_contains(stream: &mut UnixStream, buf: &mut Vec<u8>, needle: &str) {
    let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
    let mut chunk = [0_u8; 1024];
    while tokio::time::Instant::now() < deadline {
        let n = tokio::time::timeout(Duration::from_millis(250), stream.read(&mut chunk))
            .await
            .ok()
            .and_then(Result::ok)
            .expect("read sse response");
        assert_ne!(n, 0, "sse stream closed before {needle}");
        buf.extend_from_slice(&chunk[..n]);
        if String::from_utf8_lossy(buf).contains(needle) {
            return;
        }
    }
    panic!("sse response did not contain {needle}");
}
