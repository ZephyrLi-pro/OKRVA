## Why

KylinBot 的安全机制（SecurityPolicy 五道门、ApprovalManager、scrub_credentials、Sandbox 等）大量安全规则写死在源码中：高风险/中风险命令列表（policy.rs 硬编码 match）、`is_args_safe` 危险参数规则（7 条全部 deny）、环境变量白名单（shell.rs 常量数组）。用户无法按业务场景调整这些规则，只能 fork 改源码。同时没有统一的安全总开关——无法一键关闭全部安全检查进入调试模式，也无法单独关闭某个 Gate 或某个检测器。EstopManager 虽然实现了紧急停止机制但 agent loop 不读取其状态，是安全盲区。

## What Changes

- **安全总开关**：在 `AutonomyConfig` 中新增 `security.enabled`（默认 `true`）。`false` 时跳过 SecurityPolicy 五道门、ApprovalManager 审批、scrub_credentials、LeakDetector、EstopManager 检查，仅保留 Sandbox 隔离和 AuditLogger 审计。日志记录总开关状态变更事件。
- **分层子开关**：为每个安全机制提供独立开关。
  - **6 个 Gate 子开关**：`security.gates.readonly`、`security.gates.subshell`、`security.gates.redirect`、`security.gates.background`、`security.gates.whitelist`、`security.gates.dangerous_commands`。
  - **7 个机制子开关**（命名统一为扁平字段，与 tasks/spec 一致）：`security.approval_enabled`、`security.credential_scrub_enabled`、`security.leak_detector_enabled`、`security.estop_enabled`、`security.rate_limit_enabled`、`security.path_guard_enabled`、`security.env_clear_enabled`。
- **规则外置（限定于 policy.rs）**：将 `crates/kylinbot-config/src/policy.rs` 中写死的安全规则移至 TOML 配置文件。涵盖 12 类规则：
  - 命令分类：高风险命令列表（**42 项**，Unix 29 + Windows 13）、高风险子串模式（**6 项**，如 `rm -rf /` / fork bomb / `format c:`）、中风险命令映射表
  - 命令白名单：`is_args_safe` 危险参数规则（7 个命令族）、`default_allowed_commands` 默认列表（22 项）、`default_forbidden_paths` 默认列表（18 项）
  - Shell 语法 Gate 规则：Gate 2 subshell 操作符 token 表、Gate 3 redirect 操作符 token 表、Gate 4 background 阻拦命令列表（默认 `["tee"]`）、redirect 输出安全目标列表（默认 `["null", "zero", "stdout", "stderr"]`）、redirect 输入安全目标列表（默认 `["null", "zero"]`）
  - 环境：环境变量白名单（shell.rs `SAFE_ENV_VARS`，9 项 Linux 变量）

  配置项缺失时回退到内置默认值（与当前硬编码行为逐字符等价）。本次不外置 `leak_detector.rs` / `prompt_guard.rs` / `loop_.rs::scrub_credentials` 中的硬编码规则，留待后续 change 处理。
- **EstopManager 接入**：在 agent loop 和 tool_execution 中读取 estop state，KillAll 时拒绝所有工具执行，network_blocked 时拒绝 http_request，frozen_tools 命中时拒绝对应工具。
- **清理死代码**：删除 NevisAuthProvider（587 行，0 调用点）、Playbook（459 行，仅 tests）、Vulnerability（397 行，仅 tests）。

## Capabilities

### New Capabilities
- `security-switch`: 安全总开关与分层子开关的配置 schema、运行时读取、各安全机制的开关检查逻辑
- `externalized-rules`: 将写死的安全规则（高/中风险命令列表、危险参数规则、环境变量白名单、forbidden_paths）外置为可配置项，保留内置默认值作为回退
- `estop-integration`: 将 EstopManager 接入 agent loop 和 tool_execution，实现 KillAll / network-blocked / tool-freeze 三级紧急停止

### Modified Capabilities
- `kylin-vector-memory`: 无 spec 层需求变更

## Impact

- **配置 schema**：`crates/kylinbot-config/src/schema.rs`（AutonomyConfig 扩展）、`crates/kylinbot-config/src/scattered_types.rs`（新增 SecurityConfig 结构）
- **安全策略**：`crates/kylinbot-config/src/policy.rs`（Gate 2-4 开关化、高/中风险列表可配置化、is_args_safe 规则可配置化）
- **Agent Loop**：`crates/kylinbot-runtime/src/agent/loop_.rs`（总开关检查、estop 检查）
- **工具执行**：`crates/kylinbot-runtime/src/agent/tool_execution.rs`（estop 检查）
- **审批管理**：`crates/kylinbot-runtime/src/approval/mod.rs`（子开关检查）
- **凭据脱敏**：`crates/kylinbot-runtime/src/agent/loop_.rs::scrub_credentials`（子开关检查）
- **Shell 工具**：`crates/kylinbot-runtime/src/tools/shell.rs`（环境变量白名单可配置化）
- **LeakDetector**：`crates/kylinbot-channels/src/orchestrator/mod.rs`（子开关检查）
- **安全模块清理**：`crates/kylinbot-runtime/src/security/`（删除 NevisAuthProvider / Playbook / Vulnerability）
- **无 breaking change**：所有新配置项有默认值，缺失时行为与改造前一致
