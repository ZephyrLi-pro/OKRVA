use async_trait::async_trait;
use kylinbot_api::memory_traits::SummaryItem;
use kylinbot_memory::{
    self, Memory, MemoryBackendKind, MemoryCategory, VectordbFilterParams, VectordbMemory,
    VectordbRecallResult, VectordbSortOrder, decay,
};
use std::fmt::Write;

#[async_trait]
pub trait MemoryLoader: Send + Sync {
    async fn load_context(
        &self,
        memory: &dyn Memory,
        user_message: &str,
        session_id: Option<&str>,
    ) -> anyhow::Result<String>;
}

pub struct DefaultMemoryLoader {
    limit: usize,
    min_relevance_score: f64,
    backend_kind: MemoryBackendKind,
    vectordb_memory: Option<std::sync::Arc<VectordbMemory>>,
}

impl Default for DefaultMemoryLoader {
    fn default() -> Self {
        Self {
            limit: 5,
            min_relevance_score: 0.4,
            backend_kind: MemoryBackendKind::Sqlite,
            vectordb_memory: None,
        }
    }
}

impl DefaultMemoryLoader {
    pub fn new(limit: usize, min_relevance_score: f64) -> Self {
        Self {
            limit: limit.max(1),
            min_relevance_score,
            backend_kind: MemoryBackendKind::Sqlite,
            vectordb_memory: None,
        }
    }

    pub fn with_vectordb(
        limit: usize,
        min_relevance_score: f64,
        vectordb_memory: std::sync::Arc<VectordbMemory>,
    ) -> Self {
        Self {
            limit: limit.max(1),
            min_relevance_score,
            backend_kind: MemoryBackendKind::Vectordb,
            vectordb_memory: Some(vectordb_memory),
        }
    }

    async fn load_latest_summary(
        &self,
        memory: &dyn Memory,
        session_id: Option<&str>,
    ) -> Option<SummaryItem> {
        let category = MemoryCategory::Summary;
        let entries = memory.list(Some(&category), session_id).await.ok()?;

        entries
            .into_iter()
            .filter(|entry| entry.category == MemoryCategory::Summary)
            .filter_map(|entry| serde_json::from_str::<SummaryItem>(&entry.content).ok())
            .max_by(|left, right| {
                left.turn_num
                    .cmp(&right.turn_num)
                    .then_with(|| left.generated_at.cmp(&right.generated_at))
            })
    }

    fn render_workspace_summary(
        context: &mut String,
        summary: &VectordbRecallResult,
    ) -> std::fmt::Result {
        if let Some(ref text) = summary.summary_text
            && !text.trim().is_empty()
        {
            context.push_str("## Current Session State\n");
            writeln!(context, "- Recap: {}", text.trim())?;
        }
        Ok(())
    }

    async fn load_context_vectordb(
        &self,
        memory: &dyn Memory,
        vectordb_memory: &VectordbMemory,
        user_message: &str,
        session_id: Option<&str>,
    ) -> anyhow::Result<String> {
        let mut context = String::from("[Memory context]\n");

        let summary_results = vectordb_memory
            .recall_structured(
                VectordbFilterParams {
                    session_id: session_id.map(str::to_string),
                    category: Some(MemoryCategory::Summary),
                    since: None,
                    until: None,
                    limit: 1,
                },
                VectordbSortOrder::ByTurnNumDesc,
            )
            .await?;
        if let Some(summary) = summary_results.first() {
            Self::render_workspace_summary(&mut context, summary)?;
        }

        let mut core_entries = memory.recall(user_message, 3, None, None, None).await?;
        if memory.name() != "vectordb" {
            decay::apply_time_decay(&mut core_entries, decay::DEFAULT_HALF_LIFE_DAYS);
        }

        if !core_entries.is_empty() {
            context.push_str("\n## Cross-Session Facts\n");
            for entry in core_entries {
                if entry.category != MemoryCategory::Core {
                    continue;
                }
                if kylinbot_memory::is_assistant_autosave_key(&entry.key) {
                    continue;
                }
                if kylinbot_memory::is_user_autosave_key(&entry.key) {
                    continue;
                }
                if kylinbot_memory::should_skip_autosave_content(&entry.content) {
                    continue;
                }
                if let Some(score) = entry.score
                    && score < self.min_relevance_score
                {
                    continue;
                }
                let _ = writeln!(context, "- {}: {}", entry.key, entry.content);
            }
        }

        if context == "[Memory context]\n"
            || context == "[Memory context]\n\n## Cross-Session Facts\n"
        {
            return Ok(String::new());
        }

        context.push_str("[/Memory context]\n\n");
        Ok(context)
    }
}

