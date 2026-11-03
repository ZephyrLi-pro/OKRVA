## ADDED Requirements

### Requirement: Security master switch
The system SHALL provide a `security.enabled` boolean config field (default `true`) within `AutonomyConfig`. When `false`, the system SHALL skip SecurityPolicy Gates 1-5, ApprovalManager, scrub_credentials, LeakDetector, RateLimitedTool, PathGuardedTool, and env_clear checks. The system SHALL preserve Sandbox isolation and AuditLogger regardless of master switch state.

#### Scenario: Master switch enabled (default)
- **WHEN** `security.enabled` is `true` or absent from config
- **THEN** all security mechanisms operate normally (identical to current behavior)

#### Scenario: Master switch disabled
- **WHEN** `security.enabled` is `false`
- **THEN** SecurityPolicy `is_command_allowed` returns `true` for all commands (including ReadOnly), ApprovalManager `needs_approval` returns `false`, scrub_credentials is a no-op, LeakDetector returns `Clean`, RateLimitedTool passes through, PathGuardedTool passes through, env_clear is skipped
- **AND** Sandbox `wrap_command` still executes, AuditLogger still records events

#### Scenario: Master switch state logged
- **WHEN** the system loads config and reads `security.enabled`
- **THEN** an `info!` log message records the current state ("security master switch: enabled/disabled")

### Requirement: Gate-level sub-switches
The system SHALL provide independent boolean switches for each SecurityPolicy gate under `security.gates`:
- `gates.readonly` (default `true`) — controls Gate 1 (ReadOnly level check)
- `gates.subshell` (default `true`) — controls Gate 2 (subshell / `$()` / backtick / process substitution)
- `gates.redirect` (default `true`) — controls Gate 3 (unsafe `>` / `<` redirection)
- `gates.background` (default `true`) — controls Gate 4 (`&` background / `tee`)
- `gates.whitelist` (default `true`) — controls basename whitelist check + `is_args_safe` dangerous argument check
- `gates.dangerous_commands` (default `true`) — controls high/medium risk command classification and blocking (independent of whitelist)

When a gate switch is `false`, the corresponding check SHALL be skipped. The `gates.whitelist` controls checks inside `is_command_allowed`; `gates.dangerous_commands` controls checks inside `validate_command_execution` (high/medium risk classification + `block_high_risk_commands` + approval-for-medium-risk).

#### Scenario: Gate 1 disabled
- **WHEN** `gates.readonly` is `false`
- **THEN** commands pass the ReadOnly check even when `autonomy == ReadOnly`

#### Scenario: Gate 2 disabled
- **WHEN** `gates.subshell` is `false`
- **THEN** commands containing `` ` ``, `$()`, `<(`, `>(` pass the subshell check

#### Scenario: Gate 3 disabled
- **WHEN** `gates.redirect` is `false`
- **THEN** commands containing `>` or `<` pass the redirect check

#### Scenario: Gate 4 disabled
- **WHEN** `gates.background` is `false`
- **THEN** commands containing standalone `&` or `tee` pass the background check

#### Scenario: Gate 5 whitelist disabled
- **WHEN** `gates.whitelist` is `false`
- **THEN** any command basename passes the whitelist check and `is_args_safe` is skipped

#### Scenario: Gate 5 dangerous commands check disabled
- **WHEN** `gates.dangerous_commands` is `false`
- **THEN** `validate_command_execution` skips high/medium risk classification, `block_high_risk_commands` is not enforced, and `require_approval_for_medium_risk` is not enforced

#### Scenario: Whitelist disabled but dangerous commands enabled
- **WHEN** `gates.whitelist` is `false` and `gates.dangerous_commands` is `true`
- **THEN** any command basename is allowed (no whitelist restriction), but `rm`, `sudo`, `dd` etc. are still blocked by high-risk classification

#### Scenario: Master switch overrides sub-switches
- **WHEN** `security.enabled` is `false`
- **THEN** all gate sub-switches are effectively disabled regardless of their individual values

### Requirement: Mechanism-level sub-switches
The system SHALL provide independent boolean switches for other security mechanisms:
- `security.approval_enabled` (default `true`) — controls ApprovalManager
- `security.credential_scrub_enabled` (default `true`) — controls scrub_credentials
- `security.leak_detector_enabled` (default `true`) — controls LeakDetector
- `security.estop_enabled` (default `true`) — controls EstopManager integration
- `security.rate_limit_enabled` (default `true`) — controls RateLimitedTool
- `security.path_guard_enabled` (default `true`) — controls PathGuardedTool
- `security.env_clear_enabled` (default `true`) — controls shell subprocess environment variable clearing

#### Scenario: Approval disabled
- **WHEN** `security.approval_enabled` is `false`
- **THEN** `ApprovalManager::needs_approval` returns `false` for all tools

#### Scenario: Credential scrubbing disabled
- **WHEN** `security.credential_scrub_enabled` is `false`
- **THEN** `scrub_credentials` returns the input string unchanged

#### Scenario: Leak detection disabled
- **WHEN** `security.leak_detector_enabled` is `false`
- **THEN** LeakDetector returns `Clean` for all messages

#### Scenario: Estop disabled
- **WHEN** `security.estop_enabled` is `false`
- **THEN** agent loop and tool_execution skip estop state checks

#### Scenario: Rate limit disabled
- **WHEN** `security.rate_limit_enabled` is `false`
- **THEN** RateLimitedTool passes all calls through without rate tracking

#### Scenario: Path guard disabled
- **WHEN** `security.path_guard_enabled` is `false`
- **THEN** PathGuardedTool passes all calls through without forbidden_paths checking

#### Scenario: Env clear disabled
- **WHEN** `security.env_clear_enabled` is `false`
- **THEN** ShellTool passes the full parent environment to subprocesses instead of clearing to the safe whitelist

### Requirement: SecurityConfig schema structure
The system SHALL introduce a `SecurityConfig` struct in `crates/kylinbot-config/src/scattered_types.rs` containing all security switches. `AutonomyConfig` SHALL include a `security: SecurityConfig` field with a `Default` implementation that matches current behavior (all switches `true`).

#### Scenario: Default config matches current behavior
- **WHEN** `SecurityConfig` is constructed via `Default::default()`
- **THEN** `enabled` is `true`, all `gates.*` are `true`, all mechanism switches are `true`

#### Scenario: Partial config override
- **WHEN** user config specifies only `security.enabled = false`
- **THEN** remaining fields use their defaults (all `true`)

### Requirement: Audit logging for switch state changes
The system SHALL log all security switch states at startup via `tracing::info!`. Each switch state change during runtime (if any) SHALL also be logged.

#### Scenario: Startup logging
- **WHEN** the agent process starts
- **THEN** a single log line records: master switch state, all gate states, all mechanism states
