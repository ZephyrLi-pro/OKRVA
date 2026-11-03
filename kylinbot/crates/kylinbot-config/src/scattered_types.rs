//! Config types that were originally defined in their home modules (agent, channels, tools, trust)
//! but are needed by the config schema. Moved here to break circular dependencies.

use crate::traits::{ChannelConfig, HasPropKind, PropKind};
#[cfg(feature = "schema-export")]
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fmt;
use kylinbot_macros::Configurable;

// ── Agent config types ──────────────────────────────────────────

/// How deeply the model should reason for a given message.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[serde(rename_all = "lowercase")]
pub enum ThinkingLevel {
    Off,
    Minimal,
    Low,
    #[default]
    Medium,
    High,
    Max,
}

impl HasPropKind for ThinkingLevel {
    const PROP_KIND: PropKind = PropKind::Enum;
}

impl ThinkingLevel {
    pub fn from_str_insensitive(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "off" | "none" => Some(Self::Off),
            "minimal" | "min" => Some(Self::Minimal),
            "low" => Some(Self::Low),
            "medium" | "med" | "default" => Some(Self::Medium),
            "high" => Some(Self::High),
            "max" | "maximum" => Some(Self::Max),
            _ => None,
        }
    }
}

/// Configuration for thinking/reasoning level control.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "agent.thinking"]
pub struct ThinkingConfig {
    #[serde(default)]
    pub default_level: ThinkingLevel,
}

impl Default for ThinkingConfig {
    fn default() -> Self {
        Self {
            default_level: ThinkingLevel::Medium,
        }
    }
}

fn default_max_tokens() -> usize {
    8192
}
fn default_keep_recent() -> usize {
    4
}
fn default_collapse() -> bool {
    true
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "agent.history-pruning"]
pub struct HistoryPrunerConfig {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default = "default_max_tokens")]
    pub max_tokens: usize,
    #[serde(default = "default_keep_recent")]
    pub keep_recent: usize,
    #[serde(default = "default_collapse")]
    pub collapse_tool_results: bool,
}

impl Default for HistoryPrunerConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            max_tokens: 8192,
            keep_recent: 4,
            collapse_tool_results: true,
        }
    }
}

fn default_cost_optimized_hint() -> String {
    "cost-optimized".to_string()
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "agent.auto-classify"]
pub struct AutoClassifyConfig {
    #[serde(default)]
    pub simple_hint: Option<String>,
    #[serde(default)]
    pub standard_hint: Option<String>,
    #[serde(default)]
    pub complex_hint: Option<String>,
    #[serde(default = "default_cost_optimized_hint")]
    pub cost_optimized_hint: String,
}

impl Default for AutoClassifyConfig {
    fn default() -> Self {
        Self {
            simple_hint: None,
            standard_hint: None,
            complex_hint: None,
            cost_optimized_hint: default_cost_optimized_hint(),
        }
    }
}

fn default_min_quality_score() -> f64 {
    0.5
}
fn default_eval_max_retries() -> u32 {
    1
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "agent.eval"]
pub struct EvalConfig {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default = "default_min_quality_score")]
    pub min_quality_score: f64,
    #[serde(default = "default_eval_max_retries")]
    pub max_retries: u32,
}

impl Default for EvalConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            min_quality_score: default_min_quality_score(),
            max_retries: default_eval_max_retries(),
        }
    }
}

fn default_cc_enabled() -> bool {
    true
}
fn default_threshold_ratio() -> f64 {
    0.50
}
fn default_protect_first_n() -> usize {
    3
}
fn default_protect_last_n() -> usize {
    4
}
fn default_cc_max_passes() -> u32 {
    3
}
fn default_summary_max_chars() -> usize {
    4000
}
fn default_source_max_chars() -> usize {
    50_000
}
fn default_cc_timeout_secs() -> u64 {
    60
}
fn default_identifier_policy() -> String {
    "strict".to_string()
}
fn default_tool_result_retrim_chars() -> usize {
    2_000
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "agent.context-compression"]
pub struct ContextCompressionConfig {
    #[serde(default = "default_cc_enabled")]
    pub enabled: bool,
    #[serde(default = "default_threshold_ratio")]
    pub threshold_ratio: f64,
    #[serde(default = "default_protect_first_n")]
    pub protect_first_n: usize,
    #[serde(default = "default_protect_last_n")]
    pub protect_last_n: usize,
    #[serde(default = "default_cc_max_passes")]
    pub max_passes: u32,
    #[serde(default = "default_summary_max_chars")]
    pub summary_max_chars: usize,
    #[serde(default = "default_source_max_chars")]
    pub source_max_chars: usize,
    #[serde(default = "default_cc_timeout_secs")]
    pub timeout_secs: u64,
    #[serde(default)]
    pub summary_model: Option<String>,
    #[serde(default = "default_identifier_policy")]
    pub identifier_policy: String,
    #[serde(default = "default_tool_result_retrim_chars")]
    pub tool_result_retrim_chars: usize,
    #[serde(default)]
    pub tool_result_trim_exempt: Vec<String>,
}

