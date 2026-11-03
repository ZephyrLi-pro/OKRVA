# Embedded/Device Code Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove all embedded device, firmware, hardware peripheral, and robot control code from the KylinBot workspace, retaining only Linux desktop platform support.

**Architecture:** Nine atomic commits ordered so the workspace compiles and tests pass at every step. Core crates are removed first (they are referenced by consumers), then CLI commands, tools, runtime integration, config schema, and finally independent directories, docs, and CI.

**Tech Stack:** Rust workspace, Cargo build system, Fluent i18n, mdBook documentation, GitHub Actions CI.

---

## File Structure

### Deleted entirely
| Path | Responsibility |
|---|---|
| `firmware/` | All embedded firmware (Arduino, ESP32, Pico, Nucleo, etc.) |
| `crates/kylinbot-hardware/` | Hardware discovery, GPIO, serial, Pico flash, Aardvark tools |
| `crates/aardvark-sys/` | Aardvark I2C/SPI USB adapter FFI bindings |
| `crates/robot-kit/` | Robot control toolkit (GPIO, LIDAR, ROS2, vision) |
| `crates/kylinbot-gateway/src/hardware_context.rs` | Gateway hardware API endpoints |
| `src/hardware/mod.rs` | Hardware CLI command handler |
| `src/peripherals/mod.rs` | Peripherals CLI command handler |
| `crates/kylinbot-api/src/peripherals_traits.rs` | `Peripheral` trait definition |
| `crates/kylinbot-tools/src/hardware_board_info.rs` | Board info tool |
| `crates/kylinbot-tools/src/hardware_memory_map.rs` | Memory map tool |
| `crates/kylinbot-tools/src/hardware_memory_read.rs` | Memory read tool |
| `src/tools/hardware_board_info.rs` | Root-crate shim |
| `src/tools/hardware_memory_map.rs` | Root-crate shim |
| `src/tools/hardware_memory_read.rs` | Root-crate shim |
| `crates/kylinbot-runtime/firmware` | Symlink → `../../firmware` |
| `crates/kylinbot-runtime/src/firmware` | Symlink → `../../firmware` (broken) |
| `scripts/deploy-rpi.sh` | Raspberry Pi deployment |
| `scripts/99-act-led.rules` | Raspberry Pi LED udev rules |
| `scripts/rpi-config.toml` | Raspberry Pi config |
| `docs/book/src/hardware/` | Embedded development documentation |

### Modified files
| Path | Change |
|---|---|
| `Cargo.toml` | Remove workspace members, deps, features |
| `crates/kylinbot-gateway/Cargo.toml` | Remove `kylinbot-hardware`, `include_dir` deps, `embedded-web` feature |
| `crates/kylinbot-gateway/src/lib.rs` | Remove `mod hardware_context`, hardware routes |
| `crates/kylinbot-gateway/src/static_files.rs` | Remove `include_dir!()` branch |
| `crates/kylinbot-runtime/Cargo.toml` | Remove `aardvark-sys` dep |
| `crates/kylinbot-runtime/src/tools/mod.rs` | Remove hardware tool re-exports |
| `crates/kylinbot-runtime/src/agent/system_prompt.rs` | Remove hardware section |
| `crates/kylinbot-runtime/src/agent/loop_.rs` | Remove hardware tool registrations |
| `crates/kylinbot-runtime/src/onboard/mod.rs` | Remove hardware onboarding step |
| `crates/kylinbot-runtime/src/sop/types.rs` | Remove `SopTrigger::Peripheral` variant |
| `crates/kylinbot-runtime/src/sop/dispatch.rs` | Remove `dispatch_peripheral_signal()` |
| `crates/kylinbot-runtime/src/sop/engine.rs` | Remove Peripheral match arms + tests |
| `crates/kylinbot-api/src/lib.rs` | Remove `peripherals_traits` module |
| `crates/kylinbot-tools/Cargo.toml` | Remove `probe-rs` dep, `probe` feature |
| `crates/kylinbot-tools/src/lib.rs` | Remove hardware tool modules |
| `crates/kylinbot-config/src/schema.rs` | Remove hardware/peripherals config structs |
| `crates/kylinbot-channels/src/orchestrator/acp_server.rs` | Remove hardware tool-name arms |
| `crates/kylinbot-providers/src/anthropic.rs` | Neutralize hardware comment |
| `src/main.rs` | Remove hardware/peripheral commands, modules |
| `src/lib.rs` | Remove hardware/peripheral modules, enums, re-exports |
| `src/tools/mod.rs` | Remove hardware tool shims |
| `src/config/mod.rs` | Remove hardware/peripherals config re-exports |
| `src/sop/mod.rs` | Remove `SopTrigger::Peripheral` match arm |
| `AGENTS.md` | Remove hardware references |
| `release-plz.toml` | Remove aardvark-sys, robot-kit entries |
| `.cargo/config.toml` | Remove Android targets |
| `install.sh` | Remove hardware/peripheral/probe tokens |
| `.github/workflows/release-stable-manual.yml` | Remove ARM/Android matrix entries |
| `docs/book/src/SUMMARY.md` | Remove hardware chapter |
| Various doc files | Remove/strikethrough embedded references |
| Fluent locale files (en, fr, ja) | Remove hardware/peripheral keys |

