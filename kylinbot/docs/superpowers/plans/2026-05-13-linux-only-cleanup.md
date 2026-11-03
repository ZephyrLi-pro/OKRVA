# Linux-Only Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove all Windows and macOS support from KylinBot — code, CI matrices, packaging, docs — keeping the workspace compiling, clippy-clean, and tests passing at every commit.

**Architecture:** Six atomic commits in one PR. Order chosen so each commit leaves a buildable workspace: first delete the iMessage channel (cross-crate but self-contained), then macOS-only bits of Tauri, then sweep `cfg(target_os = ...)` gates across kernel crates, then drop CI matrices and Cargo target configs, then strip Windows/macOS docs and `setup.bat`, then a final validation pass.

**Tech Stack:** Rust workspace (cargo), GitHub Actions workflows (YAML), mdBook docs (Markdown), shell installers (POSIX sh + Windows batch — being removed).

**Spec:** `docs/superpowers/specs/2026-05-13-linux-only-cleanup-design.md`

---

## Task 1: Drop iMessage channel and macOS AppleScript bridge

**Files:**
- Delete: `crates/kylinbot-channels/src/imessage.rs` (1319 lines)
- Delete: `src/channels/imessage.rs` (1 line, re-export shim)
- Modify: `crates/kylinbot-channels/src/lib.rs` (lines 27–28 — `mod imessage` declaration)
- Modify: `crates/kylinbot-channels/src/orchestrator/mod.rs` (line 3929 — `cfg!(target_os = "macos")` block in `maybe_restart_managed_daemon_service`)
- Modify: `crates/kylinbot-config/src/schema.rs` (lines 6608–6610 — `imessage` field; ~7294–7313 — `IMessageConfig` type and its `ChannelConfig` impl; lines 6773–6774 region — `ConfigWrapper` entry; line 6893 — `imessage: None` default)
- Modify: `crates/kylinbot-runtime/src/integrations/registry.rs` (lines 84–95 — iMessage `IntegrationEntry`; lines 813, 901–903 — test imports/cases referencing `IMessageConfig`)
- Modify: `crates/kylinbot-runtime/src/integrations/mod.rs` (lines 132–136 — iMessage "Setup (macOS only)" branch)
- Modify: `crates/kylinbot-gateway/src/lib.rs` (line 835 — help text; line 1767 — doc comment on `/linq` route)
- Modify: `Cargo.toml` (line 257 — remove `"channel-imessage"` from default features list; line 286 — delete `channel-imessage = [...]` feature definition)
- Modify: `crates/kylinbot-channels/Cargo.toml` (line 82 — remove `"channel-imessage"` from default features list; line 101 — delete `channel-imessage = []` feature definition)
- Modify: `src/lib.rs` (lines 218, 224 — drop `imessage` from "supported channel types" doc strings)
- Modify: `src/main.rs` (line 474 — same doc string cleanup)

- [ ] **Step 1: Inventory test references to iMessage**

Run: `grep -rn 'imessage\|IMessage' tests/ crates/*/tests/ 2>/dev/null`
Expected: any test file mentioning iMessage needs the same treatment as production code (deletion or pruning). Note the list before proceeding.

- [ ] **Step 2: Delete the iMessage channel source files**

```bash
rm crates/kylinbot-channels/src/imessage.rs
rm src/channels/imessage.rs
```

- [ ] **Step 3: Remove the `mod imessage` declaration from `crates/kylinbot-channels/src/lib.rs`**

Open `crates/kylinbot-channels/src/lib.rs` and delete lines 27–28:

```rust
#[cfg(feature = "channel-imessage")]
pub mod imessage;
```

- [ ] **Step 4: Drop iMessage from `Cargo.toml` features (root)**

In `Cargo.toml`:
- Line 257: remove the string `"channel-imessage",` from the default features array (preserve surrounding commas/spacing).
- Line 286: delete the entire line `channel-imessage = ["kylinbot-channels/channel-imessage"]`.

- [ ] **Step 5: Drop iMessage from `crates/kylinbot-channels/Cargo.toml`**

In `crates/kylinbot-channels/Cargo.toml`:
- Line 82: remove `"channel-imessage",` from the default features array.
- Line 101: delete the line `channel-imessage = []`.

- [ ] **Step 6: Remove the `IMessageConfig` type and `imessage` field from the config schema**

In `crates/kylinbot-config/src/schema.rs`:
- Around lines 6608–6610, delete:
  ```rust
  /// iMessage channel configuration (macOS only).
  #[nested]
  pub imessage: Option<IMessageConfig>,
  ```
- Around line 6773–6774, delete the `ConfigWrapper` entry for `self.imessage` and the `self.imessage.is_some()` argument (it is one row in a list of nested-config wrappers). Match it by `self.imessage`.
- Around line 6893, delete `imessage: None,` from the `Default` impl.
- Around lines 7294–7313, delete the entire `IMessageConfig` struct definition, including the doc comment, derives, `#[prefix = "channels.imessage"]`, the struct body, and the `impl ChannelConfig for IMessageConfig` block immediately after.

- [ ] **Step 7: Remove iMessage from the integrations registry**

In `crates/kylinbot-runtime/src/integrations/registry.rs`:
- Delete lines 84–95 (the `IntegrationEntry { name: "iMessage", ... }` block — the entire entry including its trailing comma).
- Around line 813, in the test imports `use kylinbot_config::schema::{IMessageConfig, MatrixConfig, ...}`, remove `IMessageConfig,` from the brace list.
- Around lines 901–905, delete the test function `fn imessage_active_when_configured()` and its body. Also delete the body line `config.channels.imessage = Some(IMessageConfig { ... });` if it appears in other test functions — verify with `grep -n 'imessage\|IMessage' crates/kylinbot-runtime/src/integrations/registry.rs` and remove every occurrence.

