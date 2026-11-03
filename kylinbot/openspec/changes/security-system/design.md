## Context

KylinBot 的安全机制分布在多个 crate 中：

- `kylinbot-config`：SecurityPolicy（5 道 Gate）、AutonomyConfig schema
- `kylinbot-runtime`：ApprovalManager、scrub_credentials、EstopManager、ShellTool（env_clear + Sandbox）
- `kylinbot-channels`：LeakDetector 集成点

当前所有安全规则要么写死在 `policy.rs` 的 `match` 语句中（**29 个高风险命令** + **6 个高风险子串模式**、7 个 `is_args_safe` 命令族规则、Gate 4 的 `tee` 关键字、redirect 安全例外目标），要么是常量数组（`SAFE_ENV_VARS`、`default_allowed_commands`、`default_forbidden_paths`）。没有总开关，没有独立子开关，EstopManager 的 **421 行**代码没有被 agent loop 读取。

用户需求：总开关 + 分层子开关 + 规则外置，使安全策略可按业务场景调整。

## Goals / Non-Goals

**Goals:**
- 引入 `SecurityConfig` + `SecurityRules` 配置结构，覆盖所有开关和可外置规则（policy.rs 范围内 **12 类规则**：高风险命令列表、高风险子串模式、中风险命令映射表、`is_args_safe` 规则、`default_allowed_commands`、`default_forbidden_paths`、subshell token 表、redirect token 表、Gate 4 background 阻拦命令、redirect 输出/输入安全目标、`SAFE_ENV_VARS`）
- 在 `is_command_allowed` 中实现 Gate 级别的开关检查（短路跳过）
- 在 `ApprovalManager::needs_approval`、`scrub_credentials`、`LeakDetector` 中实现开关检查
- 将硬编码规则改为"配置优先 + 硬编码回退"
- 将 EstopManager 接入 agent loop 和 tool_execution
- **机制开关命名统一为扁平字段**（`security.<name>_enabled`），Gate 开关嵌套在 `security.gates.*` 子结构下
- 所有新配置项有默认值，缺失时行为与改造前逐字符等价

**Non-Goals:**
- 不改动 SecurityPolicy 的判定算法本身（Gate 1-4 的检测逻辑不变，只是可以跳过）
- 不引入新的安全机制（不新增 Gate）
- 不做规则热重载（配置变更需重启）
- 不做 Tier/Family 维度的安全策略路由
- 不外置 `leak_detector.rs` 的 22 个正则 / `prompt_guard.rs` 的 6 类注入模式 / `loop_.rs::scrub_credentials` 的关键字（留到后续 change）
- 不清理 PromptGuard / WorkspaceBoundary / IamPolicy（留到后续）
- 不改动 Sandbox 后端选择逻辑

## Decisions

### Decision 1: 配置结构位置 — `kylinbot-config/src/scattered_types.rs`

**选择**：在 `scattered_types.rs` 新增 `SecurityConfig` 和 `SecurityRules` 结构体，`AutonomyConfig` 新增 `security: SecurityConfig` 字段。

**替代方案**：
- (a) 在 `schema.rs` 内联 — 文件已经 5800+ 行，过于臃胀
- (b) 新建 `security_schema.rs` — 引入新文件增加模块复杂度

**理由**：`scattered_types.rs` 已经存放与 config 相关的辅助类型，`SecurityConfig` 是同类结构。

### Decision 2: Gate 开关实现方式 — 提前短路 return

**选择**：在 `is_command_allowed` 的每个 Gate 检查前加 `if !config.gates.xxx { continue; }` 跳过。

**替代方案**：
- (a) 将整个 `is_command_allowed` 拆成 5 个独立方法，外部按开关选择调用 — 过度重构
- (b) 用 trait object 动态组合 Gate — 过度工程化

**理由**：最小改动，逻辑清晰，每处只加一行 `if` 判断。

### Decision 3: 规则外置实现方式 — Option 字段 + 回退函数