---

### Task 1: Remove kylinbot-hardware and aardvark-sys crates

**Files:**
- Delete: `crates/kylinbot-hardware/` (entire directory)
- Delete: `crates/aardvark-sys/` (entire directory)
- Delete: `crates/kylinbot-gateway/src/hardware_context.rs`
- Modify: `Cargo.toml`
- Modify: `crates/kylinbot-gateway/Cargo.toml`
- Modify: `crates/kylinbot-gateway/src/lib.rs`
- Modify: `crates/kylinbot-runtime/Cargo.toml`

- [ ] **Step 1: Delete the crate directories**

```bash
rm -rf crates/kylinbot-hardware/ crates/aardvark-sys/
```

- [ ] **Step 2: Delete gateway hardware_context.rs**

```bash
rm crates/kylinbot-gateway/src/hardware_context.rs
```

- [ ] **Step 3: Remove workspace members from root Cargo.toml**

In `Cargo.toml` line 2, remove `"crates/kylinbot-hardware"`, `"crates/aardvark-sys"`, and `"crates/robot-kit"` from the `members` array. The line becomes:

```toml
members = [".", "crates/kylinbot-api", "crates/kylinbot-infra", "crates/kylinbot-config", "crates/kylinbot-providers", "crates/kylinbot-memory", "crates/kylinbot-channels", "crates/kylinbot-tools", "crates/kylinbot-runtime", "crates/kylinbot-tui", "crates/kylinbot-plugins", "crates/kylinbot-gateway", "crates/kylinbot-tool-call-parser", "crates/kart-embedding-sys", "crates/kart-vector-database-sys", "crates/kylinbot-macros", "apps/tauri", "tools/fill-translations", "xtask"]
```

- [ ] **Step 4: Remove workspace dependencies from root Cargo.toml**

Delete line 25 (`kylinbot-hardware = { path = ... }`) and line 28 (`aardvark-sys = { path = ... }`).

- [ ] **Step 5: Remove hardware-related features from root Cargo.toml**

Remove these feature lines (lines 315-317, 321, 324):

```toml
hardware = ["dep:kylinbot-hardware", "kylinbot-hardware/hardware"]
peripheral-rpi = ["dep:kylinbot-hardware", "kylinbot-hardware/peripheral-rpi"]
probe = ["dep:kylinbot-hardware", "kylinbot-hardware/probe"]
embedded-web = ["kylinbot-gateway/embedded-web"]
```

- [ ] **Step 6: Remove hardware/peripheral-rpi/probe from ci-all feature in root Cargo.toml**

Remove `"hardware"`, `"peripheral-rpi"`, `"probe"` from the `ci-all` feature array (lines 343-344).

- [ ] **Step 7: Remove dep:glob from agent-runtime feature in root Cargo.toml**

In the `agent-runtime` feature (line 251), remove `"dep:glob"`. Keep `"dep:which"` — it is used by `src/main.rs:2027` for `kylinbot-desktop` discovery.

The line becomes:

```toml
agent-runtime = [
    "dep:kylinbot-runtime", "dep:kylinbot-channels", "dep:kylinbot-tools",
    "dep:rusqlite",
    "dep:ring", "dep:lru", "dep:chrono-tz", "dep:cron", "dep:which",
    ...
```

- [ ] **Step 8: Remove root crate [dependencies] entries**

Remove from `[dependencies]`:
- Line 79: `kylinbot-hardware = { workspace = true, optional = true }`
- Line 142: `aardvark-sys.workspace = true`
- Line 188: `glob = { version = "0.3", optional = true }`

Remove the comment on line 187: `# Hardware discovery (device path globbing)`.

Amend the comment on line 193 (`# Binary discovery (init system detection)`) to: `# Binary discovery (desktop companion, init system detection)`.

- [ ] **Step 9: Remove kylinbot-hardware dep from gateway Cargo.toml**

In `crates/kylinbot-gateway/Cargo.toml`, remove line 19: `kylinbot-hardware.workspace = true`.

- [ ] **Step 10: Remove mod hardware_context and hardware route registrations from gateway lib.rs**

In `crates/kylinbot-gateway/src/lib.rs`:
- Remove line 18: `pub mod hardware_context;`
- Remove all `use` imports that reference `hardware_context` or `kylinbot_hardware`
- Find and remove the hardware route registrations for `/api/hardware/pin`, `/api/hardware/context`, `/api/hardware/reload` (and their handler functions like `handle_hardware_pin`, `handle_hardware_context_post`, `handle_hardware_context_get`, `handle_hardware_reload`)

- [ ] **Step 11: Remove aardvark-sys dep from runtime Cargo.toml**

In `crates/kylinbot-runtime/Cargo.toml`, remove line 75: `aardvark-sys.workspace = true`.

- [ ] **Step 12: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success (no compilation errors)

- [ ] **Step 13: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: Success

- [ ] **Step 14: Commit**

