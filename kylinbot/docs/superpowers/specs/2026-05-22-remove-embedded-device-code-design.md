# Embedded/Device Code Removal — Design

**Date:** 2026-05-22
**Status:** Approved
**Scope:** Remove all embedded device, firmware, hardware peripheral, and robot control code. Retain only Linux desktop platform support.

---

## Background

Following the Linux-only cleanup (commit `59adbab`, 2026-05-13) that removed Windows and macOS code, the repository still carries significant embedded/device infrastructure: firmware for Arduino/ESP32/Pico/Nucleo boards, a hardware discovery and GPIO control crate, a robot toolkit, Aardvark I2C/SPI FFI bindings, and gateway endpoints for remote hardware management. None of this is needed for a Linux desktop deployment.

## Goals

1. Delete every crate, directory, file, dependency, feature flag, and API endpoint that exists solely for embedded devices, firmware, hardware peripherals, or robot control.
2. Keep the workspace compiling, clippy-clean, and tests passing after every commit.
3. Preserve code that serves Linux desktop users (systemd service, ARM64 Linux cross-compilation targets).
4. Land the change as a single PR with atomic, individually reviewable commits.

## Non-Goals

- Not touching channels, providers, memory, or any other Linux-desktop subsystem.
- Not adding new abstractions or "future-proofing" hooks for embedded support.
- Not modifying user-facing config beyond removing embedded-related schema fields.

## Deletion Inventory

### Whole-directory / whole-file deletions

| Path | Why |
|---|---|
| `firmware/` | All embedded firmware (Arduino, ESP32, ESP32-UI, Pico, Nucleo, fw-protocol, uno-q-bridge) |
| `crates/kylinbot-hardware/` | Hardware discovery, GPIO, serial, Pico flash, Aardvark tools, peripheral management |
| `crates/aardvark-sys/` | Aardvark I2C/SPI USB adapter FFI bindings |
| `crates/robot-kit/` | Robot control toolkit (GPIO, LIDAR, ROS2, vision) |
| `scripts/deploy-rpi.sh` | Raspberry Pi deployment script |
| `scripts/99-act-led.rules` | Raspberry Pi LED udev rules |
| `scripts/rpi-config.toml` | Raspberry Pi-specific config |
| `docs/book/src/hardware/` | Embedded development documentation |
| `crates/kylinbot-gateway/src/hardware_context.rs` | Gateway hardware API (GPIO registration, device context, reload) |
| `src/hardware/mod.rs` | Hardware CLI command handler |
| `src/peripherals/mod.rs` | Peripherals CLI command handler (list/add/flash/setup) |

### Preserved files

| Path | Why |
|---|---|
| `scripts/kylinbot.service` | systemd service file, universal for Linux desktop |
| `.cargo/config.toml` — `aarch64-unknown-linux-gnu` target | ARM64 Linux desktop/server cross-compilation |
| `.cargo/config.toml` — `aarch64-unknown-linux-musl` target | ARM64 Linux static linking |
| `.cargo/config.toml` — `x86_64-unknown-linux-musl` target | x86_64 static linking |

### Modified files

#### Root Cargo.toml

- Remove workspace members: `crates/kylinbot-hardware`, `crates/robot-kit`, `crates/aardvark-sys`
- Remove workspace dependencies: `kylinbot-hardware`, `aardvark-sys`
- Remove features: `hardware`, `peripheral-rpi`, `probe`, `embedded-web`
- Remove from `ci-all`: `hardware`, `peripheral-rpi`, `probe`
- Remove from `agent-runtime`: `dep:glob`, `dep:which`
- Remove root crate dependencies: `aardvark-sys`, `glob` (optional), `which` (optional)
- Clean up release profile comments referencing Raspberry Pi constraints

#### crates/kylinbot-gateway/Cargo.toml

- Remove `kylinbot-hardware.workspace = true`
- Remove `include_dir = { version = "0.7", optional = true }`
- Remove `embedded-web = ["dep:include_dir"]` feature

#### crates/kylinbot-gateway/src/lib.rs

- Remove `mod hardware_context` and hardware route registrations (`/api/hardware/pin`, `/api/hardware/context`, `/api/hardware/reload`)
- Remove `#[cfg(feature = "embedded-web")]` branches; keep only filesystem-based dashboard serving

