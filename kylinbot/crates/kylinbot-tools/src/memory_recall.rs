use async_trait::async_trait;
use kylinbot_api::memory_traits::{RetrievalMode, SummaryItem};
use kylinbot_api::provider::ChatMessage;
use kylinbot_api::tool::{Tool, ToolResult};
use kylinbot_infra::session_store::SessionStore;
use kylinbot_memory::{Memory, MemoryCategory};
use serde_json::json;
use std::fmt::Write;
use std::path::PathBuf;
use std::sync::Arc;

/// Let the agent search its own memory
pub struct MemoryRecallTool {
    memory: Arc<dyn Memory>,
    workspace_dir: Option<PathBuf>,
}

impl MemoryRecallTool {
    pub fn new(memory: Arc<dyn Memory>) -> Self {
        Self {
            memory,
            workspace_dir: None,
        }
    }

    pub fn with_workspace(memory: Arc<dyn Memory>, workspace_dir: PathBuf) -> Self {
        Self {
            memory,
            workspace_dir: Some(workspace_dir),
        }
    }

    fn is_vectordb(&self) -> bool {
        self.memory.name() == "vectordb"
    }

    fn load_session_messages(&self, session_key: &str) -> Vec<ChatMessage> {
        let Some(workspace_dir) = &self.workspace_dir else {
            return Vec::new();
        };
        let Ok(store) = SessionStore::new(workspace_dir) else {
            return Vec::new();
        };
        store.load(session_key)
    }

    fn transcript_window_for_turn(
        messages: &[ChatMessage],
        turn_num: usize,
    ) -> Vec<(usize, String)> {
        if turn_num == 0 {
            return Vec::new();
        }

        let mut turns = Vec::new();
        let mut current_user: Option<String> = None;
        let mut current_assistant = String::new();
        let mut current_turn = 0usize;

        for msg in messages {
            match msg.role.as_str() {
                "user" => {
                    if current_user.is_some() || !current_assistant.is_empty() {
                        current_turn += 1;
                        let rendered = format!(
                            "User: {}\nAssistant: {}",
                            current_user.take().unwrap_or_default(),
                            current_assistant.trim()
                        );
                        turns.push((current_turn, rendered));
                        current_assistant.clear();
                    }
                    current_user = Some(msg.content.clone());
                }
                "assistant" => {
                    if current_assistant.is_empty() {
                        current_assistant = msg.content.clone();
                    } else {
                        current_assistant.push('\n');
                        current_assistant.push_str(&msg.content);
                    }
                }
                _ => {}
            }
        }

        if current_user.is_some() || !current_assistant.is_empty() {
            current_turn += 1;
            let rendered = format!(
                "User: {}\nAssistant: {}",
                current_user.unwrap_or_default(),
                current_assistant.trim()
            );
            turns.push((current_turn, rendered));
        }

        let window_start = turn_num.saturating_sub(1).max(1);
        turns
            .into_iter()
            .filter(|(idx, _)| *idx >= window_start && *idx <= turn_num)
            .collect()
    }

    fn append_turn_replay(&self, output: &mut String, summary: &SummaryItem) -> std::fmt::Result {
        let Some(turn_num) = summary.turn_num else {
            return Ok(());
        };
        let session_key = summary
            .transcript_session_key
            .as_deref()
            .unwrap_or(&summary.session_id);
        let messages = self.load_session_messages(session_key);
        if messages.is_empty() {
            return Ok(());
        }
        let window = Self::transcript_window_for_turn(&messages, turn_num as usize);
        if window.is_empty() {
            return Ok(());
        }

        writeln!(output, "  Turns:")?;
        for (idx, rendered) in window {
            writeln!(
                output,
                "  [turn {idx} | {}] {rendered}",
                summary.generated_at
            )?;
        }
        Ok(())
    }

