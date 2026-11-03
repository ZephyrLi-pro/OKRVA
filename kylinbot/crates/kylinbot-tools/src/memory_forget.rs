use async_trait::async_trait;
use kylinbot_api::tool::{Tool, ToolResult};
use kylinbot_config::policy::SecurityPolicy;
use kylinbot_config::policy::ToolOperation;
use kylinbot_memory::Memory;
use serde_json::json;
use std::sync::Arc;

/// Let the agent forget/delete a memory entry
pub struct MemoryForgetTool {
    memory: Arc<dyn Memory>,
    security: Arc<SecurityPolicy>,
}

impl MemoryForgetTool {
    pub fn new(memory: Arc<dyn Memory>, security: Arc<SecurityPolicy>) -> Self {
        Self { memory, security }
    }
}

#[async_trait]
impl Tool for MemoryForgetTool {
    fn name(&self) -> &str {
        "memory_forget"
    }

    fn description(&self) -> &str {
        "Remove a memory by key. Use to delete outdated facts or sensitive data. Returns whether the memory was found and removed."
    }

    fn parameters_schema(&self) -> serde_json::Value {
        json!({
            "type": "object",
            "properties": {
                "key": {
                    "type": "string",
                    "description": "The key of the memory to forget"
                }
            },
            "required": ["key"]
        })
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        let key = args
            .get("key")
            .and_then(|v| v.as_str())
            .ok_or_else(|| anyhow::anyhow!("Missing 'key' parameter"))?;

        if let Err(error) = self
            .security
            .enforce_tool_operation(ToolOperation::Act, "memory_forget")
        {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(error),
            });
        }

        match self.memory.forget(key).await {
            Ok(true) => Ok(ToolResult {
                success: true,
                output: format!("Forgot memory: {key}"),
                error: None,
            }),
            Ok(false) => Ok(ToolResult {
                success: true,
                output: format!("No memory found with key: {key}"),
                error: None,
            }),
            Err(e) => Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(format!("Failed to forget memory: {e}")),
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kylinbot_api::memory_traits::{ExportFilter, MemoryEntry, ProceduralMessage};
    use kylinbot_config::autonomy::AutonomyLevel;
    use kylinbot_config::policy::SecurityPolicy;
    use kylinbot_memory::{MemoryCategory, SqliteMemory};
    use std::collections::HashMap;
    use tempfile::TempDir;

    fn test_security() -> Arc<SecurityPolicy> {
        Arc::new(SecurityPolicy::default())
    }

    fn test_mem() -> (TempDir, Arc<dyn Memory>) {
        let tmp = TempDir::new().unwrap();
        let mem = SqliteMemory::new(tmp.path()).unwrap();
        (tmp, Arc::new(mem))
    }

    #[test]
    fn name_and_schema() {
        let (_tmp, mem) = test_mem();
        let tool = MemoryForgetTool::new(mem, test_security());
        assert_eq!(tool.name(), "memory_forget");
        assert!(tool.parameters_schema()["properties"]["key"].is_object());
    }

    #[tokio::test]
    async fn forget_existing() {
        let (_tmp, mem) = test_mem();
        mem.store("temp", "temporary", MemoryCategory::Conversation, None)
            .await
            .unwrap();

        let tool = MemoryForgetTool::new(mem.clone(), test_security());
        let result = tool.execute(json!({"key": "temp"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("Forgot"));

        assert!(mem.get("temp").await.unwrap().is_none());
    }

    #[tokio::test]
    async fn forget_nonexistent() {
        let (_tmp, mem) = test_mem();
        let tool = MemoryForgetTool::new(mem, test_security());
        let result = tool.execute(json!({"key": "nope"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("No memory found"));
    }

    #[tokio::test]
    async fn forget_summary_memory() {
        let (_tmp, mem) = test_mem();
        mem.store(
            "turn_summary",
            "summary content",
            MemoryCategory::Summary,
            None,
        )
        .await
        .unwrap();

        let tool = MemoryForgetTool::new(mem.clone(), test_security());
        let result = tool.execute(json!({"key": "turn_summary"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("Forgot"));
        assert!(mem.get("turn_summary").await.unwrap().is_none());
    }

    #[tokio::test]
    async fn forget_missing_key() {
        let (_tmp, mem) = test_mem();
        let tool = MemoryForgetTool::new(mem, test_security());
        let result = tool.execute(json!({})).await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn forget_blocked_in_readonly_mode() {
        let (_tmp, mem) = test_mem();
        mem.store("temp", "temporary", MemoryCategory::Conversation, None)
            .await
            .unwrap();
        let readonly = Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::ReadOnly,
            ..SecurityPolicy::default()
        });
        let tool = MemoryForgetTool::new(mem.clone(), readonly);
        let result = tool.execute(json!({"key": "temp"})).await.unwrap();
        assert!(!result.success);
        assert!(
            result
                .error
                .as_deref()
                .unwrap_or("")
                .contains("read-only mode")
        );
        assert!(mem.get("temp").await.unwrap().is_some());
    }

    #[tokio::test]
    async fn forget_blocked_when_rate_limited() {
        let (_tmp, mem) = test_mem();
        mem.store("temp", "temporary", MemoryCategory::Conversation, None)
            .await
            .unwrap();
        let limited = Arc::new(SecurityPolicy {
            max_actions_per_hour: 0,
            ..SecurityPolicy::default()
        });
        let tool = MemoryForgetTool::new(mem.clone(), limited);
        let result = tool.execute(json!({"key": "temp"})).await.unwrap();
        assert!(!result.success);
        assert!(
            result
                .error
                .as_deref()
                .unwrap_or("")
                .contains("Rate limit exceeded")
        );
        assert!(mem.get("temp").await.unwrap().is_some());
    }

    struct MockVectorDbMemory {
        entries: HashMap<String, MemoryEntry>,
    }

    impl MockVectorDbMemory {
        fn new() -> Self {
            Self {
                entries: HashMap::new(),
            }
        }

        fn insert(&mut self, key: &str, category: MemoryCategory) {
            self.entries.insert(
                key.to_string(),
                MemoryEntry {
                    id: "1".into(),
                    key: key.to_string(),
                    content: "test content".into(),
                    category,
                    timestamp: chrono::Utc::now().to_rfc3339(),
                    session_id: None,
                    score: None,
                    namespace: "default".into(),
                    importance: None,
                    superseded_by: None,
                },
            );
        }
    }

    #[async_trait]
    impl Memory for MockVectorDbMemory {
        fn name(&self) -> &str {
            "vectordb"
        }

        async fn store(
            &self,
            _: &str,
            _: &str,
            _: MemoryCategory,
            _: Option<&str>,
        ) -> anyhow::Result<()> {
            Ok(())
        }

        async fn recall(
            &self,
            _: &str,
            _: usize,
            _: Option<&str>,
            _: Option<&str>,
            _: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(vec![])
        }

        async fn get(&self, key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            Ok(self.entries.get(key).cloned())
        }

        async fn list(
            &self,
            _: Option<&MemoryCategory>,
            _: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(vec![])
        }

        async fn forget(&self, key: &str) -> anyhow::Result<bool> {
            Ok(self.entries.contains_key(key))
        }

        async fn purge_namespace(&self, _: &str) -> anyhow::Result<usize> {
            Ok(0)
        }

        async fn purge_session(&self, _: &str) -> anyhow::Result<usize> {
            Ok(0)
        }

        async fn count(&self) -> anyhow::Result<usize> {
            Ok(0)
        }

        async fn health_check(&self) -> bool {
            true
        }

        async fn store_procedural(
            &self,
            _: &[ProceduralMessage],
            _: Option<&str>,
        ) -> anyhow::Result<()> {
            Ok(())
        }

        async fn store_with_metadata(
            &self,
            _: &str,
            _: &str,
            _: MemoryCategory,
            _: Option<&str>,
            _: Option<&str>,
            _: Option<f64>,
        ) -> anyhow::Result<()> {
            Ok(())
        }

        async fn recall_namespaced(
            &self,
            _: &str,
            _: &str,
            _: usize,
            _: Option<&str>,
            _: Option<&str>,
            _: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(vec![])
        }

        async fn export(&self, _: &ExportFilter) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(vec![])
        }
    }

    #[tokio::test]
    async fn vectordb_forget_allows_daily_category() {
        let mut mock = MockVectorDbMemory::new();
        mock.insert("test_key", MemoryCategory::Daily);
        let tool = MemoryForgetTool::new(Arc::new(mock), test_security());
        let result = tool.execute(json!({"key": "test_key"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("Forgot memory"));
    }

    #[tokio::test]
    async fn vectordb_forget_accepts_summary_category() {
        let mut mock = MockVectorDbMemory::new();
        mock.insert("summary_key", MemoryCategory::Summary);
        let tool = MemoryForgetTool::new(Arc::new(mock), test_security());
        let result = tool.execute(json!({"key": "summary_key"})).await.unwrap();
        assert!(result.success);
        assert!(result.output.contains("Forgot memory"));
    }
}
