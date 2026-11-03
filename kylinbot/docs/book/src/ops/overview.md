# Operations — Overview

How to run KylinBot in production. The surface is intentionally small: one binary, one config file, one SQLite workspace. Most "operations" is "systemd and journald".

This section covers:

- [Service & daemon](./service.md) — keeping the process alive
- [Logs & observability](./observability.md) — reading what the agent did
- [Troubleshooting](./troubleshooting.md) — when things break
- [Network deployment](./network-deployment.md) — exposing the gateway, tunnels, reverse proxies

## The shape of a deployment

A typical always-on KylinBot install is:

```
kylinbot service (systemd / launchctl / Windows Service)
  ├── kylinbot daemon                 — the long-running process
  │   ├── gateway listener (UDS by default, TCP when configured) — REST / WebSocket / webhooks
  │   ├── channel pollers             — Telegram, IMAP, Nostr relays, etc.
  │   ├── channel listeners           — Discord / Slack / Matrix / WebSocket
  │   ├── cron scheduler              — scheduled SOPs and jobs
  │   └── agent loop (per session)    — provider call + tool execution
  ├── SQLite workspace                — ~/.kylinbot/workspace/
  ├── config.toml                     — ~/.kylinbot/config.toml
  ├── tool-receipts log               — ~/.kylinbot/workspace/receipts/
  └── platform logs                   — journald
```

Everything except the binary can move — the workspace path is configurable, config paths resolve per environment (bootstrap vs. XDG), and log destinations are platform-native by default.

## What to monitor

Four signals matter:

### 1. Service liveness

Is the process running?

```bash
systemctl --user is-active kylinbot
```

If it's dying repeatedly, check [Troubleshooting → Daemon keeps restarting](./troubleshooting.md).

### 2. Channel health

Are channels connected? The gateway exposes `/health/channels`. On the default
Linux desktop transport, query it through the Unix socket:

```bash
curl --unix-socket "$XDG_RUNTIME_DIR/kylinbot/gateway.sock" http://x/health/channels | jq
```

If `gateway.transport = "tcp"`, use the configured TCP address, for example
`http://localhost:42617/health/channels`.

```json
{
  "telegram": {"status": "connected", "last_event_ago_secs": 12},
  "discord":  {"status": "connected", "last_event_ago_secs": 4},
  "email":    {"status": "polling",   "next_poll_in_secs": 42},
  "matrix":   {"status": "disconnected", "error": "401 Unauthorized"}
}
```

Monitor `status != "connected"` on push-based channels.

### 3. Provider reliability

Are LLM calls succeeding? `/health/providers`:

```bash
curl --unix-socket "$XDG_RUNTIME_DIR/kylinbot/gateway.sock" http://x/health/providers | jq
```

```json
{
  "claude": {"ok": true,  "last_latency_ms": 1240, "error_rate_1h": 0.0},
  "local":  {"ok": true,  "last_latency_ms": 3890, "error_rate_1h": 0.0}
}
```

For fallback chains, the meta-provider reports its current working child.

### 4. Tool-call volume and blocks

`/metrics/tools` (Prometheus format):

```
kylinbot_tool_calls_total{tool="shell",outcome="success"} 342
kylinbot_tool_calls_total{tool="shell",outcome="blocked"} 4
kylinbot_tool_calls_total{tool="shell",outcome="denied"} 2
kylinbot_tool_calls_total{tool="file_write",outcome="success"} 89
```

Blocks and denials are worth looking at — if the agent is repeatedly hitting the same policy block, either your policy is wrong or your agent is misbehaving.

## Capacity

A single KylinBot instance can handle:

- Multiple concurrent conversations across all channels
- Tool calls at whatever rate the provider and sandbox allow
- Long-running agent loops (tool chains of 20+ calls)

Scale laterally by running one instance per workspace. Don't try to run two daemons on the same workspace — SQLite's single-writer model will produce lock contention and ultimately corruption.

For multi-tenant hosting, see the proposal in #2765 (closed, historical — the architecture for in-process multi-workspace routing).

## Backups

What to back up:

- `~/.kylinbot/config.toml` — contains channel credentials (encrypted if using secrets store)
- `~/.kylinbot/workspace/*.db` — SQLite conversation memory
- `~/.kylinbot/secrets.key` — master key for the encrypted secrets store (if used). **Without it, the config's secrets are unrecoverable.**
- `~/.kylinbot/workspace/receipts/` — tool-receipts log

A plain `tar czf kylinbot-$(date +%F).tar.gz ~/.kylinbot` covers everything. Restic, borg, or Duplicacy work fine for incremental backups.

**Do not back up `~/.kylinbot/workspace/cache/`** — it's regenerable and can be large.

## Updates

The service does not auto-update. Subscribe to the release feed (GitHub releases or the Discord `#releases` channel — see [Contributing → Communication](../contributing/communication.md)). Typical update cadence:

1. Read the release notes
2. Back up `~/.kylinbot/`
3. Update the binary (bootstrap re-run, or `cargo install --force`)
4. `kylinbot service restart`
5. Verify `/health/*` endpoints return green

If the new version requires config migrations, the startup log emits a warning and the binary usually auto-migrates. Check `kylinbot config list` to spot-check values after upgrade, and `kylinbot config migrate` to apply any pending schema migrations manually.

## See also

- [Setup → Service management](../setup/service.md) — install/remove/logs per platform
- [Logs & observability](./observability.md)
- [Troubleshooting](./troubleshooting.md)
- [Network deployment](./network-deployment.md)
