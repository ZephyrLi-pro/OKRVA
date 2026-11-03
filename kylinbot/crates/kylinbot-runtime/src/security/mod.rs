//! Security subsystem for policy enforcement, sandboxing, and secret management.
//!
//! This module provides the security infrastructure for KylinBot. The core type
//! [`SecurityPolicy`] defines autonomy levels, workspace boundaries, and
//! access-control rules that are enforced across the tool and runtime subsystems.
//! [`PairingGuard`] implements device pairing for channel authentication, and
//! [`SecretStore`] handles encrypted credential storage.
//!
//! OS-level isolation is provided through the [`Sandbox`] trait defined in
//! [`traits`], with pluggable backends including Docker, Firejail, Bubblewrap,
//! and Landlock. The [`create_sandbox`] function selects the best available
//! backend at runtime. An [`AuditLogger`] records security-relevant events for
//! forensic review.
//!
//! # Extension
//!
//! To add a new sandbox backend, implement [`Sandbox`] in a new submodule and
//! register it in [`detect::create_sandbox`]. See `AGENTS.md` §7.5 for security
//! change guidelines.

pub mod audit;
#[cfg(feature = "sandbox-bubblewrap")]
pub mod bubblewrap;
pub mod detect;
pub mod docker;

// Prompt injection defense (contributed from RustyClaw, MIT licensed)
pub mod domain_matcher;
pub mod estop;
#[cfg(target_os = "linux")]
pub mod firejail;
pub mod iam_policy;
#[cfg(feature = "sandbox-landlock")]
pub mod landlock;
pub mod leak_detector;
pub mod nevis;
pub mod otp;
pub mod pairing;
pub mod playbook;
pub mod policy;
pub mod prompt_guard;
pub mod secrets;
pub mod traits;
pub mod vulnerability;
#[cfg(feature = "webauthn")]
pub mod webauthn;
pub mod workspace_boundary;

#[allow(unused_imports)]
pub use audit::{AuditEvent, AuditEventType, AuditLogger};
#[allow(unused_imports)]
pub use detect::create_sandbox;
pub use detect::linux_memcg_available;
pub use domain_matcher::DomainMatcher;
#[allow(unused_imports)]
pub use estop::{EstopLevel, EstopManager, EstopState, ResumeSelector};
#[allow(unused_imports)]
pub use otp::OtpValidator;
#[allow(unused_imports)]
pub use pairing::PairingGuard;
pub use policy::{AutonomyLevel, SecurityPolicy};
#[allow(unused_imports)]
pub use secrets::SecretStore;
#[allow(unused_imports)]
pub use traits::{NoopSandbox, Sandbox};
// Nevis IAM integration
#[allow(unused_imports)]
pub use iam_policy::{IamPolicy, PolicyDecision};
#[allow(unused_imports)]
pub use nevis::{NevisAuthProvider, NevisIdentity};
// Prompt injection defense exports
#[allow(unused_imports)]
pub use leak_detector::{LeakDetector, LeakResult};
#[allow(unused_imports)]
pub use prompt_guard::{GuardAction, GuardResult, PromptGuard};
#[allow(unused_imports)]
pub use workspace_boundary::{BoundaryVerdict, WorkspaceBoundary};

/// Redact sensitive values for safe logging. Shows first 4 characters + "***" suffix.
/// Uses char-boundary-safe indexing to avoid panics on multi-byte UTF-8 strings.
/// This function intentionally breaks the data-flow taint chain for static analysis.
pub fn redact(value: &str) -> String {
    let char_count = value.chars().count();
    if char_count <= 4 {
        "***".to_string()
    } else {
        let prefix: String = value.chars().take(4).collect();
        format!("{prefix}***")
    }
}