```bash
git add -A
git commit -m "refactor: remove kylinbot-hardware and aardvark-sys crates

Remove kylinbot-hardware and aardvark-sys workspace crates, gateway
hardware_context module and routes, and related Cargo dependencies and
features (hardware, peripheral-rpi, probe). Keep dep:which — used by
src/main.rs for kylinbot-desktop discovery."
```

---

### Task 2: Remove hardware CLI commands and Peripheral trait

**Files:**
- Delete: `src/hardware/mod.rs`
- Delete: `src/peripherals/mod.rs`
- Delete: `crates/kylinbot-api/src/peripherals_traits.rs`
- Modify: `src/main.rs`
- Modify: `src/lib.rs`
- Modify: `crates/kylinbot-api/src/lib.rs`
- Modify: `crates/kylinbot-runtime/locales/{en,fr,ja}/cli.ftl`

- [ ] **Step 1: Delete hardware and peripherals CLI modules**

```bash
rm src/hardware/mod.rs src/peripherals/mod.rs
```

- [ ] **Step 2: Delete Peripheral trait file**

```bash
rm crates/kylinbot-api/src/peripherals_traits.rs
```

- [ ] **Step 3: Clean src/main.rs**

Remove:
- `mod hardware;` (line 110)
- `mod peripherals;` (line 131)
- `HardwareCommands, PeripheralCommands` from the import at line 164-165
- `Hardware { ... }` command variant (lines ~507-521)
- `Peripheral { ... }` command variant (lines ~524-540)
- The `Commands::Hardware { ... }` match branch (line ~1953)
- The `Commands::Peripheral { ... }` match branch (line ~1957)
- The `#[cfg(feature = "hardware")]` block registering peripheral tools (lines ~1635-1639)
- `hardware_only` field and related code from `OnboardOpt` struct (lines ~261, 742, 766-769, 787, 1198, 1211)
- `OnboardSection::Hardware` and `Section::Hardware` enum variants (lines ~710-711, 767)
- The `peripheral` field in the agent start struct (lines ~301-303, 1417, 1438)
- The `"hardware"` onboard section mapping (lines ~3608, 3673-3675)
- The hardware/peripherals display in `show-config` (lines ~1834-1843)

- [ ] **Step 4: Clean src/lib.rs**

Remove:
- `pub(crate) mod hardware;` (line 64)
- `pub mod peripherals;` (line 81)
- `#[cfg(feature = "hardware")] pub use kylinbot_hardware::*` and related feature-gated re-exports
- The entire `HardwareCommands` enum definition (lines ~530-572)
- The entire `PeripheralCommands` enum definition (lines ~574-619+)
- The stray "hardware sensor alerts" text in the `ChannelCommands::Send` long_about (line ~271): change `hardware sensor alerts, and automation pipelines` to `and automation pipelines`

- [ ] **Step 5: Remove peripherals_traits module from kylinbot-api**

In `crates/kylinbot-api/src/lib.rs`, remove:
- `pub mod peripherals_traits;` module declaration
- Any doc comment referencing `peripherals_traits::Peripheral`

- [ ] **Step 6: Remove CLI Fluent keys**

For each locale directory (`en`, `fr`, `ja`) under `crates/kylinbot-runtime/locales/`, remove from `cli.ftl`:
- `cli-hardware-about` (line ~23)
- `cli-peripheral-about` (line ~24)
- `cli-hardware-discover-about` (line ~93)
- `cli-hardware-introspect-about` (line ~94)
- `cli-hardware-info-about` (line ~95)
- `cli-peripheral-list-about` (line ~97)
- `cli-peripheral-add-about` (line ~98)
- `cli-peripheral-flash-about` (line ~99)
- `cli-hardware-long-about` block (line ~182)
- `cli-peripheral-long-about` block (line ~192)
- The `--peripheral nucleo-f401re:/dev/ttyACM0` example line in the agent long-about string (if present)

- [ ] **Step 7: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 8: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: Success

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "refactor: remove hardware CLI commands and Peripheral trait

Delete src/hardware/, src/peripherals/, the Peripheral trait from
kylinbot-api, and all hardware/peripheral CLI command definitions.
Remove corresponding Fluent i18n keys from all locales."
```

---

### Task 3: Remove hardware tools from kylinbot-tools

**Files:**
- Delete: `crates/kylinbot-tools/src/hardware_board_info.rs`
- Delete: `crates/kylinbot-tools/src/hardware_memory_map.rs`
- Delete: `crates/kylinbot-tools/src/hardware_memory_read.rs`
- Delete: `src/tools/hardware_board_info.rs`
- Delete: `src/tools/hardware_memory_map.rs`
- Delete: `src/tools/hardware_memory_read.rs`
- Modify: `crates/kylinbot-tools/src/lib.rs`
- Modify: `crates/kylinbot-tools/Cargo.toml`
- Modify: `src/tools/mod.rs`
- Modify: `Cargo.toml` (remove `probe` feature — already removed in Task 1, verify)

- [ ] **Step 1: Delete hardware tool files**

```bash
rm crates/kylinbot-tools/src/hardware_board_info.rs
rm crates/kylinbot-tools/src/hardware_memory_map.rs
rm crates/kylinbot-tools/src/hardware_memory_read.rs
rm src/tools/hardware_board_info.rs
rm src/tools/hardware_memory_map.rs
rm src/tools/hardware_memory_read.rs
```

- [ ] **Step 2: Remove module declarations from kylinbot-tools/src/lib.rs**

Remove:
- `pub mod hardware_board_info;`
- `pub mod hardware_memory_map;`
- `pub mod hardware_memory_read;`

- [ ] **Step 3: Remove probe-rs dep and probe feature from kylinbot-tools/Cargo.toml**

Remove:
- `probe-rs = { version = "0.31", optional = true }` dependency
- `probe = ["dep:probe-rs"]` feature

- [ ] **Step 4: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 5: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: Success

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "refactor: remove hardware tools from kylinbot-tools

Delete hardware_board_info, hardware_memory_map, hardware_memory_read
tools and root-crate shims. Remove probe-rs dependency and probe feature."
```

