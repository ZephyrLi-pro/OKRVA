use async_trait::async_trait;
use serde::{Deserialize, Serialize};

/// Filter criteria for bulk memory export (GDPR Art. 20 data portability).
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ExportFilter {
    pub namespace: Option<String>,
    pub session_id: Option<String>,
    pub category: Option<MemoryCategory>,
    /// RFC 3339 lower bound (inclusive) on created_at.
    pub since: Option<String>,
    /// RFC 3339 upper bound (inclusive) on created_at.
    pub until: Option<String>,
}

/// A single message in a conversation trace for procedural memory.
///
/// Used to capture "how to" patterns from tool-calling turns so that
/// backends that support procedural storage can learn from them.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ProceduralMessage {
    pub role: String,
    pub content: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

/// A single memory entry
#[derive(Clone, Serialize, Deserialize)]
pub struct MemoryEntry {
    pub id: String,
    pub key: String,
    pub content: String,
    pub category: MemoryCategory,
    pub timestamp: String,
    pub session_id: Option<String>,
    pub score: Option<f64>,
    /// Namespace for isolation between agents/contexts.
    #[serde(default = "default_namespace")]
    pub namespace: String,
    /// Importance score (0.0–1.0) for prioritized retrieval.
    #[serde(default)]
    pub importance: Option<f64>,
    /// If this entry was superseded by a newer conflicting entry.
    #[serde(default)]
    pub superseded_by: Option<String>,
}

fn default_namespace() -> String {
    "default".into()
}

impl std::fmt::Debug for MemoryEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("MemoryEntry")
            .field("id", &self.id)
            .field("key", &self.key)
            .field("content", &self.content)
            .field("category", &self.category)
            .field("timestamp", &self.timestamp)
            .field("score", &self.score)
            .field("namespace", &self.namespace)
            .field("importance", &self.importance)
            .finish_non_exhaustive()
    }
}

/// Memory categories for organization
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MemoryCategory {
    /// Long-term facts, preferences, decisions
    Core,
    /// Daily session logs
    Daily,
    /// Conversation context
    Conversation,
    /// Structured KylinBot summary
    Summary,
    /// User-defined custom category
    Custom(String),
}

impl serde::Serialize for MemoryCategory {
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(&self.to_string())
    }
}

impl<'de> serde::Deserialize<'de> for MemoryCategory {
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let s = String::deserialize(deserializer)?;
        Ok(match s.as_str() {
            "core" => Self::Core,
            "daily" => Self::Daily,
            "conversation" => Self::Conversation,
            "summary" => Self::Summary,
            _ => Self::Custom(s),
        })
    }
}

impl std::fmt::Display for MemoryCategory {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Core => write!(f, "core"),
            Self::Daily => write!(f, "daily"),
            Self::Conversation => write!(f, "conversation"),
            Self::Summary => write!(f, "summary"),
            Self::Custom(name) => write!(f, "{name}"),
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct SummaryItem {
    pub memory_id: String,
    pub session_id: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub transcript_session_key: Option<String>,
    pub generated_at: String,
    pub summary: String,
    pub summary_text: String,
    #[serde(default)]
    pub learned: Vec<String>,
    #[serde(default)]
    pub key_facts: Vec<String>,
    #[serde(default)]
    pub temporal_facts: Vec<String>,
    #[serde(default)]
    pub buffer_info: Vec<String>,
    #[serde(default)]
    pub user_intents_and_slots: Vec<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub turn_num: Option<i32>,
}

impl SummaryItem {
    pub fn new(
        memory_id: impl Into<String>,
        session_id: impl Into<String>,
        generated_at: impl Into<String>,
        summary: impl Into<String>,
        summary_text: impl Into<String>,
    ) -> Self {
        Self {
            memory_id: memory_id.into(),
            session_id: session_id.into(),
            transcript_session_key: None,
            generated_at: generated_at.into(),
            summary: summary.into(),
            summary_text: summary_text.into(),
            learned: Vec::new(),
            key_facts: Vec::new(),
            temporal_facts: Vec::new(),
            buffer_info: Vec::new(),
            user_intents_and_slots: Vec::new(),
            turn_num: None,
        }
    }

    pub fn has_structured_content(&self) -> bool {
        !self.learned.is_empty()
            || !self.key_facts.is_empty()
            || !self.temporal_facts.is_empty()
            || !self.buffer_info.is_empty()
            || !self.user_intents_and_slots.is_empty()
    }