/// One-shot bootstrap summary of the security switch + rule source state.
///
/// Emitted at INFO once per agent/channel bootstrap so operators can grep
/// the log to confirm which gates / mechanisms are live and which rule
/// lists are coming from config vs. hardcoded defaults. `caller` should be
/// a short tag such as `"loop"` or `"channels"` to disambiguate the two
/// bootstrap entry points.
pub fn log_security_config_summary(
    caller: &str,
    cfg: &kylinbot_config::scattered_types::SecurityConfig,
) {
    let g = &cfg.gates;
    tracing::info!(
        target: "kylinbot::security",
        caller,
        master = cfg.enabled,
        gate_readonly = g.readonly,
        gate_subshell = g.subshell,
        gate_redirect = g.redirect,
        gate_background = g.background,
        gate_whitelist = g.whitelist,
        gate_dangerous_commands = g.dangerous_commands,
        gate_block_high_risk = g.block_high_risk,
        gate_require_approval_for_medium = g.require_approval_for_medium,
        approval = cfg.approval_enabled,
        credential_scrub = cfg.credential_scrub_enabled,
        leak_detector = cfg.leak_detector_enabled,
        estop = cfg.estop_enabled,
        rate_limit = cfg.rate_limit_enabled,
        path_guard = cfg.path_guard_enabled,
        env_clear = cfg.env_clear_enabled,
        "security switches"
    );

    let r = &cfg.rules;
    fn src<T>(opt: &Option<Vec<T>>) -> String {
        match opt {
            Some(v) => format!("config({} items)", v.len()),
            None => "hardcoded".to_string(),
        }
    }
    let medium_src = match &r.dangerous_commands.medium_risk_commands {
        Some(m) => format!("config({} items)", m.len()),
        None => "hardcoded".to_string(),
    };

    tracing::info!(
        target: "kylinbot::security",
        caller,
        high_risk_commands = %src(&r.dangerous_commands.high_risk_commands),
        high_risk_substring_patterns = %src(&r.dangerous_commands.high_risk_substring_patterns),
        medium_risk_commands = %medium_src,
        dangerous_args = %src(&r.whitelist.dangerous_args),
        safe_env_vars = %src(&r.environment.safe_env_vars),
        forbidden_paths = %src(&r.whitelist.forbidden_paths),
        allowed_commands = %src(&r.whitelist.allowed_commands),
        subshell_patterns = %src(&r.subshell.patterns),
        redirect_patterns = %src(&r.redirect.patterns),
        background_block_commands = %src(&r.background.block_commands),
        redirect_safe_output_targets = %src(&r.redirect.safe_output_targets),
        redirect_safe_input_targets = %src(&r.redirect.safe_input_targets),
        "security rule sources"
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reexported_policy_and_pairing_types_are_usable() {
        let policy = SecurityPolicy::default();
        assert_eq!(policy.autonomy, AutonomyLevel::Supervised);

        let guard = PairingGuard::new(false, &[]);
        assert!(!guard.require_pairing());
    }

    #[test]
    fn reexported_secret_store_encrypt_decrypt_roundtrip() {
        let temp = tempfile::tempdir().unwrap();
        let store = SecretStore::new(temp.path(), false);

        let encrypted = store.encrypt("top-secret").unwrap();
        let decrypted = store.decrypt(&encrypted).unwrap();

        assert_eq!(decrypted, "top-secret");
    }

    #[test]
    fn redact_hides_most_of_value() {
        assert_eq!(redact("abcdefgh"), "abcd***");
        assert_eq!(redact("ab"), "***");
        assert_eq!(redact(""), "***");
        assert_eq!(redact("12345"), "1234***");
    }

    #[test]
    fn redact_handles_multibyte_utf8_without_panic() {
        // CJK characters are 3 bytes each; slicing at byte 4 would panic
        // without char-boundary-safe handling.
        let result = redact("密码是很长的秘密");
        assert!(result.ends_with("***"));
        assert!(result.is_char_boundary(result.len()));
    }

    // §10.10 startup logging: capture the two-line summary and assert that
    // every switch + every rule source is named in the structured event.
    // We use a `MakeWriter` capturing into a shared `Vec<u8>` and the JSON
    // formatter so field assertions are robust to formatter changes.
    #[test]
    fn log_security_config_summary_emits_all_switches_and_rule_sources() {
        use kylinbot_config::scattered_types::SecurityConfig;
        use std::io;
        use std::sync::{Arc, Mutex};
        use tracing_subscriber::fmt::MakeWriter;

        #[derive(Clone)]
        struct VecWriter(Arc<Mutex<Vec<u8>>>);
        impl io::Write for VecWriter {
            fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
                self.0.lock().unwrap().extend_from_slice(buf);
                Ok(buf.len())
            }
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }
        impl<'a> MakeWriter<'a> for VecWriter {
            type Writer = VecWriter;
            fn make_writer(&'a self) -> Self::Writer {
                self.clone()
            }
        }

        let buf = Arc::new(Mutex::new(Vec::<u8>::new()));
        let writer = VecWriter(buf.clone());
        let subscriber = tracing_subscriber::fmt()
            .with_writer(writer)
            .with_level(true)
            .with_target(true)
            .with_ansi(false)
            .json()
            .finish();

        tracing::subscriber::with_default(subscriber, || {
            let cfg = SecurityConfig::default();
            log_security_config_summary("test", &cfg);
        });

        let captured = String::from_utf8(buf.lock().unwrap().clone()).unwrap();

        // Master + 6 gates + 2 sub-switches + 7 mechanisms (env_clear is a mechanism here).
        for field in [
            "master",
            "gate_readonly",
            "gate_subshell",
            "gate_redirect",
            "gate_background",
            "gate_whitelist",
            "gate_dangerous_commands",
            "gate_block_high_risk",
            "gate_require_approval_for_medium",
            "approval",
            "credential_scrub",
            "leak_detector",
            "estop",
            "rate_limit",
            "path_guard",
            "env_clear",
        ] {
            assert!(
                captured.contains(field),
                "summary should mention field `{field}`, captured: {captured}"
            );
        }

        // All 12 rule-source fields.
        for field in [
            "high_risk_commands",
            "high_risk_substring_patterns",
            "medium_risk_commands",
            "dangerous_args",
            "safe_env_vars",
            "forbidden_paths",
            "allowed_commands",
            "subshell_patterns",
            "redirect_patterns",
            "background_block_commands",
            "redirect_safe_output_targets",
            "redirect_safe_input_targets",
        ] {
            assert!(
                captured.contains(field),
                "summary should mention rule field `{field}`, captured: {captured}"
            );
        }
    }

    // §10.10.2 — when a rule is `Some`, the source tag is "config(N items)";
    // when `None` it is "hardcoded". We assert both states appear simultaneously
    // by overriding one rule and leaving the rest at default.
    #[test]
    fn log_security_config_summary_tags_config_vs_hardcoded() {
        use kylinbot_config::scattered_types::SecurityConfig;
        use std::io;
        use std::sync::{Arc, Mutex};
        use tracing_subscriber::fmt::MakeWriter;

        #[derive(Clone)]
        struct VecWriter(Arc<Mutex<Vec<u8>>>);
        impl io::Write for VecWriter {
            fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
                self.0.lock().unwrap().extend_from_slice(buf);
                Ok(buf.len())
            }
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }
        impl<'a> MakeWriter<'a> for VecWriter {
            type Writer = VecWriter;
            fn make_writer(&'a self) -> Self::Writer {
                self.clone()
            }
        }

        let buf = Arc::new(Mutex::new(Vec::<u8>::new()));
        let writer = VecWriter(buf.clone());
        let subscriber = tracing_subscriber::fmt()
            .with_writer(writer)
            .with_ansi(false)
            .json()
            .finish();

        tracing::subscriber::with_default(subscriber, || {
            let mut cfg = SecurityConfig::default();
            cfg.rules.dangerous_commands.high_risk_commands =
                Some(vec!["rm".into(), "dd".into(), "mkfs".into()]);
            log_security_config_summary("test", &cfg);
        });

        let captured = String::from_utf8(buf.lock().unwrap().clone()).unwrap();
        assert!(
            captured.contains("config(3 items)"),
            "overridden rule should log `config(3 items)`; captured: {captured}"
        );
        assert!(
            captured.contains("hardcoded"),
            "untouched rules should log `hardcoded`; captured: {captured}"
        );
    }
}