fn render_summary_item(context: &mut String, item: &SummaryItem) -> std::fmt::Result {
    if !item.summary_text.trim().is_empty() {
        context.push_str("## Current Session State\n");
        writeln!(context, "- Recap: {}", item.summary_text.trim())?;
    }
    Ok(())
}

#[async_trait]
impl MemoryLoader for DefaultMemoryLoader {
    async fn load_context(
        &self,
        memory: &dyn Memory,
        user_message: &str,
        session_id: Option<&str>,
    ) -> anyhow::Result<String> {
        if self.backend_kind == MemoryBackendKind::Vectordb
            && let Some(ref vectordb_memory) = self.vectordb_memory
        {
            return self
                .load_context_vectordb(memory, vectordb_memory.as_ref(), user_message, session_id)
                .await;
        }

        let latest_summary = self.load_latest_summary(memory, session_id).await;
        let mut entries = memory
            .recall(user_message, self.limit, session_id, None, None)
            .await?;
        if entries.is_empty() && latest_summary.is_none() {
            return Ok(String::new());
        }

        if memory.name() != "vectordb" {
            decay::apply_time_decay(&mut entries, decay::DEFAULT_HALF_LIFE_DAYS);
        }

        let mut context = String::from("[Memory context]\n");
        if let Some(summary) = latest_summary {
            render_summary_item(&mut context, &summary)?;
        }
        for entry in entries {
            if entry.category == MemoryCategory::Summary {
                continue;
            }
            if kylinbot_memory::is_assistant_autosave_key(&entry.key) {
                continue;
            }
            if kylinbot_memory::is_user_autosave_key(&entry.key) {
                continue;
            }
            if kylinbot_memory::should_skip_autosave_content(&entry.content) {
                continue;
            }
            if let Some(score) = entry.score
                && score < self.min_relevance_score
            {
                continue;
            }
            let _ = writeln!(context, "- {}: {}", entry.key, entry.content);
        }

        // If all entries were below threshold, return empty
        if context == "[Memory context]\n" {
            return Ok(String::new());
        }

        context.push_str("[/Memory context]\n\n");
        Ok(context)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kylinbot_memory::{Memory, MemoryCategory, MemoryEntry};
    use std::sync::Arc;

    struct MockMemory;
    struct MockMemoryWithEntries {
        entries: Arc<Vec<MemoryEntry>>,
    }

    #[async_trait]
    impl Memory for MockMemory {
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
            limit: usize,
            _session_id: Option<&str>,
            _since: Option<&str>,
            _until: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            if limit == 0 {
                return Ok(vec![]);
            }
            Ok(vec![MemoryEntry {
                id: "1".into(),
                key: "k".into(),
                content: "v".into(),
                category: MemoryCategory::Conversation,
                timestamp: "now".into(),
                session_id: None,
                score: None,
                namespace: "default".into(),
                importance: None,
                superseded_by: None,
            }])
        }

        async fn get(&self, _key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            Ok(None)
        }

        async fn list(
            &self,
            _category: Option<&MemoryCategory>,
            _session_id: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(vec![])
        }

        async fn forget(&self, _key: &str) -> anyhow::Result<bool> {
            Ok(true)
        }

        async fn count(&self) -> anyhow::Result<usize> {
            Ok(0)
        }

        async fn health_check(&self) -> bool {
            true
        }

        fn name(&self) -> &str {
            "mock"
        }
    }

