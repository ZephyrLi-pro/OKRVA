# Linux-Only Cleanup — Design

**Date:** 2026-05-13
**Status:** Implemented
**Scope:** Remove all Windows and macOS support code, CI matrices, packaging workflows, and documentation. Simplify cross-platform conditional compilation to Linux-only.

---

## Background

KylinBot deployment is going Linux-only. The repository currently carries non-trivial Windows and macOS support: a Tauri desktop app with macOS-specific dependencies, an iMessage channel that uses AppleScript bridging, ~106 `#[cfg(target_os = "...")]` sites across kernel crates, CI matrices that build for `aarch64-apple-darwin` / `x86_64-pc-windows-msvc`, Scoop and Homebrew publishing workflows, a `setup.bat` Windows bootstrap, and documentation sections covering both platforms.

This code is now overhead: it dilutes review surface, slows CI, adds packaging maintenance, and creates dead code paths that obscure the actual Linux execution flow. The goal is one aggressive cleanup pass that removes all of it.

## Goals

1. Delete every code path, file, dependency, CI job, and doc section that exists only to support Windows or macOS.
2. Simplify `#[cfg(target_os = ...)]` and `cfg!(target_os = ...)` gates that previously branched between Linux, macOS, and Windows: the Linux branch becomes the only branch.
3. Keep the workspace compiling, clippy-clean, and tests passing after every commit.
4. Land the change as a single PR with atomic, individually reviewable commits.

## Non-Goals

- **Not** touching the `Linq` channel. Linq is a webhook-based bridge that runs on Linux even though it carries iMessage/RCS/SMS traffic.
- **Not** touching other channels (Signal, Discord, Telegram, etc.). They are already cross-platform or pure HTTP.
- **Not** rewriting git history, renaming crates, reorganizing modules, or adjusting stability tiers.
- **Not** changing user-facing config defaults beyond the necessary schema removals.
- **Not** introducing any new abstractions or "future-proofing" hooks for platform support.

## Deletion Inventory

### Whole-file / whole-module deletions

| Path | Why |
|---|---|
| `crates/kylinbot-channels/src/imessage.rs` | macOS AppleScript bridge |
| `src/channels/imessage.rs` | CLI wiring for above |
| `setup.bat` | Windows bootstrap script |
| `.github/workflows/cross-platform-build-manual.yml` | Cross-platform release build matrix |
| `.github/workflows/pub-scoop.yml` | Windows package manager publish |
| `.github/workflows/pub-homebrew-core.yml` | macOS package manager publish |
| `docs/book/src/setup/macos.md` | macOS install guide |
| `docs/book/src/setup/windows.md` | Windows install guide |

### macOS-only section deletions inside otherwise-kept files

- `apps/tauri/Cargo.toml` — drop `[target.'cfg(target_os = "macos")'.dependencies]` block (`objc2`, `objc2-app-kit`, `objc2-foundation`).
- `apps/tauri/src/lib.rs` — drop `#[cfg(target_os = "macos")]` block at line 63 and the conditional inside the runner at line 105.

### `cfg` gate simplifications across kernel crates

Every site in this table has either a `#[cfg(target_os = "macos"/"windows")]` block to delete or a `#[cfg(not(target_os = "windows"))]` / `#[cfg(any(target_os = "linux", target_os = "macos", target_os = "windows"))]` gate that collapses to unconditional Linux-only code.