**选择**：`SecurityRules` 中每个规则字段用 `Option<Vec<String>>` 或 `Option<HashMap>`，`None` 时调用现有的 `default_xxx()` 函数。

**替代方案**：
- (a) 外部 YAML/JSON 文件 — 引入文件 I/O 和解析依赖
- (b) 在 `SecurityPolicy::new` 时 merge 到结构体 — 需要改构造流程

**理由**：Option + 回退最简单，不需要新依赖，不需要改构造流程。TOML 配置已经能表达数组和表。

### Decision 4: EstopManager 接入方式 — Arc 共享状态

**选择**：将 `EstopManager` 实例通过 `Arc<EstopManager>` 注入 `ToolCallLoop`，在 `execute_tools_sequential` / `execute_tools_parallel` 入口处读取状态。

**替代方案**：
- (a) 全局静态变量 — 不利于测试
- (b) 每次读文件 — 性能开销，且 estop 状态变化不频繁

**理由**：Arc 共享状态与现有的 `Arc<SecurityPolicy>` 模式一致。

### Decision 5: SecurityPolicy 持有 SecurityConfig 引用

**选择**：`SecurityPolicy` 新增 `security_config: SecurityConfig` 字段，在构造时传入。各检查方法通过 `&self.security_config` 读取开关。

**理由**：`SecurityPolicy` 已经是持有配置的 struct，不需要新的依赖注入路径。

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| 总开关 `false` 使系统完全暴露 | AuditLogger 仍然记录所有操作；日志中明确标注"security disabled"；Sandbox 隔离层仍生效 |
| Gate 1 开关关闭后 ReadOnly 级别不生效 | 需要显式配置 `gates.readonly = false`，默认 `true`；仅限需要绕过 ReadOnly 的特殊场景 |
| 外置规则配置错误导致安全策略失效 | `is_args_safe` 规则缺失时回退到默认（更严格），不是更宽松 |
| EstopManager 文件读取失败 | `fail_closed()` 策略：读取失败等于 KillAll |
| 配置 schema 膨胀 | 所有新字段都有 `Default`，现有配置文件无需任何改动 |
| 规则外置后用户可能遗漏重要规则 | 启动时日志列出实际生效的规则来源和数量 |

## Open Questions

1. `DomainMatcher` 是否需要从 estop 专用提升为通用安全组件？（当前仅在 estop.rs 中使用）
2. 是否需要 `security.rules` 支持"追加模式"（在默认列表基础上追加）还是只支持"替换模式"？（当前设计为替换模式）
3. `EstopManager` 的 OTP resume 流程是否需要集成到 channel 审批路径？

## Test Traceability (§10.12.2)

Each spec scenario below maps to the unit / integration test that exercises it. `cfg/` = `crates/kylinbot-config/src/`, `rt/` = `crates/kylinbot-runtime/src/`, `tl/` = `crates/kylinbot-tools/src/`, `e2e/` = `crates/kylinbot-runtime/tests/security_e2e.rs`.

### security-switch