- [ ] **Step 8: Remove the iMessage setup branch from `integrations/mod.rs`**

In `crates/kylinbot-runtime/src/integrations/mod.rs`, delete lines 132–136:

```rust
"iMessage" => {
    println!("  Setup (macOS only):");
    println!("    Uses AppleScript bridge to send/receive iMessages.");
    println!("    Requires Full Disk Access in System Settings → Privacy.");
}
```

- [ ] **Step 9: Update gateway `/linq` route description**

In `crates/kylinbot-gateway/src/lib.rs`:
- Line 835: change `"  POST {pfx}/linq      — Linq message webhook (iMessage/RCS/SMS)"` to `"  POST {pfx}/linq      — Linq message webhook (RCS/SMS)"`.
- Line 1767: change the doc comment `/// POST /linq — incoming message webhook (iMessage/RCS/SMS via Linq)` to `/// POST /linq — incoming message webhook (RCS/SMS via Linq)`.

- [ ] **Step 10: Strip iMessage from the orchestrator macOS branch**

In `crates/kylinbot-channels/src/orchestrator/mod.rs`, replace the `maybe_restart_managed_daemon_service` function body. Locate the function starting around line 3928:

```rust
fn maybe_restart_managed_daemon_service() -> Result<bool> {
    if cfg!(target_os = "macos") {
        // ... launchctl plist handling ...
    }

    if cfg!(target_os = "linux") {
        // ... OpenRC / systemd handling ...
    }
    // ... fall-through ...
}
```

Delete the entire `if cfg!(target_os = "macos") { ... }` block (lines 3929 through the matching closing `}` near line 3963, immediately before the `if cfg!(target_os = "linux")` block). Then unwrap the Linux branch — change `if cfg!(target_os = "linux") {` to remove the gate and unindent its body. The macOS-only `directories::UserDirs` import in this function is shared with the Linux branch — leave that intact.

- [ ] **Step 11: Drop `imessage` from the channel-types doc strings**

- `src/lib.rs` line 218: change `Supported types: telegram, discord, slack, whatsapp, matrix, imessage, email.` → `Supported types: telegram, discord, slack, whatsapp, matrix, email.`
- `src/lib.rs` line 224: same string in a doc comment — same edit.
- `src/main.rs` line 474: same edit.

- [ ] **Step 12: Run cargo check across the workspace**

Run: `cargo check --workspace --all-targets`
Expected: green. If errors mention an undeleted iMessage reference (e.g. `kylinbot_runtime::integrations::registry` test data, or another channel file importing `IMessageConfig`), grep for the symbol and remove it. Common stragglers: macros that expand to all channel variants, test fixtures, snapshot files under `crates/*/tests/`.

- [ ] **Step 13: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: green. Fix any "unused import" warnings from imports that became dead after the deletion.

- [ ] **Step 14: Run tests**

Run: `cargo test --workspace`
Expected: green.

- [ ] **Step 15: Commit**

```bash
git add -A
git commit -m "refactor: drop iMessage channel and macOS AppleScript bridge"
```

---

## Task 2: Drop Tauri macOS-only deps and code paths

**Files:**
- Modify: `apps/tauri/Cargo.toml` (lines 22–25 — `[target.'cfg(target_os = "macos")'.dependencies]` block)
- Modify: `apps/tauri/src/lib.rs` (line 63 area — top-level `#[cfg(target_os = "macos")]` block; line 105 area — nested macOS block inside the runner)

- [ ] **Step 1: Read `apps/tauri/Cargo.toml` to confirm the macOS dependency block boundaries**

Run: `cat apps/tauri/Cargo.toml`
Confirm the block to delete:

```toml
[target.'cfg(target_os = "macos")'.dependencies]
objc2 = "0.6"
objc2-app-kit = { version = "0.3", features = ["NSApplication", "NSImage", "NSRunningApplication"] }
objc2-foundation = { version = "0.3", features = ["NSData"] }
```

- [ ] **Step 2: Delete the macOS target dependency block in `apps/tauri/Cargo.toml`**

Remove the four lines shown in Step 1, plus the blank line immediately preceding the block if it leaves a double-blank.

- [ ] **Step 3: Read `apps/tauri/src/lib.rs` lines 50–120 to map both macOS blocks**

Run: `sed -n '50,120p' apps/tauri/src/lib.rs` (use `Read` tool, not `sed`; this command is illustrative)
Identify:
- The top-level `#[cfg(target_os = "macos")]` block starting near line 63 — this is a standalone `fn` definition.
- The inner `#[cfg(target_os = "macos")]` block near line 105 inside the runner setup — this is a single conditional statement or block call to the macOS helper.

- [ ] **Step 4: Delete the standalone macOS helper function near line 63**

Delete the entire `#[cfg(target_os = "macos")] fn ... { ... }` block including its attribute and braces.

- [ ] **Step 5: Delete the inline macOS call site near line 105**

Delete the `#[cfg(target_os = "macos")] { ... }` block (or single statement) inside the runner. Its purpose was to invoke the helper deleted in Step 4 — no replacement needed.

- [ ] **Step 6: Run cargo check on the workspace**

Run: `cargo check --workspace --all-targets`
Expected: green. Tauri builds on Linux; the deleted macOS code was already excluded from Linux builds.

- [ ] **Step 7: Run clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: green.

- [ ] **Step 8: Run tests**

Run: `cargo test --workspace`
Expected: green.

- [ ] **Step 9: Commit**

```bash
git add apps/tauri/Cargo.toml apps/tauri/src/lib.rs
git commit -m "refactor: drop Tauri macOS-only deps and code paths"
```

---

## Task 3: Simplify cfg gates to Linux-only across kernel crates