impl Default for ContextCompressionConfig {
    fn default() -> Self {
        Self {
            enabled: default_cc_enabled(),
            threshold_ratio: default_threshold_ratio(),
            protect_first_n: default_protect_first_n(),
            protect_last_n: default_protect_last_n(),
            max_passes: default_cc_max_passes(),
            summary_max_chars: default_summary_max_chars(),
            source_max_chars: default_source_max_chars(),
            timeout_secs: default_cc_timeout_secs(),
            summary_model: None,
            identifier_policy: default_identifier_policy(),
            tool_result_retrim_chars: default_tool_result_retrim_chars(),
            tool_result_trim_exempt: Vec::new(),
        }
    }
}

// ── Tools config types ──────────────────────────────────────────

fn default_browser_cli() -> String {
    "claude".into()
}
fn default_browser_task_timeout() -> u64 {
    120
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "browser-delegate"]
pub struct BrowserDelegateConfig {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default = "default_browser_cli")]
    pub cli_binary: String,
    #[serde(default)]
    pub chrome_profile_dir: String,
    #[serde(default)]
    pub allowed_domains: Vec<String>,
    #[serde(default)]
    pub blocked_domains: Vec<String>,
    #[serde(default = "default_browser_task_timeout")]
    pub task_timeout_secs: u64,
}

impl Default for BrowserDelegateConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            cli_binary: default_browser_cli(),
            chrome_profile_dir: String::new(),
            allowed_domains: Vec::new(),
            blocked_domains: Vec::new(),
            task_timeout_secs: default_browser_task_timeout(),
        }
    }
}

// ── Trust config types ──────────────────────────────────────────

fn default_initial_score() -> f64 {
    0.8
}
fn default_decay_half_life() -> f64 {
    30.0
}
fn default_regression_threshold() -> f64 {
    0.5
}
fn default_correction_penalty() -> f64 {
    0.05
}
fn default_success_boost() -> f64 {
    0.01
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "trust"]
pub struct TrustConfig {
    #[serde(default = "default_initial_score")]
    pub initial_score: f64,
    #[serde(default = "default_decay_half_life")]
    pub decay_half_life_days: f64,
    #[serde(default = "default_regression_threshold")]
    pub regression_threshold: f64,
    #[serde(default = "default_correction_penalty")]
    pub correction_penalty: f64,
    #[serde(default = "default_success_boost")]
    pub success_boost: f64,
}

impl Default for TrustConfig {
    fn default() -> Self {
        Self {
            initial_score: default_initial_score(),
            decay_half_life_days: default_decay_half_life(),
            regression_threshold: default_regression_threshold(),
            correction_penalty: default_correction_penalty(),
            success_boost: default_success_boost(),
        }
    }
}

// ── Channel config types ────────────────────────────────────────

fn default_imap_port() -> u16 {
    993
}
fn default_smtp_port() -> u16 {
    465
}
fn default_imap_folder() -> String {
    "INBOX".into()
}
fn default_idle_timeout() -> u64 {
    1740
}
fn default_poll_interval_secs() -> u64 {
    60
}
fn default_true() -> bool {
    true
}
fn default_subject() -> String {
    "KylinBot Message".into()
}
fn default_max_attachment_bytes() -> usize {
    25 * 1024 * 1024
}

