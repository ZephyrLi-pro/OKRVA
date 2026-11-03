# scripts/ — KylinBot Helper Scripts

This directory contains helper scripts for development, CI, and deployment.

## Contents

| File | Description |
|---|---|
| `kylinbot.service` | systemd unit file for Linux deployment |
| `ci/` | CI pipeline helper scripts |
| `release/` | Release automation scripts (version bump, changelog) |
| `browser/` | Browser automation helper scripts |

## systemd Service

The `kylinbot.service` file can be installed as a systemd user unit for auto-start and process supervision:

```bash
cp scripts/kylinbot.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now kylinbot
```