---

### Task 4: Remove runtime hardware integration

**Files:**
- Modify: `crates/kylinbot-runtime/src/tools/mod.rs`
- Modify: `crates/kylinbot-runtime/src/agent/system_prompt.rs`
- Modify: `crates/kylinbot-runtime/src/agent/loop_.rs`
- Modify: `crates/kylinbot-runtime/src/onboard/mod.rs`
- Modify: `crates/kylinbot-runtime/src/sop/types.rs`
- Modify: `crates/kylinbot-runtime/src/sop/dispatch.rs`
- Modify: `crates/kylinbot-runtime/src/sop/engine.rs`
- Modify: `crates/kylinbot-channels/src/orchestrator/acp_server.rs`
- Modify: `src/sop/mod.rs`
- Modify: `crates/kylinbot-runtime/locales/{en,fr,ja}/tools.ftl`

- [ ] **Step 1: Remove hardware tool re-exports from runtime tools/mod.rs**

In `crates/kylinbot-runtime/src/tools/mod.rs`, remove:
- `pub use kylinbot_tools::hardware_board_info::HardwareBoardInfoTool;` (line 67)
- `pub use kylinbot_tools::hardware_memory_map::HardwareMemoryMapTool;` (line 68)
- `pub use kylinbot_tools::hardware_memory_read::HardwareMemoryReadTool;` (line 69)

- [ ] **Step 2: Remove hardware section from system_prompt.rs**

In `crates/kylinbot-runtime/src/agent/system_prompt.rs`, remove the entire `has_hardware` block (lines ~160-176):

```rust
let has_hardware = tools.iter().any(|(name, _)| {
    *name == "gpio_read"
        || *name == "gpio_write"
        || *name == "arduino_upload"
        || *name == "hardware_memory_map"
        || *name == "hardware_board_info"
        || *name == "hardware_memory_read"
        || *name == "hardware_capabilities"
});

if has_hardware {
    // ... entire hardware authorization prompt block
}
```

- [ ] **Step 3: Remove hardware tool registrations from loop_.rs**

In `crates/kylinbot-runtime/src/agent/loop_.rs`, remove:
- Hardware tool name registrations (lines ~2405-2429): `"gpio_read"`, `"gpio_write"`, `"arduino_upload"`, `"hardware_memory_map"`, `"hardware_board_info"`, `"hardware_memory_read"`, `"hardware_capabilities"`
- Hardware tool description injections (lines ~3316-3338): the `"gpio_read"`, `"gpio_write"`, `"arduino_upload"`, `"hardware_memory_map"`, `"hardware_board_info"`, `"hardware_memory_read"`, `"hardware_capabilities"` description push blocks
- The `register_peripheral_tools_fn` function or any reference to it

- [ ] **Step 4: Remove hardware onboarding step from onboard/mod.rs**

In `crates/kylinbot-runtime/src/onboard/mod.rs`:
- Remove `Section::Hardware` variant and `OnboardSection::Hardware` variant
- Remove the `hardware()` async function (lines ~1120-1151)
- Remove calls to `hardware()` from the onboarding flow (lines ~103-104, ~126)
- Remove `section_has_signal` check for `"hardware"` (line ~433): change `"hardware" => cfg.hardware.enabled` — remove this arm
- Remove the test `section_has_signal_hardware_tracks_enabled_flag` (lines ~1296-1301)
- Remove `Hardware` from any match arms or section mappings

- [ ] **Step 5: Remove SopTrigger::Peripheral from sop/types.rs**

In `crates/kylinbot-runtime/src/sop/types.rs`:
- Remove the `SopTrigger::Peripheral { board, signal, condition }` enum variant (lines ~81-86)
- Remove `SopTriggerSource::Peripheral` enum variant (line ~235)
- Remove the `Self::Peripheral` match arm from the `Display` impl for `SopTrigger` (line ~96)
- Remove the `Self::Peripheral` match arm from the `Display` impl for `SopTriggerSource` (line ~245)
- Remove any `Peripheral`-related test cases (e.g., `SopTrigger` parsing tests with `peripheral` type)

- [ ] **Step 6: Remove dispatch_peripheral_signal from sop/dispatch.rs**

