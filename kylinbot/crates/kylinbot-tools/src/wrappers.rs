//! Generic tool wrappers for crosscutting concerns.
//!
//! Each wrapper implements [`Tool`] by delegating to an inner tool while
//! applying one crosscutting concern around the `execute` call.  Wrappers
//! compose: stack them at construction time in `tools/mod.rs` rather than
//! repeating the same guard blocks inside every tool's `execute` method.
//!
//! # Composition order (outermost first)
//!
//! ```text
//! RateLimitedTool
//!   └─ PathGuardedTool
//!        └─ <concrete tool>
//! ```
//!
//! # Example
//!
//! ```rust,ignore
//! let tool = RateLimitedTool::new(
//!     PathGuardedTool::new(ShellTool::new(security.clone(), runtime), security.clone()),
//!     security.clone(),
//! );
//! ```

use async_trait::async_trait;
use std::sync::Arc;
use kylinbot_api::tool::{Tool, ToolResult};
use kylinbot_config::policy::SecurityPolicy;

/// Prefix for error messages produced by security-layer wrappers (PathGuardedTool, etc.).
/// `RateLimitedTool` refunds the action budget only when the error starts with this prefix,
/// ensuring business-logic failures (file not found, timeout) consume the budget normally.
pub const SECURITY_BLOCK_PREFIX: &str = "[security-block] ";

/// Type alias for a path-extraction closure used by [`PathGuardedTool`].
type PathExtractor = dyn Fn(&serde_json::Value) -> Option<String> + Send + Sync;

// ── RateLimitedTool ───────────────────────────────────────────────────────────

/// Wraps any [`Tool`] and enforces the [`SecurityPolicy`] rate limit.
///
/// Replaces the repeated `is_rate_limited()` / `record_action()` guard blocks
/// previously inlined in every tool's `execute` method (~30 files, ~50 call
/// sites).  The inner tool receives the call only when the rate limit allows it.
pub struct RateLimitedTool<T: Tool> {
    inner: T,
    security: Arc<SecurityPolicy>,
}

impl<T: Tool> RateLimitedTool<T> {
    pub fn new(inner: T, security: Arc<SecurityPolicy>) -> Self {
        Self { inner, security }
    }
}

#[async_trait]
impl<T: Tool> Tool for RateLimitedTool<T> {
    fn name(&self) -> &str {
        self.inner.name()
    }

    fn description(&self) -> &str {
        self.inner.description()
    }

    fn parameters_schema(&self) -> serde_json::Value {
        self.inner.parameters_schema()
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        let cfg = &self.security.security_config;
        if !cfg.enabled || !cfg.rate_limit_enabled {
            return self.inner.execute(args).await;
        }

        if self.security.is_rate_limited() {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some("Rate limit exceeded: too many actions in the last hour".into()),
            });
        }

        if !self.security.record_action() {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some("Rate limit exceeded: action budget exhausted".into()),
            });
        }

        let result = self.inner.execute(args).await;

        // Refund the action budget ONLY when the inner tool was blocked by
        // another security layer (identified by SECURITY_BLOCK_PREFIX).
        // Business-logic failures (file not found, timeout, etc.) consume
        // the budget normally — they represent real agent activity.
        if let Ok(ref tool_result) = result
            && !tool_result.success
            && tool_result
                .error
                .as_deref()
                .is_some_and(|e| e.starts_with(SECURITY_BLOCK_PREFIX))
        {
            self.security.refund_action();
        }

        result
    }
}

// ── PathGuardedTool ───────────────────────────────────────────────────────────

/// Wraps any [`Tool`] and blocks calls whose arguments contain a forbidden path.
///
/// Replaces the `forbidden_path_argument()` guard blocks previously inlined in
/// tools that accept a path-like argument (`shell`, `file_read`, `file_write`,
/// `file_edit`, `pdf_read`, `content_search`, `glob_search`, `image_info`).
///
/// Path extraction is argument-name-driven: the wrapper inspects the `"path"`,
/// `"command"`, `"pattern"`, and `"query"` fields of the JSON argument object.
/// Tools whose path argument uses a different field name can pass a custom
/// extractor at construction via [`PathGuardedTool::with_extractor`].
pub struct PathGuardedTool<T: Tool> {
    inner: T,
    security: Arc<SecurityPolicy>,
    /// Optional override: extract a path string from the args JSON.
    extractor: Option<Box<PathExtractor>>,
}

