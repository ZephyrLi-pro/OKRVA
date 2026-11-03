//! Kart provider implementation using the native SDK.
//!
//! This provider uses the kylin-genai-nlp SDK for direct integration
//! with Kart AI services.
//!
//! # Tool calling architecture
//!
//! The SDK uses a **stateful** tool calling protocol:
//!
//! 1. Session is created → tools registered → model invoked
//! 2. Model responds with text + tool calls (via callback)
//! 3. Agent executes tools and calls `continue_with_tool_results()`
//! 4. `submit_tool_result()` sends results back into the same session
//! 5. Model continues generating (may request more tools or finish)
//!
//! The session must survive across tool-call round-trips. This is managed
//! via `ToolCallState` which holds the active session and the pending
//! tool-calls JSON needed by `submit_tool_result`.

#[cfg(feature = "kart-nlp")]
use async_trait::async_trait;
#[cfg(feature = "kart-nlp")]
use futures_util::{StreamExt, stream};
#[cfg(feature = "kart-nlp")]
use std::sync::Arc;
#[cfg(feature = "kart-nlp")]
use tokio::sync::Mutex;

#[cfg(feature = "kart-nlp")]
use kart_nlp_sys::{get_model_list, ChatMessageData, KylinNlpError, KylinNlpSession};

use crate::traits::{Provider, ProviderCapabilities};
#[cfg(feature = "kart-nlp")]
use crate::traits::{
    ChatMessage, ChatRequest, ChatResponse, StreamError, StreamOptions, StreamResult, ToolCall,
};
#[cfg(feature = "kart-nlp")]
use kylinbot_api::tool::ToolSpec;
/// State held between tool-call rounds so the SDK session survives
/// across `chat_with_tools` → tool execution → `continue_with_tool_results`.
#[cfg(feature = "kart-nlp")]
struct ToolCallState {
    /// The SDK session that is mid-conversation with the model.
    session: KylinNlpSession,
    /// The JSON array string of tool calls (as received from the SDK callback)
    /// needed by `submit_tool_result` as the first argument.
    pending_tool_calls_json: String,
}

/// Provider that uses the kylin-genai-nlp native SDK.
pub struct KartProvider {
    /// Session is wrapped in Arc<Mutex> for async access.
    #[cfg(feature = "kart-nlp")]
    session: Arc<Mutex<Option<KylinNlpSession>>>,
    /// Active tool-call state — holds the session that is awaiting
    /// tool results. When `Some`, the next `chat_with_tools` call
    /// will use `submit_tool_result` instead of creating a new session.
    #[cfg(feature = "kart-nlp")]
    tool_call_state: Arc<Mutex<Option<ToolCallState>>>,
    /// Provider capabilities.
    #[cfg(feature = "kart-nlp")]
    capabilities: ProviderCapabilities,
}

impl KartProvider {
    /// Create a new Kart provider.
    #[cfg(feature = "kart-nlp")]
    pub fn new() -> anyhow::Result<Self> {
        if !kart_nlp_sys::is_available() {
            anyhow::bail!(
                "kylin-genai-nlp SDK not available. Install the SDK or set KART_SDK_PATH"
            );
        }

        Ok(Self {
            session: Arc::new(Mutex::new(None)),
            tool_call_state: Arc::new(Mutex::new(None)),
            capabilities: ProviderCapabilities {
                native_tool_calling: false,
                vision: false,
                prompt_caching: false,
            },
        })
    }

    /// Clear the SDK session's conversation history.
    /// Call this before starting a new conversation to ensure a clean state.
    #[cfg(feature = "kart-nlp")]
    pub async fn clear_session(&self) -> anyhow::Result<()> {
        // Acquire tool_call_state first to match the lock order in chat_with_tools
        // (which takes tool_call_state before session). Consistent ordering prevents
        // a potential deadlock when both methods are called concurrently.
        let mut tc_guard = self.tool_call_state.lock().await;
        *tc_guard = None;
        let mut guard = self.session.lock().await;
        if let Some(ref mut session) = *guard {
            let _: () = session.clear_history().map_err(Self::map_error)?;
        }
        Ok(())
    }

