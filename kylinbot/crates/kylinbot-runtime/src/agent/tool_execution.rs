//! Tool execution helpers extracted from `loop_`.
//!
//! Contains the functions responsible for invoking tools (single, parallel,
//! sequential) and the decision logic for choosing between parallel and
//! sequential execution.

use anyhow::Result;
use std::time::{Duration, Instant};
use tokio_util::sync::CancellationToken;

use crate::approval::ApprovalManager;
use crate::observability::{Observer, ObserverEvent};
use crate::tools::Tool;
use crate::util::truncate_with_ellipsis;

// Items that still live in `loop_` — import via the parent module.
use super::loop_::{
    ParsedToolCall, ToolLoopCancelled, current_estop_domain_matcher, current_estop_state,
    current_security_config, scrub_credentials_if_enabled,
};

// ── Helpers ──────────────────────────────────────────────────────────────

/// Look up a tool by name in a slice of boxed `dyn Tool` values.
pub fn find_tool<'a>(tools: &'a [Box<dyn Tool>], name: &str) -> Option<&'a dyn Tool> {
    tools.iter().find(|t| t.name() == name).map(|t| t.as_ref())
}

// ── Estop gate ───────────────────────────────────────────────────────────

/// Check whether the emergency-stop state would block this tool call.
///
/// Returns `Some(reason)` to block (caller SHOULD synthesize a failed
/// `ToolExecutionOutcome` from `reason`); returns `None` to allow.
///
/// Gating order: master switch (`security.enabled`) → mechanism switch
/// (`security.estop_enabled`) → estop state. When any switch is off, or no
/// EstopManager has been installed, the function returns `None` (allow).
pub(crate) fn check_estop_blocked(
    tool_name: &str,
    arguments: &serde_json::Value,
) -> Option<String> {
    let security = current_security_config();
    let state = current_estop_state()?;
    let cached_matcher = current_estop_domain_matcher();
    check_estop_blocked_pure(security, &state, cached_matcher, tool_name, arguments)
}

/// Pure variant of `check_estop_blocked` for unit testing — no OnceLock reads.
/// `cached_matcher` is the pre-built `DomainMatcher` from `EstopManager`,
/// avoiding per-call allocation in the hot path.
fn check_estop_blocked_pure(
    security: Option<&kylinbot_config::scattered_types::SecurityConfig>,
    state: &crate::security::EstopState,
    cached_matcher: Option<&crate::security::domain_matcher::DomainMatcher>,
    tool_name: &str,
    arguments: &serde_json::Value,
) -> Option<String> {
    if let Some(cfg) = security
        && (!cfg.enabled || !cfg.estop_enabled)
    {
        return None;
    }

    if state.kill_all {
        return Some("Emergency stop active (KillAll). All tool execution blocked.".to_string());
    }

    let lower = tool_name.to_ascii_lowercase();

    if state.frozen_tools.iter().any(|t| t == &lower) {
        return Some(format!("Emergency stop active (tool frozen: {tool_name})."));
    }

    if state.network_kill && matches!(lower.as_str(), "http_request" | "curl" | "wget") {
        return Some("Emergency stop active (network blocked). Network tools blocked.".to_string());
    }

    if state.network_kill
        && lower == "shell"
        && let Some(cmd) = arguments.get("command").and_then(|v| v.as_str())
    {
        let cmd_lower = cmd.to_ascii_lowercase();
        if cmd_lower.contains("curl") || cmd_lower.contains("wget") {
            return Some(
                "Emergency stop active (network blocked). Network commands blocked.".to_string(),
            );
        }
    }

    if let Some(matcher) = cached_matcher
        && lower == "http_request"
        && let Some(url) = arguments.get("url").and_then(|v| v.as_str())
        && matcher.is_gated(url)
    {
        let pattern_list = state.blocked_domains.join(", ");
        return Some(format!(
            "Emergency stop active (domain blocked: {pattern_list})."
        ));
    }

    None
}

/// Synthesize a failed `ToolExecutionOutcome` describing an estop block.
fn estop_blocked_outcome(
    call_name: &str,
    observer: &dyn Observer,
    reason: String,
) -> ToolExecutionOutcome {
    observer.record_event(&ObserverEvent::ToolCallStart {
        tool: call_name.to_string(),
        arguments: None,
    });
    observer.record_event(&ObserverEvent::ToolCall {
        tool: call_name.to_string(),
        duration: Duration::ZERO,
        success: false,
    });
    ToolExecutionOutcome {
        output: format!("Error: {reason}"),
        success: false,
        error_reason: Some(reason),
        duration: Duration::ZERO,
        receipt: None,
    }
}