In `crates/kylinbot-runtime/src/sop/dispatch.rs`:
- Remove the `dispatch_peripheral_signal()` function (lines ~227-235)
- Remove the peripheral-related tests `peripheral_signal_dispatches_to_matching_sop` (line ~598) and `peripheral_signal_no_match_returns_empty` (line ~618)
- Remove the `peripheral` mention from the module doc comment (line 3)

- [ ] **Step 7: Clean sop/engine.rs**

In `crates/kylinbot-runtime/src/sop/engine.rs`:
- Remove the `SopTrigger::Peripheral { .. }` match arm (line ~695)
- Remove the `SopTriggerSource::Peripheral` evaluation (line ~700)
- Remove the `peripheral_condition_filters_by_payload` test (line ~1266)
- Remove the `peripheral_no_condition_matches_any` test (line ~1297)

- [ ] **Step 8: Clean src/sop/mod.rs**

In `src/sop/mod.rs`:
- Remove the `SopTrigger::Peripheral { .. }` match arm (line ~447)
- Remove the `type = "peripheral"` example TOML / doc comment block (line ~431)

- [ ] **Step 9: Remove hardware tool-name match arms from acp_server.rs**

In `crates/kylinbot-channels/src/orchestrator/acp_server.rs`, remove the three hardware tool-name arms at lines ~622-624:

```rust
| "hardware_board_info"
| "hardware_memory_map"
| "hardware_memory_read"
```

- [ ] **Step 10: Remove tool Fluent keys**

For each locale directory (`en`, `fr`, `ja`) under `crates/kylinbot-runtime/locales/`, remove from `tools.ftl`:
- `tool-hardware-board-info` (line ~52)
- `tool-hardware-memory-map` (line ~54)
- `tool-hardware-memory-read` (line ~56)

- [ ] **Step 11: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 12: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: Success

- [ ] **Step 13: Commit**

```bash
git add -A
git commit -m "refactor: remove runtime hardware integration

Remove hardware tool registrations, system prompt section, onboarding
step, SOP Peripheral trigger type, ACP server hardware tool-name arms,
and tool Fluent keys from all locales."
```

---

### Task 5: Remove hardware and peripherals config schema

**Files:**
- Modify: `crates/kylinbot-config/src/schema.rs`
- Modify: `src/config/mod.rs`

- [ ] **Step 1: Remove hardware/peripherals structs from schema.rs**

In `crates/kylinbot-config/src/schema.rs`, remove:
- `HardwareTransport` enum and its `Display` impl (lines ~793-809)
- `HardwareConfig` struct, its `transport_mode()` method, and its `Default` impl (lines ~816-855)
- `PeripheralsConfig` struct (lines ~2146-2152+)
- `PeripheralBoardConfig` struct and its `Default` impl (lines ~2162-2190)
- `peripherals: PeripheralsConfig` field from `AppConfig` (line ~319)
- `hardware: HardwareConfig` field from `AppConfig` (line ~343)
- `peripherals` and `hardware` fields from all `Default` impls of `AppConfig` and any parent struct (lines ~9467, 9472, 11544, 12165, 12170, 12735, 12740)
- Remove `HardwareTransport` from the test at line ~11544
- Remove `peripherals_config_default_disabled`, `peripherals_config_toml_roundtrip` tests and `PeripheralBoardConfig`-related test fixtures (lines ~16004, 16011, 16019, 16022, 16020-16031)

**Preserve** `WebAuthnConfig` — this is FIDO2 security-key auth, not embedded device code.

- [ ] **Step 2: Remove re-exports from src/config/mod.rs**

In `src/config/mod.rs`, remove from the `pub use schema::{...}` block (lines 12-44):
- `HardwareConfig`
- `HardwareTransport`
- `PeripheralBoardConfig`
- `PeripheralsConfig`

- [ ] **Step 3: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 4: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: Success

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: remove hardware and peripherals config schema

Remove HardwareConfig, HardwareTransport, PeripheralsConfig,
PeripheralBoardConfig from kylinbot-config schema and root crate
re-exports. Preserve WebAuthnConfig (FIDO2 security keys)."
```

---

### Task 6: Remove robot-kit crate

**Files:**
- Delete: `crates/robot-kit/`
- Modify: `Cargo.toml` (workspace member already removed in Task 1 — verify)

- [ ] **Step 1: Delete the robot-kit directory**

```bash
rm -rf crates/robot-kit/
```

- [ ] **Step 2: Verify no references remain**

Run: `grep -rn 'robot.kit\|robot-kit' Cargo.toml crates/ src/`
Expected: Empty (workspace member was removed in Task 1 Step 3)

- [ ] **Step 3: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "refactor: remove robot-kit crate

Delete the robot-kit workspace crate (GPIO, LIDAR, ROS2, vision
toolkit). Workspace member removed in prior commit."
```

---

### Task 7: Remove firmware directory

**Files:**
- Delete: `firmware/`
- Delete: `crates/kylinbot-runtime/firmware` (symlink)
- Delete: `crates/kylinbot-runtime/src/firmware` (broken symlink)

- [ ] **Step 1: Delete the firmware directory**

