//! Conflict resolution for memory entries.
//!
//! Before storing Core memories, performs a semantic similarity check against
//! existing entries. If cosine similarity exceeds a threshold but content
//! differs, the old entry is marked as superseded.

use super::traits::{Memory, MemoryCategory, MemoryEntry};

/// Check for conflicting memories and retire older conflicting entries.
///
/// Returns the list of entry IDs that were retired.
pub async fn check_and_resolve_conflicts(
    memory: &dyn Memory,
    key: &str,
    content: &str,
    category: &MemoryCategory,
    threshold: f64,
) -> anyhow::Result<Vec<String>> {
    // Only check conflicts for Core memories
    if !matches!(category, MemoryCategory::Core) {
        return Ok(Vec::new());
    }

    // Search for similar existing entries
    let candidates = memory.recall(content, 10, None, None, None).await?;

    let mut superseded = Vec::new();
    let mut conflicting_keys = Vec::new();
    for candidate in &candidates {
        if candidate.key == key {
            continue; // Same key = update, not conflict
        }
        if !matches!(candidate.category, MemoryCategory::Core) {
            continue;
        }
        if let Some(score) = candidate.score
            && score > threshold
            && candidate.content != content
        {
            superseded.push(candidate.id.clone());
            conflicting_keys.push(candidate.key.clone());
        }
    }

    for conflict_key in conflicting_keys {
        let _ = memory.forget(&conflict_key).await?;
    }

    Ok(superseded)
}

/// Mark entries as superseded in SQLite by setting their `superseded_by` column.
pub fn mark_superseded(
    conn: &rusqlite::Connection,
    superseded_ids: &[String],
    new_id: &str,
) -> anyhow::Result<()> {
    if superseded_ids.is_empty() {
        return Ok(());
    }

    for id in superseded_ids {
        conn.execute(
            "UPDATE memories SET superseded_by = ?1 WHERE id = ?2",
            rusqlite::params![new_id, id],
        )?;
    }

    Ok(())
}

/// Simple text-based conflict detection without embeddings.
///
/// Uses token overlap (Jaccard similarity) as a fast approximation
/// when vector embeddings are unavailable.
pub fn jaccard_similarity(a: &str, b: &str) -> f64 {
    let words_a: std::collections::HashSet<&str> = a.split_whitespace().collect();
    let words_b: std::collections::HashSet<&str> = b.split_whitespace().collect();

    if words_a.is_empty() && words_b.is_empty() {
        return 1.0;
    }
    if words_a.is_empty() || words_b.is_empty() {
        return 0.0;
    }

    let intersection = words_a.intersection(&words_b).count();
    let union = words_a.union(&words_b).count();

    if union == 0 {
        0.0
    } else {
        intersection as f64 / union as f64
    }
}

/// Find potentially conflicting entries using text similarity when embeddings
/// are not available. Returns entries above the threshold.
pub fn find_text_conflicts(
    entries: &[MemoryEntry],
    new_content: &str,
    threshold: f64,
) -> Vec<String> {
    entries
        .iter()
        .filter(|e| {
            matches!(e.category, MemoryCategory::Core)
                && e.superseded_by.is_none()
                && jaccard_similarity(&e.content, new_content) > threshold
                && e.content != new_content
        })
        .map(|e| e.id.clone())
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use async_trait::async_trait;
    use parking_lot::Mutex;
    use std::sync::Arc;

    struct MockMemory {
        entries: Mutex<Vec<MemoryEntry>>,
        forgotten: Mutex<Vec<String>>,
    }

    #[async_trait]
    impl Memory for MockMemory {
        fn name(&self) -> &str {
            "mock"
        }

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
            Ok(self.entries.lock().clone())
        }

        async fn get(&self, key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            Ok(self
                .entries
                .lock()
                .iter()
                .find(|entry| entry.key == key)
                .cloned())
        }

        async fn list(
            &self,
            _category: Option<&MemoryCategory>,
            _session_id: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(self.entries.lock().clone())
        }

        async fn forget(&self, key: &str) -> anyhow::Result<bool> {
            self.forgotten.lock().push(key.to_string());
            self.entries.lock().retain(|entry| entry.key != key);
            Ok(true)
        }

        async fn count(&self) -> anyhow::Result<usize> {
            Ok(self.entries.lock().len())
        }

        async fn health_check(&self) -> bool {
            true
        }
    }

    #[test]
    fn jaccard_identical_strings() {
        let sim = jaccard_similarity("hello world", "hello world");
        assert!((sim - 1.0).abs() < f64::EPSILON);
    }

    #[test]
    fn jaccard_disjoint_strings() {
        let sim = jaccard_similarity("hello world", "foo bar");
        assert!(sim.abs() < f64::EPSILON);
    }

    #[test]
    fn jaccard_partial_overlap() {
        let sim = jaccard_similarity("the quick brown fox", "the slow brown dog");
        // overlap: "the", "brown" = 2; union: "the", "quick", "brown", "fox", "slow", "dog" = 6
        assert!((sim - 2.0 / 6.0).abs() < 0.01);
    }

    #[test]
    fn jaccard_empty_strings() {
        assert!((jaccard_similarity("", "") - 1.0).abs() < f64::EPSILON);
        assert!(jaccard_similarity("hello", "").abs() < f64::EPSILON);
        assert!(jaccard_similarity("", "hello").abs() < f64::EPSILON);
    }

    #[test]
    fn find_text_conflicts_filters_correctly() {
        let entries = vec![
            MemoryEntry {
                id: "1".into(),
                key: "pref".into(),
                content: "User prefers Rust for systems work".into(),
                category: MemoryCategory::Core,
                timestamp: "now".into(),
                session_id: None,
                score: None,
                namespace: "default".into(),
                importance: Some(0.7),
                superseded_by: None,
            },
            MemoryEntry {
                id: "2".into(),
                key: "daily1".into(),
                content: "User prefers Rust for systems work".into(),
                category: MemoryCategory::Daily,
                timestamp: "now".into(),
                session_id: None,
                score: None,
                namespace: "default".into(),
                importance: Some(0.3),
                superseded_by: None,
            },
        ];

        // Only Core entries should be flagged
        let conflicts = find_text_conflicts(&entries, "User now prefers Go for systems work", 0.3);
        assert_eq!(conflicts.len(), 1);
        assert_eq!(conflicts[0], "1");
    }

    #[tokio::test]
    async fn check_and_resolve_conflicts_forgets_conflicting_core_entries() {
        let memory = Arc::new(MockMemory {
            entries: Mutex::new(vec![MemoryEntry {
                id: "1".into(),
                key: "old-core".into(),
                content: "User prefers Rust for systems work".into(),
                category: MemoryCategory::Core,
                timestamp: "2026-05-23T12:00:00Z".into(),
                session_id: None,
                score: Some(0.95),
                namespace: "default".into(),
                importance: Some(0.7),
                superseded_by: None,
            }]),
            forgotten: Mutex::new(Vec::new()),
        });

        let retired = check_and_resolve_conflicts(
            memory.as_ref(),
            "new-core",
            "User prefers Go for systems work",
            &MemoryCategory::Core,
            0.85,
        )
        .await
        .unwrap();

        assert_eq!(retired, vec!["1".to_string()]);
        assert_eq!(
            memory.forgotten.lock().as_slice(),
            &["old-core".to_string()]
        );
    }
}