| Crate | Files |
|---|---|
| `kylinbot-hardware` | `lib.rs`, `discover.rs`, `serial.rs`, `uf2.rs`, `peripherals/arduino_flash.rs` |
| `kylinbot-runtime` (service) | `service/mod.rs` — drop launchd + Windows service install/uninstall/status/start/stop/log branches; keep systemd |
| `kylinbot-runtime` (security) | `security/mod.rs`, `security/detect.rs` — drop macOS Gatekeeper/sandbox detection |
| `kylinbot-runtime` (integrations) | `integrations/mod.rs`, `integrations/registry.rs` — drop iMessage entry + AppleScript description + `cfg!(target_os = "macos")` runtime branches |
| `kylinbot-runtime` (tools) | `tools/shell.rs`, `tools/file_read.rs` — drop Windows branches |
| `kylinbot-runtime` (skills) | `skills/symlink_tests.rs` — drop Windows test branch |
| `kylinbot-tools` | `browser.rs`, `browser_open.rs`, `screenshot.rs`, `cli_discovery.rs`, `pdf_read.rs`, `node_capabilities.rs`, `claude_code.rs`, `codex_cli.rs`, `gemini_cli.rs` — drop all macOS/Windows branches |
| `kylinbot-channels` | `orchestrator/mod.rs:3929` — drop `cfg!(target_os = "macos")` branch |
| `kylinbot-config` | `policy.rs` (~8 sites), `secrets.rs`, `schema.rs` (drop `IMessageConfig` type and `channels.imessage` field), `platform/native.rs` |
| `kylinbot-gateway` | `build.rs` (drop Windows `npm.cmd` branch); `src/lib.rs` (`/linq` description: drop "iMessage" mention, keep "RCS/SMS") |
| root crate | `src/main.rs`, `src/hardware/mod.rs`, `src/tunnel/mod.rs`, `src/commands/update.rs` |

### Cargo metadata cleanup

- Root `Cargo.toml`: remove `channel-imessage` feature definition and its entry from `default` features.
- `crates/kylinbot-channels/Cargo.toml`: remove `channel-imessage` feature and its entry from `default`.
- `apps/tauri/Cargo.toml`: covered above.
- `.cargo/config.toml`: drop `[target.x86_64-pc-windows-msvc]`, `[target.i686-pc-windows-msvc]`, `[target.x86_64-pc-windows-gnu]` blocks.

### CI / docs / scripts cleanup

- `.github/workflows/ci.yml` — remove macOS and Windows runners from build matrix.
- `.github/workflows/release-stable-manual.yml` — remove `aarch64-apple-darwin`, `x86_64-apple-darwin`, `x86_64-pc-windows-msvc` targets.
- `docs/book/src/SUMMARY.md`, `introduction.md`, `getting-started/quick-start.md`, `ops/troubleshooting.md`, `setup/service.md`, `foundations/fnd-001-intentional-architecture.md`, `foundations/fnd-004-engineering-infrastructure.md`, `maintainers/ci-and-actions.md`, `hardware/arduino-uno-q-setup.md` — strip Windows/macOS references and `setup.bat` mentions.
- `install.sh` — remove any internal macOS/Windows branches and user-facing prompts referencing them.
- `Dockerfile*`, `docker-compose.yml` — unchanged (already Linux-based).

## Commit Plan

One PR. Six atomic commits, ordered so the workspace compiles and tests pass at every step.

| # | Title | Contents | Size |
|---|---|---|---|
| 1 | `refactor: drop iMessage channel and macOS AppleScript bridge` | Delete `crates/kylinbot-channels/src/imessage.rs`, `src/channels/imessage.rs`; remove `IMessageConfig` and `channels.imessage` from config schema; remove iMessage entry from integrations registry + mod; drop orchestrator macOS branch; remove `channel-imessage` features; adjust gateway `/linq` description text. | M |
| 2 | `refactor: drop Tauri macOS-only deps and code paths` | Drop the `[target.'cfg(target_os = "macos")'.dependencies]` block in `apps/tauri/Cargo.toml`; drop `#[cfg(target_os = "macos")]` blocks in `apps/tauri/src/lib.rs`. | S |
| 3 | `refactor: simplify cfg gates to Linux-only across kernel crates` | Apply the cfg simplification table across `kylinbot-hardware`, `kylinbot-runtime` (service / security / tools / skills / integrations), `kylinbot-tools`, `kylinbot-config` (incl. `policy.rs`), `kylinbot-gateway/build.rs`, root crate sources. Reverse-direction gates (`#[cfg(not(target_os = "windows"))]`) become unconditional. | L |
| 4 | `chore: remove Windows/macOS CI matrices and publish workflows` | Delete `cross-platform-build-manual.yml`, `pub-scoop.yml`, `pub-homebrew-core.yml`; simplify `ci.yml` and `release-stable-manual.yml` matrices; drop Windows target sections from `.cargo/config.toml`. | M |
| 5 | `chore: remove setup.bat and Windows/macOS setup docs` | Delete `setup.bat`, `docs/book/src/setup/macos.md`, `docs/book/src/setup/windows.md`; strip references from SUMMARY, introduction, quick-start, troubleshooting, service, foundations, ci-and-actions, arduino-uno-q-setup; clean macOS/Windows prompts out of `install.sh`. | M |
| 6 | `chore: final validation pass` | Optional. Only if 1–5 leave stray unused imports, dead references, or clippy warnings. Skipped if not needed. | XS / absent |

