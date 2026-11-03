# Linux

Install, update, run as a service, and uninstall — all Linux distributions.

## Install

`install.sh` is the preferred path on every Linux distro. Pipe it from `curl`, or clone and run it locally — both do the same thing.

### Option 1 — `install.sh` via curl (fastest)

```bash
curl -fsSL https://gitee.com/openkylin/kylinbot/raw/master/install.sh | bash
```

### Option 2 — `install.sh` from a clone

```bash
git clone https://gitee.com/openkylin/kylinbot.git
cd kylinbot
./install.sh
```

### What the installer does

1. Detects your distribution and architecture
2. Asks whether you want a prebuilt binary or to build from source (the default is interactive — non-interactive shells default to prebuilt when available)
3. Places the binary at `~/.cargo/bin/kylinbot`
4. Runs `kylinbot onboard` to complete first-time setup

Flags:

```bash
./install.sh --prebuilt                      # always prebuilt, skip the prompt
./install.sh --source                        # always build from source
./install.sh --minimal                       # kernel only (~6.6 MB)
./install.sh --source --features agent-runtime,channel-discord   # custom features
./install.sh --skip-onboard                  # install only; run `kylinbot onboard` later
./install.sh --list-features                 # print available features and exit
./install.sh --help                          # full flag reference
```

## System dependencies

The core binary is statically linked where possible. Some features require system libraries:

| Feature | Package (Debian/Ubuntu) | Package (Arch) | Package (Fedora) |
|---|---|---|---|
| Docs translation (`cargo mdbook sync`) | `gettext` | `gettext` | `gettext` |
| Browser tool (playwright) | `libnss3`, `libatk1.0-0`, `libcups2` (see `playwright --help`) | `nss`, `atk`, `cups` | `nss`, `atk`, `cups` |
| Audio (TTS, voice channels) | `libasound2-dev` | `alsa-lib` | `alsa-lib-devel` |

Most deployments don't need any of these.

## Running as a service

Systemd is the default. OpenRC is detected and supported as a fallback.

```bash
kylinbot service install
kylinbot service start
kylinbot service status
```

Logs go to the systemd journal by default:

```bash
journalctl --user -u kylinbot -f
```

Full details: [Service management](./service.md).

## Update

Re-run the installer — it detects the existing install and upgrades in place:

```bash
curl -fsSL https://gitee.com/openkylin/kylinbot/raw/master/install.sh | bash -s -- --skip-onboard
```

Or from a clone:

```bash
cd /path/to/kylinbot
git pull
./install.sh --skip-onboard
```

After updating, restart the service:

```bash
kylinbot service restart
```

## Uninstall

Stop and remove the service:

```bash
kylinbot service stop
kylinbot service uninstall
```

Remove the binary:

```bash
# cargo install / bootstrap
rm ~/.cargo/bin/kylinbot
```

Remove config and workspace (optional — this deletes conversation history):

```bash
rm -rf ~/.kylinbot ~/.config/kylinbot
```

## Next

- [Service management](./service.md) — systemd unit details, logs, auto-start
- [Quick start](../getting-started/quick-start.md) — once installed, getting talking
- [Operations → Overview](../ops/overview.md) — running in production