    /// Create a new Kart provider (stub implementation when SDK not available).
    #[cfg(not(feature = "kart-nlp"))]
    pub fn new() -> anyhow::Result<Self> {
        anyhow::bail!("kart feature not enabled")
    }

    /// Convert SDK error to anyhow error.
    #[cfg(feature = "kart-nlp")]
    fn map_error(e: KylinNlpError) -> anyhow::Error {
        anyhow::anyhow!("Kart error: {}", e)
    }

    /// Parse tool calls from the SDK's JSON callback string into ToolCall structs.
    #[cfg(feature = "kart-nlp")]
    fn parse_tool_calls(json_str: &str) -> Vec<ToolCall> {
        let parsed = match serde_json::from_str::<serde_json::Value>(json_str) {
            Ok(v) => v,
            Err(_) => return Vec::new(),
        };

        parsed
            .as_array()
            .cloned()
            .unwrap_or_default()
            .into_iter()
            .filter_map(|v| {
                // SDK returns tool_name (not "name") and arguments_json (not "arguments")
                let name = v.get("tool_name").and_then(|n| n.as_str()).unwrap_or("");
                let call_id = v
                    .get("call_id")
                    .and_then(|c| c.as_str())
                    .unwrap_or("")
                    .to_string();
                let arguments = match v.get("arguments_json") {
                    Some(serde_json::Value::String(s)) => s.clone(),
                    Some(other) => serde_json::to_string(other).unwrap_or_else(|_| "{}".to_string()),
                    None => "{}".to_string(),
                };
                if name.is_empty() {
                    return None;
                }
                Some(ToolCall {
                    id: if call_id.is_empty() {
                        format!("kart_tc_{}", name)
                    } else {
                        call_id
                    },
                    name: name.to_string(),
                    arguments,
                })
            })
            .collect()
    }

    /// Convert SDK tool call format to the format expected by submit_tool_result.
    ///
    /// SDK returns: `[{"tool_name": "foo", "call_id": "id", "arguments_json": {...}}]`
    /// submit_tool_result expects: `[{"type": "function", "function": {"name": "foo", "arguments": "{...}"}, "id": "id"}]`
    #[cfg(feature = "kart-nlp")]
    fn convert_tool_calls_for_submit(tool_calls_json: &str) -> Vec<serde_json::Value> {
        let parsed = match serde_json::from_str::<serde_json::Value>(tool_calls_json) {
            Ok(v) => v,
            Err(_) => return Vec::new(),
        };

        parsed
            .as_array()
            .cloned()
            .unwrap_or_default()
            .into_iter()
            .filter_map(|tc| {
                let tool_name = tc.get("tool_name").and_then(|n| n.as_str()).unwrap_or("");
                let call_id = tc.get("call_id").and_then(|c| c.as_str()).unwrap_or("");
                // SDK 通常把 arguments_json 作为字符串字面量返回（内含已序列化的 JSON）。
                // 此时直接取内层字符串，避免再被 JSON-encode 一次（"{}" 变成 "\"{}\"")。
                // 兼容性：若 SDK 改成直接返回对象/数组，则原样序列化。
                let arguments_json = match tc.get("arguments_json") {
                    Some(serde_json::Value::String(s)) => s.clone(),
                    Some(other) => serde_json::to_string(other).unwrap_or_else(|_| "{}".to_string()),
                    None => "{}".to_string(),
                };

                if tool_name.is_empty() {
                    return None;
                }

                Some(serde_json::json!({
                    "type": "function",
                    "id": call_id,
                    "function": {
                        "name": tool_name,
                        "arguments": arguments_json
                    }
                }))
            })
            .collect()
    }