#[derive(Debug, Clone, Serialize, Deserialize, kylinbot_macros::Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "channels.email"]
pub struct EmailConfig {
    #[serde(default)]
    pub enabled: bool,
    pub imap_host: String,
    #[serde(default = "default_imap_port")]
    pub imap_port: u16,
    #[serde(default = "default_imap_folder")]
    pub imap_folder: String,
    pub smtp_host: String,
    #[serde(default = "default_smtp_port")]
    pub smtp_port: u16,
    #[serde(default = "default_true")]
    pub smtp_tls: bool,
    pub username: String,
    #[secret]
    pub password: String,
    pub from_address: String,
    #[serde(default = "default_idle_timeout")]
    pub idle_timeout_secs: u64,
    /// Polling interval used when the IMAP server does not advertise the IDLE
    /// capability (RFC 2177). Ignored when IDLE is available.
    #[serde(default = "default_poll_interval_secs")]
    pub poll_interval_secs: u64,
    #[serde(default)]
    pub allowed_senders: Vec<String>,
    #[serde(default = "default_subject")]
    pub default_subject: String,
    #[serde(default = "default_max_attachment_bytes")]
    pub max_attachment_bytes: usize,
}

impl ChannelConfig for EmailConfig {
    fn name() -> &'static str {
        "Email"
    }
    fn desc() -> &'static str {
        "Email over IMAP/SMTP"
    }
}

impl Default for EmailConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            imap_host: String::new(),
            imap_port: default_imap_port(),
            imap_folder: default_imap_folder(),
            smtp_host: String::new(),
            smtp_port: default_smtp_port(),
            smtp_tls: true,
            username: String::new(),
            password: String::new(),
            from_address: String::new(),
            idle_timeout_secs: default_idle_timeout(),
            poll_interval_secs: default_poll_interval_secs(),
            allowed_senders: Vec::new(),
            default_subject: default_subject(),
            max_attachment_bytes: default_max_attachment_bytes(),
        }
    }
}

fn default_label_filter() -> Vec<String> {
    vec!["INBOX".into()]
}

#[derive(Debug, Clone, Serialize, Deserialize, kylinbot_macros::Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "channels.gmail"]
pub struct GmailPushConfig {
    #[serde(default)]
    pub enabled: bool,
    pub topic: String,
    #[serde(default = "default_label_filter")]
    pub label_filter: Vec<String>,
    #[serde(default)]
    #[secret]
    pub oauth_token: String,
    #[serde(default)]
    pub allowed_senders: Vec<String>,
    #[serde(default)]
    pub webhook_url: String,
    #[serde(default)]
    pub webhook_secret: String,
}

impl ChannelConfig for GmailPushConfig {
    fn name() -> &'static str {
        "Gmail Push"
    }
    fn desc() -> &'static str {
        "Gmail Pub/Sub push notifications"
    }
}

impl Default for GmailPushConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            topic: String::new(),
            label_filter: default_label_filter(),
            oauth_token: String::new(),
            allowed_senders: Vec::new(),
            webhook_url: String::new(),
            webhook_secret: String::new(),
        }
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, kylinbot_macros::Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "channels.clawdtalk"]
pub struct ClawdTalkConfig {
    #[serde(default)]
    pub enabled: bool,
    #[secret]
    pub api_key: String,
    pub connection_id: String,
    pub from_number: String,
    #[serde(default)]
    pub allowed_destinations: Vec<String>,
    #[serde(default)]
    #[secret]
    pub webhook_secret: Option<String>,
}

impl ChannelConfig for ClawdTalkConfig {
    fn name() -> &'static str {
        "ClawdTalk"
    }
    fn desc() -> &'static str {
        "ClawdTalk Channel"
    }
}

/// Which telephony provider to use.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[serde(rename_all = "lowercase")]
pub enum VoiceProvider {
    #[default]
    Twilio,
    Telnyx,
    Plivo,
}

impl HasPropKind for VoiceProvider {
    const PROP_KIND: PropKind = PropKind::Enum;
}

impl fmt::Display for VoiceProvider {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Twilio => write!(f, "twilio"),
            Self::Telnyx => write!(f, "telnyx"),
            Self::Plivo => write!(f, "plivo"),
        }
    }
}

fn default_webhook_port() -> u16 {
    8090
}
fn default_max_call_duration() -> u64 {
    3600
}

#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "channels.voice-call"]
pub struct VoiceCallConfig {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default)]
    pub provider: VoiceProvider,
    pub account_id: String,
    pub auth_token: String,
    pub from_number: String,
    #[serde(default = "default_webhook_port")]
    pub webhook_port: u16,
    #[serde(default = "default_true")]
    pub require_outbound_approval: bool,
    #[serde(default = "default_true")]
    pub transcription_logging: bool,
    #[serde(default)]
    pub tts_voice: Option<String>,
    #[serde(default = "default_max_call_duration")]
    pub max_call_duration_secs: u64,
    #[serde(default)]
    pub webhook_base_url: Option<String>,
}