// ── Outcome ──────────────────────────────────────────────────────────────

pub struct ToolExecutionOutcome {
    pub output: String,
    pub success: bool,
    pub error_reason: Option<String>,
    pub duration: Duration,
    /// Cryptographic HMAC receipt proving this tool actually executed.
    /// Present only when tool receipts are enabled in config.
    pub receipt: Option<String>,
}

// ── Single tool execution ────────────────────────────────────────────────

pub async fn execute_one_tool(
    call_name: &str,
    call_arguments: serde_json::Value,
    tools_registry: &[Box<dyn Tool>],
    activated_tools: Option<&std::sync::Arc<std::sync::Mutex<crate::tools::ActivatedToolSet>>>,
    observer: &dyn Observer,
    cancellation_token: Option<&CancellationToken>,
    receipt_generator: Option<&super::tool_receipts::ReceiptGenerator>,
) -> Result<ToolExecutionOutcome> {
    let args_summary = truncate_with_ellipsis(&call_arguments.to_string(), 300);
    observer.record_event(&ObserverEvent::ToolCallStart {
        tool: call_name.to_string(),
        arguments: Some(args_summary),
    });
    let start = Instant::now();

    let static_tool = find_tool(tools_registry, call_name);
    let activated_arc = if static_tool.is_none() {
        activated_tools.and_then(|at| {
            at.lock()
                .unwrap_or_else(|e| e.into_inner())
                .get_resolved(call_name)
        })
    } else {
        None
    };
    let Some(tool) = static_tool.or(activated_arc.as_deref()) else {
        let reason = format!("Unknown tool: {call_name}");
        let duration = start.elapsed();
        observer.record_event(&ObserverEvent::ToolCall {
            tool: call_name.to_string(),
            duration,
            success: false,
        });
        return Ok(ToolExecutionOutcome {
            output: reason.clone(),
            success: false,
            error_reason: Some(scrub_credentials_if_enabled(&reason)),
            duration,
            receipt: None,
        });
    };

    let tool_future = tool.execute(call_arguments.clone());
    let tool_result = if let Some(token) = cancellation_token {
        tokio::select! {
            () = token.cancelled() => return Err(ToolLoopCancelled.into()),
            result = tool_future => result,
        }
    } else {
        tool_future.await
    };

    match tool_result {
        Ok(r) => {
            let duration = start.elapsed();
            observer.record_event(&ObserverEvent::ToolCall {
                tool: call_name.to_string(),
                duration,
                success: r.success,
            });
            if r.success {
                let normalized_output = if r.output.is_empty() {
                    "(no output)"
                } else {
                    &r.output
                };
                let output = scrub_credentials_if_enabled(normalized_output);
                let receipt = receipt_generator.map(|receipt_gen| {
                    receipt_gen.generate_now(call_name, &call_arguments, &output)
                });
                Ok(ToolExecutionOutcome {
                    output,
                    success: true,
                    error_reason: None,
                    duration,
                    receipt,
                })
            } else {
                let reason = r.error.unwrap_or(r.output);
                Ok(ToolExecutionOutcome {
                    output: format!("Error: {reason}"),
                    success: false,
                    error_reason: Some(scrub_credentials_if_enabled(&reason)),
                    duration,
                    receipt: None,
                })
            }
        }
        Err(e) => {
            let duration = start.elapsed();
            observer.record_event(&ObserverEvent::ToolCall {
                tool: call_name.to_string(),
                duration,
                success: false,
            });
            let reason = format!("Error executing {call_name}: {e}");
            Ok(ToolExecutionOutcome {
                output: reason.clone(),
                success: false,
                error_reason: Some(scrub_credentials_if_enabled(&reason)),
                duration,
                receipt: None,
            })
        }
    }
}

// ── Parallel / sequential decision ───────────────────────────────────────