    pub fn searchable_text(&self) -> String {
        let mut parts = Vec::new();
        push_non_empty(&mut parts, &self.summary);
        push_non_empty(&mut parts, &self.summary_text);
        push_non_empty_list(&mut parts, &self.key_facts);
        push_non_empty_list(&mut parts, &self.temporal_facts);
        push_non_empty_list(&mut parts, &self.buffer_info);
        push_non_empty_list(&mut parts, &self.user_intents_and_slots);
        parts.join("\n")
    }
}

fn push_non_empty(parts: &mut Vec<String>, value: &str) {
    let value = value.trim();
    if !value.is_empty() {
        parts.push(value.to_string());
    }
}

fn push_non_empty_list(parts: &mut Vec<String>, values: &[String]) {
    for value in values {
        push_non_empty(parts, value);
    }
}

#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum RetrievalMode {
    #[default]
    SummaryOnly,
    SummaryWithTurns,
    TurnsOnly,
}

impl std::fmt::Display for RetrievalMode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::SummaryOnly => write!(f, "summary_only"),
            Self::SummaryWithTurns => write!(f, "summary_with_turns"),
            Self::TurnsOnly => write!(f, "turns_only"),
        }
    }
}

/// Core memory trait — implement for any persistence backend
#[async_trait]
pub trait Memory: Send + Sync {
    /// Backend name
    fn name(&self) -> &str;

    /// Store a memory entry, optionally scoped to a session
    async fn store(
        &self,
        key: &str,
        content: &str,
        category: MemoryCategory,
        session_id: Option<&str>,
    ) -> anyhow::Result<()>;

    /// Recall memories matching a query (keyword search), optionally scoped to a session
    /// and time range. Time bounds use RFC 3339 / ISO 8601 format
    /// (e.g. "2025-03-01T00:00:00Z"); inclusive (created_at >= since, created_at <= until).
    async fn recall(
        &self,
        query: &str,
        limit: usize,
        session_id: Option<&str>,
        since: Option<&str>,
        until: Option<&str>,
    ) -> anyhow::Result<Vec<MemoryEntry>>;

    /// Get a specific memory by key
    async fn get(&self, key: &str) -> anyhow::Result<Option<MemoryEntry>>;

    /// List all memory keys, optionally filtered by category and/or session
    async fn list(
        &self,
        category: Option<&MemoryCategory>,
        session_id: Option<&str>,
    ) -> anyhow::Result<Vec<MemoryEntry>>;

    /// Remove a memory by key
    async fn forget(&self, key: &str) -> anyhow::Result<bool>;

    /// Remove all memories in a namespace (category).
    /// Returns the number of deleted entries.
    /// Default: returns unsupported error. Backends that support bulk deletion override this.
    async fn purge_namespace(&self, _namespace: &str) -> anyhow::Result<usize> {
        anyhow::bail!("purge_namespace not supported by this memory backend")
    }

    /// Remove all memories in a session.
    /// Returns the number of deleted entries.
    /// Default: returns unsupported error. Backends that support bulk deletion override this.
    async fn purge_session(&self, _session_id: &str) -> anyhow::Result<usize> {
        anyhow::bail!("purge_session not supported by this memory backend")
    }

    /// Count total memories
    async fn count(&self) -> anyhow::Result<usize>;

    /// Health check
    async fn health_check(&self) -> bool;

    /// Store a conversation trace as procedural memory.
    ///
    /// Backends that support procedural storage override this
    /// to extract "how to" patterns from tool-calling turns.  The default
    /// implementation is a no-op.
    async fn store_procedural(
        &self,
        _messages: &[ProceduralMessage],
        _session_id: Option<&str>,
    ) -> anyhow::Result<()> {
        Ok(())
    }

    /// Recall memories scoped to a specific namespace.
    ///
    /// Default implementation delegates to `recall()` and filters by namespace.
    /// Backends with native namespace support should override for efficiency.
    async fn recall_namespaced(
        &self,
        namespace: &str,
        query: &str,
        limit: usize,
        session_id: Option<&str>,
        since: Option<&str>,
        until: Option<&str>,
    ) -> anyhow::Result<Vec<MemoryEntry>> {
        let entries = self
            .recall(query, limit * 2, session_id, since, until)
            .await?;
        let filtered: Vec<MemoryEntry> = entries
            .into_iter()
            .filter(|e| e.namespace == namespace)
            .take(limit)
            .collect();
        Ok(filtered)
    }