impl Default for VoiceCallConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            provider: VoiceProvider::default(),
            account_id: String::new(),
            auth_token: String::new(),
            from_number: String::new(),
            webhook_port: default_webhook_port(),
            require_outbound_approval: default_true(),
            transcription_logging: default_true(),
            tts_voice: None,
            max_call_duration_secs: default_max_call_duration(),
            webhook_base_url: None,
        }
    }
}

// ── Security config types ───────────────────────────────────────

/// String sentinel used by [`MediumRiskTrigger::Always`] for unambiguous
/// TOML round-trip: deserializes from the literal string `"always"` and
/// serializes back to the same string.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[serde(rename_all = "lowercase")]
pub enum AlwaysMarker {
    #[default]
    Always,
}

impl HasPropKind for AlwaysMarker {
    const PROP_KIND: PropKind = PropKind::Enum;
}

/// How a medium-risk command is triggered.
///
/// Two encodings in TOML:
/// - `command = "always"` — medium-risk regardless of arguments
/// - `command = ["sub1", "sub2"]` — medium-risk only when the first
///   argument matches one of the listed subcommands
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[serde(untagged)]
pub enum MediumRiskTrigger {
    /// `command = "always"` — always medium-risk regardless of arguments.
    Always(AlwaysMarker),
    /// `command = ["push", "reset", ...]` — medium-risk only when the first
    /// positional argument matches one of these subcommand names.
    Subcommands(Vec<String>),
}

/// One entry of [`WhitelistRules::dangerous_args`]: arguments that should
/// disqualify an otherwise allowed command from medium/low risk paths.
///
/// `deny_args` matches by exact equality (e.g. `find -exec`).
/// `deny_arg_prefixes` matches by `starts_with` (required by hardcoded
/// defaults like `git config.*` / `python -c*` / `node --eval*`).
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
pub struct DangerousArgRule {
    pub command: String,
    #[serde(default)]
    pub deny_args: Vec<String>,
    #[serde(default)]
    pub deny_arg_prefixes: Vec<String>,
}

/// Rules consumed by Gate 2 (subshell). Toggle the whole gate via
/// `autonomy.security.gates.subshell`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules.subshell"]
pub struct SubshellRules {
    /// Subshell operator tokens blocked by Gate 2.
    /// `None` → `["`", "$(", "<(", ">("]`.
    #[serde(default)]
    pub patterns: Option<Vec<String>>,
}

impl Default for SubshellRules {
    fn default() -> Self {
        Self {
            patterns: Some(crate::policy::default_subshell_patterns()),
        }
    }
}

/// Rules consumed by Gate 3 (redirect). Toggle the whole gate via
/// `autonomy.security.gates.redirect`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules.redirect"]
pub struct RedirectRules {
    /// Redirect operator tokens blocked by Gate 3. `None` → `[">", "<"]`.
    #[serde(default)]
    pub patterns: Option<Vec<String>>,

    /// `/dev/*` basenames exempt from Gate 3 output-redirect block.
    /// `None` → `["null", "zero", "stdout", "stderr"]`.
    #[serde(default)]
    pub safe_output_targets: Option<Vec<String>>,

    /// `/dev/*` basenames exempt from Gate 3 input-redirect block.
    /// `None` → `["null", "zero"]`.
    #[serde(default)]
    pub safe_input_targets: Option<Vec<String>>,
}

impl Default for RedirectRules {
    fn default() -> Self {
        use crate::policy;
        Self {
            patterns: Some(policy::default_redirect_patterns()),
            safe_output_targets: Some(policy::default_safe_output_targets()),
            safe_input_targets: Some(policy::default_safe_input_targets()),
        }
    }
}

/// Rules consumed by Gate 4 (background). Toggle the whole gate via
/// `autonomy.security.gates.background`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules.background"]
pub struct BackgroundRules {
    /// Command basenames blocked by Gate 4 (background). `None` → `["tee"]`.
    /// The standalone `&` chaining check remains hardcoded inside Gate 4.
    #[serde(default)]
    pub block_commands: Option<Vec<String>>,
}

impl Default for BackgroundRules {
    fn default() -> Self {
        Self {
            block_commands: Some(crate::policy::default_background_block_commands()),
        }
    }
}