**Ordering rationale:**
1. iMessage first because it crosses crate boundaries (channel + config + integrations + orchestrator + gateway). Removing it once avoids re-touching the same files later.
2. Tauri is a self-contained subpackage; isolating it gives a clean reviewable diff.
3. The cfg sweep is the largest commit and gets its own slot.
4. CI/Cargo metadata after code: it doesn't affect local compilation, so it's safe to defer.
5. Docs and setup.bat last: no compile impact.
6. Validation commit is a backstop only.

## Verification

Per-commit gates:

| Commit | Commands | Required |
|---|---|---|
| 1, 2, 3 | `cargo check --workspace --all-targets` · `cargo clippy --workspace --all-targets -- -D warnings` · `cargo test --workspace` | All green |
| 4 | Manual review of remaining CI matrix entries; `grep -rn 'windows\|macos\|darwin' .github/workflows/*.yml` must return only contextual mentions, not active matrix entries | All green |
| 5 | `markdownlint-cli2` per project config; `grep -rn 'setup/macos\|setup/windows\|setup.bat' docs/` must be empty | All green |
| Final | `./dev/ci.sh all` (the AGENTS.md pre-PR full validation entry point) | All green |

## Risks and Mitigations

1. **Hidden upstream dependency on a deleted public item in `kylinbot-hardware`.** Before deleting macOS/Windows-only `pub` items, search the workspace for callers and verify they are themselves macOS/Windows-gated. Caught by `cargo check` in commit 3.
2. **`channels.imessage` field in existing user configs.** Verified: `ChannelsConfig` does **not** use `#[serde(deny_unknown_fields)]`, so existing configs carrying `channels.imessage = {...}` will deserialize cleanly and the field will be silently ignored. No breakage, but the changelog entry should still note the removal so users can clean up their configs.
3. **Branch protection requires deleted CI checks.** GitHub branch protection rules may reference job names from `cross-platform-build-manual.yml` or the per-OS matrix in `ci.yml`. After commit 4, repository admin must update required checks. Flag this in the PR description.
4. **Doc link rot.** A markdown link somewhere may still point to `setup/macos.md` or `setup/windows.md`. Caught by `grep` in the commit 5 verification step.

## Additional Changes (Beyond Original Spec)

The implementation also covered these files not listed in the original inventory:

### `cfg` gate simplifications in additional crates

| Crate | Files | Change |
|---|---|---|
| `kylinbot-channels` | `matrix.rs`, `wechat.rs` | Removed `#[cfg(unix)]` on permission-setting code and `#[cfg(not(unix))]` fallbacks |
| `kylinbot-providers` | `claude_code.rs`, `copilot.rs`, `ollama.rs` | Removed `#[cfg(unix)]` on Unix-only permission code and `#[cfg(not(unix))]` fallbacks; replaced `brew install` with Linux install commands |
| `kylinbot-runtime` | `daemon/mod.rs`, `doctor/mod.rs`, `observability/runtime_trace.rs`, `security/estop.rs`, `security/otp.rs`, `security/webauthn.rs`, `skills/audit.rs` | Removed `#[cfg(unix)]` on permission code and Unix-only tests; dropped Windows path detection in audit; removed Windows `.ps1/.bat/.cmd` script suffixes |
| `kylinbot-tui` | `onboarding.rs` | Removed Windows `notepad` fallback for `$EDITOR` |
| `aardvark-sys` | `lib.rs` | Removed macOS-specific architecture mismatch hint |
| root crate | `src/main.rs`, `src/commands/update.rs` | Removed `#[cfg(unix)]`/`#[cfg(not(unix))]` on `set_owner_only_permissions`; removed Windows `.exe` handling in update command |
| `xtask` | `cmd/mdbook/sync.rs`, `util.rs` | Removed `brew install` hints; removed `.exe` suffix handling |

