# Troubleshooting

Common failure modes, in the order you're likely to encounter them.

First stop for any issue:

```bash
kylinbot doctor
```

Runs a series of checks and prints a summary. Most of what follows is the detailed version of what `doctor` flags.

---

## Install-time

### `cargo` not found

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

Or pass `--prebuilt` to `install.sh` to skip Rust entirely.

### Missing build dependencies (Linux)

Install the baseline toolchain for your distro, then re-run `./install.sh`:

```bash
# Debian / Ubuntu
sudo apt install build-essential pkg-config

# Fedora / RHEL
sudo dnf group install development-tools && sudo dnf install pkg-config

# Arch
sudo pacman -S base-devel
```

Full per-distro list: [Setup → Linux](../setup/linux.md).

### Build OOMs on low-RAM hosts

Compiling KylinBot from source needs ~2 GB RAM at peak. On a low-memory host, you may OOM.

Options:

1. **Use a prebuilt** — `./install.sh --prebuilt` skips the toolchain and downloads from GitHub Releases
2. **Cross-compile on a bigger machine and copy the binary**
3. **Serialise the build** — `CARGO_BUILD_JOBS=1 cargo build --release --locked`
4. **Add swap** (works for RAM, costs disk — check you have both)

### Build is very slow

The Matrix E2EE stack (`matrix-sdk`, `ruma`, `vodozemac`) and TLS/crypto native deps (`aws-lc-sys`, `ring`) are the main cost. Opt out if you don't need them:

```bash
cargo build --release --locked --no-default-features --features "default-lean"
```

Or check what's happening:

```bash
cargo check --timings
# report at target/cargo-timings/cargo-timing.html
```

### `kylinbot: command not found` after install

`cargo install` puts binaries in `~/.cargo/bin/`. Add to PATH:

```bash
export PATH="$HOME/.cargo/bin:$PATH"
```

Persist in your shell profile.

---

## Onboarding

### Wizard insists on a config that doesn't exist

If an earlier install left `~/.kylinbot/config.toml`, re-run with `--force`:

```bash
kylinbot onboard --force
```

Or just delete the directory and start over:

```bash
rm -rf ~/.kylinbot
kylinbot onboard
```

### Wizard writes to the wrong path

If you set `KYLINBOT_WORKSPACE` to a custom location before onboarding but the wizard still writes to the default path, ensure the environment variable is exported in the same shell session:

```bash
export KYLINBOT_WORKSPACE="/path/to/your/workspace"
kylinbot onboard
```

Or manually symlink once:

```bash
ln -s "/path/to/your/workspace" ~/.kylinbot
```

---

## Runtime

### Daemon starts, then immediately exits

Check journald / the platform log (see [Logs & observability](./observability.md)) for the actual error. Common causes:

- **Invalid config** — `kylinbot config list` to print resolved values, `kylinbot config schema` to see the expected shape
- **Port conflict** — only applies when `gateway.transport = "tcp"`; another process on `42617` means change `[gateway] port` or free the port
- **Missing secrets** — encrypted secrets store can't decrypt because the key file is gone; restore from backup or re-run onboarding

### Daemon keeps restarting

`systemctl --user status kylinbot` shows the last exit. If it's a config error, it stopped restarting (exit 2) and you need to fix the config. If it's a panic, the unit retries every 10 s.

Enable debug logging and catch the next failure:

```bash
kylinbot service stop
RUST_LOG=kylinbot=debug kylinbot daemon
```

### Gateway unreachable

```bash
curl --unix-socket "$XDG_RUNTIME_DIR/kylinbot/gateway.sock" http://x/health
```

If the socket path is different, check the startup log for `unix://...` or inspect
`gateway.socket_path`. If `gateway.transport = "tcp"`, use:

```bash
curl -sv http://localhost:42617/health
```

If connection refused in TCP mode: daemon isn't running, or it's bound to a
different interface. Check `[gateway] host` / `port` in config.