    fn render_summary_entry(
        output: &mut String,
        summary: &SummaryItem,
        score: &str,
        include_turns: bool,
        replay_turns: impl Fn(&mut String, &SummaryItem) -> std::fmt::Result,
    ) -> std::fmt::Result {
        let topic = if summary.summary.trim().is_empty() {
            "summary"
        } else {
            summary.summary.trim()
        };
        writeln!(output, "- [summary] {topic}{score}")?;
        if include_turns {
            replay_turns(output, summary)?;
        }
        writeln!(output, "  Session: {}", summary.session_id)?;
        if let Some(turn_num) = summary.turn_num {
            writeln!(output, "  Turn: {turn_num}")?;
        }
        if !summary.summary_text.trim().is_empty() {
            writeln!(output, "  Summary Text: {}", summary.summary_text.trim())?;
        }
        if !summary.learned.is_empty() {
            writeln!(output, "  Learned: {}", summary.learned.join("; "))?;
        }
        if !summary.key_facts.is_empty() {
            writeln!(output, "  Key facts: {}", summary.key_facts.join("; "))?;
        }
        if !summary.temporal_facts.is_empty(){
            writeln!(output, "  Temporal facts: {}", summary.temporal_facts.join("; "))?;
        }
        if !summary.buffer_info.is_empty(){
            writeln!(output, "  Buffer info facts: {}", summary.buffer_info.join("; "))?;
        }
        if !summary.user_intents_and_slots.is_empty() {
            writeln!(
                output,
                "  Intents: {}",
                summary.user_intents_and_slots.join("; ")
            )?;
        }
        Ok(())
    }

    fn render_entry(
        &self,
        output: &mut String,
        entry: &kylinbot_memory::MemoryEntry,
        retrieval_mode: RetrievalMode,
    ) -> std::fmt::Result {
        let score = entry
            .score
            .map_or_else(String::new, |s| format!(" [{:.0}%]", s * 100.0));
        if entry.category == MemoryCategory::Summary
            && let Ok(summary) = serde_json::from_str::<SummaryItem>(&entry.content)
        {
            return match retrieval_mode {
                RetrievalMode::SummaryOnly => {
                    Self::render_summary_entry(output, &summary, &score, false, |_, _| Ok(()))
                }
                RetrievalMode::SummaryWithTurns => {
                    Self::render_summary_entry(output, &summary, &score, true, |out, item| {
                        self.append_turn_replay(out, item)
                    })
                }
                RetrievalMode::TurnsOnly => {
                    let mut turn_only = String::new();
                    self.append_turn_replay(&mut turn_only, &summary)?;
                    if !turn_only.is_empty() {
                        writeln!(output, "- [session {}]", summary.session_id)?;
                        write!(output, "{turn_only}")?;
                    }
                    Ok(())
                }
            };
        }
        if retrieval_mode == RetrievalMode::TurnsOnly {
            return Ok(());
        }
        writeln!(
            output,
            "- [{}] {}: {}{score}",
            entry.category, entry.key, entry.content
        )
    }
}

#[async_trait]
impl Tool for MemoryRecallTool {
    fn name(&self) -> &str {
        "memory_recall"
    }

    fn description(&self) -> &str {
        "Search long-term memory for relevant facts, summaries, and prior conversation context. Returns scored results ranked by relevance. When using the vectordb backend, this tool can also replay session turns from local transcript JSONL files by setting retrieval_mode to 'summary_with_turns' or 'turns_only'. Use it when the user asks to review, resume, trace back, replay, or inspect what was said earlier."
    }