    /// Collect the full response from a chat receiver, handling both
    /// text chunks and tool call callbacks.
    ///
    /// Returns `(text, tool_calls_json_option)`.
    ///
    /// The `is_final` flag from the SDK is not fully trusted because
    /// the tool callback may arrive *after* `is_final`. Instead we
    /// wait for the chat channel to close (receiver returns `None`)
    /// or for a tool callback, whichever comes first.
    #[cfg(feature = "kart-nlp")]
    async fn collect_response(
        chat_rx: &mut tokio::sync::mpsc::UnboundedReceiver<kart_nlp_sys::StreamChunk>,
        tool_rx: &mut tokio::sync::mpsc::UnboundedReceiver<kart_nlp_sys::ToolCallResult>,
    ) -> (String, Option<String>) {
        let mut full_text = String::new();
        let mut tool_calls_json = None;
        let mut chat_done = false;

        loop {
            if chat_done && tool_rx.is_empty() {
                // Chat finished and no pending tool callbacks — we're done.
                break;
            }

            tokio::select! {
                // Priority: check for tool call notification
                tool_result = tool_rx.recv(), if !chat_done || !tool_rx.is_empty() => {
                    match tool_result {
                        Some(result) => {
                            tool_calls_json = Some(result.tools_json.clone());
                            // SDK 在 tool 回调后本轮就结束了，chat 通道不会再发 is_final，
                            // 也不会关闭（callback 上下文随 session 存活）。
                            // 给 chat_rx 一个短窗口排空可能已经在路上的文本块，然后退出。
                            let drain_deadline = tokio::time::Instant::now()
                                + tokio::time::Duration::from_millis(50);
                            loop {
                                tokio::select! {
                                    biased;
                                    _ = tokio::time::sleep_until(drain_deadline) => break,
                                    chunk = chat_rx.recv() => match chunk {
                                        Some(c) => {
                                            if c.error_code != 0 {
                                                let err_msg = c.error_message
                                                    .unwrap_or_else(|| format!("Error code: {}", c.error_code));
                                                tracing::error!("Kart SDK error during drain: {}", err_msg);
                                                break;
                                            }
                                            if let Some(t) = c.text { full_text.push_str(&t); }
                                            if c.is_final { break; }
                                        }
                                        None => break,
                                    }
                                }
                            }
                            break;
                        }
                        None => {
                            // Tool channel closed — no more tool calls coming.
                            if chat_done {
                                break;
                            }
                        }
                    }
                }
                // Collect text chunks
                chunk = chat_rx.recv(), if !chat_done => {
                    match chunk {
                        Some(chunk) => {
                            if chunk.error_code != 0 {
                                let err_msg = chunk
                                    .error_message
                                    .unwrap_or_else(|| format!("Error code: {}", chunk.error_code));
                                tracing::error!("Kart SDK error: {}", err_msg);
                                break;
                            }
                            if let Some(text) = chunk.text {
                                full_text.push_str(&text);
                            }
                            if chunk.is_final {
                                // Mark chat as done but don't break yet —
                                // tool callback may still arrive.
                                chat_done = true;
                            }
                        }
                        None => {
                            // Chat channel closed.
                            chat_done = true;
                        }
                    }
                }
                else => {
                    // Both branches inactive — exit.
                    break;
                }
            }
        }

        (full_text, tool_calls_json)
    }
}

#[cfg(feature = "kart-nlp")]
#[async_trait]
impl Provider for KartProvider {
    fn capabilities(&self) -> ProviderCapabilities {
        self.capabilities.clone()
    }

    async fn list_models(&self) -> anyhow::Result<Vec<String>> {
        let models = get_model_list().map_err(Self::map_error)?;
        Ok(models.into_iter().map(|m| m.model_name).collect())
    }

    fn supports_native_tools(&self) -> bool {
        false
    }

    fn convert_tools(&self, tools: &[ToolSpec]) -> crate::traits::ToolsPayload {
        let tools_json: Vec<serde_json::Value> = tools
            .iter()
            .map(|t| {
                serde_json::json!({
                    "name": t.name,
                    "description": t.description,
                    "parameters": t.parameters,
                })
            })
            .collect();

        crate::traits::ToolsPayload::OpenAI { tools: tools_json }
    }