    /// Bulk-export memories matching the given filter criteria.
    ///
    /// Intended for GDPR Art. 20 data portability. Returns entries ordered by
    /// creation time (ascending). Embeddings are excluded.
    ///
    /// Default implementation delegates to `list()` and post-filters on
    /// namespace and time range. Backends with native query support should
    /// override for efficiency.
    async fn export(&self, filter: &ExportFilter) -> anyhow::Result<Vec<MemoryEntry>> {
        let entries = self
            .list(filter.category.as_ref(), filter.session_id.as_deref())
            .await?;
        let filtered: Vec<MemoryEntry> = entries
            .into_iter()
            .filter(|e| {
                if let Some(ref ns) = filter.namespace
                    && e.namespace != *ns
                {
                    return false;
                }
                if let Some(ref since) = filter.since
                    && e.timestamp.as_str() < since.as_str()
                {
                    return false;
                }
                if let Some(ref until) = filter.until
                    && e.timestamp.as_str() > until.as_str()
                {
                    return false;
                }
                true
            })
            .collect();
        Ok(filtered)
    }

    /// Store a memory entry with namespace and importance.
    ///
    /// Default implementation delegates to `store()`. Backends with native
    /// namespace/importance support should override.
    async fn store_with_metadata(
        &self,
        key: &str,
        content: &str,
        category: MemoryCategory,
        session_id: Option<&str>,
        _namespace: Option<&str>,
        _importance: Option<f64>,
    ) -> anyhow::Result<()> {
        self.store(key, content, category, session_id).await
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn memory_category_display_outputs_expected_values() {
        assert_eq!(MemoryCategory::Core.to_string(), "core");
        assert_eq!(MemoryCategory::Daily.to_string(), "daily");
        assert_eq!(MemoryCategory::Conversation.to_string(), "conversation");
        assert_eq!(
            MemoryCategory::Custom("project_notes".into()).to_string(),
            "project_notes"
        );
    }

    #[test]
    fn memory_category_serde_uses_snake_case() {
        let core = serde_json::to_string(&MemoryCategory::Core).unwrap();
        let daily = serde_json::to_string(&MemoryCategory::Daily).unwrap();
        let conversation = serde_json::to_string(&MemoryCategory::Conversation).unwrap();

        assert_eq!(core, "\"core\"");
        assert_eq!(daily, "\"daily\"");
        assert_eq!(conversation, "\"conversation\"");
    }

    #[test]
    fn memory_category_custom_roundtrip() {
        let custom = MemoryCategory::Custom("project_notes".into());
        let json = serde_json::to_string(&custom).unwrap();
        assert_eq!(json, "\"project_notes\"");
        let parsed: MemoryCategory = serde_json::from_str(&json).unwrap();
        assert_eq!(parsed, custom);
    }

    #[test]
    fn memory_category_summary_display() {
        assert_eq!(MemoryCategory::Summary.to_string(), "summary");
    }

    #[test]
    fn memory_category_summary_serde_round_trip() {
        let serialized = serde_json::to_string(&MemoryCategory::Summary).unwrap();
        assert_eq!(serialized, "\"summary\"");
        let parsed: MemoryCategory = serde_json::from_str(&serialized).unwrap();
        assert_eq!(parsed, MemoryCategory::Summary);
    }

    #[test]
    fn memory_category_unknown_string_still_falls_back_to_custom() {
        let parsed: MemoryCategory = serde_json::from_str("\"project_notes\"").unwrap();
        assert_eq!(parsed, MemoryCategory::Custom("project_notes".into()));
    }

    #[test]
    fn summary_item_new_sets_required_fields_and_empties_vectors() {
        let item = SummaryItem::new(
            "mem_1",
            "sess_1",
            "2026-05-15T10:00:00Z",
            "topic",
            "long narrative",
        );
        assert_eq!(item.memory_id, "mem_1");
        assert_eq!(item.session_id, "sess_1");
        assert!(item.transcript_session_key.is_none());
        assert_eq!(item.summary, "topic");
        assert!(item.learned.is_empty());
        assert!(item.user_intents_and_slots.is_empty());
        assert!(item.turn_num.is_none());
        assert!(!item.has_structured_content());
    }

    #[test]
    fn summary_item_serde_round_trip_preserves_all_fields() {
        let mut item = SummaryItem::new(
            "mem_2",
            "sess_2",
            "2026-05-15T11:00:00Z",
            "second topic",
            "second narrative",
        );
        item.learned.push("user prefers async".into());
        item.key_facts.push("decided to use KylinVectorDB".into());
        item.user_intents_and_slots
            .push("intent=order slot=time:tomorrow".into());
        item.temporal_facts.push("meeting tomorrow at 3pm".into());
        item.buffer_info.push("supplementary note".into());
        item.transcript_session_key = Some("gw_sess_2".into());

        let json = serde_json::to_string(&item).unwrap();
        let parsed: SummaryItem = serde_json::from_str(&json).unwrap();
        assert_eq!(parsed, item);
        assert!(parsed.has_structured_content());
    }

    #[test]
    fn summary_item_searchable_text_includes_only_semantic_fields() {
        let mut item = SummaryItem::new(
            "mem_search_1",
            "sess_search_1",
            "2026-05-15T11:00:00Z",
            "short topic",
            "long narrative",
        );
        item.transcript_session_key = Some("transcript_key".into());
        item.learned.push("learned fact should be separate".into());
        item.buffer_info
            .push("buffer note should be ignored".into());
        item.key_facts.push("key fact one".into());
        item.temporal_facts.push("temporal fact one".into());
        item.user_intents_and_slots.push("intent with slot".into());
        item.turn_num = Some(42);

        let text = item.searchable_text();

        for expected in [
            "short topic",
            "long narrative",
            "key fact one",
            "temporal fact one",
            "intent with slot",
        ] {
            assert!(text.contains(expected));
        }
        for unexpected in [
            "learned fact should be separate",
            "buffer note should be ignored",
            "mem_search_1",
            "sess_search_1",
            "transcript_key",
            "2026-05-15T11:00:00Z",
            "42",
            "memory_id",
            "session_id",
            "generated_at",
            "turn_num",
        ] {
            assert!(!text.contains(unexpected));
        }
    }

    #[test]
    fn summary_item_searchable_text_skips_empty_fields() {
        let mut item = SummaryItem::new("mem", "sess", "2026-05-15T11:00:00Z", "  ", "");
        item.key_facts.push("key fact".into());
        item.key_facts.push("   ".into());
        item.temporal_facts.push(String::new());
        item.user_intents_and_slots.push("intent".into());

        assert_eq!(item.searchable_text(), "key fact\nintent");
    }

    #[test]
    fn summary_item_searchable_text_returns_empty_when_semantic_fields_empty() {
        let mut item = SummaryItem::new("mem", "sess", "2026-05-15T11:00:00Z", "", " ");
        item.learned.push("ignored learned".into());
        item.buffer_info.push("ignored buffer".into());

        assert_eq!(item.searchable_text(), "");
    }

    #[test]
    fn summary_item_deserialize_tolerates_missing_optional_vectors() {
        let json = r#"{
            "memory_id": "m",
            "session_id": "s",
            "generated_at": "2026-05-15T00:00:00Z",
            "summary": "t",
            "summary_text": "n"
        }"#;
        let parsed: SummaryItem = serde_json::from_str(json).unwrap();
        assert!(parsed.learned.is_empty());
        assert!(parsed.user_intents_and_slots.is_empty());
        assert!(parsed.transcript_session_key.is_none());
    }