    fn parameters_schema(&self) -> serde_json::Value {
        json!({
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Keywords or phrase to search for in memory (optional if since/until provided)"
                },
                "limit": {
                    "type": "integer",
                    "description": "Max results to return (default: 5)"
                },
                "session_id": {
                    "type": "string",
                    "description": "Optional session ID filter. Useful when replaying transcript turns for a known session."
                },
                "since": {
                    "type": "string",
                    "description": "Filter memories created at or after this time (RFC 3339, e.g. 2025-03-01T00:00:00Z)"
                },
                "until": {
                    "type": "string",
                    "description": "Filter memories created at or before this time (RFC 3339)"
                },
                "search_mode": {
                    "type": "string",
                    "enum": ["bm25", "embedding", "hybrid"],
                    "description": "Search strategy: bm25 (keyword), embedding (semantic), or hybrid (both). Defaults to config value."
                },
                "retrieval_mode": {
                    "type": "string",
                    "enum": ["summary_only", "summary_with_turns", "turns_only"],
                    "description": "Vectordb retrieval shape. 'summary_only' searches structured summaries only. 'summary_with_turns' searches summaries and replays matching prior session turns from local transcript JSONL files. 'turns_only' replays original prior turns without summary text."
                }
            }
        })
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        let query = args.get("query").and_then(|v| v.as_str()).unwrap_or("");
        let session_id = args.get("session_id").and_then(|v| v.as_str());
        let since = args.get("since").and_then(|v| v.as_str());
        let until = args.get("until").and_then(|v| v.as_str());
        let requested_search_mode = args.get("search_mode").and_then(|v| v.as_str());
        let retrieval_mode = match args.get("retrieval_mode") {
            Some(value) => serde_json::from_value(value.clone())
                .map_err(|e| anyhow::anyhow!("Invalid 'retrieval_mode': {e}"))?,
            None if self.is_vectordb() => RetrievalMode::SummaryWithTurns,
            None => RetrievalMode::SummaryOnly,
        };
        let effective_search_mode = if self.is_vectordb() {
            requested_search_mode.unwrap_or("hybrid")
        } else {
            requested_search_mode.unwrap_or("default")
        };

        if self.is_vectordb() && effective_search_mode != "hybrid" {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(
                    "vectordb memory_recall currently supports only 'hybrid' search_mode. Omit 'search_mode' or set it to 'hybrid'.".into(),
                ),
            });
        }

        if self.is_vectordb() && query.trim().is_empty() {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(
                    "vectordb hybrid recall requires a non-empty 'query'. Time-only recall is not supported on this path.".into(),
                ),
            });
        }

        if !self.is_vectordb() && retrieval_mode != RetrievalMode::SummaryOnly {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(
                    "retrieval_mode values other than 'summary_only' are currently supported only for the vectordb backend.".into(),
                ),
            });
        }

        if query.trim().is_empty() && since.is_none() && until.is_none() {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(
                    "Provide at least 'query' (keywords) or time range ('since'/'until')".into(),
                ),
            });
        }

        // Validate date strings
        if let Some(s) = since
            && chrono::DateTime::parse_from_rfc3339(s).is_err()
        {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(format!(
                    "Invalid 'since' date: {s}. Expected RFC 3339 format, e.g. 2025-03-01T00:00:00Z"
                )),
            });
        }
        if let Some(u) = until
            && chrono::DateTime::parse_from_rfc3339(u).is_err()
        {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(format!(
                    "Invalid 'until' date: {u}. Expected RFC 3339 format, e.g. 2025-03-01T00:00:00Z"
                )),
            });
        }
        if let (Some(s), Some(u)) = (since, until)
            && let (Ok(s_dt), Ok(u_dt)) = (
                chrono::DateTime::parse_from_rfc3339(s),
                chrono::DateTime::parse_from_rfc3339(u),
            )
            && s_dt >= u_dt
        {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some("'since' must be before 'until'".into()),
            });
        }

        #[allow(clippy::cast_possible_truncation)]
        let limit = args
            .get("limit")
            .and_then(serde_json::Value::as_u64)
            .map_or(5, |v| v as usize);

        match self
            .memory
            .recall(query, limit, session_id, since, until)
            .await
        {
            Ok(entries) if entries.is_empty() => Ok(ToolResult {
                success: true,
                output: if self.is_vectordb() {
                    format!(
                        "Search mode: hybrid\nRetrieval mode: {retrieval_mode}\nNo memories found."
                    )
                } else {
                    "No memories found.".into()
                },
                error: None,
            }),
            Ok(entries) => {
                let mut output = if self.is_vectordb() {
                    format!(
                        "Search mode: hybrid\nRetrieval mode: {retrieval_mode}\nFound {} memories:\n",
                        entries.len()
                    )
                } else {
                    format!("Found {} memories:\n", entries.len())
                };
                for entry in &entries {
                    let _ = self.render_entry(&mut output, entry, retrieval_mode);
                }
                Ok(ToolResult {
                    success: true,
                    output,
                    error: None,
                })
            }
            Err(e) => Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(format!("Memory recall failed: {e}")),
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kylinbot_infra::session_store::SessionStore;
    use kylinbot_memory::{MemoryCategory, MemoryEntry, SqliteMemory};
    use tempfile::TempDir;

    fn seeded_mem() -> (TempDir, Arc<dyn Memory>) {
        let tmp = TempDir::new().unwrap();
        let mem = SqliteMemory::new(tmp.path()).unwrap();
        (tmp, Arc::new(mem))
    }

    #[tokio::test]
    async fn recall_empty() {
        let (_tmp, mem) = seeded_mem();
        let tool = MemoryRecallTool::new(mem);
        let result = tool.execute(json!({"query": "anything"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("No memories found"));
    }

    #[tokio::test]
    async fn recall_finds_match() {
        let (_tmp, mem) = seeded_mem();
        mem.store("lang", "User prefers Rust", MemoryCategory::Core, None)
            .await
            .unwrap();
        mem.store("tz", "Timezone is EST", MemoryCategory::Core, None)
            .await
            .unwrap();

        let tool = MemoryRecallTool::new(mem);
        let result = tool.execute(json!({"query": "Rust"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("Rust"));
        assert!(result.output.contains("Found 1"));
    }

    #[tokio::test]
    async fn recall_respects_limit() {
        let (_tmp, mem) = seeded_mem();
        for i in 0..10 {
            mem.store(
                &format!("k{i}"),
                &format!("Rust fact {i}"),
                MemoryCategory::Core,
                None,
            )
            .await
            .unwrap();
        }

        let tool = MemoryRecallTool::new(mem);
        let result = tool
            .execute(json!({"query": "Rust", "limit": 3}))
            .await
            .unwrap();
        assert!(result.success);
        assert!(result.output.contains("Found 3"));
    }

    #[tokio::test]
    async fn recall_requires_query_or_time() {
        let (_tmp, mem) = seeded_mem();
        let tool = MemoryRecallTool::new(mem);
        let result = tool.execute(json!({})).await.unwrap();
        assert!(!result.success);
        assert!(result.error.as_ref().unwrap().contains("at least"));
    }

    #[tokio::test]
    async fn recall_time_only_returns_entries() {
        let (_tmp, mem) = seeded_mem();
        mem.store("lang", "User prefers Rust", MemoryCategory::Core, None)
            .await
            .unwrap();
        let tool = MemoryRecallTool::new(mem);
        // Time-only: since far in past
        let result = tool
            .execute(json!({"since": "2020-01-01T00:00:00Z", "limit": 5}))
            .await
            .unwrap();
        assert!(result.success);
        assert!(result.output.contains("Found 1"));
        assert!(result.output.contains("Rust"));
    }

    struct MockVectorDbMemory {
        entries: Vec<MemoryEntry>,
    }

    #[async_trait]
    impl Memory for MockVectorDbMemory {
        async fn store(
            &self,
            _key: &str,
            _content: &str,
            _category: MemoryCategory,
            _session_id: Option<&str>,
        ) -> anyhow::Result<()> {
            Ok(())
        }

        async fn recall(
            &self,
            _query: &str,
            _limit: usize,
            _session_id: Option<&str>,
            _since: Option<&str>,
            _until: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(self.entries.clone())
        }

        async fn get(&self, _key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            Ok(None)
        }

        async fn list(
            &self,
            _category: Option<&MemoryCategory>,
            _session_id: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(self.entries.clone())
        }

        async fn forget(&self, _key: &str) -> anyhow::Result<bool> {
            Ok(false)
        }

        async fn count(&self) -> anyhow::Result<usize> {
            Ok(self.entries.len())
        }

        async fn health_check(&self) -> bool {
            true
        }

        fn name(&self) -> &str {
            "vectordb"
        }
    }

    #[test]
    fn name_and_schema() {
        let (_tmp, mem) = seeded_mem();
        let tool = MemoryRecallTool::new(mem);
        assert_eq!(tool.name(), "memory_recall");
        assert!(tool.parameters_schema()["properties"]["query"].is_object());
    }

    #[test]
    fn score_formatted_as_percent() {
        let score: Option<f64> = Some(0.63);
        let formatted = score.map_or_else(String::new, |s| format!(" [{:.0}%]", s * 100.0));
        assert_eq!(formatted, " [63%]");

        let score: Option<f64> = Some(0.42);
        let formatted = score.map_or_else(String::new, |s| format!(" [{:.0}%]", s * 100.0));
        assert_eq!(formatted, " [42%]");

        let score: Option<f64> = Some(1.0);
        let formatted = score.map_or_else(String::new, |s| format!(" [{:.0}%]", s * 100.0));
        assert_eq!(formatted, " [100%]");

        let score: Option<f64> = Some(0.0);
        let formatted = score.map_or_else(String::new, |s| format!(" [{:.0}%]", s * 100.0));
        assert_eq!(formatted, " [0%]");

        let score: Option<f64> = None;
        let formatted = score.map_or_else(String::new, |s| format!(" [{:.0}%]", s * 100.0));
        assert_eq!(formatted, "");
    }

    #[test]
    fn schema_includes_search_mode_parameter() {
        let (_tmp, mem) = seeded_mem();
        let tool = MemoryRecallTool::new(mem);
        let schema = tool.parameters_schema();
        let search_mode = &schema["properties"]["search_mode"];
        assert_eq!(search_mode["type"], "string");
        let enum_values = search_mode["enum"].as_array().unwrap();
        assert_eq!(enum_values.len(), 3);
        assert!(enum_values.contains(&json!("bm25")));
        assert!(enum_values.contains(&json!("embedding")));
        assert!(enum_values.contains(&json!("hybrid")));
    }

    #[tokio::test]
    async fn recall_renders_summary_entries_structurally() {
        let summary = SummaryItem {
            memory_id: "summary-1".into(),
            session_id: "sess-a".into(),
            transcript_session_key: Some("sess-a".into()),
            generated_at: "2026-05-23T12:00:00Z".into(),
            summary: "Implementation recap".into(),
            summary_text: "The vectordb memory path was connected.".into(),
            learned: vec!["User wants the backend split into clean commits".into()],
            key_facts: vec!["The branch is based on feat/add-memory-module".into()],
            temporal_facts: vec![],
            buffer_info: vec![],
            user_intents_and_slots: vec!["complete third commit".into()],
            turn_num: Some(4),
        };
        let tool = MemoryRecallTool::new(Arc::new(MockVectorDbMemory {
            entries: vec![MemoryEntry {
                id: "1".into(),
                key: "summary-1".into(),
                content: serde_json::to_string(&summary).unwrap(),
                category: MemoryCategory::Summary,
                timestamp: "2026-05-23T12:00:00Z".into(),
                session_id: Some("sess-a".into()),
                score: Some(0.83),
                namespace: "default".into(),
                importance: None,
                superseded_by: None,
            }],
        }));

        let result = tool
            .execute(json!({"query": "implementation"}))
            .await
            .unwrap();
        assert!(result.success);
        assert!(result.output.contains("Search mode: hybrid"));
        assert!(result.output.contains("Retrieval mode: summary_with_turns"));
        assert!(
            result
                .output
                .contains("[summary] Implementation recap [83%]")
        );
        assert!(result.output.contains("Turn: 4"));
        assert!(
            result
                .output
                .contains("Recap: The vectordb memory path was connected.")
        );
        assert!(
            result
                .output
                .contains("Learned: User wants the backend split into clean commits")
        );
    }

    #[tokio::test]
    async fn sqlite_defaults_to_summary_only_retrieval_mode() {
        let (_tmp, mem) = seeded_mem();
        mem.store("lang", "User prefers Rust", MemoryCategory::Core, None)
            .await
            .unwrap();

        let tool = MemoryRecallTool::new(mem);
        let result = tool.execute(json!({"query": "Rust"})).await.unwrap();

        assert!(result.success);
        assert!(result.error.is_none());
        assert!(result.output.contains("User prefers Rust"));
    }

    #[tokio::test]
    async fn recall_rejects_time_only_vectordb_queries() {
        let tool = MemoryRecallTool::new(Arc::new(MockVectorDbMemory { entries: vec![] }));
        let result = tool
            .execute(json!({"since": "2020-01-01T00:00:00Z"}))
            .await
            .unwrap();
        assert!(!result.success);
        assert!(
            result
                .error
                .as_deref()
                .unwrap_or_default()
                .contains("requires a non-empty 'query'")
        );
    }

    #[tokio::test]
    async fn vectordb_summary_with_turns_replays_local_transcript() {
        let tmp = TempDir::new().unwrap();
        let store = SessionStore::new(tmp.path()).unwrap();
        store
            .append(
                "sess-a",
                &ChatMessage::user("How does the vectordb path work?"),
            )
            .unwrap();
        store
            .append(
                "sess-a",
                &ChatMessage::assistant("It stores summaries and recalls core facts."),
            )
            .unwrap();

        let summary = SummaryItem {
            memory_id: "summary-1".into(),
            session_id: "sess-a".into(),
            transcript_session_key: Some("sess-a".into()),
            generated_at: "2026-05-23T12:00:00Z".into(),
            summary: "Vectordb recap".into(),
            summary_text: "The session discussed vectordb recall.".into(),
            learned: vec![],
            key_facts: vec![],
            temporal_facts: vec![],
            buffer_info: vec![],
            user_intents_and_slots: vec![],
            turn_num: Some(1),
        };

        let tool = MemoryRecallTool::with_workspace(
            Arc::new(MockVectorDbMemory {
                entries: vec![MemoryEntry {
                    id: "1".into(),
                    key: "summary-1".into(),
                    content: serde_json::to_string(&summary).unwrap(),
                    category: MemoryCategory::Summary,
                    timestamp: "2026-05-23T12:00:00Z".into(),
                    session_id: Some("sess-a".into()),
                    score: Some(0.8),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                }],
            }),
            tmp.path().to_path_buf(),
        );

        let result = tool.execute(json!({"query": "vectordb"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("Retrieval mode: summary_with_turns"));
        assert!(result.output.contains("Turns:"));
        assert!(result.output.contains("[turn 1 | 2026-05-23T12:00:00Z]"));
        let turns_index = result.output.find("Turns:").unwrap();
        let session_index = result.output.find("Session:").unwrap();
        let turn_meta_index = result.output.find("  Turn:").unwrap();
        let recap_index = result.output.find("Recap:").unwrap();
        assert!(turns_index < session_index);
        assert!(turns_index < turn_meta_index);
        assert!(turns_index < recap_index);
        assert!(
            result
                .output
                .contains("User: How does the vectordb path work?")
        );
        assert!(
            result
                .output
                .contains("Assistant: It stores summaries and recalls core facts.")
        );
    }

    #[tokio::test]
    async fn vectordb_summary_with_turns_replays_transcript_session_key() {
        let tmp = TempDir::new().unwrap();
        let store = SessionStore::new(tmp.path()).unwrap();
        store
            .append(
                "gw_session-123",
                &ChatMessage::user("Resume the gateway-backed session"),
            )
            .unwrap();
        store
            .append(
                "gw_session-123",
                &ChatMessage::assistant("The transcript is stored under a gw_ key."),
            )
            .unwrap();

        let summary = SummaryItem {
            memory_id: "summary-gw".into(),
            session_id: "session-123".into(),
            transcript_session_key: Some("gw_session-123".into()),
            generated_at: "2026-05-25T12:00:00Z".into(),
            summary: "Gateway recap".into(),
            summary_text: "The session used gateway persistence.".into(),
            learned: vec![],
            key_facts: vec![],
            temporal_facts: vec![],
            buffer_info: vec![],
            user_intents_and_slots: vec![],
            turn_num: Some(1),
        };

        let tool = MemoryRecallTool::with_workspace(
            Arc::new(MockVectorDbMemory {
                entries: vec![MemoryEntry {
                    id: "1".into(),
                    key: "summary-gw".into(),
                    content: serde_json::to_string(&summary).unwrap(),
                    category: MemoryCategory::Summary,
                    timestamp: "2026-05-25T12:00:00Z".into(),
                    session_id: Some("session-123".into()),
                    score: Some(0.8),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                }],
            }),
            tmp.path().to_path_buf(),
        );

        let result = tool
            .execute(json!({"query": "gateway", "retrieval_mode": "summary_with_turns"}))
            .await
            .unwrap();
        assert!(result.success);
        assert!(result.output.contains("Turns:"));
        assert!(
            result
                .output
                .contains("User: Resume the gateway-backed session")
        );
        assert!(
            result
                .output
                .contains("Assistant: The transcript is stored under a gw_ key.")
        );
    }

    #[tokio::test]
    async fn vectordb_turns_only_returns_transcript_without_summary_fields() {
        let tmp = TempDir::new().unwrap();
        let store = SessionStore::new(tmp.path()).unwrap();
        store
            .append("sess-b", &ChatMessage::user("Show me the original turn"))
            .unwrap();
        store
            .append(
                "sess-b",
                &ChatMessage::assistant("Here is the original turn."),
            )
            .unwrap();

        let summary = SummaryItem {
            memory_id: "summary-2".into(),
            session_id: "sess-b".into(),
            transcript_session_key: Some("sess-b".into()),
            generated_at: "2026-05-23T12:00:00Z".into(),
            summary: "Turn replay".into(),
            summary_text: "A turn should be replayed.".into(),
            learned: vec![],
            key_facts: vec![],
            temporal_facts: vec![],
            buffer_info: vec![],
            user_intents_and_slots: vec![],
            turn_num: Some(1),
        };

        let tool = MemoryRecallTool::with_workspace(
            Arc::new(MockVectorDbMemory {
                entries: vec![MemoryEntry {
                    id: "1".into(),
                    key: "summary-2".into(),
                    content: serde_json::to_string(&summary).unwrap(),
                    category: MemoryCategory::Summary,
                    timestamp: "2026-05-23T12:00:00Z".into(),
                    session_id: Some("sess-b".into()),
                    score: Some(0.8),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                }],
            }),
            tmp.path().to_path_buf(),
        );

        let result = tool
            .execute(json!({"query": "original", "retrieval_mode": "turns_only"}))
            .await
            .unwrap();
        assert!(result.success);
        assert!(result.output.contains("Retrieval mode: turns_only"));
        assert!(result.output.contains("[session sess-b]"));
        assert!(result.output.contains("User: Show me the original turn"));
        assert!(!result.output.contains("Recap:"));
    }
}