    async fn chat_with_tools(
        &self,
        messages: &[ChatMessage],
        tools: &[serde_json::Value],
        model: &str,
        temperature: Option<f64>,
    ) -> anyhow::Result<ChatResponse> {
        // ── Check for continuation: if we have pending tool results, use submit_tool_result ──
        let mut tc_guard = self.tool_call_state.lock().await;
        if tc_guard.is_some() {
            let ToolCallState {
                mut session,
                pending_tool_calls_json,
            } = tc_guard.take().expect("checked is_some");

            // Build the tool results JSON from messages with role "tool".
            //
            // Agent dispatcher serializes each result as
            //   {"tool_call_id": "...", "content": "..."}
            // But Kylin SDK's submit_tool_result requires the field to be `id`
            // (error: "ToolResponseMessage must have an id"). Rename here.
            let tool_results: Vec<serde_json::Value> = messages
                .iter()
                .filter(|m| m.role == "tool")
                .filter_map(|m| serde_json::from_str::<serde_json::Value>(&m.content).ok())
                .map(|mut v| {
                    if let Some(obj) = v.as_object_mut() {
                        if !obj.contains_key("id") {
                            if let Some(id) = obj.remove("tool_call_id") {
                                obj.insert("id".to_string(), id);
                            }
                        }
                    }
                    v
                })
                .collect();

            let results_json = serde_json::to_string(&tool_results).unwrap_or_else(|_| "[]".to_string());

            // Convert SDK's tool_calls format to what submit_tool_result expects.
            // SDK returns: [{"tool_name": "foo", "call_id": "id", "arguments_json": {...}}]
            // submit expects: [{"type": "function", "function": {"name": "foo", "arguments": "{...}"}, "id": "id"}]
            let submit_tool_calls = Self::convert_tool_calls_for_submit(&pending_tool_calls_json);
            let submit_tool_calls_json =
                serde_json::to_string(&submit_tool_calls).unwrap_or_else(|_| "[]".to_string());

            // Submit tool results back to the SDK session
            session
                .submit_tool_result(&submit_tool_calls_json, &results_json)
                .map_err(Self::map_error)?;

            // Re-register callbacks to receive the model's continued response
            let mut chat_rx = session.re_register_chat_callback().map_err(Self::map_error)?;
            let mut tool_rx = session.re_register_tool_handler().map_err(Self::map_error)?;

            // Collect the model's response after tool result submission
            let (full_text, tool_calls_json) =
                Self::collect_response(&mut chat_rx, &mut tool_rx).await;

            if let Some(ref json_str) = tool_calls_json {
                // Model requested more tools — store state for next round
                *tc_guard = Some(ToolCallState {
                    session,
                    pending_tool_calls_json: json_str.clone(),
                });
            } else {
                // Model finished — store session back for potential reuse
                let mut session_guard = self.session.lock().await;
                *session_guard = Some(session);
            }

            let tool_calls = tool_calls_json
                .as_deref()
                .map(Self::parse_tool_calls)
                .unwrap_or_default();

            return Ok(ChatResponse {
                text: Some(full_text),
                tool_calls,
                usage: None,
                reasoning_content: None,
            });
        }

        // ── Fresh conversation: create new session ──
        let mut session_guard = self.session.lock().await;

        // Destroy any existing session and create a fresh one
        *session_guard = None;
        let mut session = KylinNlpSession::new().map_err(Self::map_error)?;
        session.init().map_err(Self::map_error)?;

        // Register tools
        for tool in tools {
            let name = tool.get("name").and_then(|v| v.as_str()).unwrap_or("");
            let schema = serde_json::to_string(tool).unwrap_or_default();
            session.register_tool(name, &schema).map_err(Self::map_error)?;
        }

        // Configure tool choice (auto mode, optionally force a specific tool)
        {
            let mut config = KylinNlpSession::create_tool_choice_config(
                0u32 as kart_nlp_sys::GenAiToolChoiceMode,
            )
            .map_err(Self::map_error)?;

            // Check for forced tool override from agent loop
            let forced_tool_name = kylinbot_api::TOOL_CHOICE_OVERRIDE
                .try_with(Clone::clone)
                .ok()
                .flatten();
            if let Some(ref tool_name) = forced_tool_name {
                KylinNlpSession::tool_choice_config_set_specific_tool(config, tool_name)
                    .map_err(Self::map_error)?;
            }

            session
                .set_tool_choice_config(config)
                .map_err(Self::map_error)?;
            KylinNlpSession::destroy_tool_choice_config(&mut config as *mut _ as *mut _)
                .map_err(Self::map_error)?;
        }

        // Set up tool handler to receive tool calls
        let mut tool_rx = session.set_tool_handler().map_err(Self::map_error)?;

        // Set model
        let temp = temperature.unwrap_or(1.0);
        session.set_model(model, temp).map_err(Self::map_error)?;

        // Build history and start chat
        let history: Vec<ChatMessageData> = messages
            .iter()
            .filter(|m| m.role != "tool") // tool messages handled via submit_tool_result
            .map(|m| ChatMessageData {
                role: m.role.clone(),
                content: m.content.clone(),
            })
            .collect();

        let mut chat_rx = session
            .chat_with_history(&history)
            .map_err(Self::map_error)?;

        // Drop the session guard before receiving (session is local var now)
        drop(session_guard);

        // Collect text and wait for tool calls
        let (full_text, tool_calls_json) =
            Self::collect_response(&mut chat_rx, &mut tool_rx).await;

        // If we got tool calls, keep the session alive for the continuation round
        if let Some(ref json_str) = tool_calls_json {
            *tc_guard = Some(ToolCallState {
                session,
                pending_tool_calls_json: json_str.clone(),
            });
        } else {
            // No tool calls — store session back
            let mut session_guard = self.session.lock().await;
            *session_guard = Some(session);
        }

        let tool_calls = tool_calls_json
            .as_deref()
            .map(Self::parse_tool_calls)
            .unwrap_or_default();

        Ok(ChatResponse {
            text: Some(full_text),
            tool_calls,
            usage: None,
            reasoning_content: None,
        })
    }