**Strategy:** This is the largest commit by line count but the simplest by pattern. For every `#[cfg(target_os = "macos"/"windows")]` block: delete it. For every `#[cfg(not(target_os = "windows"))]` block: delete the attribute, keep the code. For every `#[cfg(any(target_os = "linux", target_os = "macos", target_os = "windows"))]` gate: delete the attribute, keep the code (Linux is in the `any` list). For every runtime `if cfg!(target_os = "macos") { ... } else if cfg!(target_os = "windows") { ... } else { /* linux */ ... }` chain: keep the Linux branch's body, delete the rest, unindent.

**Files (by crate):**
- `crates/kylinbot-hardware/src/lib.rs` (lines 14, 20, 418, 510, 536, 573)
- `crates/kylinbot-hardware/src/discover.rs` (line 7)
- `crates/kylinbot-hardware/src/serial.rs` (lines 224, 232, 235, 264, 266, 268, 287, 289, 291)
- `crates/kylinbot-hardware/src/uf2.rs` (lines 217, 221)
- `crates/kylinbot-hardware/src/peripherals/arduino_flash.rs` (lines 30, 59)
- `crates/kylinbot-runtime/src/service/mod.rs` (~20 sites — see Step 1 enumeration below)
- `crates/kylinbot-runtime/src/security/mod.rs` (line 42)
- `crates/kylinbot-runtime/src/security/detect.rs` (lines 53, 73, 117)
- `crates/kylinbot-runtime/src/tools/shell.rs` (lines 18, 25)
- `crates/kylinbot-runtime/src/tools/file_read.rs` (line 346)
- `crates/kylinbot-runtime/src/skills/symlink_tests.rs` (line 55)
- `crates/kylinbot-tools/src/browser.rs` (lines 247, 344, 346, 454, 2649, 2655)
- `crates/kylinbot-tools/src/browser_open.rs` (lines 128, 193, 227)
- `crates/kylinbot-tools/src/screenshot.rs` (lines 30, 95, 300)
- `crates/kylinbot-tools/src/cli_discovery.rs` (lines 182, 184)
- `crates/kylinbot-tools/src/pdf_read.rs` (line 302)
- `crates/kylinbot-tools/src/node_capabilities.rs` (lines 147, 163, 168, 172)
- `crates/kylinbot-tools/src/claude_code.rs` (line 188)
- `crates/kylinbot-tools/src/codex_cli.rs` (line 149)
- `crates/kylinbot-tools/src/gemini_cli.rs` (line 149)
- `crates/kylinbot-config/src/policy.rs` (lines 183, 220, 257, 282, 318, 322, 741, 786, 822)
- `crates/kylinbot-config/src/secrets.rs` (line 192)
- `crates/kylinbot-config/src/schema.rs` (line 11372)
- `crates/kylinbot-config/src/platform/native.rs` (lines 48, 55)
- `crates/kylinbot-gateway/build.rs` (line 34)
- `src/main.rs` (lines 79, 85, 1584)
- `src/hardware/mod.rs` (lines 20, 31)
- `src/tunnel/mod.rs` (line 275)
- `src/commands/update.rs`

**Sub-task 3a: `kylinbot-hardware` crate**

- [ ] **Step 1: Read `crates/kylinbot-hardware/src/lib.rs` to confirm gate structure**

Use `Read` on the file. Find the six `cfg` sites listed above. Each is one of:
  - A module-level `#![cfg(any(target_os = "linux", target_os = "macos", target_os = "windows"))]` or `#[cfg(...)]` on a `mod` — delete the attribute (the contained code becomes unconditional Linux code).
  - A function/block gate — same treatment.

- [ ] **Step 2: Simplify gates in `crates/kylinbot-hardware/src/lib.rs`**

For each of lines 14, 20, 418, 510, 536, 573:
- If the line is `#[cfg(any(target_os = "linux", target_os = "macos", target_os = "windows"))]` → delete the line.
- If it is `#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]` → delete the line **and** the immediately following item (function/module/block) — that body is the non-supported-platform stub.

- [ ] **Step 3: Simplify `crates/kylinbot-hardware/src/discover.rs`**

Line 7: `#![cfg(any(target_os = "linux", target_os = "macos", target_os = "windows"))]` — delete this line. The whole module is now unconditionally compiled.

- [ ] **Step 4: Simplify `crates/kylinbot-hardware/src/serial.rs`**

The file has three repetitions of the same pattern around lines 224–235, 264–268, 287–291:

```rust
#[cfg(target_os = "macos")]
// macOS body
#[cfg(target_os = "windows")]
// windows body
#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
// fallback body
```

Within each function, there is an additional (implicit or explicit) Linux branch. For each cluster: delete the macOS, windows, and fallback bodies along with their `#[cfg]` attributes. Keep only the Linux body and remove its `#[cfg(target_os = "linux")]` attribute (if present) so the code becomes unconditional. If a Linux block is missing — re-read the function and identify the default-case body; sometimes the Linux body is unguarded.

- [ ] **Step 5: Simplify `crates/kylinbot-hardware/src/uf2.rs`**

Lines 217 and 221:
- Line 217: `#[cfg(target_os = "macos")]` — delete the attribute **and** the immediately following item (function or block).
- Line 221: `#[cfg(not(any(target_os = "macos", target_os = "linux")))]` — delete the attribute and following item (this is the unsupported-platform fallback).
- If there is a `#[cfg(target_os = "linux")]` between them, remove its attribute, keep its body unconditional.

- [ ] **Step 6: Simplify `crates/kylinbot-hardware/src/peripherals/arduino_flash.rs`**

Lines 30 and 59 — same pattern as Step 5. Delete macOS and unsupported-fallback bodies; keep Linux body unconditional.

- [ ] **Step 7: Compile and lint the hardware crate alone**

Run: `cargo check -p kylinbot-hardware --all-targets && cargo clippy -p kylinbot-hardware --all-targets -- -D warnings`
Expected: green.

- [ ] **Step 8: Commit progress checkpoint** (optional — squash into final Task 3 commit at the end)

