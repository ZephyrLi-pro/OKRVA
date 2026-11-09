//! Exercise real subprocess streams, including a cancelled request's late reply.
use super::*;
use crate::mcp_client::McpServer;
use serde_json::json;

const SERVER: &str = r#"
import json, sys, time
for line in sys.stdin:
    request = json.loads(line)
    if 'id' not in request:
        continue
    method = request['method']
    if method == 'initialize':
        result = {'protocolVersion': '2024-11-05', 'capabilities': {}, 'serverInfo': {'name': 'stdio-test', 'version': '1'}}
    elif method == 'tools/list':
        result = {'tools': [{'name': 'wait', 'inputSchema': {'type': 'object'}}]}
    else:
        args = request.get('params', {}).get('arguments', {})
        time.sleep(args.get('delay', 0))
        if args.get('notification'):
            print(json.dumps({'jsonrpc': '2.0', 'method': 'notifications/message', 'params': {'level': 'info'}}), flush=True)
        if args.get('unknown_id'):
            print(json.dumps({'jsonrpc': '2.0', 'id': 99999, 'result': {'marker': 'unrelated'}}), flush=True)
            continue
        if args.get('missing_id'):
            print(json.dumps({'jsonrpc': '2.0', 'result': None if args.get('null_result') else {'marker': 'unrelated'}}), flush=True)
            continue
        result = {'marker': args.get('marker')}
    print(json.dumps({'jsonrpc': '2.0', 'id': request['id'], 'result': result}), flush=True)
"#;

fn config() -> McpServerConfig {
    McpServerConfig {
        name: "stdio-test".into(),
        transport: McpTransport::Stdio,
        command: "/usr/bin/python3".into(),
        args: vec!["-u".into(), "-c".into(), SERVER.into()],
        ..Default::default()
    }
}

#[tokio::test]
async fn tool_call_can_receive_a_real_response_after_thirty_seconds() {
    // Deliberately exceeds the old fixed 30s transport cutoff. The public
    // client still owns the operation's bound; the transport adds no shorter one.
    let mut config = config();
    config.tool_timeout_secs = Some(40);
    let server = McpServer::connect(config).await.expect("connect test MCP");
    let result = timeout(
        Duration::from_secs(45),
        server.call_tool("wait", json!({"delay": 31, "marker": "completed"})),
    )
    .await
    .expect("test watchdog")
    .expect("configured tool timeout must allow more than thirty seconds");
    assert_eq!(result["marker"], "completed");
}

#[tokio::test]
async fn configured_timeout_does_not_assign_the_late_reply_to_the_next_call() {
    let mut config = config();
    config.tool_timeout_secs = Some(1);
    let server = McpServer::connect(config).await.expect("connect test MCP");
    let error = server
        .call_tool("wait", json!({"delay": 1.4, "marker": "expired"}))
        .await
        .expect_err("per-server one-second timeout must still apply");
    assert!(error.to_string().contains("timed out after 1s"));
    // The child writes the old response before it can read this second call.
    let result = server
        .call_tool("wait", json!({"marker": "current"}))
        .await
        .expect("known late ID should be discarded, not delivered or fatal");
    assert_eq!(result["marker"], "current");
}

#[tokio::test]
async fn unknown_response_id_is_not_treated_as_a_known_late_reply() {
    let mut transport = StdioTransport::new(&config()).expect("spawn test MCP");
    let request = JsonRpcRequest::new(
        10,
        "tools/call",
        json!({"name": "wait", "arguments": {"unknown_id": true}}),
    );
    let error = timeout(Duration::from_secs(3), transport.send_and_recv(&request))
        .await
        .expect("unknown ID must fail promptly")
        .expect_err("unknown ID must never be assigned to current request");
    assert!(error.to_string().contains("unrecognized response id"));
}

#[tokio::test]
async fn response_without_id_is_not_accepted_as_a_notification() {
    for null_result in [false, true] {
        let mut transport = StdioTransport::new(&config()).expect("spawn test MCP");
        let request = JsonRpcRequest::new(
            10,
            "tools/call",
            json!({"name": "wait", "arguments": {"missing_id": true, "null_result": null_result}}),
        );
        let error = timeout(Duration::from_secs(3), transport.send_and_recv(&request))
            .await
            .expect("invalid response must fail promptly")
            .expect_err("result lacking ID is not a notification");
        assert!(error.to_string().contains("missing its request id"));
    }
}

#[tokio::test]
async fn server_notifications_are_skipped_before_the_matching_response() {
    let mut transport = StdioTransport::new(&config()).expect("spawn test MCP");
    let request = JsonRpcRequest::new(
        10,
        "tools/call",
        json!({"name": "wait", "arguments": {"notification": true, "marker": "current"}}),
    );
    let response = timeout(Duration::from_secs(3), transport.send_and_recv(&request))
        .await
        .expect("test watchdog")
        .expect("notification must not replace a matching response");
    assert_eq!(response.id, request.id);
    assert_eq!(response.result.unwrap()["marker"], "current");
}