    async fn chat_with_system(
        &self,
        system_prompt: Option<&str>,
        message: &str,
        model: &str,
        temperature: Option<f64>,
    ) -> anyhow::Result<String> {
        let mut messages: Vec<ChatMessage> = Vec::new();
        if let Some(prompt) = system_prompt {
            messages.push(ChatMessage {
                role: "system".to_string(),
                content: prompt.to_string(),
                turn_num: None,
            });
        }
        messages.push(ChatMessage {
            role: "user".to_string(),
            content: message.to_string(),
            turn_num: None,
        });

        self.chat_with_history(&messages, model, temperature).await
    }

    async fn chat_with_history(
        &self,
        messages: &[ChatMessage],
        model: &str,
        temperature: Option<f64>,
    ) -> anyhow::Result<String> {
        let mut session_guard = self.session.lock().await;

        // Destroy any existing session and create a fresh one.
        *session_guard = None;
        let mut session = KylinNlpSession::new().map_err(Self::map_error)?;
        session.init().map_err(Self::map_error)?;

        // Set model
        let temp = temperature.unwrap_or(1.0);
        session.set_model(model, temp).map_err(Self::map_error)?;

        // Convert messages (exclude tool role — not supported by SDK history API)
        let history: Vec<ChatMessageData> = messages
            .iter()
            .filter(|m| m.role != "tool")
            .map(|m| ChatMessageData {
                role: m.role.clone(),
                content: m.content.clone(),
            })
            .collect();

        // Start streaming chat with history
        let mut rx = session
            .chat_with_history(&history)
            .map_err(Self::map_error)?;

        // Store session before dropping guard
        *session_guard = Some(session);

        // Drop the guard before receiving
        drop(session_guard);

        // Collect all chunks
        let mut full_text = String::new();

        while let Some(chunk) = rx.recv().await {
            if chunk.error_code != 0 {
                let err_msg = chunk
                    .error_message
                    .unwrap_or_else(|| format!("Error code: {}", chunk.error_code));
                return Err(anyhow::anyhow!("SDK error: {}", err_msg));
            }

            if let Some(text) = chunk.text {
                full_text.push_str(&text);
            }

            if chunk.is_final {
                break;
            }
        }

        Ok(full_text)
    }