Skip; the commit happens at end of Task 3.

**Sub-task 3b: `kylinbot-runtime` crate**

- [ ] **Step 9: Re-grep the runtime crate to confirm current cfg sites**

Run: `grep -rn 'target_os = "macos"\|target_os = "windows"\|cfg(windows)' crates/kylinbot-runtime/src/`
Use the output as the authoritative list (file:line) — if it differs from the file list above, the spec line numbers shifted during earlier tasks. Use the live grep output.

- [ ] **Step 10: Simplify `crates/kylinbot-runtime/src/service/mod.rs`**

This file has the most sites (~20). Strategy:
- For every `if cfg!(target_os = "macos") { /* mac body */ } else if cfg!(target_os = "windows") { /* win body */ } else { /* linux body */ }` chain → replace the whole chain with the linux body, unindented.
- For every `if cfg!(target_os = "macos") { /* mac body */ } else { /* linux body */ }` → replace with linux body.
- For every standalone `if cfg!(target_os = "macos") { /* body */ return; }` early-return → delete the block (the post-return code is the Linux path).
- For every `#[cfg(not(target_os = "windows"))] fn foo() { ... }` → delete the attribute, keep the function unconditional.
- For every `#[cfg(target_os = "windows")] fn foo() { ... }` → delete the attribute **and** the function.

Walk each of these line ranges (use `Read` on a ±10 line window around each): 94–105, 129–135, 142–160, 186–201, 228, 240, 266, 286, 338, 345, 516, 531, 1398–1422, 1435, 1443, 1659, 1667. After each edit, the function's behavior is what the Linux branch did before.

- [ ] **Step 11: Simplify `crates/kylinbot-runtime/src/security/mod.rs`**

Line 42: `#[cfg(target_os = "macos")]` — delete the attribute and the following item (function/use/block).

- [ ] **Step 12: Simplify `crates/kylinbot-runtime/src/security/detect.rs`**

- Line 53: `#[cfg(any(target_os = "linux", target_os = "macos"))]` — delete the attribute, keep the body unconditional. (Linux is in the `any` list.)
- Line 73: `#[cfg(target_os = "macos")]` — delete the attribute and the following item.
- Line 117: `#[cfg(target_os = "macos")]` — same.

- [ ] **Step 13: Simplify `crates/kylinbot-runtime/src/tools/shell.rs`**

Lines 18 and 25:
- Line 18: `#[cfg(not(target_os = "windows"))]` — delete the attribute, the body is now unconditional.
- Line 25: `#[cfg(target_os = "windows")]` — delete the attribute and the following item.

- [ ] **Step 14: Simplify `crates/kylinbot-runtime/src/tools/file_read.rs`**

Line 346: `#[cfg(windows)]` — delete the attribute and the following item.

- [ ] **Step 15: Simplify `crates/kylinbot-runtime/src/skills/symlink_tests.rs`**

Line 55: `#[cfg(windows)]` — delete the attribute and the following test (or test arm).

- [ ] **Step 16: Compile and lint the runtime crate**

Run: `cargo check -p kylinbot-runtime --all-targets && cargo clippy -p kylinbot-runtime --all-targets -- -D warnings`
Expected: green.

**Sub-task 3c: `kylinbot-tools` crate**

- [ ] **Step 17: Re-grep tools crate cfg sites**

Run: `grep -rn 'target_os = "macos"\|target_os = "windows"\|cfg(windows)' crates/kylinbot-tools/src/`

- [ ] **Step 18: Simplify `crates/kylinbot-tools/src/browser.rs`**

Lines 247, 344, 346, 454, 2649, 2655 — these are runtime `if cfg!(target_os = "windows") { ... } else { ... }` chains selecting binary names or path separators. Replace each chain with the Linux/non-Windows arm (no `.exe` suffix; forward slashes; etc.). For the lines that are `#[cfg(target_os = "windows")]` / `#[cfg(not(target_os = "windows"))]` attribute pairs (344/346): delete the Windows-guarded item and unguard the non-Windows one.

- [ ] **Step 19: Simplify `crates/kylinbot-tools/src/browser_open.rs`**

- Lines 128 and 193: `#[cfg(target_os = "macos")]` and `#[cfg(target_os = "windows")]` — delete the attributes and the following items.
- Line 227: `#[cfg(not(any(target_os = "macos", target_os = "linux", target_os = "windows")))]` — delete the attribute and following item (unsupported-fallback).
- Find the Linux-guarded function in this file; remove its `#[cfg]` so it becomes the only implementation.

- [ ] **Step 20: Simplify `crates/kylinbot-tools/src/screenshot.rs`**

- Lines 30 and 95: `if cfg!(target_os = "macos")` runtime branches selecting macOS screencapture command — delete the macOS arms, keep the Linux arms.
- Line 300: `#[cfg(any(target_os = "macos", target_os = "linux"))]` — delete the attribute, body becomes unconditional.

- [ ] **Step 21: Simplify `crates/kylinbot-tools/src/cli_discovery.rs`**

Lines 182 and 184:
- Line 182: `#[cfg(target_os = "windows")]` — delete attribute and item.
- Line 184: `#[cfg(not(target_os = "windows"))]` — delete attribute, keep body.

- [ ] **Step 22: Simplify `crates/kylinbot-tools/src/pdf_read.rs`**

Line 302: `#[cfg(windows)]` — delete attribute and item.

- [ ] **Step 23: Simplify `crates/kylinbot-tools/src/node_capabilities.rs`**

- Lines 147 and 163: `#[cfg(target_os = "macos")]` and `#[cfg(target_os = "windows")]` — delete attributes and items.
- Lines 168–172: `#[cfg(not(any(target_os = "macos", target_os = "windows", ...)))]` — read the full attribute (multi-line), delete attribute, keep body unconditional. Or, if the gate excludes Linux, delete attribute and item.

