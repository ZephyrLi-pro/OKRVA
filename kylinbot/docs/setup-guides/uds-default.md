# Default Gateway UDS Transport

KylinBot defaults the gateway to a Unix domain socket on Linux. Local clients use
filesystem permissions instead of opening `127.0.0.1:42617`.

## Find the Socket

If `gateway.socket_path` is set in `config.toml`, that value wins. Otherwise the
runtime resolves the path in this order:

1. `$XDG_RUNTIME_DIR/kylinbot/gateway.sock`
2. `${workspace_dir}/run/gateway.sock`

The gateway prints the resolved address at startup:

```text
KylinBot Gateway listening on unix:///run/user/1000/kylinbot/gateway.sock
```

## Health Check

Use `curl --unix-socket` with any placeholder HTTP host:

```bash
curl --unix-socket "$XDG_RUNTIME_DIR/kylinbot/gateway.sock" http://x/health
```

If `XDG_RUNTIME_DIR` is not set, use the workspace fallback:

```bash
curl --unix-socket "$HOME/.kylinbot/run/gateway.sock" http://x/health
```

## Override the Socket Path

```toml
[gateway]
transport = "uds"
socket_path = "/run/user/1000/kylinbot/gateway.sock"
```

Or for one process:

```bash
KYLINBOT_GATEWAY_SOCKET=/tmp/kylinbot-gateway.sock kylinbot daemon
```

## Switch Back to TCP

Remote access, tunnels, Docker, docker-compose, Kubernetes, and non-Linux
packages must use TCP explicitly.

```toml
[gateway]
transport = "tcp"
host = "127.0.0.1"
port = 42617
```

Or for one process:

```bash
KYLINBOT_GATEWAY_TRANSPORT=tcp kylinbot daemon
```

When `transport = "uds"`, TLS/mTLS and tunnels are rejected at config validation.
Set `transport = "tcp"` before enabling `gateway.tls` or `tunnel.provider`.

## Tauri Desktop Client

The ZeroClaw desktop app (`zeroclaw-desktop`) connects to the gateway via UDS on
both Linux and macOS. It registers a custom URI protocol (`zeroclaw://gateway/`) so
the embedded WebView can reach the gateway without opening a TCP port.

### Default Socket Path

| Platform | Default |
|---|---|
| Linux | `$XDG_RUNTIME_DIR/zeroclaw/gateway.sock` → `${workspace_dir}/run/gateway.sock` |
| macOS | `$XDG_RUNTIME_DIR/zeroclaw/gateway.sock` → `~/Library/Application Support/zeroclaw/gateway.sock` |

### Switch to TCP (Remote Gateway)

Open the settings panel in the desktop app and change the transport to TCP, or
set `gateway.transport = "tcp"` in the gateway config and restart. The desktop
client will use reqwest to connect to the remote gateway over HTTP.