pub fn should_execute_tools_in_parallel(
    tool_calls: &[ParsedToolCall],
    approval: Option<&ApprovalManager>,
) -> bool {
    if tool_calls.len() <= 1 {
        return false;
    }

    // tool_search activates deferred MCP tools into ActivatedToolSet.
    // Running tool_search in parallel with the tools it activates causes a
    // race condition where the tool lookup happens before activation completes.
    // Force sequential execution whenever tool_search is in the batch.
    if tool_calls.iter().any(|call| call.name == "tool_search") {
        return false;
    }

    if let Some(mgr) = approval
        && tool_calls.iter().any(|call| mgr.needs_approval(&call.name))
    {
        // Approval-gated calls must keep sequential handling so the caller can
        // enforce CLI prompt/deny policy consistently.
        return false;
    }

    true
}

// ── Parallel execution ───────────────────────────────────────────────────

pub async fn execute_tools_parallel(
    tool_calls: &[ParsedToolCall],
    tools_registry: &[Box<dyn Tool>],
    activated_tools: Option<&std::sync::Arc<std::sync::Mutex<crate::tools::ActivatedToolSet>>>,
    observer: &dyn Observer,
    cancellation_token: Option<&CancellationToken>,
    receipt_generator: Option<&super::tool_receipts::ReceiptGenerator>,
) -> Result<Vec<ToolExecutionOutcome>> {
    enum Slot<F> {
        Blocked(ToolExecutionOutcome),
        Running(F),
    }

    let slots: Vec<_> = tool_calls
        .iter()
        .map(|call| {
            if let Some(reason) = check_estop_blocked(&call.name, &call.arguments) {
                Slot::Blocked(estop_blocked_outcome(&call.name, observer, reason))
            } else {
                Slot::Running(execute_one_tool(
                    &call.name,
                    call.arguments.clone(),
                    tools_registry,
                    activated_tools,
                    observer,
                    cancellation_token,
                    receipt_generator,
                ))
            }
        })
        .collect();

    let mut outcomes = Vec::with_capacity(slots.len());
    let mut running = Vec::new();
    let mut running_indices = Vec::new();
    for (idx, slot) in slots.into_iter().enumerate() {
        match slot {
            Slot::Blocked(outcome) => outcomes.push((idx, Ok(outcome))),
            Slot::Running(fut) => {
                running_indices.push(idx);
                running.push(fut);
            }
        }
    }

    let run_results = futures_util::future::join_all(running).await;
    for (idx, result) in running_indices.into_iter().zip(run_results) {
        outcomes.push((idx, result));
    }
    outcomes.sort_by_key(|(idx, _)| *idx);
    outcomes.into_iter().map(|(_, r)| r).collect()
}

// ── Sequential execution ─────────────────────────────────────────────────