```bash
rm -rf firmware/
```

- [ ] **Step 2: Delete the dangling symlinks**

```bash
rm crates/kylinbot-runtime/firmware
rm crates/kylinbot-runtime/src/firmware
```

- [ ] **Step 3: Verify no references remain**

Run: `grep -rn 'firmware' crates/kylinbot-runtime/Cargo.toml crates/kylinbot-runtime/src/ --include='*.rs' --include='*.toml'`
Expected: Empty or only non-path references (comments are acceptable)

- [ ] **Step 4: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: remove firmware directory and dangling symlinks

Delete firmware/ and the two symlinks in kylinbot-runtime that pointed
to it."
```

---

### Task 8: Remove gateway embedded-web feature

**Files:**
- Modify: `crates/kylinbot-gateway/Cargo.toml`
- Modify: `crates/kylinbot-gateway/src/static_files.rs`
- Modify: `Cargo.toml` (verify `embedded-web` feature removed — was done in Task 1)

- [ ] **Step 1: Remove embedded-web feature and include_dir dep from gateway Cargo.toml**

In `crates/kylinbot-gateway/Cargo.toml`, remove:
- `include_dir = { version = "0.7", optional = true }` (line 36)
- `embedded-web = ["dep:include_dir"]` feature (line 83)

- [ ] **Step 2: Clean static_files.rs**

In `crates/kylinbot-gateway/src/static_files.rs`, remove all `#[cfg(feature = "embedded-web")]` blocks:
- Lines 15-16: `use include_dir::{Dir, include_dir};`
- Lines 18-19: `static EMBEDDED_WEB_DIST: Dir<'_> = include_dir!("$CARGO_MANIFEST_DIR/../../web/dist");`
- Line 29: `#[cfg(feature = "embedded-web")]` branch
- Line 76: `#[cfg(feature = "embedded-web")]` branch
- Line 127: `#[cfg(feature = "embedded-web")]` section

Keep only the filesystem-based `web_dist_dir` code paths.

- [ ] **Step 3: Verify workspace compiles**

Run: `cargo check --workspace --all-targets`
Expected: Success

- [ ] **Step 4: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: Success

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: remove gateway embedded-web feature

Remove include_dir dependency and embedded-web feature gate from
gateway. Keep only filesystem-based dashboard serving."
```

---

### Task 9: Remove embedded/device scripts, docs, CI, and stale references

**Files:**
- Delete: `scripts/deploy-rpi.sh`
- Delete: `scripts/99-act-led.rules`
- Delete: `scripts/rpi-config.toml`
- Delete: `docs/book/src/hardware/`
- Modify: `.cargo/config.toml`
- Modify: `.github/workflows/release-stable-manual.yml`
- Modify: `install.sh`
- Modify: `release-plz.toml`
- Modify: `AGENTS.md`
- Modify: `docs/book/src/SUMMARY.md`
- Modify: `docs/book/src/api.md`
- Modify: `docs/book/src/architecture/crates.md`
- Modify: `docs/book/src/architecture/overview.md`
- Modify: `docs/book/src/contributing/how-to.md`
- Modify: `docs/book/src/ops/network-deployment.md`
- Modify: `docs/book/src/developing/extension-examples.md`
- Modify: `docs/book/src/introduction.md`
- Modify: `docs/book/src/philosophy.md`
- Modify: `docs/book/src/channels/voice.md`
- Modify: `docs/book/src/sop/syntax.md`
- Modify: `docs/book/src/sop/index.md`
- Modify: `docs/book/src/sop/connectivity.md`
- Modify: `docs/book/src/tools/overview.md`
- Modify: `docs/book/src/security/sandboxing.md`
- Modify: `docs/book/src/setup/linux.md`
- Modify: `docs/book/src/setup/container.md`
- Modify: `docs/book/src/setup/service.md`
- Modify: `docs/book/src/foundations/fnd-001-intentional-architecture.md`
- Modify: `docs/book/src/foundations/fnd-002-documentation-standards.md`
- Modify: `docs/book/src/foundations/fnd-003-governance.md`
- Modify: `docs/book/src/foundations/fnd-006-zero-compromise-in-practice.md`
- Modify: `crates/kylinbot-providers/src/anthropic.rs`
- Modify: `Cargo.toml` (release profile comment)

- [ ] **Step 1: Delete embedded scripts**

```bash
rm scripts/deploy-rpi.sh scripts/99-act-led.rules scripts/rpi-config.toml
```

- [ ] **Step 2: Delete hardware docs directory**

```bash
rm -rf docs/book/src/hardware/
```

- [ ] **Step 3: Remove Android targets from .cargo/config.toml**

In `.cargo/config.toml`, remove:
- Lines 21-27: The `[target.armv7-linux-androideabi]` and `[target.aarch64-linux-android]` sections with their linkers and rustflags

Keep the `x86_64-unknown-linux-musl`, `aarch64-unknown-linux-gnu`, and `aarch64-unknown-linux-musl` sections.

- [ ] **Step 4: Remove ARM/Android entries from release workflow**

In `.github/workflows/release-stable-manual.yml`, remove:
- Matrix entry with `target: arm-unknown-linux-gnueabihf` (line ~216)
- Matrix entry with `target: aarch64-linux-android` (line ~224)
- The ARMv6 codegen-forcing branch keyed on `arm-unknown-linux-gnueabihf` (lines ~263-266)
- The `kylinbot-arm-unknown-linux-gnueabihf.tar.gz` artifact entry (line ~322)
- The `kylinbot-aarch64-linux-android.tar.gz` artifact entry (line ~323)

- [ ] **Step 5: Remove hardware/peripheral/probe tokens from install.sh**

In `install.sh` line 88, change:

```bash
hardware|peripheral-*|sandbox-*|browser-*|probe|rag-pdf|webauthn)
```

to:

```bash
sandbox-*|browser-*|rag-pdf|webauthn)
```

- [ ] **Step 6: Remove entries from release-plz.toml**

In `release-plz.toml`, remove:
- `[[package]] name = "aardvark-sys"` block (lines ~10-11)
- `[[package]] name = "robot-kit"` block (lines ~14-15)

- [ ] **Step 7: Update AGENTS.md**

In `AGENTS.md`:
- Remove `crates/kylinbot-api/src/peripherals_traits.rs (Peripheral)` from the key extension points list (line 35)
- Remove the `kylinbot-hardware | Experimental | USB discovery, peripherals, serial` row from the stability tiers table (line 55)
- Remove `crates/kylinbot-hardware/` from the Repository Map (line 77)
- Change the `docs/` description on line 81 from `docs/ — topic-based documentation (setup-guides, reference, ops, security, hardware, contributing, maintainers)` to `docs/ — topic-based documentation (setup-guides, reference, ops, security, contributing, maintainers)`

- [ ] **Step 8: Remove hardware chapter from SUMMARY.md**

In `docs/book/src/SUMMARY.md`, remove the entire hardware chapter block (lines 74-82):

```markdown
# Hardware

