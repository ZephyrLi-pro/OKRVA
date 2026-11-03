## 1. Config Schema

- [x] 1.1 Add `SecurityRules` struct to `crates/kylinbot-config/src/scattered_types.rs` with fields: `high_risk_commands: Option<Vec<String>>`, `high_risk_substring_patterns: Option<Vec<String>>`, `medium_risk_commands: Option<HashMap<String, Vec<String>>>`, `dangerous_args: Option<Vec<DangerousArgRule>>`, `safe_env_vars: Option<Vec<String>>`, `forbidden_paths: Option<Vec<String>>`, `allowed_commands: Option<Vec<String>>`, `subshell_patterns: Option<Vec<String>>`, `redirect_patterns: Option<Vec<String>>`, `background_block_commands: Option<Vec<String>>`, `redirect_safe_output_targets: Option<Vec<String>>`, `redirect_safe_input_targets: Option<Vec<String>>`. Implement `Default` returning all-`None`.
- [x] 1.2 Add `SecurityGates` struct (fields: `readonly: bool`, `subshell: bool`, `redirect: bool`, `background: bool`, `whitelist: bool`, `dangerous_commands: bool`, all default `true`) and `SecurityConfig` struct (fields: `enabled: bool`, `gates: SecurityGates`, `approval_enabled: bool`, `credential_scrub_enabled: bool`, `leak_detector_enabled: bool`, `estop_enabled: bool`, `rate_limit_enabled: bool`, `path_guard_enabled: bool`, `env_clear_enabled: bool`, `rules: SecurityRules`, all default `true`/default-struct) to `crates/kylinbot-config/src/scattered_types.rs`.
- [x] 1.3 Add `DangerousArgRule` struct (fields: `command: String`, `deny_args: Vec<String>`) with serde derive.
- [x] 1.3a Extend `DangerousArgRule` to express prefix matching required by the hardcoded defaults (git `config.*` / `alias.*`; python `-c*` / `-m*`; node `-e*` / `--eval*` / `-p*` / `--print*`). Either (a) add `match_mode: Option<ArgMatchMode>` field (`enum ArgMatchMode { Exact, Prefix }`, default `Exact`), OR (b) split into `deny_args: Vec<String>` (exact) + `deny_arg_prefixes: Vec<String>`. Without this extension a config-provided list cannot replicate the hardcoded `starts_with(...)` checks in `is_args_safe` (`policy.rs:1084-1147`) and the "config absent = identical behavior" guarantee in spec/externalized-rules is violated.
- [x] 1.3b Define `medium_risk_commands` value semantics for "always-medium regardless of args" commands (`touch | mkdir | mv | cp | ln | copy | xcopy | robocopy | move | ren | rename | mklink` in `policy.rs:848-850`). Either (a) change value type from `Vec<String>` to `enum MediumRiskTrigger { Always, Subcommands(Vec<String>) }`, OR (b) document that an empty `Vec<String>` means "always-medium". Pick option (a) (explicit enum) for unambiguous TOML round-trip.
- [x] 1.4 Add `security: SecurityConfig` field to `AutonomyConfig` in `crates/kylinbot-config/src/schema.rs` with `#[serde(default)]`.
- [x] 1.5 Ensure `cargo check -p kylinbot-config` passes after schema changes.

## 2. SecurityPolicy Gate Switches