pub async fn execute_tools_sequential(
    tool_calls: &[ParsedToolCall],
    tools_registry: &[Box<dyn Tool>],
    activated_tools: Option<&std::sync::Arc<std::sync::Mutex<crate::tools::ActivatedToolSet>>>,
    observer: &dyn Observer,
    cancellation_token: Option<&CancellationToken>,
    receipt_generator: Option<&super::tool_receipts::ReceiptGenerator>,
) -> Result<Vec<ToolExecutionOutcome>> {
    let mut outcomes = Vec::with_capacity(tool_calls.len());

    for call in tool_calls {
        if let Some(reason) = check_estop_blocked(&call.name, &call.arguments) {
            outcomes.push(estop_blocked_outcome(&call.name, observer, reason));
            continue;
        }
        outcomes.push(
            execute_one_tool(
                &call.name,
                call.arguments.clone(),
                tools_registry,
                activated_tools,
                observer,
                cancellation_token,
                receipt_generator,
            )
            .await?,
        );
    }

    Ok(outcomes)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::security::EstopState;
    use kylinbot_config::scattered_types::SecurityConfig;
    use serde_json::json;

    fn enabled_security() -> SecurityConfig {
        SecurityConfig::default()
    }

    #[test]
    fn allow_when_state_clean() {
        let state = EstopState::default();
        let sec = enabled_security();
        assert!(check_estop_blocked_pure(Some(&sec), &state, None, "shell", &json!({})).is_none());
    }

    #[test]
    fn kill_all_blocks_everything() {
        let state = EstopState {
            kill_all: true,
            ..Default::default()
        };
        let sec = enabled_security();
        let reason = check_estop_blocked_pure(Some(&sec), &state, None, "file_read", &json!({}))
            .expect("kill_all should block");
        assert!(reason.contains("KillAll"));
    }

    #[test]
    fn frozen_tools_block_matching_name() {
        let state = EstopState {
            frozen_tools: vec!["shell".to_string()],
            ..Default::default()
        };
        let sec = enabled_security();
        assert!(
            check_estop_blocked_pure(Some(&sec), &state, None, "shell", &json!({"command": "ls"}))
                .is_some()
        );
        assert!(
            check_estop_blocked_pure(Some(&sec), &state, None, "file_read", &json!({})).is_none(),
            "non-frozen tools should pass"
        );
    }

    #[test]
    fn frozen_tools_match_case_insensitive() {
        let state = EstopState {
            frozen_tools: vec!["shell".to_string()],
            ..Default::default()
        };
        let sec = enabled_security();
        assert!(check_estop_blocked_pure(Some(&sec), &state, None, "SHELL", &json!({})).is_some());
    }

    #[test]
    fn network_kill_blocks_direct_network_tools() {
        let state = EstopState {
            network_kill: true,
            ..Default::default()
        };
        let sec = enabled_security();
        for tool in ["http_request", "curl", "wget"] {
            assert!(
                check_estop_blocked_pure(Some(&sec), &state, None, tool, &json!({})).is_some(),
                "{tool} should be blocked"
            );
        }
        assert!(
            check_estop_blocked_pure(Some(&sec), &state, None, "file_read", &json!({})).is_none(),
            "non-network tools pass"
        );
    }

    #[test]
    fn network_kill_blocks_shell_curl_wget_commands() {
        let state = EstopState {
            network_kill: true,
            ..Default::default()
        };
        let sec = enabled_security();
        assert!(
            check_estop_blocked_pure(
                Some(&sec),
                &state,
                None,
                "shell",
                &json!({"command": "curl https://example.com"})
            )
            .is_some()
        );
        assert!(
            check_estop_blocked_pure(
                Some(&sec),
                &state,
                None,
                "shell",
                &json!({"command": "wget -q https://x"})
            )
            .is_some()
        );
        assert!(
            check_estop_blocked_pure(Some(&sec), &state, None, "shell", &json!({"command": "ls -la"}))
                .is_none(),
            "benign shell commands pass"
        );
    }

    #[test]
    fn blocked_domains_block_http_request_by_url() {
        let state = EstopState {
            blocked_domains: vec!["example.com".to_string()],
            ..Default::default()
        };
        let matcher =
            crate::security::domain_matcher::DomainMatcher::new(&state.blocked_domains, &[])
                .unwrap();
        let sec = enabled_security();
        assert!(
            check_estop_blocked_pure(
                Some(&sec),
                &state,
                Some(&matcher),
                "http_request",
                &json!({"url": "https://example.com/path"})
            )
            .is_some()
        );
        assert!(
            check_estop_blocked_pure(
                Some(&sec),
                &state,
                Some(&matcher),
                "http_request",
                &json!({"url": "https://allowed.com/"})
            )
            .is_none(),
            "non-matching domain passes"
        );
    }

    #[test]
    fn master_switch_off_bypasses_estop() {
        let state = EstopState {
            kill_all: true,
            ..Default::default()
        };
        let sec = SecurityConfig {
            enabled: false,
            ..SecurityConfig::default()
        };
        assert!(check_estop_blocked_pure(Some(&sec), &state, None, "shell", &json!({})).is_none());
    }

    #[test]
    fn mechanism_switch_off_bypasses_estop() {
        let state = EstopState {
            kill_all: true,
            ..Default::default()
        };
        let sec = SecurityConfig {
            estop_enabled: false,
            ..SecurityConfig::default()
        };
        assert!(check_estop_blocked_pure(Some(&sec), &state, None, "shell", &json!({})).is_none());
    }

    #[test]
    fn no_security_config_defaults_enabled() {
        let state = EstopState {
            kill_all: true,
            ..Default::default()
        };
        assert!(check_estop_blocked_pure(None, &state, None, "shell", &json!({})).is_some());
    }

    #[test]
    fn master_and_mechanism_off_bypasses_estop() {
        let state = EstopState {
            kill_all: true,
            ..Default::default()
        };
        let sec = SecurityConfig {
            enabled: false,
            estop_enabled: false,
            ..SecurityConfig::default()
        };
        assert!(check_estop_blocked_pure(Some(&sec), &state, None, "shell", &json!({})).is_none());
    }
}