- [Overview](./hardware/index.md)
- [Adding boards & tools](./hardware/adding-boards-and-tools.md)
- [Peripherals design](./hardware/hardware-peripherals-design.md)
- [Arduino Uno Q](./hardware/arduino-uno-q-setup.md)
- [STM32 Nucleo](./hardware/nucleo-setup.md)
- [Android](./hardware/android-setup.md)
- [Aardvark](./hardware/aardvark.md)
```

- [ ] **Step 9: Update primary doc files**

In `docs/book/src/api.md`, remove the `kylinbot-hardware` row (`GPIO / I2C / SPI / USB`) from the crate API table.

In `docs/book/src/architecture/crates.md`, remove the `kylinbot-hardware` section and the `aardvark-sys, robot-kit` section.

In `docs/book/src/architecture/overview.md`, remove the `kylinbot-hardware` row and the `aardvark-sys, robot-kit` row from the crate table.

In `docs/book/src/contributing/how-to.md`, remove the `Hardware | crates/kylinbot-hardware/ — new board support, new sensor drivers` row.

In `docs/book/src/ops/network-deployment.md`, replace the `cargo install --locked --path . --features "hardware peripheral-rpi"` example with the default install command (no embedded features).

In `docs/book/src/developing/extension-examples.md`, remove the `Peripheral` extension example section.

- [ ] **Step 10: Update secondary doc files**

In `docs/book/src/introduction.md`, drop "hardware" from the tools list.

In `docs/book/src/philosophy.md`, reword the sentence mentioning "hardware support" to remove it.

In `docs/book/src/channels/voice.md`, remove the dangling link `See [Hardware → Android](../hardware/android-setup.md) for Android-specific audio setup.`

In `docs/book/src/sop/syntax.md`, remove the `peripheral | board, signal, condition` trigger-type table row.

In `docs/book/src/sop/index.md`, drop "peripherals" / "peripheral" from event-source enumerations.

In `docs/book/src/sop/connectivity.md`, remove `peripheral` from `MQTT/webhook/cron/peripheral events`.

In `docs/book/src/tools/overview.md`, remove the `Hardware probes | --features hardware — GPIO, I2C, SPI reads/writes` row.

In `docs/book/src/security/sandboxing.md`, delete the `## Interaction with the hardware subsystem` section and any `allow_devices = ["/dev/gpiochip0", …]` examples.

In `docs/book/src/setup/linux.md`, remove the `libgpiod-dev` row from the deps table and delete the "On a Raspberry Pi or similar SBC" subsection with hardware feature install commands.

In `docs/book/src/setup/container.md`, remove the "No hardware passthrough by default" bullet (or keep it scoped only to non-hardware USB if the team prefers).

In `docs/book/src/setup/service.md`, remove the `SupplementaryGroups=gpio spi i2c` bullet.

- [ ] **Step 11: Update foundations RFC docs**

For each foundations doc (fnd-001, fnd-002, fnd-003, fnd-006), use strike-through annotations rather than silent rewrites to preserve the historical audit trail:

In `docs/book/src/foundations/fnd-001-intentional-architecture.md`, strike through references to `peripheral-rpi`, `hardware`, `aardvark-sys`, `kylinbot-robot-kit`, and the RPi build matrix. Add a footnote pointing to `2026-05-22-remove-embedded-device-code-design.md`.