- [ ] **Step 24: Simplify `claude_code.rs`, `codex_cli.rs`, `gemini_cli.rs`**

Each has a single line of the form:
```rust
let bin = if cfg!(target_os = "windows") { "foo.exe" } else { "foo" };
```
Replace with: `let bin = "foo";`
- `crates/kylinbot-tools/src/claude_code.rs:188` → `let claude_bin = "claude";` (confirm the literal string by reading the line first).
- `crates/kylinbot-tools/src/codex_cli.rs:149` → `let codex_bin = "codex";`
- `crates/kylinbot-tools/src/gemini_cli.rs:149` → `let gemini_bin = "gemini";`

- [ ] **Step 25: Compile and lint the tools crate**

Run: `cargo check -p kylinbot-tools --all-targets && cargo clippy -p kylinbot-tools --all-targets -- -D warnings`
Expected: green.

**Sub-task 3d: `kylinbot-config` crate**

- [ ] **Step 26: Re-grep config crate**

Run: `grep -rn 'target_os = "macos"\|target_os = "windows"\|cfg(windows)' crates/kylinbot-config/src/`

- [ ] **Step 27: Simplify `crates/kylinbot-config/src/policy.rs`**

Lines 183, 220, 257, 282, 318, 322, 741, 786, 822 — all Windows or `not(windows)` gates:
- `#[cfg(not(target_os = "windows"))]` (lines 183, 257, 318) → delete attributes, keep bodies unconditional.
- `#[cfg(target_os = "windows")]` (lines 220, 282, 322, 822) → delete attributes **and** following items.
- `cfg!(target_os = "windows")` runtime checks (lines 741, 786) → these are `if cfg!(target_os = "windows") || other_cond` style expressions or branches. Replace `cfg!(target_os = "windows")` with `false`, then simplify the boolean expression. For `if cfg!(target_os = "windows") { ... } else { /* linux */ ... }` → keep the else body.

- [ ] **Step 28: Simplify `crates/kylinbot-config/src/secrets.rs`**

Line 192: `#[cfg(windows)]` — delete attribute and following item.

- [ ] **Step 29: Simplify `crates/kylinbot-config/src/schema.rs`**

Line 11372: `#[cfg(windows)]` — delete attribute and following item.

- [ ] **Step 30: Simplify `crates/kylinbot-config/src/platform/native.rs`**

- Line 48: `#[cfg(not(target_os = "windows"))]` — delete attribute, keep body unconditional.
- Line 55: `#[cfg(target_os = "windows")]` — delete attribute and following item.

- [ ] **Step 31: Compile and lint the config crate**

Run: `cargo check -p kylinbot-config --all-targets && cargo clippy -p kylinbot-config --all-targets -- -D warnings`
Expected: green.

**Sub-task 3e: gateway build script + root crate sources**

- [ ] **Step 32: Simplify `crates/kylinbot-gateway/build.rs`**

Line 34: `let npm = if cfg!(target_os = "windows") { "npm.cmd" } else { "npm" };` → replace with `let npm = "npm";`.

- [ ] **Step 33: Simplify `src/main.rs`**

- Line 79: `#[cfg(windows)]` — delete attribute and item (likely an inner block inside `main`).
- Line 85: `#[cfg(windows)]` — same.
- Line 1584: `if cfg!(target_os = "macos") { ... } else { ... }` (inside a doctor/diagnostics path probably) — read ±10 lines context, then keep the non-macOS branch.

- [ ] **Step 34: Simplify `src/hardware/mod.rs`**

Lines 20 and 31:
- Line 20: `#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]` → delete attribute and following item (this is the unsupported-platform stub).
- Line 31: `#[cfg(any(target_os = "linux", target_os = "macos", target_os = "windows"))]` → delete attribute, keep body unconditional.

- [ ] **Step 35: Simplify `src/tunnel/mod.rs`**

Line 275: `let mut cmd = if cfg!(target_os = "windows") { Command::new("cmd"); ... } else { Command::new("sh"); ... };` — replace with the non-Windows branch directly: `let mut cmd = Command::new("sh"); cmd.arg("-c").arg(...);` (read the exact arms first).

- [ ] **Step 36: Simplify `src/commands/update.rs`**

Re-grep to find the exact line: `grep -n 'target_os = "macos"\|target_os = "windows"\|cfg(windows)' src/commands/update.rs`. Apply the same rules: delete macOS/Windows gates and bodies, keep Linux/non-Windows code unconditional.

- [ ] **Step 37: Workspace check**

Run: `cargo check --workspace --all-targets`
Expected: green. If a deleted `#[cfg(target_os = "macos")] fn foo` was referenced from a Linux call site, the compiler will say "function `foo` not found" — investigate and either restore the function as unconditional Linux code (if it was actually needed) or delete the caller.

- [ ] **Step 38: Workspace clippy**

Run: `cargo clippy --workspace --all-targets -- -D warnings`
Expected: green. Common cleanup at this stage: unused `use` imports for `directories::UserDirs` / `std::process::Command` that became unused after macOS deletions; dead helper functions only used from deleted branches.

- [ ] **Step 39: Workspace tests**

Run: `cargo test --workspace`
Expected: green.

- [ ] **Step 40: Commit Task 3 as one atomic commit**

```bash
git add -A
git commit -m "refactor: simplify cfg gates to Linux-only across kernel crates"
```

---

## Task 4: Remove Windows/macOS CI matrices and publish workflows

**Files:**
- Delete: `.github/workflows/cross-platform-build-manual.yml`
- Delete: `.github/workflows/pub-scoop.yml`
- Delete: `.github/workflows/pub-homebrew-core.yml`
- Modify: `.github/workflows/ci.yml` (remove macOS and Windows runners from build matrix)
- Modify: `.github/workflows/release-stable-manual.yml` (remove `aarch64-apple-darwin`, `x86_64-apple-darwin`, `x86_64-pc-windows-msvc` target entries)
- Modify: `.cargo/config.toml` (delete the three Windows `[target.*]` blocks at lines 22–28)