    fn supports_streaming(&self) -> bool {
        true
    }

    fn stream_chat_with_system(
        &self,
        system_prompt: Option<&str>,
        message: &str,
        model: &str,
        temperature: Option<f64>,
        _options: StreamOptions,
    ) -> stream::BoxStream<'static, StreamResult<crate::traits::StreamChunk>> {
        let session = self.session.clone();
        let model = model.to_string();
        let temperature = temperature.unwrap_or(1.0);
        let system_prompt = system_prompt.map(String::from);
        let message = message.to_string();

        async_stream::try_stream! {
            let mut session_guard = session.lock().await;

            // Create and init session if needed
            if session_guard.is_none() {
                let mut session = KylinNlpSession::new()
                    .map_err(|e| StreamError::Provider(e.to_string()))?;
                session.init()
                    .map_err(|e| StreamError::Provider(e.to_string()))?;
                *session_guard = Some(session);
            }

            let session = session_guard.as_mut()
                .ok_or_else(|| StreamError::Provider("Session not initialized".to_string()))?;

            // Set model
            session.set_model(&model, temperature)
                .map_err(|e| StreamError::Provider(e.to_string()))?;

            // Set system prompt if provided
            if let Some(ref prompt) = system_prompt {
                session.set_system_prompt(prompt)
                    .map_err(|e| StreamError::Provider(e.to_string()))?;
            }

            // Start streaming
            let mut rx = session.chat_stream(&message)
                .map_err(|e| StreamError::Provider(e.to_string()))?;

            // Store session before dropping guard
            // Note: we don't drop the session here because the stream
            // needs the session to stay alive for the callback context.
            // The session_guard is held until the stream completes.

            // Stream chunks
            while let Some(chunk) = rx.recv().await {
                if chunk.error_code != 0 {
                    let msg = chunk.error_message
                        .unwrap_or_else(|| format!("Error code: {}", chunk.error_code));
                    yield crate::traits::StreamChunk::error(msg);
                    break;
                }

                let is_final = chunk.is_final;

                // Emit reasoning chunk first if present
                if let Some(reasoning) = chunk.reasoning {
                    yield crate::traits::StreamChunk::reasoning(reasoning).with_token_estimate();
                }

                // Emit text chunk
                if let Some(text) = chunk.text {
                    yield crate::traits::StreamChunk::delta(text).with_token_estimate();
                }

                if is_final {
                    yield crate::traits::StreamChunk::final_chunk();
                    break;
                }
            }

            // Now drop the guard — stream is done
            drop(session_guard);
        }
        .boxed()
    }

    async fn chat(
        &self,
        request: ChatRequest<'_>,
        model: &str,
        temperature: Option<f64>,
    ) -> anyhow::Result<ChatResponse> {
        if let Some(tools) = request.tools && !tools.is_empty() {
            let tools_json: Vec<serde_json::Value> = tools
                .iter()
                .map(|t| {
                    serde_json::json!({
                        "name": t.name,
                        "description": t.description,
                        "parameters": t.parameters,
                    })
                })
                .collect();
            return self
                .chat_with_tools(request.messages, &tools_json, model, temperature)
                .await;
        }

        let text = self
            .chat_with_history(request.messages, model, temperature)
            .await?;

        Ok(ChatResponse {
            text: Some(text),
            tool_calls: Vec::new(),
            usage: None,
            reasoning_content: None,
        })
    }
}

#[cfg(feature = "kart-nlp")]
impl Default for KartProvider {
    fn default() -> Self {
        Self::new().expect("Failed to create KartProvider")
    }
}

#[cfg(not(feature = "kart-nlp"))]
#[async_trait::async_trait]
impl Provider for KartProvider {
    fn capabilities(&self) -> ProviderCapabilities {
        ProviderCapabilities::default()
    }

    async fn chat_with_system(
        &self,
        _system_prompt: Option<&str>,
        _message: &str,
        _model: &str,
        _temperature: Option<f64>,
    ) -> anyhow::Result<String> {
        anyhow::bail!("kart feature not enabled")
    }
}

// ─── Tests ────────────────────────────────────────────────────────────────────