impl<T: Tool> PathGuardedTool<T> {
    pub fn new(inner: T, security: Arc<SecurityPolicy>) -> Self {
        Self {
            inner,
            security,
            extractor: None,
        }
    }

    /// Supply a custom path-extraction closure for tools with non-standard arg names.
    pub fn with_extractor<F>(mut self, f: F) -> Self
    where
        F: Fn(&serde_json::Value) -> Option<String> + Send + Sync + 'static,
    {
        self.extractor = Some(Box::new(f));
        self
    }

    fn extract_path_string(&self, args: &serde_json::Value) -> Option<String> {
        if let Some(ref f) = self.extractor {
            return f(args);
        }
        // Default: check common argument names used across KylinBot tools.
        for field in &["path", "command", "pattern", "query", "file"] {
            if let Some(s) = args.get(field).and_then(|v| v.as_str()) {
                return Some(s.to_string());
            }
        }
        None
    }
}

#[async_trait]
impl<T: Tool> Tool for PathGuardedTool<T> {
    fn name(&self) -> &str {
        self.inner.name()
    }

    fn description(&self) -> &str {
        self.inner.description()
    }

    fn parameters_schema(&self) -> serde_json::Value {
        self.inner.parameters_schema()
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        let cfg = &self.security.security_config;
        if !cfg.enabled || !cfg.path_guard_enabled {
            return self.inner.execute(args).await;
        }

        if let Some(arg) = self.extract_path_string(&args) {
            // For shell command arguments, use the full token-aware scanner.
            // For plain path values (e.g. "path" or custom extractor), fall back
            // to the direct path check.
            let blocked = if self.extractor.is_none()
                && args.get("command").and_then(|v| v.as_str()).is_some()
            {
                self.security.forbidden_path_argument(&arg)
            } else if !self.security.is_path_allowed(&arg) {
                Some(arg.clone())
            } else {
                None
            };

            if let Some(path) = blocked {
                return Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some(format!("{SECURITY_BLOCK_PREFIX}Path blocked by security policy: {path}")),
                });
            }
        }

        self.inner.execute(args).await
    }
}

