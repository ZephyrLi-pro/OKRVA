# Sandboxing

The runtime executes tool invocations inside an OS-level sandbox when one is available. The sandbox restricts filesystem access to the workspace, limits network reachability, and removes access to the parent process's secrets.

This is distinct from the autonomy system and command allow-lists. Those are *policy* layers that decide whether a tool may run; the sandbox is a *mechanism* layer that confines what a running tool can reach if it does run.

## Auto-detection

KylinBot picks the best available backend at startup:

| Platform | Preferred order |
|---|---|
| Linux | Landlock (kernel 5.13+) → Bubblewrap → Firejail → Docker → none |
| Any | Docker (if daemon reachable) → none |

You can force a backend:

```toml
[security.sandbox]
backend = "bubblewrap"        # or "landlock", "firejail", "docker", "noop"
```

Set `backend = "noop"` to disable sandboxing entirely (part of [YOLO mode](../getting-started/yolo.md)).

## What the sandbox confines

### Filesystem

- **Read access** — restricted to the workspace, `/usr`, `/lib`, `/etc` (read-only), and explicitly-listed extra paths
- **Write access** — restricted to the workspace and `/tmp`
- **Forbidden paths** — `~/.ssh`, `~/.aws`, `~/.config` (except KylinBot's own), anything in `[autonomy] forbidden_paths`

### Network

By default, sandboxed tools have full network egress but no inbound listening.

For tighter control:

```toml
[security.sandbox]
network = "allowed-domains"
allowed_domains = ["api.openai.com", "api.anthropic.com", "api.github.com"]
```

Or `network = "none"` to block network entirely (useful for pure-local tools).

### Environment

The sandbox passes through only the env vars listed in `[autonomy] shell_env_passthrough`. Inherited secrets do not reach sandboxed tools unless explicitly passed.

### Process limits

- CPU: soft-limited to the KylinBot service's share
- Memory: capped at `[security.sandbox] memory_limit_mb` (default unset — no cap)
- Subprocesses: capped at `[security.sandbox] max_subprocesses` (default unset)
- Wall time: tool-specific timeout (default 300 seconds for `shell`)

## Per-backend notes

### Landlock

The Linux-native path. Zero setup, kernel-enforced, very low overhead. Requires kernel 5.13+.

Limitations:

- No network confinement — Landlock only controls filesystem access
- `forbidden_paths` enforced via path-based rules, not inode-based, so a clever symlink can sometimes escape (we resolve links before handing to Landlock to mitigate this)

### Bubblewrap (`bwrap`)

User-namespace-based sandbox from Flatpak. Confines filesystem and can block network. Require `bubblewrap` installed.

```bash
# install
sudo apt install bubblewrap       # Debian/Ubuntu
sudo pacman -S bubblewrap         # Arch
sudo dnf install bubblewrap       # Fedora
```

### Firejail

SUID-based sandbox. Older but widely available.

```bash
sudo apt install firejail
```

Firejail's default profile is fairly permissive; we apply a custom profile bundled with KylinBot.

### Docker

Works anywhere Docker does. Runs each tool invocation in an ephemeral container (the `<TODO-registry>/openkylin/tool-runner` image).

```toml
[security.sandbox]
backend = "docker"
image = "<TODO-registry>/openkylin/tool-runner:latest"
```

Pros: strong isolation, works on any OS. Cons: per-invocation container startup cost (100–500 ms). Best for production deployments where the overhead is acceptable.

### `noop`

No sandboxing. Tools run with the full privileges of the KylinBot service user. This is what YOLO mode enables. Loud, obvious, intentional.

## Troubleshooting

- **"Sandbox backend unavailable"** on startup — check `kylinbot service status` and the journal; the auto-detect logs which backends it tried.
- **Tools working on dev, failing in service** — the service user often differs from the CLI user. Verify both have whatever sandbox-adjacent permissions are needed (Landlock: nothing; Bubblewrap: userns enabled; Docker: service user in `docker` group).
- **Slow tool invocations** on Docker — first invocation pulls the image, subsequent are fast. Pre-pull with `docker pull <TODO-registry>/openkylin/tool-runner`.

## Code reference

- Detection: `crates/kylinbot-runtime/src/security/detect.rs`
- Backends: `crates/kylinbot-runtime/src/security/sandbox/` (one file per backend)
- Config: `[security.sandbox]` block in `crates/kylinbot-config/src/schema.rs`