- [ ] **Step 1: Delete the three standalone workflow files**

```bash
rm .github/workflows/cross-platform-build-manual.yml
rm .github/workflows/pub-scoop.yml
rm .github/workflows/pub-homebrew-core.yml
```

- [ ] **Step 2: Read `.github/workflows/ci.yml` to find the build matrix**

Use `Read` on the file. Locate any `matrix:` block with an `os:` (or `runner:` / `target:`) list containing `macos-*` or `windows-*` entries.

- [ ] **Step 3: Edit `.github/workflows/ci.yml` to drop non-Linux matrix entries**

In each matrix that mentions macOS/Windows:
- Remove `macos-*` and `windows-*` entries from the `os:` list (leave only Linux runners: `ubuntu-*`).
- Remove any `include:` / `exclude:` entries keyed on macOS or Windows runners.
- If a step inside the job uses `if: matrix.os == 'macos-...'` or `if: runner.os == 'macOS'`, delete the whole step.
- If a step uses `if: runner.os == 'Linux'`, the conditional is now trivially true — remove the `if:` line.

- [ ] **Step 4: Read `.github/workflows/release-stable-manual.yml` to find target list**

Locate the matrix or list of cross-compile targets. Look for `aarch64-apple-darwin`, `x86_64-apple-darwin`, `x86_64-pc-windows-msvc`, `x86_64-pc-windows-gnu`, `i686-pc-windows-msvc`.

- [ ] **Step 5: Edit `.github/workflows/release-stable-manual.yml` to drop non-Linux targets**

Remove those target entries from the matrix. Drop any `include:` entries that pair a non-Linux target with a non-Linux runner. Keep only Linux targets (`x86_64-unknown-linux-musl`, `x86_64-unknown-linux-gnu`, `aarch64-unknown-linux-gnu`, `armv7-unknown-linux-gnueabihf`, etc.).

- [ ] **Step 6: Edit `.cargo/config.toml` to drop Windows target blocks**

Open `.cargo/config.toml`. Delete the three blocks at lines 22–28:

```toml
[target.x86_64-pc-windows-msvc]
# ... settings ...
[target.i686-pc-windows-msvc]
# ... settings ...
[target.x86_64-pc-windows-gnu]
# ... settings ...
```

(There may be `rustflags` or `linker` lines under each header — delete the whole stanza including those.)

- [ ] **Step 7: YAML sanity check**

Run: `python3 -c "import yaml, glob; [yaml.safe_load(open(f)) for f in glob.glob('.github/workflows/*.yml')]"`
Expected: no exceptions. If `yaml` is not installed, run `cat .github/workflows/ci.yml | head -50` and `cat .github/workflows/release-stable-manual.yml | head -50` and visually confirm no orphan list entries (e.g., a trailing `-` with nothing after it).

- [ ] **Step 8: Workspace check still green (no code touched, but sanity)**