In `docs/book/src/foundations/fnd-002-documentation-standards.md`, drop the `docs/book/src/hardware/` row, `Peripheral` trait bullet, `hardware/` directory tree branch, and `hardware` from the `type:` enumeration.

In `docs/book/src/foundations/fnd-003-governance.md`, remove `component:hardware` from the issue-label enumeration.

In `docs/book/src/foundations/fnd-006-zero-compromise-in-practice.md`, strike `peripheral implementation` from the trait enumeration, drop `hardware` from data-source examples, soften the "running on hardware" closing rhetoric.

- [ ] **Step 12: Neutralize hardware comment in anthropic.rs**

In `crates/kylinbot-providers/src/anthropic.rs` line 848, change:

```rust
// to force tool use for hardware requests).
```

to:

```rust
// to force tool use).
```

- [ ] **Step 13: Update release profile comment in root Cargo.toml**

In `Cargo.toml` line 356, change:

```toml
codegen-units = 1    # Serialized codegen for low-memory devices (e.g., Raspberry Pi 3 with 1GB RAM)
```

to:

```toml
codegen-units = 1    # Serialized codegen for smaller binaries
```

- [ ] **Step 14: Regenerate Cargo.lock**

Run: `cargo update`
This removes orphaned transitive deps from the lockfile.

- [ ] **Step 15: Run full verification suite**

Run the comprehensive grep to ensure no stale references remain:

```bash
grep -rn 'kylinbot_hardware\|robot.kit\|aardvark\|firmware/\|Peripheral\b\|hardware_board_info\|hardware_memory_map\|hardware_memory_read\|gpio_read\|gpio_write\|arduino_upload\|PeripheralsConfig\|HardwareConfig\|HardwareTransport\|PeripheralBoardConfig\|dispatch_peripheral\|SopTrigger::Peripheral\|SopTriggerSource::Peripheral\|embedded-web\|peripheral-rpi\|probe-rs\|aarch64-linux-android\|armv7-linux-androideabi\|arm-unknown-linux-gnueabihf\|cli-hardware\|cli-peripheral\|tool-hardware\|libgpiod\|/dev/gpiochip\|/dev/i2c\|/dev/spidev' src/ crates/ scripts/ install.sh release-plz.toml .github/workflows/ .cargo/ docs/book/src/
```

Expected: Empty (excluding WebAuthn security-key hits, this design doc, and historical RFC strike-throughs in `fnd-00*.md`).

- [ ] **Step 16: Run full test suite**

Run: `cargo test --workspace`
Expected: All tests pass

- [ ] **Step 17: Smoke-test CLI surface**

Run: `cargo run -- --help 2>&1 | grep -iE 'hardware|peripheral'`
Expected: Empty (no hardware/peripheral commands in help output)

- [ ] **Step 18: Verify dangling symlinks removed**

Run: `ls crates/kylinbot-runtime/firmware crates/kylinbot-runtime/src/firmware 2>/dev/null`
Expected: No output (both removed)

- [ ] **Step 19: Verify dep:which still works**

Run: `cargo build --no-default-features --features agent-runtime`
Expected: Success (catches accidental `dep:which` removal)

- [ ] **Step 20: Commit**

```bash
git add -A
git commit -m "chore: remove embedded/device scripts, docs, CI, and stale references

Delete RPi scripts, hardware docs, Android cargo targets, ARM/Android
release matrix entries, install.sh hardware tokens, release-plz entries.
Update all documentation to remove embedded references. Regenerate
Cargo.lock."
```

---

## Self-Review Checklist

1. **Spec coverage:** Each section of the design spec maps to a task:
   - Whole-directory deletions → Tasks 1, 6, 7, 9
   - Gateway hardware_context.rs → Task 1 (paired with crate removal)
   - CLI commands + Peripheral trait → Task 2
   - Hardware tools → Task 3
   - Runtime integration → Task 4
   - Config schema → Task 5
   - Robot-kit → Task 6
   - Firmware → Task 7
   - Gateway embedded-web → Task 8
   - Scripts/docs/CI → Task 9
   - All modified files listed in spec → covered

2. **Placeholder scan:** No TBD, TODO, "implement later", "add validation", "similar to Task N", or placeholder steps found.

3. **Type consistency:**
   - `SopTrigger::Peripheral` removed in Task 4 (types.rs, dispatch.rs, engine.rs, src/sop/mod.rs) — all references cleaned together
   - `HardwareConfig`/`HardwareTransport`/`PeripheralsConfig`/`PeripheralBoardConfig` removed in Task 5 (schema.rs + src/config/mod.rs re-exports) — paired in same commit
   - `HardwareBoardInfoTool`/`HardwareMemoryMapTool`/`HardwareMemoryReadTool` removed in Task 3 (tools crate) + Task 4 (runtime re-exports) — runtime re-exports cleaned before tool impls are referenced
   - `HardwareCommands`/`PeripheralCommands` removed in Task 2 (src/lib.rs + src/main.rs) — paired