/// Rules consumed by Gate 5a (whitelist + `is_args_safe`). Toggle the whole
/// gate via `autonomy.security.gates.whitelist`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules.whitelist"]
pub struct WhitelistRules {
    /// Default allowed shell command names. `None` → hardcoded 22-command default.
    /// Priority: `AutonomyConfig.allowed_commands` > this > hardcoded default.
    #[serde(default)]
    pub allowed_commands: Option<Vec<String>>,

    /// Default forbidden filesystem paths. `None` → hardcoded 18-path default.
    /// Priority: `AutonomyConfig.forbidden_paths` > this > hardcoded default.
    #[serde(default)]
    pub forbidden_paths: Option<Vec<String>>,

    /// Per-command dangerous argument rules. `None` → 7 hardcoded rules.
    #[serde(default)]
    pub dangerous_args: Option<Vec<DangerousArgRule>>,
}

impl Default for WhitelistRules {
    fn default() -> Self {
        use crate::policy;
        Self {
            allowed_commands: Some(policy::default_allowed_commands()),
            forbidden_paths: Some(policy::default_forbidden_paths()),
            dangerous_args: Some(policy::default_dangerous_args()),
        }
    }
}

/// Rules consumed by Gate 5b (high/medium-risk classification). Toggle the
/// whole gate via `autonomy.security.gates.dangerous_commands`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules.dangerous_commands"]
pub struct DangerousCommandRules {
    /// High-risk command basenames. `None` → hardcoded 29-command default (Unix only).
    #[serde(default)]
    pub high_risk_commands: Option<Vec<String>>,

    /// High-risk substring patterns matched against the lowercased command
    /// segment (e.g. `"rm -rf /"`, fork bomb). `None` → 6 hardcoded patterns.
    #[serde(default)]
    pub high_risk_substring_patterns: Option<Vec<String>>,

    /// Medium-risk command mapping. Value can be `"always"` or a list of
    /// triggering subcommands. `None` → hardcoded mapping.
    #[serde(default)]
    pub medium_risk_commands: Option<HashMap<String, MediumRiskTrigger>>,
}

impl Default for DangerousCommandRules {
    fn default() -> Self {
        use crate::policy;
        Self {
            high_risk_commands: Some(policy::default_high_risk_commands()),
            high_risk_substring_patterns: Some(policy::default_high_risk_substring_patterns()),
            medium_risk_commands: Some(policy::default_medium_risk_commands()),
        }
    }
}

/// Shell environment-variable rules. Not gated by `SecurityGates`; the
/// `env_clear()` mechanism is toggled via `autonomy.security.env_clear_enabled`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules.environment"]
pub struct EnvironmentRules {
    /// Environment variable allowlist passed through after `env_clear()`.
    /// `None` → hardcoded `SAFE_ENV_VARS` baseline. `shell_env_passthrough`
    /// appends to whichever list is active.
    #[serde(default)]
    pub safe_env_vars: Option<Vec<String>>,
}

impl Default for EnvironmentRules {
    fn default() -> Self {
        Self {
            safe_env_vars: Some(crate::policy::default_safe_env_vars()),
        }
    }
}

/// Externalizable security rule lists, grouped to mirror [`SecurityGates`].
/// Each sub-table maps 1:1 to a gate switch (plus `environment`, which is
/// driven by the `env_clear_enabled` mechanism switch). `None` fields fall
/// back to the hardcoded default in `policy.rs`; user-provided lists fully
/// replace the defaults (no append semantics).
#[derive(Debug, Clone, Default, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.rules"]
pub struct SecurityRules {
    /// Gate 2 rules.
    #[nested]
    #[serde(default)]
    pub subshell: SubshellRules,

    /// Gate 3 rules.
    #[nested]
    #[serde(default)]
    pub redirect: RedirectRules,

    /// Gate 4 rules.
    #[nested]
    #[serde(default)]
    pub background: BackgroundRules,

    /// Gate 5a rules.
    #[nested]
    #[serde(default)]
    pub whitelist: WhitelistRules,

    /// Gate 5b rules.
    #[nested]
    #[serde(default)]
    pub dangerous_commands: DangerousCommandRules,

    /// Shell environment rules (driven by `env_clear_enabled`, not a gate).
    #[nested]
    #[serde(default)]
    pub environment: EnvironmentRules,
}