Run: `cargo check --workspace --all-targets`
Expected: green.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "chore: remove Windows/macOS CI matrices and publish workflows"
```

---

## Task 5: Remove setup.bat and Windows/macOS setup docs

**Files:**
- Delete: `setup.bat`
- Delete: `docs/book/src/setup/macos.md`
- Delete: `docs/book/src/setup/windows.md`
- Modify: `docs/book/src/SUMMARY.md` (lines 15–16 — drop `macos.md` and `windows.md` entries)
- Modify: `docs/book/src/introduction.md` (line 15 — drop `[macOS](./setup/macos.md)` and `[Windows](./setup/windows.md)` links from the "Installing on a specific platform" line)
- Modify: `docs/book/src/getting-started/quick-start.md` (line 23 — drop the `setup.bat` / Setup → Windows reference)
- Modify: `docs/book/src/ops/troubleshooting.md` (line 23 — drop `setup.bat` mention)
- Modify: `docs/book/src/setup/service.md` (lines 110, 145 — drop Administrator cmd.exe block and Windows setup link)
- Modify: `docs/book/src/foundations/fnd-001-intentional-architecture.md` (line 376 — drop non-Linux targets from kernel binary target list)
- Modify: `docs/book/src/foundations/fnd-004-engineering-infrastructure.md` (line 263 — drop darwin/windows from kernel binary targets)
- Modify: `docs/book/src/maintainers/ci-and-actions.md` (line 12 — drop darwin/msvc from build matrix description)
- Modify: `docs/book/src/hardware/arduino-uno-q-setup.md` (line 86 — drop `brew tap messense/macos-cross-toolchains` line and any surrounding macOS-only context)
- Modify: `install.sh` (line 7 comment — drop "macOS, everywhere" mention; line 135 — drop `Darwin) echo "aarch64-apple-darwin" ;;` arm of the platform detection case statement; check surrounding case statements for other Darwin/MINGW/CYGWIN arms and delete those too)

- [ ] **Step 1: Delete `setup.bat`**

```bash
rm setup.bat
```

- [ ] **Step 2: Delete the macOS and Windows setup pages**

```bash
rm docs/book/src/setup/macos.md
rm docs/book/src/setup/windows.md
```

- [ ] **Step 3: Edit `docs/book/src/SUMMARY.md`**

Open the file, delete the lines (around 15 and 16):
```markdown
- [macOS](./setup/macos.md)
- [Windows](./setup/windows.md)
```

- [ ] **Step 4: Edit `docs/book/src/introduction.md`**

On the line that reads:
```markdown
- Installing on a specific platform? → [Linux](./setup/linux.md) · [macOS](./setup/macos.md) · [Windows](./setup/windows.md) · [Docker](./setup/container.md)
```
Change it to:
```markdown
- Installing on a specific platform? → [Linux](./setup/linux.md) · [Docker](./setup/container.md)
```

- [ ] **Step 5: Edit `docs/book/src/getting-started/quick-start.md`**

Around line 23, the file references `setup.bat` / Setup → Windows. Read ±5 lines and rewrite the section to reference only `install.sh` / Linux setup. If the section is exclusively about Windows, delete the whole paragraph.

- [ ] **Step 6: Edit `docs/book/src/ops/troubleshooting.md`**

Around line 23, the file mentions "Or pass `--prebuilt` to `install.sh` / `setup.bat` to skip Rust entirely." Change to: "Or pass `--prebuilt` to `install.sh` to skip Rust entirely."

- [ ] **Step 7: Edit `docs/book/src/setup/service.md`**

- Around line 110, delete the `:: Administrator cmd.exe` block (Windows-specific service install instructions). Read ±15 lines to find the start and end of the Windows block, delete it.
- Around line 145, change `- [Linux setup](./linux.md), [macOS setup](./macos.md), [Windows setup](./windows.md)` to `- [Linux setup](./linux.md)`.

- [ ] **Step 8: Edit `docs/book/src/foundations/fnd-001-intentional-architecture.md`**

Around line 376, the kernel binary target list:
```
x86_64-unknown-linux-musl, aarch64-unknown-linux-gnu, armv7-unknown-linux-gnueabihf, x86_64-apple-darwin, aarch64-apple-darwin, x86_64-pc-windows-msvc
```
Change to:
```
x86_64-unknown-linux-musl, aarch64-unknown-linux-gnu, armv7-unknown-linux-gnueabihf
```
Also update the description in the same row (e.g., "Static musl build for Linux x86_64; GNU for ARM targets" — keep as-is).

- [ ] **Step 9: Edit `docs/book/src/foundations/fnd-004-engineering-infrastructure.md`**

Around line 263, the target list:
```
x86_64-linux-musl, aarch64-linux-gnu, armv7-linux-gnueabihf, x86_64-darwin, aarch64-darwin, x86_64-windows
```
Change to:
```
x86_64-linux-musl, aarch64-linux-gnu, armv7-linux-gnueabihf
```

- [ ] **Step 10: Edit `docs/book/src/maintainers/ci-and-actions.md`**

Around line 12: `**build** — matrix: `x86_64-unknown-linux-gnu`, `aarch64-apple-darwin`, `x86_64-pc-windows-msvc``. Change to: `**build** — matrix: `x86_64-unknown-linux-gnu``.

- [ ] **Step 11: Edit `docs/book/src/hardware/arduino-uno-q-setup.md`**

Around line 86, the file has:
```
brew tap messense/macos-cross-toolchains
```
Read ±10 lines. The surrounding paragraph likely describes how to install the ARM cross-toolchain on macOS via brew. Replace the section with the Linux equivalent: usually `apt install gcc-arm-linux-gnueabihf` or similar (consult the original surrounding text to confirm what the section is actually trying to install). If the document already has a Linux alternative below, just delete the macOS paragraph entirely.

- [ ] **Step 12: Edit `install.sh`**

- Line 7: change the comment from `# POSIX sh — no bash required. Works on Alpine, Debian, macOS, everywhere.` to `# POSIX sh — no bash required. Works on Alpine, Debian, and other Linux distros.`
- Line 135: in the case statement detecting the OS, delete the `Darwin) echo "aarch64-apple-darwin" ;;   # presume M-series` arm. Also re-grep `install.sh` for any other Darwin / MINGW / CYGWIN / MSYS arms and delete those.
  ```bash
  grep -n 'Darwin\|MINGW\|CYGWIN\|MSYS\|apple\|windows' install.sh
  ```
  Delete each matching arm of the case statement.

- [ ] **Step 13: Verify install.sh parses**

Run: `bash -n install.sh`
Expected: no syntax error.

- [ ] **Step 14: Verify no surviving doc references to deleted pages**

Run: `grep -rn 'setup/macos\|setup/windows\|setup\.bat' docs/`
Expected: empty (or at most matches inside the spec/plan files under `docs/superpowers/` — those are local-only and may be ignored). If the grep finds anything under `docs/book/`, fix it.

- [ ] **Step 15: Markdown lint**

Run: `markdownlint-cli2 'docs/**/*.md' '!docs/superpowers/**' 2>&1 | tail -20`
Expected: no errors (or pre-existing baseline). If new errors come from the edits — most commonly orphan `-` list items after a deletion, or broken local links — fix them.

- [ ] **Step 16: Workspace check (no code touched, but confirm)**

Run: `cargo check --workspace --all-targets`
Expected: green.

- [ ] **Step 17: Commit**

```bash
git add -A
git commit -m "chore: remove setup.bat and Windows/macOS setup docs"
```

---

## Task 6: Final validation pass

**Files:** any straggler unused imports, dead helpers, or doc/CI references missed in Tasks 1–5.

This task is optional — if Tasks 1–5 land cleanly with all checks green, skip directly to the PR. Run this task only if any of the verification steps below surface issues.

- [ ] **Step 1: Re-grep the workspace for residual non-Linux references**

```bash
grep -rn 'target_os = "macos"\|target_os = "windows"\|cfg(windows)\|cfg(macos)' --include="*.rs"
```
Expected: empty (or matches only inside string literals / comments that are unrelated to compilation gates).

```bash
grep -rn 'iMessage\|IMessage\|imessage' --include="*.rs" --include="*.toml"
```
Expected: empty.

```bash
grep -rn 'setup\.bat\|aarch64-apple-darwin\|x86_64-pc-windows' --include="*.md" --include="*.yml" --include="*.toml" --include="*.sh"
```
Expected: empty (except possibly in `docs/superpowers/` history files — ignore those).

- [ ] **Step 2: Run the AGENTS.md pre-PR full validation**

Run: `./dev/ci.sh all`
Expected: green. This is the project's authoritative pre-PR check.

