## ADDED Requirements

### Requirement: SecurityRules grouped by gate
The system SHALL group externalizable rule lists under `[autonomy.security.rules]` into sub-tables that mirror `[autonomy.security.gates]` 1:1. Each sub-table SHALL map to exactly one gate switch (plus `environment`, which is driven by the `env_clear_enabled` mechanism switch, not a gate). This makes the relationship between a switch and the rules it controls discoverable from the config structure alone, without requiring readers to consult comments or external docs.

The sub-table layout is:

| Sub-table | Controlling switch | Fields |
|-----------|--------------------|--------|
| `rules.subshell` | `gates.subshell` | `patterns` |
| `rules.redirect` | `gates.redirect` | `patterns`, `safe_output_targets`, `safe_input_targets` |
| `rules.background` | `gates.background` | `block_commands` |
| `rules.whitelist` | `gates.whitelist` | `allowed_commands`, `forbidden_paths`, `dangerous_args` |
| `rules.dangerous_commands` | `gates.dangerous_commands` | `high_risk_commands`, `high_risk_substring_patterns`, `medium_risk_commands` |
| `rules.environment` | `env_clear_enabled` | `safe_env_vars` |

`gates.readonly` has no rule data (it is a pure ReadOnly-autonomy enforcement check) and therefore has no corresponding sub-table.

#### Scenario: Sub-table maps to its gate
- **WHEN** a reader inspects `[autonomy.security.rules.subshell]`
- **THEN** every key inside is consumed exclusively when `gates.subshell = true`, and disabling that gate short-circuits every key in the sub-table

#### Scenario: Default for any sub-table is empty
- **WHEN** any `SubshellRules`, `RedirectRules`, `BackgroundRules`, `WhitelistRules`, `DangerousCommandRules`, or `EnvironmentRules` is constructed via `Default`
- **THEN** every field is `None`, causing fallback to the hardcoded defaults in `policy.rs` / `shell.rs`

### Requirement: High-risk command list externalizable
The system SHALL support configuring the high-risk command list via `security.rules.dangerous_commands.high_risk_commands` in config (TOML array of strings). When the config key is absent (`None`), the system SHALL fall back to the current hardcoded list in `policy.rs` (**29 commands**: Unix only). A config-provided list fully replaces the hardcoded list (no append). An explicit empty list (`[]`) is a valid replacement meaning "no high-risk commands"; to keep the protection while disabling customization, omit the key rather than setting `[]`. To turn the gate off entirely, use `security.gates.dangerous_commands = false`.

#### Scenario: Custom high-risk list
- **WHEN** `security.rules.dangerous_commands.high_risk_commands = ["rm", "dd", "mkfs"]`
- **THEN** only `rm`, `dd`, `mkfs` are treated as high-risk; `sudo`, `curl` etc. are no longer high-risk

#### Scenario: Absent config uses defaults
- **WHEN** `security.rules.dangerous_commands.high_risk_commands` is absent (`None`)
- **THEN** the hardcoded default list of 29 commands is used

#### Scenario: Explicit empty list replaces with empty
- **WHEN** `security.rules.dangerous_commands.high_risk_commands = []`
- **THEN** no command is classified high-risk by basename; the hardcoded defaults are NOT restored (replace-not-append applies to empty lists too)

### Requirement: High-risk substring patterns externalizable
The system SHALL support configuring high-risk substring patterns via `security.rules.dangerous_commands.high_risk_substring_patterns` in config (TOML array of strings). These are case-sensitive substrings matched against the lowercased command segment (separate from basename-level `high_risk_commands`). When absent, the system SHALL fall back to the current 6 hardcoded patterns in `policy.rs` (lines 806-815).

The 6 hardcoded defaults are: `"rm -rf /"`, `"rm -fr /"`, `":(){:|:&};:"` (fork bomb), `"del /s /q"`, `"rmdir /s /q"`, `"format c:"`.

#### Scenario: Custom substring patterns
- **WHEN** `security.rules.dangerous_commands.high_risk_substring_patterns = ["rm -rf /", "mkfs.ext4 /"]`
- **THEN** only those two substrings trigger the high-risk classification; the fork bomb `:(){:|:&};:` is no longer flagged

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.dangerous_commands.high_risk_substring_patterns` is absent or `[]`
- **THEN** all 6 hardcoded patterns are used

### Requirement: Medium-risk command mapping externalizable
The system SHALL support configuring the medium-risk command mapping via `security.rules.dangerous_commands.medium_risk_commands` in config (TOML table: command name -> array of triggering subcommands). When absent, the system SHALL fall back to the current hardcoded mapping.

#### Scenario: Custom medium-risk mapping
- **WHEN** `security.rules.dangerous_commands.medium_risk_commands = { git = ["push", "reset"], npm = ["install"] }`
- **THEN** only those subcommands trigger medium risk for those commands

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.dangerous_commands.medium_risk_commands` is absent
- **THEN** the hardcoded default mapping is used