/// Per-Gate sub-switches inside [`SecurityConfig`]. All default `true`
/// (gates active). Setting a gate to `false` short-circuits the gate
/// inside `SecurityPolicy::is_command_allowed`.
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security.gates"]
pub struct SecurityGates {
    /// Gate 1: ReadOnly autonomy enforcement.
    #[serde(default = "default_true")]
    pub readonly: bool,
    /// Gate 2: subshell/command-substitution operator block.
    #[serde(default = "default_true")]
    pub subshell: bool,
    /// Gate 3: redirect operator block.
    #[serde(default = "default_true")]
    pub redirect: bool,
    /// Gate 4: background chaining + `tee`-style block.
    #[serde(default = "default_true")]
    pub background: bool,
    /// Gate 5a: command whitelist + `is_args_safe` arg-level block.
    #[serde(default = "default_true")]
    pub whitelist: bool,
    /// Gate 5b: high/medium risk classification (drives approval prompts
    /// and the `block_high_risk` / `require_approval_for_medium` sub-switches).
    #[serde(default = "default_true")]
    pub dangerous_commands: bool,

    /// Gate 5b-a: when `dangerous_commands` is active, block high-risk
    /// commands that are not explicitly allowlisted. Replaces the legacy
    /// `AutonomyConfig.block_high_risk_commands`.
    #[serde(default = "default_true")]
    pub block_high_risk: bool,

    /// Gate 5b-b: when `dangerous_commands` is active, require explicit
    /// approval for medium-risk commands in Supervised mode. Replaces the
    /// legacy `AutonomyConfig.require_approval_for_medium_risk`.
    #[serde(default = "default_true")]
    pub require_approval_for_medium: bool,
}

impl Default for SecurityGates {
    fn default() -> Self {
        Self {
            readonly: true,
            subshell: true,
            redirect: true,
            background: true,
            whitelist: true,
            dangerous_commands: true,
            block_high_risk: true,
            require_approval_for_medium: true,
        }
    }
}

/// Master security configuration. Master switch + Gate sub-switches +
/// mechanism sub-switches + externalizable rule lists.
///
/// Defaults preserve pre-change behavior: master and every sub-switch
/// are `true`, every rule list is `None` (use hardcoded fallback).
#[derive(Debug, Clone, Serialize, Deserialize, Configurable)]
#[cfg_attr(feature = "schema-export", derive(schemars::JsonSchema))]
#[prefix = "autonomy.security"]
pub struct SecurityConfig {
    /// Master switch. `false` disables the entire security system: every
    /// gate, mechanism, and rule check is short-circuited to allow.
    /// Audit logs and sandbox isolation are not affected.
    #[serde(default = "default_true")]
    pub enabled: bool,

    /// Per-Gate switches (see [`SecurityGates`]).
    #[nested]
    #[serde(default)]
    pub gates: SecurityGates,

    /// ApprovalManager. `false` → `needs_approval` always returns `false`.
    #[serde(default = "default_true")]
    pub approval_enabled: bool,

    /// `scrub_credentials` tool-output sanitizer. `false` → input passthrough.
    #[serde(default = "default_true")]
    pub credential_scrub_enabled: bool,

    /// LeakDetector inbound message scanner. `false` → no scanning.
    #[serde(default = "default_true")]
    pub leak_detector_enabled: bool,

    /// EstopManager agent-loop integration. `false` → estop state ignored.
    #[serde(default = "default_true")]
    pub estop_enabled: bool,

    /// Both the policy-level `record_action` rate gate and the runtime-level
    /// `RateLimitedTool` wrapper. `false` → passthrough on both.
    #[serde(default = "default_true")]
    pub rate_limit_enabled: bool,

    /// `PathGuardedTool` wrapper. `false` → passthrough.
    #[serde(default = "default_true")]
    pub path_guard_enabled: bool,

    /// `ShellTool` `env_clear()` + safe-env-vars allowlist. `false` →
    /// inherit full parent environment (debug/test escape hatch only).
    #[serde(default = "default_true")]
    pub env_clear_enabled: bool,

    /// Externalizable rule lists (see [`SecurityRules`]).
    #[nested]
    #[serde(default)]
    pub rules: SecurityRules,
}

impl Default for SecurityConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            gates: SecurityGates::default(),
            approval_enabled: true,
            credential_scrub_enabled: true,
            leak_detector_enabled: true,
            estop_enabled: true,
            rate_limit_enabled: true,
            path_guard_enabled: true,
            env_clear_enabled: true,
            rules: SecurityRules::default(),
        }
    }
}
