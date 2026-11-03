# Service Management

KylinBot ships with first-class service integration for systemd (Linux). The service is driven by one CLI surface:

```bash
kylinbot service install     # register the service
kylinbot service start       # start it
kylinbot service stop        # stop it
kylinbot service restart     # stop + start
kylinbot service status      # running / stopped, last exit code
kylinbot service uninstall   # remove it
```

The platform-specific backends are implemented in `crates/kylinbot-runtime/src/service/`. You don't have to think about them — but knowing what they produce helps when debugging.

## Linux — systemd

`kylinbot service install` writes a user-scoped unit at `~/.config/systemd/user/kylinbot.service`.

The unit:

- `Type=simple` with the agent process staying in the foreground
- `User=` set to the invoking user
- `Restart=on-failure` with a 10-second backoff
- `ExecStart=/home/$USER/.cargo/bin/kylinbot daemon`

### Manual control

```bash
systemctl --user start kylinbot
systemctl --user stop kylinbot
systemctl --user status kylinbot
systemctl --user enable kylinbot     # start on login
```

### Logs

```bash
journalctl --user -u kylinbot -f        # follow
journalctl --user -u kylinbot --since "1h ago"
```

### System-scope (root) service

If you need KylinBot to start before user login (headless SBCs, VPSes), run the install command as root:

```bash
sudo kylinbot service install
sudo systemctl enable --now kylinbot
```

When invoked with sudo/root, `kylinbot service install` creates a system-scope unit at `/etc/systemd/system/kylinbot.service` and provisions a dedicated `kylinbot` service user.

## Linux — OpenRC

Detected automatically when `/run/openrc` exists (Alpine, some Gentoo configs).

```bash
kylinbot service install   # writes /etc/init.d/kylinbot
rc-service kylinbot start
rc-update add kylinbot default    # start on boot
```

## Config path resolution

The service reads config from whichever workspace it was installed against. Order:

1. `$KYLINBOT_CONFIG_DIR/config.toml` if set
2. `$KYLINBOT_WORKSPACE/.kylinbot/config.toml` if set
3. `~/.kylinbot/config.toml`

If your service seems to ignore config changes, check which path the daemon is reading:

```bash
kylinbot config list
```

The first few lines of its output show the config file path it resolved against.

## Auto-update

The service does **not** auto-update. That's deliberate — you pick when to take new code. Subscribe to the GitHub release feed or the Discord `#releases` channel (see [Contributing → Communication](../contributing/communication.md)).

## See also

- [Linux setup](./linux.md)
- [Operations → Logs & observability](../ops/observability.md)
- [Operations → Troubleshooting](../ops/troubleshooting.md)