### Requirement: Dangerous argument rules externalizable
The system SHALL support configuring `is_args_safe` rules via `security.rules.whitelist.dangerous_args` in config (TOML array of tables: `{ command = "find", deny_args = ["-exec", "-ok"] }`). When absent, the system SHALL fall back to the current 7 hardcoded rules.

#### Scenario: Custom dangerous args
- **WHEN** `security.rules.whitelist.dangerous_args = [{ command = "find", deny_args = ["-exec"] }]`
- **THEN** only `find -exec` is blocked; `find -ok` is allowed

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.whitelist.dangerous_args` is absent
- **THEN** the hardcoded 7 rules are used

### Requirement: Environment variable whitelist externalizable
The system SHALL support configuring the shell environment variable whitelist via `security.rules.environment.safe_env_vars` in config (TOML array of strings). When absent, the system SHALL fall back to `SAFE_ENV_VARS` in `shell.rs`. The `shell_env_passthrough` config continues to append additional vars to whatever base list is active.

This rule is not gated by any `SecurityGates` switch. It is consumed by the `ShellTool` `env_clear()` mechanism, which is toggled via `autonomy.security.env_clear_enabled`. Setting `env_clear_enabled = false` causes the shell to inherit the full parent environment and bypasses the whitelist entirely.

#### Scenario: Custom env whitelist
- **WHEN** `security.rules.environment.safe_env_vars = ["PATH", "HOME", "LANG"]`
- **THEN** only PATH, HOME, LANG (plus any `shell_env_passthrough`) are passed to shell subprocesses

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.environment.safe_env_vars` is absent
- **THEN** the hardcoded `SAFE_ENV_VARS` list (9 Linux vars) is used

### Requirement: Forbidden paths default list externalizable
The system SHALL support configuring default forbidden paths via `security.rules.whitelist.forbidden_paths` in config (TOML array of strings). This coexists with the existing `forbidden_paths` field in `AutonomyConfig`. The `security.rules.whitelist.forbidden_paths` overrides the hardcoded default, while `AutonomyConfig.forbidden_paths` takes final precedence.

Priority: `AutonomyConfig.forbidden_paths` > `security.rules.whitelist.forbidden_paths` > hardcoded default.

#### Scenario: Custom forbidden paths via rules
- **WHEN** `security.rules.whitelist.forbidden_paths = ["/etc", "/root"]` and `AutonomyConfig.forbidden_paths` is absent
- **THEN** only `/etc`, `/root` are forbidden

#### Scenario: AutonomyConfig overrides rules
- **WHEN** `security.rules.whitelist.forbidden_paths = ["/etc", "/root"]` and `AutonomyConfig.forbidden_paths = ["/opt"]`
- **THEN** only `/opt` is forbidden (AutonomyConfig takes precedence)

#### Scenario: Empty config uses defaults
- **WHEN** both config keys are absent
- **THEN** the hardcoded default (18 Unix paths) is used

### Requirement: Allowed commands default list externalizable
The system SHALL support configuring default allowed commands via `security.rules.whitelist.allowed_commands` in config (TOML array of strings). Priority follows the same pattern as forbidden_paths: `AutonomyConfig.allowed_commands` > `security.rules.whitelist.allowed_commands` > hardcoded default.

#### Scenario: Custom allowed commands via rules
- **WHEN** `security.rules.whitelist.allowed_commands = ["git", "ls", "cat"]` and `AutonomyConfig.allowed_commands` is absent
- **THEN** only `git`, `ls`, `cat` are allowed

#### Scenario: Empty config uses defaults
- **WHEN** both config keys are absent
- **THEN** the hardcoded default (22 Unix commands) is used