| Spec scenario | Test |
|---|---|
| Master switch enabled (default) | `cfg/policy.rs::security_default_behavior_unchanged` |
| Master switch disabled — `is_command_allowed` short-circuits | `cfg/policy.rs::master_off_short_circuits_is_command_allowed` |
| Master switch disabled — `validate_command_execution` short-circuits | `cfg/policy.rs::master_off_short_circuits_validate_command_execution` |
| Master switch disabled — `enforce_tool_operation` short-circuits | `cfg/policy.rs::master_off_short_circuits_enforce_tool_operation` |
| Master switch overrides dangerous-commands gate | `cfg/policy.rs::master_off_overrides_dangerous_commands_gate` |
| Master switch state logged | `rt/security/mod.rs::log_security_config_summary_emits_all_switches_and_rule_sources` |
| Gate 1 (readonly) toggle | `cfg/policy.rs::gate_readonly_toggle`, `enforce_readonly_gate_off_skips_can_act` |
| Gate 2 (subshell) toggle | `cfg/policy.rs::gate_subshell_toggle` |
| Gate 3 (redirect) toggle | `cfg/policy.rs::gate_redirect_toggle` |
| Gate 4 (background) toggle | `cfg/policy.rs::gate_background_toggle` |
| Gate 5a (whitelist) toggle | `cfg/policy.rs::gate_whitelist_toggle` |
| Gate 5b (dangerous commands) toggle | `cfg/policy.rs::gate_dangerous_commands_classifies_and_can_disable` |
| Whitelist vs. dangerous-commands independence | `cfg/policy.rs::gate_whitelist_and_dangerous_commands_are_independent` |
| All gates orthogonal | `cfg/policy.rs::enforce_gates_orthogonal` |
| Mechanism — Approval disabled (master on, mech off) | `rt/approval/mod.rs::needs_approval_mechanism_off_bypasses_supervised` |
| Mechanism — Approval (master off, mech on) | `rt/approval/mod.rs::needs_approval_master_switch_off_bypasses_supervised` |
| Mechanism — Approval (default on/on) | `rt/approval/mod.rs::needs_approval_default_security_config_preserves_behaviour` |
| Mechanism — Approval (master off + mech off) | `rt/approval/mod.rs::needs_approval_master_and_mechanism_off_bypasses_supervised` |
| Mechanism — Credential scrub (T,T) | `rt/agent/loop_.rs::scrub_kill_switch_master_on_mechanism_on_scrubs` |
| Mechanism — Credential scrub (F,T) | `rt/agent/loop_.rs::scrub_kill_switch_master_off_mechanism_on_skips` |
| Mechanism — Credential scrub (T,F) | `rt/agent/loop_.rs::scrub_kill_switch_master_on_mechanism_off_skips` |
| Mechanism — Credential scrub (F,F) | `rt/agent/loop_.rs::scrub_kill_switch_master_off_mechanism_off_skips` |
| Mechanism — Credential scrub no-installer legacy | `rt/agent/loop_.rs::scrub_kill_switch_no_installer_preserves_legacy` |
| Mechanism — Leak detector (T,T) | `rt/security/leak_detector.rs::scan_if_enabled_default_runs_detection` |
| Mechanism — Leak detector (F,T) | `rt/security/leak_detector.rs::scan_if_enabled_skips_when_master_off` |
| Mechanism — Leak detector (T,F) | `rt/security/leak_detector.rs::scan_if_enabled_skips_when_mechanism_off` |
| Mechanism — Leak detector (F,F) | `rt/security/leak_detector.rs::scan_if_enabled_skips_when_master_and_mechanism_off` |
| Mechanism — Rate limit (F,T) | `tl/wrappers.rs::rate_limited_passthrough_when_master_off` |
| Mechanism — Rate limit (T,F) | `tl/wrappers.rs::rate_limited_passthrough_when_mechanism_off` |
| Mechanism — Rate limit (F,F) | `tl/wrappers.rs::rate_limited_passthrough_when_master_and_mechanism_off` |
| Mechanism — Path guard (F,T) | `tl/wrappers.rs::path_guard_passthrough_when_master_off` |
| Mechanism — Path guard (T,F) | `tl/wrappers.rs::path_guard_passthrough_when_mechanism_off` |
| Mechanism — Path guard (F,F) | `tl/wrappers.rs::path_guard_passthrough_when_master_and_mechanism_off` |
| Mechanism — Env clear all 4 cells | `rt/tools/shell.rs::shell_env_clear_disabled_leaks_parent_env` (inline 4-cell) |
| Default config matches current behavior | `cfg/policy.rs::security_default_behavior_unchanged` |
| TOML with no `[autonomy.security]` section | `cfg/policy.rs::autonomy_toml_without_security_section_uses_defaults` |
| Startup logging (switches + rule sources) | `rt/security/mod.rs::log_security_config_summary_emits_all_switches_and_rule_sources`, `..._tags_config_vs_hardcoded` |