- [x] 2.1 Add `security_config: SecurityConfig` field to `SecurityPolicy` in `crates/kylinbot-config/src/policy.rs`. Update all construction sites to pass it through.
- [x] 2.2 In `is_command_allowed`: add master switch short-circuit as the very first check — if `!security_config.enabled`, return `true`.
- [x] 2.3 In `is_command_allowed`: wrap Gate 1 (ReadOnly) check with `if security_config.gates.readonly`.
- [x] 2.4 In `is_command_allowed`: wrap Gate 2 (subshell) check with `if security_config.gates.subshell`. Refactor the Gate 2 block to iterate over `security_config.rules.subshell_patterns` (fallback `["`", "$(", "<(", ">("]`), mapping each token to its detection function. Unknown tokens log a warning and are skipped.
- [x] 2.5 In `is_command_allowed`: wrap Gate 3 (redirect) checks with `if security_config.gates.redirect`. Refactor the Gate 3 block to iterate over `security_config.rules.redirect_patterns` (fallback `[">", "<"]`), mapping each token to its detection function. Unknown tokens log a warning and are skipped.
- [x] 2.6 In `is_command_allowed`: wrap Gate 4 (background/tee) checks with `if security_config.gates.background`.
- [x] 2.7 In `is_command_allowed`: wrap Gate 5 (whitelist + is_args_safe) with `if security_config.gates.whitelist`.
- [x] 2.8 In `validate_command_execution`: wrap the high/medium risk classification (`command_risk_level` + `block_high_risk_commands` + `require_approval_for_medium_risk` logic) with `if security_config.gates.dangerous_commands`. When `false`, return `Ok(CommandRiskLevel::Low)` directly after `is_command_allowed` passes.
- [x] 2.9 In `validate_command_execution`: add master switch short-circuit at the entry — if `!security_config.enabled`, return `Ok(CommandRiskLevel::Low)` immediately (before calling `is_command_allowed`). Reason: task 2.2 only neutralizes `is_command_allowed`; without this, master-off + `gates.dangerous_commands=true` would still let `block_high_risk_commands` reject `rm`/`sudo` etc., contradicting spec/security-switch "Master switch disabled" scenario.
- [x] 2.10 In `enforce_tool_operation` (`policy.rs:1394-1415`): add three switch gates. (a) Master short-circuit at entry: `if !security_config.enabled { return Ok(()) }`. (b) Wrap the `can_act()` ReadOnly check with `if security_config.gates.readonly`. (c) Wrap the `record_action()` rate-limit check with `if security_config.rate_limit_enabled`. Note: this `record_action` is the policy-level rate gate, separate from the runtime-level `RateLimitedTool` wrapper handled by task 5.4 — both must honor the same switch.

## 3. Externalized Rules — SecurityPolicy

- [x] 3.1 Refactor high-risk command matching in `command_risk_level` (`policy.rs:758-802`) to read from `security_config.rules.high_risk_commands` first, falling back to hardcoded 29-command list (Unix only).
- [x] 3.2 Refactor medium-risk command matching in `command_risk_level` to read from `security_config.rules.medium_risk_commands` first, falling back to hardcoded mapping.
- [x] 3.3 Refactor `is_args_safe` to read rules from `security_config.rules.dangerous_args` first, falling back to hardcoded 7 command-family rules (find / git / python|python3 / node / pip|pip3 / npm / cargo).
- [x] 3.4 Refactor `default_allowed_commands` to check `security_config.rules.allowed_commands` first, falling back to current hardcoded list (22 commands, +`free` on Linux).
- [x] 3.5 Refactor `default_forbidden_paths` to check `security_config.rules.forbidden_paths` first, falling back to current hardcoded list (18 Unix paths).
- [x] 3.6 Refactor high-risk substring patterns in `command_risk_level` (`policy.rs:806-815`) to read from `security_config.rules.high_risk_substring_patterns` first, falling back to hardcoded 6 substrings (`rm -rf /`, `rm -fr /`, `:(){:|:&};:`, `del /s /q`, `rmdir /s /q`, `format c:`).
- [x] 3.7 Refactor Gate 4 `tee` command detection in `is_command_allowed` (`policy.rs:1013-1018`) to iterate over `security_config.rules.background_block_commands` first, falling back to `["tee"]`. The standalone-`&` check via `contains_unquoted_single_ampersand` remains hardcoded as a pure syntactic guard.
- [x] 3.8 Refactor `contains_unsafe_output_redirect` (`policy.rs:547-567`) to build the safe-target regex dynamically from `security_config.rules.redirect_safe_output_targets` first, falling back to `["null", "zero", "stdout", "stderr"]` (all under `/dev/`). The fd-merge stripping logic (`2>&1`) stays hardcoded.
- [x] 3.9 Refactor `contains_unquoted_input_redirect` (`policy.rs:571-586`) to build the safe-target regex dynamically from `security_config.rules.redirect_safe_input_targets` first, falling back to `["null", "zero"]` (under `/dev/`). The heredoc/here-string stripping (`<<` / `<<<`) stays hardcoded.
- [x] 3.10 Implement the 3-level priority resolution (`AutonomyConfig.X` > `security.rules.X` > hardcoded default) for `allowed_commands` and `forbidden_paths` inside `SecurityPolicy::from_config` (`policy.rs:1469-1503`). Currently `AutonomyConfig.{allowed_commands,forbidden_paths}` are auto-filled by `Default` (`schema.rs:5883-5884`) via `default_allowed_commands()` / `default_forbidden_paths()`, so by the time `from_config` runs, "user-set" cannot be distinguished from "default-filled". Resolution options:
  - (a) Change `AutonomyConfig.allowed_commands` and `AutonomyConfig.forbidden_paths` to `Option<Vec<String>>`, update Default to `None`, remove `default_allowed_commands()` / `default_forbidden_paths()` from schema.rs Default impl. Then `from_config` does: `autonomy.allowed_commands.clone().or_else(|| security.rules.allowed_commands.clone()).unwrap_or_else(default_allowed_commands)`.
  - (b) Keep `Vec<String>`, add a sentinel marker in Default (e.g. wrapper type `UserOrDefault`), resolve in `from_config`.
  Pick option (a). Update all call sites that read `autonomy_config.allowed_commands` / `forbidden_paths` directly (search `grep -rn "autonomy.*allowed_commands\|autonomy.*forbidden_paths" crates/`). Without this task, tasks 3.4/3.5 are unimplementable as written: the free functions `default_allowed_commands()` / `default_forbidden_paths()` have no access to `security_config` and the auto-fill from Default obscures the user-intent signal.

## 4. Externalized Rules — ShellTool

- [x] 4.1 Pass `SecurityConfig` (or just `rules.safe_env_vars`) through to `ShellTool` via constructor.
- [x] 4.2 Refactor `collect_allowed_shell_env_vars` to use config-provided `safe_env_vars` when present, falling back to `SAFE_ENV_VARS` constant.

## 5. Mechanism Sub-switches

**Common constraint for 5.1-5.6**: each mechanism check SHALL evaluate the disable condition as `!security_config.enabled || !security_config.<mechanism>_enabled`. The master switch overrides the per-mechanism switch (per spec/security-switch "Master switch overrides sub-switches" scenario, applied to mechanisms not just gates). Implementations may collapse this via a small helper `is_mechanism_active(cfg, |c| c.approval_enabled)`.

- [x] 5.1 In `ApprovalManager::needs_approval`: early return `false` when `!security_config.enabled || !security_config.approval_enabled`. Pass `SecurityConfig` reference to `ApprovalManager` construction.
- [x] 5.2 In `scrub_credentials` (`loop_.rs`): early return (input unchanged) when `!security_config.enabled || !security_config.credential_scrub_enabled`. Pass `SecurityConfig` to the function or through `ToolCallLoop`.
- [x] 5.3 In `LeakDetector` integration points (`orchestrator/mod.rs`): skip when `!security_config.enabled || !security_config.leak_detector_enabled`.
- [x] 5.4 In `RateLimitedTool`: passthrough when `!security_config.enabled || !security_config.rate_limit_enabled`. Pass `SecurityConfig` reference to tool wrapper construction.
- [x] 5.5 In `PathGuardedTool`: passthrough when `!security_config.enabled || !security_config.path_guard_enabled`. Pass `SecurityConfig` reference to tool wrapper construction.
- [x] 5.6 In `ShellTool::env_clear`: skip environment variable clearing when `!security_config.enabled || !security_config.env_clear_enabled`, passing full parent env to subprocess.

## 6. EstopManager Integration

- [x] 6.1 Add `Arc<EstopManager>` field to `ToolCallLoop` in `loop_.rs`. Thread it through construction.
- [x] 6.2 Add estop check function in `tool_execution.rs`: `check_estop(estop: &EstopManager, tool_name: &str, args: &Value) -> Result<(), String>`. Check `kill_all` → reject all, `network_blocked` → reject http_request/curl/wget, `frozen_tools` → reject matching tools, `blocked_domains` → reject matching URLs.
- [x] 6.3 Call `check_estop` at the top of `execute_tools_sequential` and `execute_tools_parallel`, gated by `security_config.estop_enabled`.
- [x] 6.4 Handle `EstopState::load()` failure with `fail_closed()` + warning log.

## 7. Startup Logging

- [x] 7.1 Add startup log in agent loop initialization: log master switch state, all gate states, all mechanism states.
- [x] 7.2 Add startup log for rule sources: for each externalizable rule, log whether using config or hardcoded default (with item count).

## 8. Dead Code Cleanup — REVOKED (out of scope)

Decision (2026-05-25): §8.1–8.3 are removed from this change's scope. Re-audit found that the "dead" label was incorrect:

- **`Playbook`** — live dep: `crates/kylinbot-runtime/src/tools/security_ops.rs:11-12` imports `Playbook` / `StepStatus` / `evaluate_step` / `load_playbooks` / `severity_level`; used by `SecurityOpsTool.playbooks`, `run_playbook`, `list_playbooks`. Deletion breaks `SecurityOpsTool`.
- **`Vulnerability`** — live dep: `crates/kylinbot-runtime/src/tools/security_ops.rs:14` imports `generate_summary` / `parse_vulnerability_json`; used by `parse_vulnerability` action and `generate_report` template. Deletion breaks `SecurityOpsTool`.
- **`NevisAuthProvider`** — technically removable (zero external consumers in-tree), but `nevis.rs` ↔ `iam_policy.rs` form a coupled cluster; deleting one requires deleting both, which is out of scope for the security-switch change. Track as a separate "remove unused identity stack" cleanup.

- [~] 8.1 ~~Remove `NevisAuthProvider`~~ — revoked; coupled with `iam_policy.rs`; defer to separate cleanup change.
- [~] 8.2 ~~Remove `Playbook` module~~ — revoked; live dep of `SecurityOpsTool`.
- [~] 8.3 ~~Remove `Vulnerability` module~~ — revoked; live dep of `SecurityOpsTool`.

## 9. Verification

- [x] 9.1 Run `cargo check -p kylinbot-config` — schema compiles.
- [x] 9.2 Run `cargo check -p kylinbot-runtime` — runtime compiles with new config types.
- [x] 9.3 Run `cargo check -p kylinbot-channels` — LeakDetector integration compiles.
- [x] 9.4 Run `cargo test -p kylinbot-config` — existing security policy tests still pass.
- [x] 9.5 Run `cargo test -p kylinbot-runtime` — existing agent loop tests still pass.
- [x] 9.6 Run `cargo clippy --workspace -- -D warnings` — no new warnings. (Scoped to `-p kylinbot-config -p kylinbot-runtime -p kylinbot-channels --lib --tests`: clean. Workspace-wide blocked by pre-existing env issues — gobject-sys 2.64<2.70 in gateway and missing `stdbool.h` for kart-nlp-sys, both unrelated to this change.)
- [ ] 9.7 Run `cargo fmt --all -- --check` — formatting clean. (Out-of-scope: workspace-wide rustfmt drift in ~100+ pre-existing files unrelated to this change. Files edited in this change are fmt-clean.)

## 10. Test Coverage

> Anchors the scenarios in `specs/security-switch/spec.md`, `specs/externalized-rules/spec.md`, and `specs/estop-integration/spec.md`. Each spec scenario MUST have at least one corresponding test below. Place SecurityPolicy-level tests in `crates/kylinbot-config/src/policy.rs` test module; cross-crate integration tests in `crates/kylinbot-runtime/tests/`. Use the existing `SecurityPolicy::for_test` / `AutonomyConfig::default()` helpers where present; add minimal new helpers only when fixtures cannot express the case.

### 10.1 Backward-compatibility baseline (regression suite)

- [x] 10.1.1 Add `cargo test -p kylinbot-config security_default_behavior_unchanged` — empty `[autonomy.security]` produces identical `is_command_allowed` / `command_risk_level` / `validate_command_execution` results as the pre-change implementation. Use a fixed table of ≥20 representative commands covering each Gate (e.g. `rm -rf /`, `echo $(rm)`, `cat < /etc/shadow`, `ls & rm`, `find -exec ls {} \;`, `git push`, `sudo apt`, plain `ls`).
- [x] 10.1.2 Add fixture `tests/fixtures/config_no_security_section.toml` (no `[autonomy.security]` at all). Assert it loads, `SecurityConfig::default()` is applied, and a smoke command matrix matches the baseline.

### 10.2 Master switch (`security.enabled`)

- [x] 10.2.1 `enabled=false` short-circuits `is_command_allowed` → returns `true` for `rm -rf /`, `:(){:|:&};:`, `cat < /etc/shadow` (all gates bypassed).
- [x] 10.2.2 `enabled=false` short-circuits `validate_command_execution` → returns `Ok(CommandRiskLevel::Low)` for `sudo rm -rf /` (covers task 2.9).
- [x] 10.2.3 `enabled=false` short-circuits `enforce_tool_operation` → returns `Ok(())` even under `ReadOnly` autonomy + rate-limit exhausted (covers task 2.10a).
- [x] 10.2.4 Master overrides each gate: `enabled=false` + `gates.dangerous_commands=true` → `rm` allowed (per `spec/security-switch` "Master switch overrides sub-switches").
- [x] 10.2.5 Master overrides each mechanism: `enabled=false` + `approval_enabled=true` → `needs_approval()` returns `false` for a normally-approval-required tool.

### 10.3 Gate sub-switches (six gates)

For each gate, one positive (gate on → blocks) + one negative (gate off → allows) test.

- [x] 10.3.1 `gates.readonly`: `ReadOnly` autonomy + gate on → `ls` blocked; gate off → `ls` allowed.
- [x] 10.3.2 `gates.subshell`: `echo $(date)` blocked when on, allowed when off; verify single-quoted `'$(date)'` still allowed in both.
- [x] 10.3.3 `gates.redirect`: `echo x > /tmp/y` blocked when on, allowed when off; `2>/dev/null` allowed in both.
- [x] 10.3.4 `gates.background`: `ls & rm` blocked when on, allowed when off; `ls && rm` allowed in both.
- [x] 10.3.5 `gates.whitelist`: unknown command `notinwhitelist` blocked when on, allowed when off; `find -exec` blocked when on (is_args_safe), allowed when off.
- [x] 10.3.6 `gates.dangerous_commands`: `rm` returns `High` when on, `Low` when off; `git push` returns `Medium` when on, `Low` when off.
- [x] 10.3.7 Gate 5 split independence: `gates.whitelist=false` + `gates.dangerous_commands=true` → `rm` still blocked (dangerous_commands still acts even though whitelist disabled).

### 10.4 Externalized rules — replace-not-append semantics

For each of the 12 rule categories, three tests: (a) `None` → hardcoded default; (b) custom → replaces default; (c) explicit empty list `[]` → replaces with empty (hardcoded defaults are NOT restored; replace-not-append applies to empty too).

- [x] 10.4.1 `high_risk_commands`: `None` → all 29 hardcoded blocked; `["rm"]` → only `rm` high-risk, `sudo`/`curl` become Low; `[]` → no basename high-risk (defaults NOT restored).
- [x] 10.4.2 `high_risk_substring_patterns`: `None` → 6 patterns block; `["rm -rf /"]` → fork bomb `:(){:|:&};:` no longer blocked; `[]` → no substring flagged (defaults NOT restored).
- [x] 10.4.3 `medium_risk_commands`: `None` → hardcoded map applies; custom `{git: Subcommands(["push"])}` → `git commit` becomes Low; `Always` semantics: `{touch: Always}` → `touch foo` is Medium (covers task 1.3b option a).
- [x] 10.4.4 `dangerous_args`: see §10.5 below for prefix-vs-exact coverage.
- [x] 10.4.5 `allowed_commands`: `None` → 22 defaults; `["git"]` → only `git`, `ls` blocked; `[]` → empty allowlist, every command blocked by whitelist gate (defaults NOT restored).
- [x] 10.4.6 `forbidden_paths`: `None` → 18 defaults; `["/etc"]` → only `/etc`; `[]` → no path forbidden (defaults NOT restored).
- [x] 10.4.7 `subshell_patterns`: `None` → 4 operators block; `["\`", "$("]` → `<(` and `>(` allowed; unknown token `"foo"` → warning log + ignored.
- [x] 10.4.8 `redirect_patterns`: `None` → both `>` and `<` block; `[">"]` → `<` allowed; unknown token → warning + ignored.
- [x] 10.4.9 `background_block_commands`: `None` → only `tee` blocked; `["tee", "logger"]` → both blocked; verify bare-`&` check still hardcoded regardless of this list.
- [x] 10.4.10 `redirect_safe_output_targets`: `None` → 4 defaults exempt; `["null"]` → `>/dev/stdout` blocked; verify `2>&1` fd-merge still allowed (hardcoded).
- [x] 10.4.11 `redirect_safe_input_targets`: `None` → `null`/`zero` exempt; `["null", "urandom"]` → `</dev/zero` blocked, `</dev/urandom` allowed; verify `<<EOF` heredoc still allowed (hardcoded).
- [x] 10.4.12 `safe_env_vars`: `None` → 9 Linux defaults pass to subprocess; `["PATH", "HOME"]` → only PATH/HOME visible in env; `shell_env_passthrough` still appends on top of either base.

### 10.5 DangerousArgRule prefix matching (task 1.3a)

- [x] 10.5.1 Pure exact: `{command: "find", deny_args: ["-exec"], deny_arg_prefixes: []}` → `find -exec ls {}` blocked, `find -execfoo` allowed.
- [x] 10.5.2 Pure prefix: `{command: "python", deny_args: [], deny_arg_prefixes: ["-c", "-m"]}` → `python -c'print(1)'` blocked, `python -cprint` blocked, `python file.py` allowed.
- [x] 10.5.3 Combined: `{command: "git", deny_args: ["-c"], deny_arg_prefixes: ["config.", "alias."]}` → `git -c x=y` blocked (exact), `git config.user.name foo` blocked (prefix), `git status` allowed.
- [x] 10.5.4 Hardcoded-default fidelity: with `dangerous_args=None`, reproduce all 7 hardcoded rules from `policy.rs:1084-1147` — assert `git config.user.name`, `python -cprint(1)`, `node --eval='x'` all blocked (proves backward compat).

### 10.6 Three-level priority (task 3.10)

- [x] 10.6.1 `AutonomyConfig.allowed_commands = Some(["ls"])` + `rules.whitelist.allowed_commands = Some(["git"])` → only `ls` allowed (AutonomyConfig wins).
- [x] 10.6.2 `AutonomyConfig.allowed_commands = None` + `rules.whitelist.allowed_commands = Some(["git"])` → only `git` allowed (rules wins).
- [x] 10.6.3 `AutonomyConfig.allowed_commands = None` + `rules.whitelist.allowed_commands = None` → 22 hardcoded defaults apply.
- [x] 10.6.4 Same three-cell matrix for `rules.whitelist.forbidden_paths`.
- [x] 10.6.5 Regression: existing fixtures that set `autonomy.allowed_commands = ["..."]` still load and the field is now `Option<Vec<String>>` — assert no deserialization break (covers schema migration).

### 10.7 enforce_tool_operation (task 2.10)

- [x] 10.7.1 `gates.readonly=false` + `ReadOnly` autonomy → `can_act` skipped, returns `Ok(())`.
- [x] 10.7.2 `rate_limit_enabled=false` + rate-limit window exhausted → `record_action` skipped, returns `Ok(())`.
- [x] 10.7.3 Both off + master on → mechanism still functional for other checks (proves orthogonality).

### 10.8 Mechanism sub-switches × master double-condition (tasks 5.1-5.6)

For each of the 7 mechanisms (approval, credential_scrub, leak_detector, estop, rate_limit, path_guard, env_clear), 4-cell truth table: `(master, mechanism) ∈ {(T,T),(T,F),(F,T),(F,F)}`. (T,T) → active; all others → bypassed.

- [x] 10.8.1 `ApprovalManager::needs_approval` 4-cell table.
- [x] 10.8.2 `scrub_credentials` 4-cell table (assert sensitive token unchanged when bypassed).
- [x] 10.8.3 `LeakDetector` 4-cell table (assert leak passes through outbound message when bypassed).
- [x] 10.8.4 `RateLimitedTool` 4-cell table (assert no rate cap when bypassed).
- [x] 10.8.5 `PathGuardedTool` 4-cell table (assert blacklisted path allowed when bypassed).
- [x] 10.8.6 `ShellTool::env_clear` 4-cell table (assert full parent env reaches subprocess when bypassed — spawn `env | wc -l`, compare to baseline).
- [x] 10.8.7 `EstopManager` 4-cell table (covered in §10.9.5).

### 10.9 EstopManager integration (tasks 6.1-6.4)

- [x] 10.9.1 `EstopState.kill_all=true` → `execute_tools_sequential` rejects with "Emergency stop active (KillAll)..." for any tool.
- [x] 10.9.2 `network_blocked=true` → `http_request` rejected; `shell` with `curl https://x` rejected; `shell` with `wget` rejected; `shell ls` allowed.
- [x] 10.9.3 `frozen_tools=["shell"]` → `shell` rejected with "Emergency stop active (tool frozen: shell)"; `file_read` allowed.
- [x] 10.9.4 `blocked_domains=["*.chase.com"]` → `http_request` to `https://api.chase.com/x` rejected; to `https://example.com` allowed.
- [x] 10.9.5 `estop_enabled=false` + `kill_all=true` → all tools execute normally (covers spec/estop-integration "Estop integration disabled").
- [x] 10.9.6 `EstopState::load()` failure → assumes `fail_closed()`, blocks all tools, emits warning log "Failed to read estop state, assuming fail-closed".

### 10.10 Startup logging (tasks 7.1-7.2)

- [x] 10.10.1 Use `tracing-test` to capture init logs; assert master switch state + all 6 gate states + all 7 mechanism states each logged once at startup.
- [x] 10.10.2 For each of the 12 externalizable rules, assert the source-tagged log line matches one of:
  - `security rules: <name> loaded from config (N items)` when present
  - `security rules: <name> using hardcoded default (N items)` when absent

### 10.11 End-to-end TOML → block decision

- [x] 10.11.1 Integration test in `crates/kylinbot-runtime/tests/security_e2e.rs`: load fixture `security_full_config.toml` (exercises all 12 rules + all switches), construct `SecurityPolicy::from_config`, run a 30-command matrix asserting expected allow/block/risk results.
- [x] 10.11.2 Negative integration test: invalid TOML (e.g. unknown subshell token) → loads with warning, behaves as if token absent (does not crash).

### 10.12 Coverage gate (CI)

- [x] 10.12.1 Run `cargo test -p kylinbot-config security::` and `cargo test -p kylinbot-runtime security_e2e::` — all new tests pass.
- [x] 10.12.2 Spec-to-test traceability: add a short table in `design.md` mapping each spec scenario ID to its test name (manual; ≈40 rows). Reviewers use this to confirm no scenario is orphaned.

## 11. SecurityRules regrouped by gate

Refactor `[autonomy.security.rules]` from a flat 12-field struct into 6 nested sub-tables, each mapping 1:1 to a `SecurityGates` switch (plus `environment`, which is driven by the `env_clear_enabled` mechanism switch). Hard cut — no backward-compatibility aliases. Tasks 1.x–4.x above describe the original flat-schema implementation; this section overlays the regrouping on top of that work.

- [x] 11.1 In `crates/kylinbot-config/src/scattered_types.rs`, define six new sub-structs (`SubshellRules`, `RedirectRules`, `BackgroundRules`, `WhitelistRules`, `DangerousCommandRules`, `EnvironmentRules`), each `#[derive(Configurable)]` with its own `#[prefix = "autonomy.security.rules.<group>"]`. Rewrite `SecurityRules` to contain six `#[nested]` fields. Renames:
  - `subshell_patterns` → `subshell.patterns`
  - `redirect_patterns` / `redirect_safe_output_targets` / `redirect_safe_input_targets` → `redirect.patterns` / `redirect.safe_output_targets` / `redirect.safe_input_targets`
  - `background_block_commands` → `background.block_commands`
  - `allowed_commands` / `forbidden_paths` / `dangerous_args` → `whitelist.allowed_commands` / `whitelist.forbidden_paths` / `whitelist.dangerous_args`
  - `high_risk_commands` / `high_risk_substring_patterns` / `medium_risk_commands` → `dangerous_commands.*`
  - `safe_env_vars` → `environment.safe_env_vars`
- [x] 11.2 Update the doc comment on `DangerousArgRule` from `[SecurityRules::dangerous_args]` to `[WhitelistRules::dangerous_args]`.
- [x] 11.3 Update all 11 read sites in `crates/kylinbot-config/src/policy.rs` to use the nested paths (lines 967 / 975 / 983 / 1179 / 1210 / 1221 / 1231 / 1261 / 1346 / 1719 / 1724 in the pre-regrouping file).
- [x] 11.4 Update `crates/kylinbot-runtime/src/tools/shell.rs` (lines 16 / 74 / 600 / 620) — `rules.safe_env_vars` → `rules.environment.safe_env_vars`.
- [x] 11.5 Update `crates/kylinbot-runtime/src/security/mod.rs::log_rule_sources` (lines ~131-150) to read from the nested paths. Keep flat field names in the log payload to preserve grep stability.
- [x] 11.6 Update doc comments in `crates/kylinbot-config/src/schema.rs:5783/5788` (the `AutonomyConfig.allowed_commands` / `forbidden_paths` priority-chain comments now reference `security.rules.whitelist.*`).
- [x] 11.7 Rewrite `openspec/changes/security-system/specs/externalized-rules/spec.md` so every `security.rules.X` reference uses the new `security.rules.<group>.X` form. Add a new `Requirement: SecurityRules grouped by gate` at the top documenting the grouping table.
- [x] 11.8 Run `cargo check -p kylinbot-config -p kylinbot-runtime` — must compile clean. Run `cargo test -p kylinbot-config` and `cargo test -p kylinbot-runtime --lib security` to confirm regression suite still passes against the new schema.