    #[async_trait]
    impl Memory for MockMemoryWithEntries {
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
            Ok(self.entries.as_ref().clone())
        }

        async fn get(&self, _key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            Ok(None)
        }

        async fn list(
            &self,
            category: Option<&MemoryCategory>,
            _session_id: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(self
                .entries
                .iter()
                .filter(|entry| category.is_none_or(|wanted| &entry.category == wanted))
                .cloned()
                .collect())
        }

        async fn forget(&self, _key: &str) -> anyhow::Result<bool> {
            Ok(true)
        }

        async fn count(&self) -> anyhow::Result<usize> {
            Ok(self.entries.len())
        }

        async fn health_check(&self) -> bool {
            true
        }

        fn name(&self) -> &str {
            "mock-with-entries"
        }
    }

    #[tokio::test]
    async fn default_loader_formats_context() {
        let loader = DefaultMemoryLoader::default();
        let context = loader
            .load_context(&MockMemory, "hello", None)
            .await
            .unwrap();
        assert!(context.contains("[Memory context]"));
        assert!(context.contains("- k: v"));
    }

    #[tokio::test]
    async fn default_loader_skips_legacy_assistant_autosave_entries() {
        let loader = DefaultMemoryLoader::new(5, 0.0);
        let memory = MockMemoryWithEntries {
            entries: Arc::new(vec![
                MemoryEntry {
                    id: "1".into(),
                    key: "assistant_resp_legacy".into(),
                    content: "fabricated detail".into(),
                    category: MemoryCategory::Daily,
                    timestamp: "now".into(),
                    session_id: None,
                    score: Some(0.95),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
                MemoryEntry {
                    id: "2".into(),
                    key: "user_fact".into(),
                    content: "User prefers concise answers".into(),
                    category: MemoryCategory::Conversation,
                    timestamp: "now".into(),
                    session_id: None,
                    score: Some(0.9),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
            ]),
        };

        let context = loader
            .load_context(&memory, "answer style", None)
            .await
            .unwrap();
        assert!(context.contains("user_fact"));
        assert!(!context.contains("assistant_resp_legacy"));
        assert!(!context.contains("fabricated detail"));
    }

    #[tokio::test]
    async fn default_loader_skips_user_autosave_entries() {
        let loader = DefaultMemoryLoader::new(5, 0.0);
        let memory = MockMemoryWithEntries {
            entries: Arc::new(vec![
                MemoryEntry {
                    id: "1".into(),
                    key: "user_msg_e5f6g7h8".into(),
                    content: "User message embedding prior context verbatim".into(),
                    category: MemoryCategory::Conversation,
                    timestamp: "now".into(),
                    session_id: None,
                    score: Some(0.95),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
                MemoryEntry {
                    id: "2".into(),
                    key: "user_fact".into(),
                    content: "User prefers concise answers".into(),
                    category: MemoryCategory::Conversation,
                    timestamp: "now".into(),
                    session_id: None,
                    score: Some(0.9),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
            ]),
        };

        let context = loader
            .load_context(&memory, "answer style", None)
            .await
            .unwrap();
        assert!(context.contains("user_fact"));
        assert!(!context.contains("user_msg_e5f6g7h8"));
        assert!(!context.contains("embedding prior context"));
    }

    #[tokio::test]
    async fn default_loader_renders_latest_summary_structurally() {
        let loader = DefaultMemoryLoader::new(5, 0.0);
        let summary = SummaryItem {
            memory_id: "summary-1".into(),
            session_id: "sess-a".into(),
            transcript_session_key: Some("sess-a".into()),
            generated_at: "2026-05-23T12:00:00Z".into(),
            summary: "Preference update".into(),
            summary_text: "User asked for concise technical answers.".into(),
            learned: vec!["User prefers concise technical answers".into()],
            key_facts: vec!["The active branch is feat/add-memory-module".into()],
            temporal_facts: vec!["The work is happening on 2026-05-23".into()],
            buffer_info: vec![],
            user_intents_and_slots: vec!["finish memory backend implementation".into()],
            turn_num: Some(3),
        };
        let memory = MockMemoryWithEntries {
            entries: Arc::new(vec![
                MemoryEntry {
                    id: "1".into(),
                    key: "summary-1".into(),
                    content: serde_json::to_string(&summary).unwrap(),
                    category: MemoryCategory::Summary,
                    timestamp: "2026-05-23T12:00:00Z".into(),
                    session_id: Some("sess-a".into()),
                    score: Some(0.99),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
                MemoryEntry {
                    id: "2".into(),
                    key: "user_fact".into(),
                    content: "User prefers concise answers".into(),
                    category: MemoryCategory::Conversation,
                    timestamp: "2026-05-23T12:01:00Z".into(),
                    session_id: Some("sess-a".into()),
                    score: Some(0.9),
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
            ]),
        };

        let context = loader
            .load_context(&memory, "answer style", Some("sess-a"))
            .await
            .unwrap();

        assert!(context.contains("## Current Session State"));
        assert!(context.contains("Recap: User asked for concise technical answers."));
        assert!(!context.contains("Turn 3: Preference update"));
        assert!(!context.contains("Learned: User prefers concise technical answers"));
        assert!(!context.contains("Intent: finish memory backend implementation"));
        assert!(context.contains("- user_fact: User prefers concise answers"));
        assert!(!context.contains("\"summary_text\""));
    }

    #[tokio::test]
    async fn default_loader_skips_empty_latest_summary_text() {
        let loader = DefaultMemoryLoader::new(5, 0.0);
        let summary = SummaryItem {
            memory_id: "summary-1".into(),
            session_id: "sess-a".into(),
            transcript_session_key: Some("sess-a".into()),
            generated_at: "2026-05-23T12:00:00Z".into(),
            summary: "Preference update".into(),
            summary_text: "   ".into(),
            learned: vec!["User prefers concise technical answers".into()],
            key_facts: vec![],
            temporal_facts: vec![],
            buffer_info: vec![],
            user_intents_and_slots: vec!["finish memory backend implementation".into()],
            turn_num: Some(3),
        };
        let memory = MockMemoryWithEntries {
            entries: Arc::new(vec![MemoryEntry {
                id: "1".into(),
                key: "summary-1".into(),
                content: serde_json::to_string(&summary).unwrap(),
                category: MemoryCategory::Summary,
                timestamp: "2026-05-23T12:00:00Z".into(),
                session_id: Some("sess-a".into()),
                score: Some(0.99),
                namespace: "default".into(),
                importance: None,
                superseded_by: None,
            }]),
        };

        let context = loader
            .load_context(&memory, "answer style", Some("sess-a"))
            .await
            .unwrap();

        assert_eq!(context, "");
    }

    #[tokio::test]
    async fn vectordb_loader_injects_cross_session_core_facts() {
        let tmp = tempfile::TempDir::new().unwrap();
        let vectordb_memory =
            std::sync::Arc::new(VectordbMemory::default_config(tmp.path().to_path_buf()));
        let loader = DefaultMemoryLoader::with_vectordb(5, 0.0, vectordb_memory);
        let memory = MockMemoryWithEntries {
            entries: Arc::new(vec![MemoryEntry {
                id: "2".into(),
                key: "project_stack".into(),
                content: "Project uses Rust".into(),
                category: MemoryCategory::Core,
                timestamp: "2026-05-23T12:01:00Z".into(),
                session_id: Some("other-session".into()),
                score: Some(0.91),
                namespace: "default".into(),
                importance: None,
                superseded_by: None,
            }]),
        };

        let context = loader
            .load_context(&memory, "vectordb context", Some("sess-a"))
            .await
            .unwrap();

        assert!(context.contains("## Cross-Session Facts"));
        assert!(context.contains("- project_stack: Project uses Rust"));
    }
}