    #[test]
    fn retrieval_mode_default_is_summary_only() {
        assert_eq!(RetrievalMode::default(), RetrievalMode::SummaryOnly);
    }

    #[test]
    fn retrieval_mode_display_and_serde_use_snake_case() {
        assert_eq!(RetrievalMode::SummaryOnly.to_string(), "summary_only");
        assert_eq!(
            RetrievalMode::SummaryWithTurns.to_string(),
            "summary_with_turns"
        );
        assert_eq!(RetrievalMode::TurnsOnly.to_string(), "turns_only");

        assert_eq!(
            serde_json::to_string(&RetrievalMode::SummaryWithTurns).unwrap(),
            "\"summary_with_turns\""
        );
        let parsed: RetrievalMode = serde_json::from_str("\"turns_only\"").unwrap();
        assert_eq!(parsed, RetrievalMode::TurnsOnly);
    }

    #[test]
    fn memory_entry_roundtrip_preserves_optional_fields() {
        let entry = MemoryEntry {
            id: "id-1".into(),
            key: "favorite_language".into(),
            content: "Rust".into(),
            category: MemoryCategory::Core,
            timestamp: "2026-02-16T00:00:00Z".into(),
            session_id: Some("session-abc".into()),
            score: Some(0.98),
            namespace: "default".into(),
            importance: Some(0.7),
            superseded_by: None,
        };

        let json = serde_json::to_string(&entry).unwrap();
        let parsed: MemoryEntry = serde_json::from_str(&json).unwrap();

        assert_eq!(parsed.id, "id-1");
        assert_eq!(parsed.key, "favorite_language");
        assert_eq!(parsed.content, "Rust");
        assert_eq!(parsed.category, MemoryCategory::Core);
        assert_eq!(parsed.session_id.as_deref(), Some("session-abc"));
        assert_eq!(parsed.score, Some(0.98));
        assert_eq!(parsed.namespace, "default");
        assert_eq!(parsed.importance, Some(0.7));
        assert!(parsed.superseded_by.is_none());
    }
}