// ── Tests ─────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use async_trait::async_trait;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use kylinbot_config::autonomy::AutonomyLevel;
    use kylinbot_config::policy::SecurityPolicy;

    // ── Helpers ───────────────────────────────────────────────────────────────

    fn policy(autonomy: AutonomyLevel) -> Arc<SecurityPolicy> {
        Arc::new(SecurityPolicy {
            autonomy,
            workspace_dir: std::env::temp_dir(),
            ..SecurityPolicy::default()
        })
    }

    /// A minimal tool that records how many times `execute` was called.
    struct CountingTool {
        calls: Arc<AtomicUsize>,
    }

    impl CountingTool {
        fn new() -> (Self, Arc<AtomicUsize>) {
            let counter = Arc::new(AtomicUsize::new(0));
            (
                CountingTool {
                    calls: counter.clone(),
                },
                counter,
            )
        }
    }

    #[async_trait]
    impl Tool for CountingTool {
        fn name(&self) -> &str {
            "counting"
        }
        fn description(&self) -> &str {
            "counts calls"
        }
        fn parameters_schema(&self) -> serde_json::Value {
            serde_json::json!({})
        }
        async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
            self.calls.fetch_add(1, Ordering::SeqCst);
            if args.get("fail").and_then(|v| v.as_bool()).unwrap_or(false) {
                Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some("file not found".into()),
                })
            } else {
                Ok(ToolResult {
                    success: true,
                    output: "ok".into(),
                    error: None,
                })
            }
        }
    }

    // ── RateLimitedTool tests ─────────────────────────────────────────────────

    #[tokio::test]
    async fn rate_limited_allows_call_within_budget() {
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, policy(AutonomyLevel::Full));
        let result = tool
            .execute(serde_json::json!({}))
            .await
            .expect("should succeed");
        assert!(result.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn rate_limited_delegates_name_and_schema() {
        let (inner, _) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, policy(AutonomyLevel::Full));
        assert_eq!(tool.name(), "counting");
        assert_eq!(tool.description(), "counts calls");
        assert!(tool.parameters_schema().is_object());
    }

    #[tokio::test]
    async fn rate_limited_blocks_when_exhausted() {
        // Use a policy with a tiny action budget (1 action per window).
        let sec = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            max_actions_per_hour: 1,
            ..SecurityPolicy::default()
        });
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, sec);

        let r1 = tool.execute(serde_json::json!({})).await.unwrap();
        assert!(r1.success, "first call should succeed");

        let r2 = tool.execute(serde_json::json!({})).await.unwrap();
        assert!(!r2.success, "second call should be rate-limited");
        assert!(r2.error.unwrap().contains("Rate limit exceeded"));
        // Inner tool must NOT have been called on the blocked attempt.
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    // ── PathGuardedTool tests ─────────────────────────────────────────────────

    #[tokio::test]
    async fn path_guard_allows_safe_path() {
        let (inner, counter) = CountingTool::new();
        let tool = PathGuardedTool::new(inner, policy(AutonomyLevel::Full));
        let result = tool
            .execute(serde_json::json!({"path": "src/main.rs"}))
            .await
            .unwrap();
        assert!(result.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn path_guard_blocks_forbidden_path() {
        let (inner, counter) = CountingTool::new();
        let tool = PathGuardedTool::new(inner, policy(AutonomyLevel::Full));
        let result = tool
            .execute(serde_json::json!({"command": "cat /etc/passwd"}))
            .await
            .unwrap();
        assert!(!result.success);
        assert!(result.error.unwrap().contains("Path blocked"));
        assert_eq!(
            counter.load(Ordering::SeqCst),
            0,
            "inner must not be called"
        );
    }

    #[tokio::test]
    async fn path_guard_no_path_arg_passes_through() {
        let (inner, counter) = CountingTool::new();
        let tool = PathGuardedTool::new(inner, policy(AutonomyLevel::Full));
        // No recognised path field — wrapper must not block.
        let result = tool
            .execute(serde_json::json!({"value": "hello"}))
            .await
            .unwrap();
        assert!(result.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn path_guard_custom_extractor() {
        let (inner, counter) = CountingTool::new();
        let tool =
            PathGuardedTool::new(inner, policy(AutonomyLevel::Full)).with_extractor(|args| {
                args.get("target")
                    .and_then(|v| v.as_str())
                    .map(String::from)
            });
        let result = tool
            .execute(serde_json::json!({"target": "/etc/shadow"}))
            .await
            .unwrap();
        assert!(!result.success);
        assert!(result.error.unwrap().contains("Path blocked"));
        assert_eq!(counter.load(Ordering::SeqCst), 0);
    }

    // ── Composition test ──────────────────────────────────────────────────────

    #[tokio::test]
    async fn composed_wrappers_both_enforce() {
        // RateLimited(PathGuarded(CountingTool)) — path check happens inside
        // the rate-limit window, so a forbidden path must still be blocked
        // (and not consume a rate-limit slot).
        let sec = policy(AutonomyLevel::Full);
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(PathGuardedTool::new(inner, sec.clone()), sec);

        let blocked = tool
            .execute(serde_json::json!({"path": "/etc/passwd"}))
            .await
            .unwrap();
        assert!(!blocked.success);
        assert_eq!(counter.load(Ordering::SeqCst), 0);
    }

    // ── Mechanism kill-switch tests (M5) ──────────────────────────────────────

    fn policy_with_security(
        autonomy: AutonomyLevel,
        cfg: kylinbot_config::scattered_types::SecurityConfig,
    ) -> Arc<SecurityPolicy> {
        Arc::new(SecurityPolicy {
            autonomy,
            workspace_dir: std::env::temp_dir(),
            security_config: cfg,
            ..SecurityPolicy::default()
        })
    }

    #[tokio::test]
    async fn rate_limited_passthrough_when_master_off() {
        let cfg = kylinbot_config::scattered_types::SecurityConfig {
            enabled: false,
            ..Default::default()
        };
        let sec = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            security_config: cfg,
            max_actions_per_hour: 1,
            ..SecurityPolicy::default()
        });
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, sec);

        // Both calls should pass through even though budget is 1.
        assert!(tool.execute(serde_json::json!({})).await.unwrap().success);
        assert!(tool.execute(serde_json::json!({})).await.unwrap().success);
        assert_eq!(counter.load(Ordering::SeqCst), 2);
    }

    #[tokio::test]
    async fn rate_limited_passthrough_when_mechanism_off() {
        let cfg = kylinbot_config::scattered_types::SecurityConfig {
            rate_limit_enabled: false,
            ..Default::default()
        };
        let sec = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            security_config: cfg,
            max_actions_per_hour: 1,
            ..SecurityPolicy::default()
        });
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, sec);

        assert!(tool.execute(serde_json::json!({})).await.unwrap().success);
        assert!(tool.execute(serde_json::json!({})).await.unwrap().success);
        assert_eq!(counter.load(Ordering::SeqCst), 2);
    }

    #[tokio::test]
    async fn path_guard_passthrough_when_master_off() {
        let cfg = kylinbot_config::scattered_types::SecurityConfig {
            enabled: false,
            ..Default::default()
        };
        let (inner, counter) = CountingTool::new();
        let tool = PathGuardedTool::new(inner, policy_with_security(AutonomyLevel::Full, cfg));
        let result = tool
            .execute(serde_json::json!({"command": "cat /etc/passwd"}))
            .await
            .unwrap();
        assert!(result.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn path_guard_passthrough_when_mechanism_off() {
        let cfg = kylinbot_config::scattered_types::SecurityConfig {
            path_guard_enabled: false,
            ..Default::default()
        };
        let (inner, counter) = CountingTool::new();
        let tool = PathGuardedTool::new(inner, policy_with_security(AutonomyLevel::Full, cfg));
        let result = tool
            .execute(serde_json::json!({"command": "cat /etc/passwd"}))
            .await
            .unwrap();
        assert!(result.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn rate_limited_passthrough_when_master_and_mechanism_off() {
        let cfg = kylinbot_config::scattered_types::SecurityConfig {
            enabled: false,
            rate_limit_enabled: false,
            ..Default::default()
        };
        let sec = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            security_config: cfg,
            max_actions_per_hour: 1,
            ..SecurityPolicy::default()
        });
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, sec);
        assert!(tool.execute(serde_json::json!({})).await.unwrap().success);
        assert!(tool.execute(serde_json::json!({})).await.unwrap().success);
        assert_eq!(counter.load(Ordering::SeqCst), 2);
    }

    #[tokio::test]
    async fn path_guard_passthrough_when_master_and_mechanism_off() {
        let cfg = kylinbot_config::scattered_types::SecurityConfig {
            enabled: false,
            path_guard_enabled: false,
            ..Default::default()
        };
        let (inner, counter) = CountingTool::new();
        let tool = PathGuardedTool::new(inner, policy_with_security(AutonomyLevel::Full, cfg));
        let result = tool
            .execute(serde_json::json!({"command": "cat /etc/passwd"}))
            .await
            .unwrap();
        assert!(result.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    // ── Refund test: blocked calls must not consume rate-limit budget ────────

    #[tokio::test]
    async fn rate_limit_refunds_on_blocked_path() {
        let sec = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            max_actions_per_hour: 2,
            ..SecurityPolicy::default()
        });
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(PathGuardedTool::new(inner, sec.clone()), sec);

        // Call 1: blocked by PathGuardedTool (forbidden path) — budget NOT consumed.
        let r1 = tool
            .execute(serde_json::json!({"path": "/etc/passwd"}))
            .await
            .unwrap();
        assert!(!r1.success);
        assert!(r1.error.unwrap().contains("Path blocked"));
        assert_eq!(counter.load(Ordering::SeqCst), 0);

        // Call 2: blocked again — budget still NOT consumed.
        let r2 = tool
            .execute(serde_json::json!({"path": "/etc/shadow"}))
            .await
            .unwrap();
        assert!(!r2.success);
        assert_eq!(counter.load(Ordering::SeqCst), 0);

        // Call 3: safe call — should succeed because budget was refunded.
        let r3 = tool
            .execute(serde_json::json!({"safe": "path"}))
            .await
            .unwrap();
        assert!(r3.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn rate_limit_does_not_refund_on_business_failure() {
        let sec = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            max_actions_per_hour: 1,
            ..SecurityPolicy::default()
        });
        let (inner, counter) = CountingTool::new();
        let tool = RateLimitedTool::new(inner, sec);

        // Call 1: business failure (error without SECURITY_BLOCK_PREFIX) — budget consumed.
        let r1 = tool
            .execute(serde_json::json!({"fail": true}))
            .await
            .unwrap();
        assert!(!r1.success);
        assert_eq!(counter.load(Ordering::SeqCst), 1);

        // Call 2: budget exhausted — should be rate-limited.
        let r2 = tool
            .execute(serde_json::json!({"safe": "path"}))
            .await
            .unwrap();
        assert!(!r2.success);
        assert!(r2.error.unwrap().contains("Rate limit"));
    }
}