### Requirement: Subshell operator patterns externalizable
The system SHALL support configuring which subshell operators to block via `security.rules.subshell.patterns` in config (TOML array of strings). Each string is a shell operator token. The system maps known tokens to their existing detection functions. When absent, the system SHALL fall back to the default list `["`", "$(", "<(", ">("]`.

**Token-to-detection mapping:**

| Token | Detection function | Behavior |
|-------|-------------------|----------|
| `` ` `` | `command.contains('`')` | Block backtick subshell |
| `$(` | `contains_unquoted_shell_variable_expansion` | Block command substitution (quote-aware: single-quoted `'$(cmd)'` allowed, double-quoted `"$(cmd)"` blocked) |
| `<(` | `command.contains("<(")` | Block process substitution in |
| `>(` | `command.contains(">(")` | Block process substitution out |

Unknown tokens are ignored with a warning log.

#### Scenario: Custom subshell patterns
- **WHEN** `security.rules.subshell.patterns = ["`", "$("]`
- **THEN** only backtick and `$()` are blocked; `<(` and `>(` are allowed

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.subshell.patterns` is absent
- **THEN** all 4 operators are blocked (current hardcoded behavior)

### Requirement: Redirect operator patterns externalizable
The system SHALL support configuring which redirect operators to block via `security.rules.redirect.patterns` in config (TOML array of strings). Each string is a redirect operator token. The system maps known tokens to their existing detection functions. When absent, the system SHALL fall back to the default list `[">", "<"]`.

**Token-to-detection mapping:**

| Token | Detection function | Safe exceptions |
|-------|-------------------|-----------------|
| `>` | `contains_unsafe_output_redirect` | `>/dev/null`, `>/dev/zero`, `>/dev/stdout`, `>/dev/stderr`, `2>/dev/null`, `2>&1` |
| `<` | `contains_unquoted_input_redirect` | `<<` heredoc, `<<<` here-string |

Unknown tokens are ignored with a warning log.

#### Scenario: Custom redirect patterns
- **WHEN** `security.rules.redirect.patterns = [">"]`
- **THEN** only output redirect `>` is blocked; input redirect `<` is allowed

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.redirect.patterns` is absent
- **THEN** both `>` and `<` are blocked (current hardcoded behavior)

### Requirement: Gate 4 background-block commands externalizable
The system SHALL support configuring which command names are blocked by Gate 4 (background) via `security.rules.background.block_commands` in config (TOML array of strings). Each entry is matched against either the exact basename or the suffix `/<entry>` of any token in the command. When absent, the system SHALL fall back to the default list `["tee"]`.

The standalone-`&` background-chaining check (via `contains_unquoted_single_ampersand`) is a pure syntactic guard and SHALL remain hardcoded inside Gate 4. To disable it, use `gates.background = false` (the gate-level switch).

#### Scenario: Custom background-block commands
- **WHEN** `security.rules.background.block_commands = ["tee", "logger"]`
- **THEN** both `tee` and `logger` are blocked when Gate 4 is active

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.background.block_commands` is absent
- **THEN** only `tee` is blocked (current hardcoded behavior)

### Requirement: Redirect-safe output targets externalizable
The system SHALL support configuring which `/dev/*` device names are exempt from the Gate 3 output-redirect block via `security.rules.redirect.safe_output_targets` in config (TOML array of strings). Each entry is a basename under `/dev/` (no slashes, no `/dev/` prefix). The system internally builds the safe-target regex `\d*>[ ]?/dev/(<targets joined by |>)(\s|[;&|)]|$)`. When absent, the system SHALL fall back to `["null", "zero", "stdout", "stderr"]`.

The fd-merge redirect stripping (`2>&1`, `1>&2`, etc.) is a separate syntactic transformation and SHALL remain hardcoded.

#### Scenario: Custom output-safe targets
- **WHEN** `security.rules.redirect.safe_output_targets = ["null"]`
- **THEN** only `>/dev/null` and `2>/dev/null` are exempt; `>/dev/stdout` is blocked

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.redirect.safe_output_targets` is absent
- **THEN** all 4 default targets (`null`, `zero`, `stdout`, `stderr`) are exempt

### Requirement: Redirect-safe input targets externalizable
The system SHALL support configuring which `/dev/*` device names are exempt from the Gate 3 input-redirect block via `security.rules.redirect.safe_input_targets` in config (TOML array of strings). Each entry is a basename under `/dev/`. The system internally builds the safe-target regex `<[ ]?/dev/(<targets joined by |>)(\s|[;&|)]|$)`. When absent, the system SHALL fall back to `["null", "zero"]`.

The heredoc (`<<`) and here-string (`<<<`) stripping is a separate syntactic transformation and SHALL remain hardcoded.

#### Scenario: Custom input-safe targets
- **WHEN** `security.rules.redirect.safe_input_targets = ["null", "urandom"]`
- **THEN** only `</dev/null` and `</dev/urandom` are exempt; `</dev/zero` is blocked

#### Scenario: Empty config uses defaults
- **WHEN** `security.rules.redirect.safe_input_targets` is absent
- **THEN** both `null` and `zero` are exempt

### Requirement: SecurityRules config structure
The system SHALL introduce a `SecurityRules` struct in `crates/kylinbot-config/src/scattered_types.rs` containing six `#[nested]` sub-structs (`SubshellRules`, `RedirectRules`, `BackgroundRules`, `WhitelistRules`, `DangerousCommandRules`, `EnvironmentRules`). Each sub-struct SHALL implement `Default` returning all-`None` fields (indicating "use hardcoded defaults"). `SecurityConfig` SHALL include a `rules: SecurityRules` field whose `Default` impl produces the empty composition.

#### Scenario: Default SecurityRules is empty
- **WHEN** `SecurityRules::default()` is constructed
- **THEN** every nested sub-struct has all fields set to `None` (or empty), causing fallback to hardcoded defaults across every gate

### Requirement: Rule loading logged
The system SHALL log which rule source is active at startup (hardcoded default vs config-provided) for each externalizable rule, using the same flat field names in the log payload to keep grep patterns stable across the schema change.

#### Scenario: Rule source logging
- **WHEN** the agent starts with custom `dangerous_commands.high_risk_commands` in config
- **THEN** log message: `high_risk_commands = "config(N items)"`