### Additional doc/script cleanup

| File | Change |
|---|---|
| `.claude/skills/kylinbot/references/cli-reference.md` | Removed macOS log path reference; removed iMessage from channel list (21→20); replaced macOS device path `/dev/cu.usbmodem101` with `/dev/ttyACM0` |
| `.claude/skills/kylinbot/references/rest-api.md` | Removed iMessage from Linq webhook heading |
| `README.md` | Removed macOS/Windows platform links and service references |
| `docs/book/src/architecture/crates.md` | Removed launchd/Homebrew references |
| `docs/book/src/developing/building-docs.md` | Removed `brew install gettext` hint |
| `docs/book/src/channels/chat-others.md` | Removed entire iMessage (macOS only) section |
| `docs/book/src/channels/overview.md` | Removed iMessage from channel table |
| `docs/book/src/foundations/fnd-001-intentional-architecture.md` | Removed Homebrew from package manager list |
| `docs/book/src/foundations/fnd-004-engineering-infrastructure.md` | Removed Homebrew/Scoop workflow references |
| `docs/book/src/getting-started/quick-start.md` | Removed Homebrew install option |
| `docs/book/src/getting-started/yolo.md` | Removed Seatbelt from sandbox backend list |
| `docs/book/src/hardware/adding-boards-and-tools.md` | Replaced macOS `/dev/cu.usbmodem*` device paths with Linux equivalents; removed macOS troubleshooting hint |
| `docs/book/src/hardware/nucleo-setup.md` | Removed macOS serial port section; replaced `/dev/cu.usbmodem101` config example with `/dev/ttyACM0` |
| `docs/book/src/maintainers/labels.md` | Removed `channel:imessage` label entry |
| `docs/book/src/ops/observability.md` | Removed macOS/Windows log destinations |
| `docs/book/src/ops/overview.md` | Removed macOS/Windows service commands |
| `docs/book/src/ops/service.md` | Removed macOS launchd section |
| `docs/book/src/ops/troubleshooting.md` | Removed Homebrew and `brew services` references |
| `docs/book/src/philosophy.md` | Removed Seatbelt and launchd references |
| `docs/book/src/security/overview.md` | Removed macOS Seatbelt and Windows AppContainer rows from sandbox backend table |
| `docs/book/src/security/sandboxing.md` | Removed Seatbelt and AppContainer sections |
| `docs/book/src/setup/linux.md` | Removed Homebrew (Linuxbrew) install/update/uninstall sections |
| `docs/book/src/setup/service.md` | Removed macOS LaunchAgent section and Homebrew config path |
| `docs/book/src/maintainers/release-runbook.md` | Removed Scoop/Homebrew distribution references |
| `firmware/esp32/README.md`, `firmware/esp32/SETUP.md` | Replaced Homebrew Python with `apt install`; removed macOS-specific paths |
| `scripts/README.md`, `scripts/deploy-rpi.sh` | Replaced `brew install` with `apt install`; removed Apple Silicon/Docker Desktop references |
| `.github/workflows/master-branch-flow.md` | Updated build/publish targets to Linux-only |

## Verification Results

All verification gates passed after the complete implementation:
- `cargo check --workspace --all-targets` — green
- `cargo clippy --workspace --all-targets -- -D warnings` — green
- `cargo test --workspace` — 6873 tests passed, 0 failed

## Open Questions for Implementation Phase

None. All items resolved during implementation.