### estop-integration

| Spec scenario | Test |
|---|---|
| KillAll active blocks all tools | `rt/agent/tool_execution.rs::kill_all_blocks_everything` |
| KillAll inactive allows tools | `rt/agent/tool_execution.rs::allow_when_state_clean` |
| Network blocked — `http_request` | `rt/agent/tool_execution.rs::network_kill_blocks_direct_network_tools` |
| Network blocked — shell with curl/wget | `rt/agent/tool_execution.rs::network_kill_blocks_shell_curl_wget_commands` |
| Tool frozen (case-insensitive) | `rt/agent/tool_execution.rs::frozen_tools_block_matching_name`, `frozen_tools_match_case_insensitive` |
| Estop disabled by mechanism switch | `rt/agent/tool_execution.rs::mechanism_switch_off_bypasses_estop` |
| Estop disabled by master switch | `rt/agent/tool_execution.rs::master_switch_off_bypasses_estop` |
| Estop disabled by both switches | `rt/agent/tool_execution.rs::master_and_mechanism_off_bypasses_estop` |
| Estop enabled (defaults) | `rt/agent/tool_execution.rs::no_security_config_defaults_enabled` |
| Domain block in `http_request` | `rt/agent/tool_execution.rs::blocked_domains_block_http_request_by_url` |

### externalized-rules

| Spec scenario | Test |
|---|---|
| Custom high-risk command list | `e2e/matrix_risk_classification` (cases 24–25) |
| High-risk substring patterns | `e2e/matrix_risk_classification` (case 26) |
| Medium-risk mapping (`always` + subcommand list) | `e2e/matrix_risk_classification` (cases 21, 22, 23, 29) |
| Dangerous-arg rules (exact / prefix / combined) | `cfg/policy.rs::dangerous_args_pure_exact`, `dangerous_args_pure_prefix`, `dangerous_args_combined_exact_and_prefix`, `dangerous_args_hardcoded_default_fidelity` |
| Subshell patterns externalizable | `e2e/matrix_block_violating_commands` (cases 12, 13), `e2e/invalid_subshell_token_loads_and_behaves_as_absent` |
| Redirect patterns + safe output targets | `e2e/matrix_block_violating_commands` (case 14), `e2e/matrix_allow_basic_commands` (case 8) |
| Redirect-safe input targets | `e2e/matrix_allow_basic_commands` (case 9) |
| Background block list (`tee` + custom) | `e2e/matrix_block_violating_commands` (cases 15, 16, 17) |
| Whitelist allowed-commands externalizable | `e2e/matrix_allow_basic_commands` (cases 1–7, 10), `e2e/matrix_block_violating_commands` (cases 11, 19, 20) |
| Forbidden paths externalizable | `e2e/forbidden_paths_blocks_under_listed_prefixes` |
| `AutonomyConfig.allowed_commands` overrides rules | `cfg/policy.rs::priority_rules_wins_when_autonomy_none`, `priority_hardcoded_when_both_none` |
| Empty config uses defaults — across all rule lists | `cfg/policy.rs::autonomy_toml_without_security_section_uses_defaults` |
| Explicit empty list replaces with empty (Option B) | `cfg/policy.rs::empty_allowlist_blocks_everything` |
| Environment safe-env-vars externalizable | `e2e/fixture_loads_with_all_rules_populated` (asserts probe in list) |
| Rule-source logging (per-rule config-vs-hardcoded) | `rt/security/mod.rs::log_security_config_summary_tags_config_vs_hardcoded` |
| Full 30-command matrix via TOML fixture | `e2e/matrix_allow_basic_commands`, `e2e/matrix_block_violating_commands`, `e2e/matrix_risk_classification` |
| Invalid subshell token tolerated | `e2e/invalid_subshell_token_loads_and_behaves_as_absent` |