If 403 / 401: pairing not completed or token expired. Run the pairing flow again.

---

## Channels

### Telegram: `terminated by other getUpdates request`

Two processes are polling the same bot token. Telegram only allows one poller at a time.

Fix: stop all but one `kylinbot daemon` / `kylinbot channel start` using that token.

### Discord / Slack auth failures

Discord tokens expire if you regenerate them in the Developer Portal. Slack bot tokens don't expire but can be revoked. Check the bot is still installed in the target workspace/guild.

For either:

```bash
kylinbot channel doctor discord
kylinbot channel doctor slack
```

### Matrix: "unknown device"

If you re-onboarded without keeping device keys, the homeserver sees a new device that hasn't been verified. Re-verify from another logged-in client, or reset the key store:

```bash
rm -rf ~/.kylinbot/workspace/matrix-crypto
# re-run pairing flow on next channel start
```

### IMAP polling stopped

Most often an auth failure — provider rotated the password or the app-password expired. Check:

```bash
journalctl --user -u kylinbot -n 200 | grep -i imap
```

---

## Providers

### "Connection timed out" to Ollama

- Ollama daemon not running: `systemctl status ollama`
- Wrong URL in config — from inside a container, `localhost:11434` doesn't reach the host; use `host.docker.internal` or the host's LAN IP
- Firewall blocking port 11434 — rare locally, common on shared LANs

### Anthropic / OpenAI 401

API key invalid or expired. Regenerate at the provider's dashboard, update in `[providers.models.<name>] api_key`, restart the service.

If using OAuth (`sk-ant-oat*`), the OAuth token may have expired — OAuth-issued tokens are longer-lived but not infinite. Re-authenticate.

### Fallback chain always uses the fallback, never the primary

Check latency and error logs — the primary may be consistently timing out. If so, lower the retry count on the primary so it fails through faster, or fix whatever's making the primary flaky.

---

## Tools

### Shell commands "blocked by policy"

Expected behaviour at `Supervised` autonomy for unknown commands. Either:

- Approve inline when prompted
- Add the command to `[autonomy] allowed_commands`
- Raise autonomy to `Full` if you trust the context

See [Security → Autonomy levels](../security/autonomy.md).

### Tool invocations fail inside Docker sandbox

- Container image isn't pulled — `docker pull <TODO-registry>/openkylin/tool-runner`
- Docker daemon not reachable from the KylinBot user — check `docker info`
- Tool needs a device that's not passed through — extend `allow_devices`

### Browser tool hangs on first use

Playwright downloads Chromium (~150 MB) on first launch. Let it finish. If it keeps hanging, check disk space and proxy config.

---

## Service mode

### Service installed but shows inactive

```bash
kylinbot service start
kylinbot service status
```

If that succeeds interactively but the service dies in the background, it's almost always config or permissions — read the journal:

```bash
journalctl --user -u kylinbot --since "5 minutes ago"
```

### Service can't find config

The service and CLI may resolve config differently if they run as different users or with different env vars. Force-print the path the daemon sees:

```bash
kylinbot config list
```

If the paths differ between `kylinbot config list` (as you) and the service (as its user), either:

- Set `KYLINBOT_CONFIG_DIR` in the service unit's `Environment=`
- Run the service as you (lingering-enabled user service)
- Copy/symlink the config to the path the service expects

---

## Still stuck?

Gather diagnostics and file an issue:

```bash
kylinbot --version
kylinbot doctor
kylinbot channel doctor
journalctl --user -u kylinbot --since "1 hour ago" > kylinbot-log.txt
```

Sanitise `kylinbot-log.txt` (redact channel tokens if any slipped through — they shouldn't) and attach it to the issue. See [Contributing → Communication](../contributing/communication.md) for where.

## See also

- [Logs & observability](./observability.md)
- [Service & daemon](./service.md)
- [Setup → Service management](../setup/service.md)
- [Reference → Config](../reference/config.md)