#### crates/kylinbot-gateway/src/static_files.rs

- Remove `include_dir!()` branch; keep only `web_dist_dir` filesystem reads

#### src/main.rs

- Remove `HardwareCommands` and `PeripheralCommands` enum/Clap definitions
- Remove `mod hardware` and `mod peripherals` module declarations
- Remove command match branches calling hardware/peripheral handlers

#### src/lib.rs

- Remove `#[cfg(feature = "hardware")] pub use kylinbot_hardware::*`
- Remove related feature-gated re-exports

#### crates/kylinbot-runtime/Cargo.toml

- Remove `aardvark-sys.workspace = true`

#### .cargo/config.toml

- Remove Android cross-compilation targets: `armv7-linux-androideabi`, `aarch64-linux-android`

#### docs/book/src/SUMMARY.md

- Remove hardware chapter references

## Commit Plan

One PR. Six atomic commits, ordered so the workspace compiles and tests pass at every step.

| # | Title | Contents | Size |
|---|---|---|---|
| 1 | `refactor: remove kylinbot-hardware and aardvark-sys crates` | Delete `crates/kylinbot-hardware/`, `crates/aardvark-sys/`; remove workspace members/deps/features from root Cargo.toml (`hardware`, `peripheral-rpi`, `probe`); remove from `ci-all` and `agent-runtime`; remove `aardvark-sys`/`glob`/`which` deps; delete `src/hardware/mod.rs`, `src/peripherals/mod.rs`; clean `src/main.rs` and `src/lib.rs` modules and commands; clean `kylinbot-runtime/Cargo.toml` | L |
| 2 | `refactor: remove robot-kit crate` | Delete `crates/robot-kit/`; remove workspace member from root Cargo.toml | S |
| 3 | `refactor: remove firmware directory` | Delete `firmware/` entire directory | S |
| 4 | `refactor: remove gateway hardware API and embedded-web feature` | Delete `hardware_context.rs`; remove `kylinbot-hardware` dep and `embedded-web` feature from gateway Cargo.toml; clean `lib.rs` route registration; clean `static_files.rs` conditional compilation; remove `embedded-web` feature from root Cargo.toml | M |
| 5 | `chore: remove embedded/device scripts and docs` | Delete `scripts/deploy-rpi.sh`, `scripts/99-act-led.rules`, `scripts/rpi-config.toml`, `docs/book/src/hardware/`; remove Android targets from `.cargo/config.toml`; update `SUMMARY.md`; clean release profile embedded-device comments | M |
| 6 | `chore: final validation pass` | Fix stray references, clippy warnings, dead code | XS |

**Ordering rationale:**
1. Core crates first — they are referenced by other code, removing them forces all consumers to be cleaned in one pass.
2. Robot-kit is independent but still a workspace member.
3. Firmware directory has no compile-time impact but is a large deletion.
4. Gateway cleanup depends on step 1 having removed `kylinbot-hardware`.
5. Scripts/docs/config have no compile impact.
6. Validation backstop.

## Verification

Per-commit gates:

| Commit | Commands | Required |
|---|---|---|
| 1–4 | `cargo check --workspace --all-targets`; `cargo clippy --workspace --all-targets -- -D warnings` | All green |
| 5 | `grep -rn 'kylinbot_hardware\|robot.kit\|aardvark\|firmware/' src/ crates/` must return empty | All green |
| 6 | `cargo test --workspace` | All green |

## Risks and Mitigations

1. **Gateway code references `kylinbot_hardware::load_hardware_context_prompt`** — the `handle_hardware_reload` function directly calls it. Commit 4 removes the entire file, so this is a clean deletion.
2. **`peripherals` config schema in user configs** — existing `config.toml` files with `[peripherals]` sections will deserialize cleanly (no `deny_unknown_fields`). Note removal in changelog.
3. **`hardware` feature-gated code paths in other crates** — the previous Linux-only cleanup already simplified most `cfg` gates. Commit 6 catches any stragglers.
4. **Documentation link rot** — `SUMMARY.md` references to `hardware/` chapters must be removed. Caught by commit 5 grep.
