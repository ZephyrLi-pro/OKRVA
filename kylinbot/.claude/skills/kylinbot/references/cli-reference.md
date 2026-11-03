# KylinBot CLI Reference

Complete command reference for the `kylinbot` binary.

## Table of Contents

1. [Agent](#agent)
2. [Onboarding](#onboarding)
3. [Status & Diagnostics](#status--diagnostics)
4. [Memory](#memory)
5. [Cron](#cron)
6. [Providers & Models](#providers--models)
7. [Gateway & Daemon](#gateway--daemon)
8. [Service Management](#service-management)
9. [Channels](#channels)
10. [Security & Emergency Stop](#security--emergency-stop)
11. [Skills](#skills)
12. [Shell Completions](#shell-completions)

---

## Agent

Interactive chat or single-message mode.

```bash
kylinbot agent                                          # Interactive REPL
kylinbot agent -m "Summarize today's logs"              # Single message
kylinbot agent -p anthropic --model claude-sonnet-4-6   # Override provider/model
kylinbot agent -t 0.3                                   # Set temperature
```

**Key flags:**
- `-m <message>` — single message mode (no REPL)
- `-p <provider>` — override provider (openrouter, anthropic, openai, ollama)
- `--model <model>` — override model
- `-t <float>` — temperature (0.0–2.0)

The agent has access to 30+ tools gated by security policy: shell, file_read, file_write, file_edit, glob_search, content_search, memory_store, memory_recall, memory_forget, browser, http_request, web_fetch, web_search, cron, delegate, git, and more. Max tool iterations defaults to 10.

---

## Onboarding

First-time setup or reconfiguration.

```bash
kylinbot onboard                                 # Quick mode (default: openrouter)
kylinbot onboard --provider anthropic            # Quick mode with specific provider
kylinbot onboard                                 # Guided wizard (default)
kylinbot onboard --memory sqlite                 # Set memory backend
kylinbot onboard --force                         # Overwrite existing config
kylinbot onboard --channels-only                 # Repair channels only
```

**Key flags:**
- `--provider <name>` — openrouter (default), anthropic, openai, ollama
- `--model <model>` — default model
- `--memory <backend>` — sqlite, markdown, lucid, none
- `--force` — overwrite existing config.toml
- `--channels-only` — only repair channel configuration
- `--reinit` — start fresh (backs up existing config)

Creates `~/.kylinbot/config.toml` with `0600` permissions.

---

## Status & Diagnostics

```bash
kylinbot status                    # System overview
kylinbot doctor                    # Run all diagnostic checks
kylinbot doctor models             # Probe model connectivity
kylinbot doctor traces             # Query execution traces
```

---

## Memory

```bash
kylinbot memory list                              # List all entries
kylinbot memory list --category core --limit 10   # Filtered list
kylinbot memory get "some-key"                    # Get specific entry
kylinbot memory stats                             # Usage statistics
kylinbot memory clear --key "prefix" --yes        # Delete entries (requires --yes)
```

**Key flags:**
- `--category <name>` — filter by category (core, daily, conversation, custom)
- `--limit <n>` — limit results
- `--key <prefix>` — key prefix for clear operations
- `--yes` — skip confirmation (required for clear)

---

## Cron

```bash
kylinbot cron list                                                      # List all jobs
kylinbot cron add '0 9 * * 1-5' 'Good morning' --tz America/New_York   # Recurring (cron expr)
kylinbot cron add-at '2026-03-11T10:00:00Z' 'Remind me about meeting'  # One-time at specific time
kylinbot cron add-every 3600000 'Check server health'                   # Interval in milliseconds
kylinbot cron once 30m 'Follow up on that task'                         # Delay from now
kylinbot cron pause <id>                                                # Pause job
kylinbot cron resume <id>                                               # Resume job
kylinbot cron remove <id>                                               # Delete job
```

**Subcommands:**
- `add <cron-expr> <command>` — standard cron expression (5-field)
- `add-at <iso-datetime> <command>` — fire once at exact time
- `add-every <ms> <command>` — repeating interval
- `once <duration> <command>` — delay from now (e.g., `30m`, `2h`, `1d`)

---

## Providers & Models

```bash
kylinbot providers                                # List all 40+ supported providers
kylinbot models list                              # Show cached model catalog
kylinbot models refresh --all                     # Refresh catalogs from all providers
kylinbot models set anthropic/claude-sonnet-4-6   # Set default model
kylinbot models status                            # Current model info
```

Model routing in config.toml:
```toml
[[model_routes]]
hint = "reasoning"
provider = "openrouter"
model = "anthropic/claude-sonnet-4-6"
```

---

## Gateway & Daemon

```bash
kylinbot gateway                                 # Start HTTP gateway (foreground)
kylinbot gateway -p 8080 --host 127.0.0.1        # Custom port/host

kylinbot daemon                                  # Gateway + channels + scheduler + heartbeat
kylinbot daemon -p 8080 --host 0.0.0.0           # Custom bind
```

**Gateway defaults:**
- Port: 42617
- Host: 127.0.0.1
- Pairing required: true
- Public bind allowed: false

---

## Service Management

OS service lifecycle (systemd on Linux).

```bash
kylinbot service install     # Install as system service
kylinbot service start       # Start the service
kylinbot service status      # Check service status
kylinbot service stop        # Stop the service
kylinbot service restart     # Restart the service
kylinbot service uninstall   # Remove the service
```

**Logs:**
- Linux: `journalctl -u kylinbot`

---

## Channels

Channels are configured in `config.toml` under `[channels]` and `[channels_config.*]`.

```bash
kylinbot channels list       # List configured channels
kylinbot channels doctor     # Check channel health
```

Supported channels (20 total): Telegram, Discord, Slack, WhatsApp (Meta), WATI, Linq (RCS/SMS), Email (IMAP/SMTP), IRC, Matrix, Nostr, Signal, Nextcloud Talk, and more.

Channel config example (Telegram):
```toml
[channels]
telegram = true

[channels_config.telegram]
bot_token = "..."
allowed_users = [123456789]
```

---

## Security & Emergency Stop

```bash
kylinbot estop --level kill-all                              # Stop everything
kylinbot estop --level network-kill                          # Block all network access
kylinbot estop --level domain-block --domain "*.example.com" # Block specific domains
kylinbot estop --level tool-freeze --tool shell              # Freeze specific tool
kylinbot estop status                                        # Check estop state
kylinbot estop resume --network                              # Resume (may require OTP)
```

**Estop levels:**
- `kill-all` — nuclear option, stops all agent activity
- `network-kill` — blocks all outbound network
- `domain-block` — blocks specific domain patterns
- `tool-freeze` — freezes individual tools

Autonomy config in config.toml:
```toml
[autonomy]
level = "supervised"                           # read_only | supervised | full
workspace_only = true
allowed_commands = ["git", "cargo", "python"]
forbidden_paths = ["/etc", "/root", "~/.ssh"]
max_actions_per_hour = 20
max_cost_per_day_cents = 500
```

---

## Skills

```bash
kylinbot skills list         # List installed skills
kylinbot skills install <path-or-url>  # Install a skill
kylinbot skills audit        # Audit installed skills
kylinbot skills remove <name>  # Remove a skill
```

---

## Shell Completions

```bash
kylinbot completions zsh     # Generate Zsh completions
kylinbot completions bash    # Generate Bash completions
kylinbot completions fish    # Generate Fish completions
```

---

## Config File

Default location: `~/.kylinbot/config.toml`

Config resolution order (first match wins):
1. `KYLINBOT_CONFIG_DIR` environment variable
2. `KYLINBOT_WORKSPACE` environment variable
3. `~/.kylinbot/active_workspace.toml` marker file
4. `~/.kylinbot/config.toml` (default)