#[cfg(all(test, feature = "kart-nlp"))]
mod tests {
    use super::*;

    #[test]
    fn test_kart_provider_creation() {
        let provider = KartProvider::new();
        assert!(provider.is_ok());
    }

    #[test]
    fn test_kart_capabilities() {
        let provider = KartProvider::new().expect("Failed to create provider");
        let caps = provider.capabilities();

        assert!(!caps.native_tool_calling, "native_tool_calling should be false (XML mode)");
        assert!(!caps.vision, "vision should be false");
        assert!(!caps.prompt_caching, "prompt_caching should be false");
    }

    #[test]
    fn test_supports_native_tools() {
        let provider = KartProvider::new().expect("Failed to create provider");
        assert!(!provider.supports_native_tools(), "should use XML tool calling mode");
    }

    #[test]
    fn test_supports_streaming() {
        let provider = KartProvider::new().expect("Failed to create provider");
        assert!(provider.supports_streaming(), "should support streaming");
    }

    #[test]
    fn test_convert_tools_returns_openai_format() {
        let provider = KartProvider::new().expect("Failed to create provider");

        let tools = vec![ToolSpec {
            name: "get_weather".to_string(),
            description: "Get weather for a location".to_string(),
            parameters: serde_json::json!({
                "type": "object",
                "properties": {
                    "location": {"type": "string"}
                }
            }),
        }];

        let payload = provider.convert_tools(&tools);

        match payload {
            crate::traits::ToolsPayload::OpenAI { tools: resolved } => {
                assert_eq!(resolved.len(), 1);
                let t = &resolved[0];
                assert_eq!(t.get("name").and_then(|v| v.as_str()), Some("get_weather"));
                assert!(t.get("parameters").is_some());
            }
            crate::traits::ToolsPayload::PromptGuided { .. } => {
                panic!("Expected OpenAI format, got PromptGuided");
            }
            other => {
                panic!("Unexpected tools payload variant: {:?}", other);
            }
        }
    }

    #[test]
    fn test_convert_tools_multiple_tools() {
        let provider = KartProvider::new().expect("Failed to create provider");

        let tools = vec![
            ToolSpec {
                name: "tool_one".to_string(),
                description: "First tool".to_string(),
                parameters: serde_json::json!({}),
            },
            ToolSpec {
                name: "tool_two".to_string(),
                description: "Second tool".to_string(),
                parameters: serde_json::json!({}),
            },
        ];

        let payload = provider.convert_tools(&tools);

        if let crate::traits::ToolsPayload::OpenAI { tools } = payload {
            assert_eq!(tools.len(), 2);
        } else {
            panic!("Expected OpenAI format");
        }
    }

    #[test]
    fn test_parse_tool_calls_valid() {
        let json = r#"[
            {"tool_name": "get_weather", "call_id": "tc_1", "arguments_json": "{\"location\": \"Beijing\"}"},
            {"tool_name": "calculator", "call_id": "tc_2", "arguments_json": "{\"expr\": \"1+1\"}"}
        ]"#;

        let calls = KartProvider::parse_tool_calls(json);
        assert_eq!(calls.len(), 2);
        assert_eq!(calls[0].name, "get_weather");
        assert_eq!(calls[0].id, "tc_1");
        assert_eq!(calls[1].name, "calculator");
    }

    #[test]
    fn test_parse_tool_calls_missing_call_id() {
        let json = r#"[{"tool_name": "bash", "arguments_json": "{}"}]"#;

        let calls = KartProvider::parse_tool_calls(json);
        assert_eq!(calls.len(), 1);
        assert_eq!(calls[0].name, "bash");
        // Should generate a fallback ID
        assert!(calls[0].id.starts_with("kart_tc_"));
    }

    #[test]
    fn test_parse_tool_calls_empty_array() {
        let calls = KartProvider::parse_tool_calls("[]");
        assert!(calls.is_empty());
    }

    #[test]
    fn test_parse_tool_calls_invalid_json() {
        let calls = KartProvider::parse_tool_calls("not json");
        assert!(calls.is_empty());
    }
}