- [ ] **Step 3: If anything in Steps 1–2 surfaced fixes, commit them as a final cleanup commit**

```bash
git add -A
git commit -m "chore: final validation pass for Linux-only cleanup"
```

If nothing needed fixing, no commit is produced — proceed to the PR.

---

## PR Submission

After all task commits land on the branch, prepare the PR description with the following points (per AGENTS.md `pull_request_template.md`):

- **Summary:** Remove Windows and macOS support. KylinBot is now Linux-only.
- **Risk tier:** Medium-to-high (touches `kylinbot-runtime` and `kylinbot-config`; Tier requires reviewer attention). Cite the spec at `docs/superpowers/specs/2026-05-13-linux-only-cleanup-design.md`.
- **Behavior changes:** macOS launchd / Windows service installation paths removed; `channels.imessage` config field silently ignored (does not break configs — `ChannelsConfig` is not `deny_unknown_fields`); Tauri desktop app now Linux-only; CI no longer builds for macOS or Windows; release artifacts no longer include darwin/windows targets; `setup.bat` removed; macOS/Windows install docs removed.
- **Rollback:** revert the PR commits in reverse order.
- **Action required from repo admin:** GitHub branch protection rules may reference deleted CI checks (`pub-scoop`, `pub-homebrew-core`, `cross-platform-build-manual`, and the non-Linux entries from the `ci.yml` matrix). Required-checks list needs updating after merge.

---

## Self-Review Notes

Performed after writing this plan:

- **Spec coverage:** Every line of the deletion inventory in the spec has a corresponding step in Tasks 1–5. Risk items (hidden upstream dependency, branch-protection cleanup, doc link rot) are addressed: cargo check after each task catches dependency breaks; PR description calls out branch protection; Step 14 of Task 5 explicitly greps for stale doc links.
- **Placeholder scan:** No "TBD" / "implement later" / "similar to Task N". Each step gives the exact file, line region, and replacement text or deletion target.
- **Type consistency:** No new types defined. All edits are deletions or refactors of existing call sites. `IMessageConfig` and `imessage` field are removed atomically in Task 1, before any other task references them.
- **One open hazard:** line numbers in Task 3 are point-in-time. If Task 1 or Task 2 shifts a line in the same file (only possible for `crates/kylinbot-channels/src/orchestrator/mod.rs`, which is touched in Task 1 and again in Task 3 sub-task 3c via `kylinbot-tools`/... wait — orchestrator is in `kylinbot-channels`, not tools — Task 3 does not re-touch it). Safe. For all other files, Task 3 is the first touch and the grep at the start of each sub-task (Steps 9, 17, 26) re-validates current line numbers.

---

## Implementation Completion Notes

All six tasks completed successfully. The following additional work was done beyond the original plan:

### Extended cfg gate cleanup (Task 3)

The cfg gate sweep was extended to crates not originally listed:
- `kylinbot-channels`: `matrix.rs`, `wechat.rs` — removed `#[cfg(unix)]` / `#[cfg(not(unix))]` on permission code
- `kylinbot-providers`: `claude_code.rs`, `copilot.rs`, `ollama.rs` — removed `#[cfg(unix)]` / `#[cfg(not(unix))]`; replaced `brew install` hints with Linux equivalents
- `kylinbot-runtime`: `daemon/mod.rs`, `doctor/mod.rs`, `observability/runtime_trace.rs`, `security/estop.rs`, `security/otp.rs`, `security/webauthn.rs`, `skills/audit.rs` — removed `#[cfg(unix)]` on permission code and tests; dropped Windows path detection and `.ps1/.bat/.cmd` suffixes
- `kylinbot-tui`: `onboarding.rs` — removed Windows `notepad` fallback
- `aardvark-sys`: `lib.rs` — removed macOS architecture mismatch hint
- `xtask`: `sync.rs`, `util.rs` — removed `brew install` hints and `.exe` handling
- root crate: `src/main.rs`, `src/commands/update.rs` — removed `#[cfg(unix)]`/`#[cfg(not(unix))]` on `set_owner_only_permissions`; removed Windows `.exe` handling

### Extended doc/script cleanup (Task 5)

Documentation and script cleanup was extended to files not originally listed:
- `.claude/skills/kylinbot/references/cli-reference.md` — removed iMessage from channel list (21→20); replaced macOS device path `/dev/cu.usbmodem101` with `/dev/ttyACM0`
- `.claude/skills/kylinbot/references/rest-api.md` — removed iMessage from Linq webhook heading
- `README.md`
- `docs/book/src/` — `architecture/crates.md`, `channels/chat-others.md` (removed entire iMessage section), `channels/overview.md` (removed iMessage from table), `developing/building-docs.md`, `getting-started/quick-start.md` (removed Homebrew option), `getting-started/yolo.md` (removed Seatbelt), `hardware/adding-boards-and-tools.md` (replaced macOS device paths with Linux equivalents), `hardware/nucleo-setup.md` (removed macOS serial port info, replaced device path), `maintainers/labels.md` (removed `channel:imessage`), `ops/observability.md`, `ops/overview.md`, `ops/service.md`, `ops/troubleshooting.md`, `philosophy.md`, `security/overview.md` (removed macOS Seatbelt and Windows AppContainer rows), `security/sandboxing.md`, `setup/linux.md` (removed Homebrew install/update/uninstall), `setup/service.md`, `maintainers/release-runbook.md`
- `firmware/esp32/README.md`, `firmware/esp32/SETUP.md`
- `scripts/README.md`, `scripts/deploy-rpi.sh`

### Additional commit (Task 6 replacement)

Commit `322dadf` ("chore: update master-branch-flow.md for Linux-only targets") updates the CI flow diagram to reflect Linux-only build/publish targets.

### Verification

All checks pass: `cargo check`, `cargo clippy -D warnings`, `cargo test` (6873 tests, 0 failures).
