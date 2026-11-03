//! Unified Vectordb memory backend.
//!
//! This file is the single implementation source for the local vector memory
//! backend. When the `vectordb` Cargo feature is enabled it delegates to the
//! real SDK-backed engine. Otherwise it exposes the same public types with a
//! safe no-op fallback so dependent crates can still compile.

use std::path::{Path, PathBuf};
#[cfg(feature = "vectordb")]
use std::sync::Arc;

use async_trait::async_trait;

#[cfg(any(test, feature = "vectordb"))]
use super::decay;
#[cfg(feature = "vectordb")]
use super::embeddings::{create_embedding_provider, EmbeddingProvider};
#[cfg(any(test, feature = "vectordb"))]
use super::importance;
use super::traits::{
    ExportFilter, Memory, MemoryCategory, MemoryEntry, ProceduralMessage, SummaryItem,
};

/// Schema version for the vectordb collection layout.
/// Bump this constant whenever a field is added or removed so that
/// `ensure_collection` can detect stale on-disk collections.
/// Current version: 1 (added f_search_text).
#[cfg(any(test, feature = "vectordb"))]
const VECTORDB_COLLECTION_SCHEMA_VERSION: u32 = 1;

#[cfg(any(test, feature = "vectordb"))]
#[derive(Debug, Clone, PartialEq, Eq)]
enum SchemaVersionStatus {
    Current,
    Missing,
    Unreadable(String),
    Invalid(String),
    Mismatch(u32),
}

#[cfg(any(test, feature = "vectordb"))]
fn schema_version_status(path: &Path, required_version: u32) -> SchemaVersionStatus {
    match std::fs::read_to_string(path) {
        Ok(raw) => match raw.trim().parse::<u32>() {
            Ok(version) if version == required_version => SchemaVersionStatus::Current,
            Ok(version) => SchemaVersionStatus::Mismatch(version),
            Err(_) => SchemaVersionStatus::Invalid(raw.trim().to_string()),
        },
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => SchemaVersionStatus::Missing,
        Err(err) => SchemaVersionStatus::Unreadable(err.to_string()),
    }
}

#[derive(Debug, Clone)]
pub struct VectordbConfig {
    pub db_name: String,
    pub collection_name: String,
    pub embedding_provider: String,
    pub embedding_model: String,
    pub embedding_dims: usize,
}

impl Default for VectordbConfig {
    fn default() -> Self {
        Self {
            db_name: "vector".into(),
            collection_name: "kylinbot_memory".into(),
            embedding_provider: "kart-embedding".into(),
            embedding_model: String::new(),
            embedding_dims: 384,
        }
    }
}

impl VectordbConfig {
    pub fn from_schema(
        schema: &kylinbot_config::schema::KylinVectorDBMemoryConfig,
        embedding_provider: &str,
        embedding_model: &str,
        embedding_dims: usize,
    ) -> Self {
        Self {
            db_name: schema.db_name.clone(),
            collection_name: schema.collection.clone(),
            embedding_provider: if schema.embedder.trim().is_empty() {
                embedding_provider.to_string()
            } else {
                schema.embedder.clone()
            },
            embedding_model: embedding_model.to_string(),
            embedding_dims,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VectordbSortOrder {
    ByRelevance,
    ByTurnNumDesc,
}

#[derive(Debug, Clone, Default)]
pub struct VectordbFilterParams {
    pub session_id: Option<String>,
    pub category: Option<MemoryCategory>,
    pub since: Option<String>,
    pub until: Option<String>,
    pub limit: usize,
}

#[derive(Debug, Clone)]
pub struct VectordbOriginalTurn {
    pub user_message: String,
    pub assistant_response: String,
}

#[derive(Debug, Clone)]
pub struct VectordbRecallResult {
    pub memory_id: String,
    pub session_id: String,
    pub turn_num: Option<i32>,
    pub category: MemoryCategory,
    pub relevance_score: f64,
    pub summary: Option<String>,
    pub summary_text: Option<String>,
    pub learned: Vec<String>,
    pub key_facts: Vec<String>,
    pub user_intents_and_slots: Vec<String>,
    pub key: Option<String>,
    pub flat_content: Option<String>,
    pub original_turn: Option<VectordbOriginalTurn>,
    pub is_archived: bool,
}

pub fn prune_expired_entries_with_config(
    config: &kylinbot_config::schema::KylinVectorDBMemoryConfig,
    workspace_dir: &Path,
    retention_by_category: &[(MemoryCategory, u32)],
) -> anyhow::Result<u64> {
    #[cfg(feature = "vectordb")]
    {
        use kart_vector_database_sys::VectorDBEngine;

        let db_dir = workspace_dir.join("memory");
        if !db_dir.is_dir() {
            return Ok(0);
        }

        let db_path = db_dir.join(format!("{}.db", config.db_name));
        if !db_path.exists() {
            return Ok(0);
        }

        let engine = VectorDBEngine::new("kylinbot")?;
        engine.load_db_file(&db_path, false, None)?;
        if !engine.has_collection(&config.collection)? {
            return Ok(0);
        }

        let mut deleted = 0_u64;
        for (category, retention_days) in retention_by_category {
            if *retention_days == 0 {
                continue;
            }

            let cutoff = (chrono::Utc::now() - chrono::Duration::days(i64::from(*retention_days)))
                .to_rfc3339();
            let expr = format!(
                "f_category == \"{}\" and f_timestamp < \"{}\"",
                category, cutoff
            );
            deleted += u64::try_from(engine.delete(&config.collection, &expr)?).unwrap_or(0);
        }

        return Ok(deleted);
    }

    #[cfg(not(feature = "vectordb"))]
    {
        let _ = (config, workspace_dir, retention_by_category);
        Ok(0)
    }
}

pub struct VectordbMemory {
    #[cfg(feature = "vectordb")]
    workspace_dir: PathBuf,
    #[cfg(feature = "vectordb")]
    local_backend: Option<Arc<real_backend::RealVectordbBackend>>,
}

impl VectordbMemory {
    #[cfg(feature = "vectordb")]
    pub fn new(config: VectordbConfig, workspace_dir: PathBuf) -> Self {
        let embedding = create_embedding_provider(
            &config.embedding_provider,
            None,
            &config.embedding_model,
            config.embedding_dims,
        );
        let actual_dims = embedding.dimensions();
        let mut config = config;
        if actual_dims > 0 && config.embedding_dims != actual_dims {
            tracing::warn!(
                configured_dims = config.embedding_dims,
                actual_dims,
                provider = %config.embedding_provider,
                model = %config.embedding_model,
                "Embedding provider dimensions differ from configured Vectordb dimensions; using provider dimensions"
            );
            config.embedding_dims = actual_dims;
        }

        let embedding: Arc<dyn EmbeddingProvider> = Arc::from(embedding);

        Self {
            workspace_dir: workspace_dir.clone(),
            local_backend: Self::build_local_backend(&config, &workspace_dir, embedding),
        }
    }

    #[cfg(not(feature = "vectordb"))]
    pub fn new(_config: VectordbConfig, _workspace_dir: PathBuf) -> Self {
        Self {}
    }

    #[cfg(feature = "vectordb")]
    pub fn default_config(workspace_dir: PathBuf) -> Self {
        Self::new(VectordbConfig::default(), workspace_dir)
    }

    #[cfg(feature = "vectordb")]
    pub fn new_lazy(
        workspace_dir: &Path,
        db_name: &str,
        collection: &str,
        embedder: Arc<dyn EmbeddingProvider>,
    ) -> anyhow::Result<Self> {
        let backend = real_backend::RealVectordbBackend::new_lazy(
            workspace_dir,
            db_name,
            collection,
            embedder,
        )?;

        Ok(Self {
            workspace_dir: workspace_dir.to_path_buf(),
            local_backend: Some(Arc::new(backend)),
        })
    }

    #[cfg(feature = "vectordb")]
    fn build_local_backend(
        config: &VectordbConfig,
        workspace_dir: &Path,
        embedding: Arc<dyn EmbeddingProvider>,
    ) -> Option<Arc<real_backend::RealVectordbBackend>> {
        match real_backend::RealVectordbBackend::new_lazy(
            workspace_dir,
            &config.db_name,
            &config.collection_name,
            embedding,
        ) {
            Ok(backend) => Some(Arc::new(backend)),
            Err(err) => {
                tracing::warn!(
                    error = %err,
                    db_name = %config.db_name,
                    collection = %config.collection_name,
                    "Vectordb backend initialization failed"
                );
                None
            }
        }
    }

    pub fn backend_ready(&self) -> bool {
        #[cfg(feature = "vectordb")]
        {
            self.local_backend.is_some()
        }
        #[cfg(not(feature = "vectordb"))]
        {
            false
        }
    }

    pub async fn recall_structured(
        &self,
        filter: VectordbFilterParams,
        sort: VectordbSortOrder,
    ) -> anyhow::Result<Vec<VectordbRecallResult>> {
        let mut entries = self
            .list(filter.category.as_ref(), filter.session_id.as_deref())
            .await?;
        entries.retain(|entry| {
            if let Some(since) = filter.since.as_deref()
                && entry.timestamp.as_str() < since
            {
                return false;
            }
            if let Some(until) = filter.until.as_deref()
                && entry.timestamp.as_str() > until
            {
                return false;
            }
            true
        });

        match sort {
            VectordbSortOrder::ByRelevance => entries.sort_by(|a, b| {
                b.score
                    .partial_cmp(&a.score)
                    .unwrap_or(std::cmp::Ordering::Equal)
            }),
            VectordbSortOrder::ByTurnNumDesc => {
                entries.sort_by(|a, b| {
                    extract_turn_num_from_entry(b).cmp(&extract_turn_num_from_entry(a))
                });
            }
        }

        if filter.limit > 0 && entries.len() > filter.limit {
            entries.truncate(filter.limit);
        }

        Ok(entries
            .into_iter()
            .map(|entry| entry_to_recall_result(&entry))
            .collect())
    }

    pub async fn prune_old_entries(&self) -> anyhow::Result<u64> {
        let cutoff = (chrono::Utc::now() - chrono::Duration::days(30)).to_rfc3339();
        let stale = self
            .list(Some(&MemoryCategory::Conversation), None)
            .await?
            .into_iter()
            .filter(|entry| entry.timestamp < cutoff)
            .collect::<Vec<_>>();
        for entry in &stale {
            let _ = self.forget(&entry.key).await?;
        }
        Ok(stale.len() as u64)
    }

    pub async fn upsert(
        &self,
        key: &str,
        content: &str,
        category: MemoryCategory,
        session_id: Option<&str>,
    ) -> anyhow::Result<()> {
        let _ = self.forget(key).await?;
        self.store(key, content, category, session_id).await
    }

    pub async fn delete_by_expression(&self, expr: &str) -> anyhow::Result<usize> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.delete_by_expression(expr).await;
        }

        let _ = expr;
        anyhow::bail!(
            "vectordb backend runtime unavailable; rebuild with `--features vectordb` and ensure the local SDK is installed"
        )
    }

    pub async fn has_collection(&self) -> anyhow::Result<bool> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.has_collection().await;
        }

        Ok(false)
    }

    pub async fn drop_collection(&self) -> anyhow::Result<()> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.drop_collection().await;
        }

        anyhow::bail!("vectordb backend runtime unavailable")
    }

    pub async fn show_collections(&self) -> anyhow::Result<Vec<String>> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.show_collections().await;
        }

        Ok(Vec::new())
    }

    #[cfg(feature = "vectordb")]
    pub async fn describe_collection(
        &self,
    ) -> anyhow::Result<kart_vector_database_sys::CollectionDesc> {
        if let Some(backend) = &self.local_backend {
            return backend.describe_collection().await;
        }
        anyhow::bail!("vectordb backend runtime unavailable")
    }

    #[cfg(feature = "vectordb")]
    pub async fn describe_index(
        &self,
        field: &str,
    ) -> anyhow::Result<kart_vector_database_sys::IndexDesc> {
        if let Some(backend) = &self.local_backend {
            return backend.describe_index(field).await;
        }
        anyhow::bail!("vectordb backend runtime unavailable")
    }

    #[cfg(feature = "vectordb")]
    pub async fn collection_statistics(
        &self,
    ) -> anyhow::Result<kart_vector_database_sys::CollectionStat> {
        if let Some(backend) = &self.local_backend {
            return backend.collection_statistics().await;
        }
        anyhow::bail!("vectordb backend runtime unavailable")
    }

    #[cfg(feature = "vectordb")]
    pub async fn drop_index(&self, index_name: &str) -> anyhow::Result<()> {
        if let Some(backend) = &self.local_backend {
            return backend.drop_index(index_name).await;
        }
        anyhow::bail!("vectordb backend runtime unavailable")
    }
}

/// Normalize raw RRF score and compute final retrieval score.
///
/// RRF score = sum of 1/(k+rank) across N retrieval systems.
/// For N=2 (dense + sparse) with k=60:
///   Max_RRF = N / (k+1) = 2 / 61 ≈ 0.03279
///   Normalization: RRF_norm = raw_score * (k+1) / N = raw_score * 30.5
///
/// This maps RRF scores to [0,1] range where 1.0 means the document ranked #1
/// in all retrieval systems.
#[cfg(any(test, feature = "vectordb"))]
fn fused_recall_score(raw_rrf_score: f64, importance_score: Option<f64>, timestamp: &str) -> f64 {
    const RRF_K: i32 = 60;
    const RRF_NUM_SYSTEMS: f64 = 2.0; // dense + sparse
    const RRF_NORMALIZE_FACTOR: f64 = (RRF_K + 1) as f64 / RRF_NUM_SYSTEMS; // 61 / 2 = 30.5

    // Normalize RRF score to [0,1] range
    let normalized_hybrid_score = (raw_rrf_score * RRF_NORMALIZE_FACTOR).min(1.0);

    let recency =
        decay::recency_decay_factor(timestamp, decay::DEFAULT_HALF_LIFE_DAYS).unwrap_or(1.0);
    let importance = importance_score.unwrap_or(0.5);
    importance::weighted_final_score(normalized_hybrid_score, importance, recency)
}

#[cfg(any(test, feature = "vectordb"))]
fn resolve_importance(content: &str, category: &MemoryCategory, importance: Option<f64>) -> f64 {
    importance.unwrap_or_else(|| importance::compute_importance(content, category))
}

#[async_trait]
impl Memory for VectordbMemory {
    fn name(&self) -> &str {
        "vectordb"
    }

    async fn store(
        &self,
        key: &str,
        content: &str,
        category: MemoryCategory,
        session_id: Option<&str>,
    ) -> anyhow::Result<()> {
        self.store_with_metadata(key, content, category, session_id, None, None)
            .await
    }

    async fn store_with_metadata(
        &self,
        key: &str,
        content: &str,
        category: MemoryCategory,
        session_id: Option<&str>,
        namespace: Option<&str>,
        importance: Option<f64>,
    ) -> anyhow::Result<()> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend
                .store_with_metadata(key, content, category, session_id, namespace, importance)
                .await;
        }

        let _ = (key, content, category, session_id, namespace, importance);
        Ok(())
    }

    async fn recall(
        &self,
        query: &str,
        limit: usize,
        session_id: Option<&str>,
        since: Option<&str>,
        until: Option<&str>,
    ) -> anyhow::Result<Vec<MemoryEntry>> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.recall(query, limit, session_id, since, until).await;
        }

        let _ = (query, limit, session_id, since, until);
        Ok(Vec::new())
    }

    async fn recall_namespaced(
        &self,
        namespace: &str,
        query: &str,
        limit: usize,
        session_id: Option<&str>,
        since: Option<&str>,
        until: Option<&str>,
    ) -> anyhow::Result<Vec<MemoryEntry>> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend
                .recall_namespaced(namespace, query, limit, session_id, since, until)
                .await;
        }

        let _ = (namespace, query, limit, session_id, since, until);
        Ok(Vec::new())
    }

    async fn get(&self, key: &str) -> anyhow::Result<Option<MemoryEntry>> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.get(key).await;
        }

        let _ = key;
        Ok(None)
    }

    async fn list(
        &self,
        category: Option<&MemoryCategory>,
        session_id: Option<&str>,
    ) -> anyhow::Result<Vec<MemoryEntry>> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.list(category, session_id).await;
        }

        let _ = (category, session_id);
        Ok(Vec::new())
    }

    async fn forget(&self, key: &str) -> anyhow::Result<bool> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.forget(key).await;
        }

        let _ = key;
        Ok(false)
    }

    async fn purge_namespace(&self, namespace: &str) -> anyhow::Result<usize> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.purge_namespace(namespace).await;
        }

        let _ = namespace;
        Ok(0)
    }

    async fn purge_session(&self, session_id: &str) -> anyhow::Result<usize> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.purge_session(session_id).await;
        }

        let _ = session_id;
        Ok(0)
    }

    async fn count(&self) -> anyhow::Result<usize> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.count().await;
        }

        Ok(0)
    }

    async fn health_check(&self) -> bool {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.health_check().await;
        }

        false
    }

    async fn store_procedural(
        &self,
        messages: &[ProceduralMessage],
        session_id: Option<&str>,
    ) -> anyhow::Result<()> {
        let _ = (messages, session_id);
        Ok(())
    }

    async fn export(&self, filter: &ExportFilter) -> anyhow::Result<Vec<MemoryEntry>> {
        #[cfg(feature = "vectordb")]
        if let Some(backend) = &self.local_backend {
            return backend.export(filter).await;
        }

        let _ = filter;
        Ok(Vec::new())
    }
}

fn extract_turn_num_from_entry(entry: &MemoryEntry) -> i32 {
    if entry.category != MemoryCategory::Summary {
        return 0;
    }

    serde_json::from_str::<SummaryItem>(&entry.content)
        .ok()
        .and_then(|summary| summary.turn_num)
        .unwrap_or(0)
}

fn entry_to_recall_result(entry: &MemoryEntry) -> VectordbRecallResult {
    if entry.category == MemoryCategory::Summary {
        let item = serde_json::from_str::<SummaryItem>(&entry.content).ok();
        VectordbRecallResult {
            memory_id: entry.id.clone(),
            session_id: entry.session_id.clone().unwrap_or_default(),
            turn_num: item.as_ref().and_then(|summary| summary.turn_num),
            category: entry.category.clone(),
            relevance_score: entry.score.unwrap_or(0.0),
            summary: item.as_ref().map(|summary| summary.summary.clone()),
            summary_text: item.as_ref().map(|summary| summary.summary_text.clone()),
            learned: item
                .as_ref()
                .map(|summary| summary.learned.clone())
                .unwrap_or_default(),
            key_facts: item
                .as_ref()
                .map(|summary| summary.key_facts.clone())
                .unwrap_or_default(),
            user_intents_and_slots: item
                .as_ref()
                .map(|summary| summary.user_intents_and_slots.clone())
                .unwrap_or_default(),
            key: None,
            flat_content: None,
            original_turn: None,
            is_archived: false,
        }
    } else {
        VectordbRecallResult {
            memory_id: entry.id.clone(),
            session_id: entry.session_id.clone().unwrap_or_default(),
            turn_num: None,
            category: entry.category.clone(),
            relevance_score: entry.score.unwrap_or(0.0),
            summary: None,
            summary_text: None,
            learned: Vec::new(),
            key_facts: Vec::new(),
            user_intents_and_slots: Vec::new(),
            key: Some(entry.key.clone()),
            flat_content: Some(entry.content.clone()),
            original_turn: None,
            is_archived: false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fused_recall_score_prefers_recent_high_importance_entries() {
        let old_timestamp = (chrono::Utc::now() - chrono::Duration::days(30)).to_rfc3339();
        let fresh_timestamp = chrono::Utc::now().to_rfc3339();

        let old_score = fused_recall_score(0.8, Some(0.2), &old_timestamp);
        let fresh_score = fused_recall_score(0.8, Some(0.9), &fresh_timestamp);

        assert!(fresh_score > old_score);
    }

    #[test]
    fn resolve_importance_computes_default_from_category() {
        let score = resolve_importance("chat message", &MemoryCategory::Conversation, None);
        assert!((score - 0.1).abs() < f64::EPSILON);
    }

    #[test]
    fn resolve_importance_preserves_explicit_value() {
        let score = resolve_importance("chat message", &MemoryCategory::Conversation, Some(0.9));
        assert!((score - 0.9).abs() < f64::EPSILON);
    }

    #[test]
    fn schema_version_status_distinguishes_missing_file_from_mismatch() {
        let temp = tempfile::tempdir().expect("create temp dir");
        let path = temp.path().join("missing.schema_version");

        assert_eq!(
            schema_version_status(&path, VECTORDB_COLLECTION_SCHEMA_VERSION),
            SchemaVersionStatus::Missing
        );
    }

    #[test]
    fn schema_version_status_reports_explicit_old_version_as_mismatch() {
        let temp = tempfile::tempdir().expect("create temp dir");
        let path = temp.path().join("collection.schema_version");
        std::fs::write(&path, "0").expect("write schema version");

        assert_eq!(
            schema_version_status(&path, VECTORDB_COLLECTION_SCHEMA_VERSION),
            SchemaVersionStatus::Mismatch(0)
        );
    }

    #[test]
    fn schema_version_status_reports_current_version() {
        let temp = tempfile::tempdir().expect("create temp dir");
        let path = temp.path().join("collection.schema_version");
        std::fs::write(&path, VECTORDB_COLLECTION_SCHEMA_VERSION.to_string())
            .expect("write schema version");

        assert_eq!(
            schema_version_status(&path, VECTORDB_COLLECTION_SCHEMA_VERSION),
            SchemaVersionStatus::Current
        );
    }
}

#[cfg(feature = "vectordb")]
mod real_backend {
    use super::*;
    use kart_vector_database_sys::{
        AnnSearchRequestBuilder, CollectionSchemaBuilder, DataType, FieldData, FieldSchemaBuilder,
        FunctionSchemaBuilder, IndexType, KylinVectorDBError, MetricType, QueryArgumentsBuilder,
        RrfRankerBuilder, VectorDBEngine,
    };

    pub(super) struct RealVectordbBackend {
        engine: Arc<VectorDBEngine>,
        collection: String,
        db_dir: std::path::PathBuf,
        embedder: Arc<dyn EmbeddingProvider>,
        initialized: tokio::sync::OnceCell<()>,
    }

    impl RealVectordbBackend {
        pub(super) fn new_lazy(
            workspace_dir: &Path,
            db_name: &str,
            collection: &str,
            embedder: Arc<dyn EmbeddingProvider>,
        ) -> anyhow::Result<Self> {
            let db_dir = workspace_dir.join("memory");
            std::fs::create_dir_all(&db_dir)?;
            let db_path = db_dir.join(format!("{db_name}.db"));

            let engine = VectorDBEngine::new("kylinbot")?;
            engine.load_db_file(&db_path, false, None)?;

            Ok(Self {
                engine: Arc::new(engine),
                collection: collection.to_string(),
                db_dir,
                embedder,
                initialized: tokio::sync::OnceCell::new(),
            })
        }

        fn schema_version_path(&self) -> std::path::PathBuf {
            self.db_dir
                .join(format!("{}.schema_version", self.collection))
        }

        fn schema_version_status(&self) -> SchemaVersionStatus {
            schema_version_status(
                &self.schema_version_path(),
                VECTORDB_COLLECTION_SCHEMA_VERSION,
            )
        }

        fn write_schema_version(&self) -> std::io::Result<()> {
            std::fs::write(
                self.schema_version_path(),
                VECTORDB_COLLECTION_SCHEMA_VERSION.to_string(),
            )
        }

        fn preserve_existing_collection_without_schema_version(&self, reason: &str) {
            tracing::error!(
                collection = %self.collection,
                reason,
                "vectordb: preserving existing collection because schema version file is unavailable; \
                 delete the collection manually if schema errors appear"
            );
            if let Err(err) = self.write_schema_version() {
                tracing::error!(
                    collection = %self.collection,
                    error = %err,
                    "vectordb: failed to repair missing schema version file for existing collection"
                );
            }
        }

        async fn ensure_collection(&self) -> anyhow::Result<()> {
            let dims = self.embedder.dimensions();
            if dims == 0 {
                tracing::warn!(
                    "vectordb: embedder dimensions == 0 (NoopEmbedding); vector search unavailable, skipping collection creation"
                );
                return Ok(());
            }

            if self.engine.has_collection(&self.collection)? {
                match self.schema_version_status() {
                    SchemaVersionStatus::Current => return Ok(()),
                    SchemaVersionStatus::Missing => {
                        self.preserve_existing_collection_without_schema_version("missing");
                        return Ok(());
                    }
                    SchemaVersionStatus::Unreadable(error) => {
                        self.preserve_existing_collection_without_schema_version(&error);
                        return Ok(());
                    }
                    SchemaVersionStatus::Invalid(value) => {
                        self.preserve_existing_collection_without_schema_version(&format!(
                            "invalid schema version value '{value}'"
                        ));
                        return Ok(());
                    }
                    SchemaVersionStatus::Mismatch(current_version) => {
                        tracing::warn!(
                            collection = %self.collection,
                            current_version,
                            required_version = VECTORDB_COLLECTION_SCHEMA_VERSION,
                            "vectordb: schema version mismatch - dropping and recreating collection; \
                             existing summaries will be cleared and must be re-generated"
                        );
                        self.engine.drop_collection(&self.collection)?;
                    }
                }
            }

            let dim_i32 = i32::try_from(dims).map_err(|_| {
                KylinVectorDBError::InvalidInput(format!(
                    "embedding dimension {dims} overflows i32"
                ))
            })?;

            let schema = CollectionSchemaBuilder::new(&self.collection, "KylinBot memory", false)?;

            let f_id = FieldSchemaBuilder::new("f_id", DataType::Int64)?
                .description("auto-increment primary key")?
                .primary_key(true)?
                .auto_id(true)?;
            let f_key = FieldSchemaBuilder::new("f_key", DataType::VarChar)?
                .description("business key")?
                .max_len(128)?;
            let f_content = FieldSchemaBuilder::new("f_content", DataType::VarChar)?
                .description("memory content")?
                .max_len(8192)?;
            let f_search_text = FieldSchemaBuilder::new("f_search_text", DataType::VarChar)?
                .description("search text for dense and BM25 indexing")?
                .max_len(8192)?;
            let f_category = FieldSchemaBuilder::new("f_category", DataType::VarChar)?
                .description("memory category")?
                .max_len(64)?;
            let f_timestamp = FieldSchemaBuilder::new("f_timestamp", DataType::VarChar)?
                .description("ISO 8601 timestamp")?
                .max_len(32)?;
            let f_session_id = FieldSchemaBuilder::new("f_session_id", DataType::VarChar)?
                .description("session identifier")?
                .max_len(64)?;
            let f_namespace = FieldSchemaBuilder::new("f_namespace", DataType::VarChar)?
                .description("memory namespace")?
                .max_len(256)?;
            let f_importance = FieldSchemaBuilder::new("f_importance", DataType::VarChar)?
                .description("importance score encoded as string")?
                .max_len(32)?;
            let f_turn_num = FieldSchemaBuilder::new("f_turn_num", DataType::Int64)?
                .description("1-based turn number for summary replay")?;
            let f_embedding = FieldSchemaBuilder::new("f_embedding", DataType::FloatVector)?
                .description("dense embedding vector")?
                .dimension(dim_i32)?;
            let f_sparse = FieldSchemaBuilder::new("f_sparse", DataType::SparseVector)?
                .description("BM25 sparse vector (auto-generated)")?;

            let bm25_fn = FunctionSchemaBuilder::bm25("content_bm25", "f_search_text", "f_sparse")?;

            let schema = schema
                .add_field(&f_id)?
                .add_field(&f_key)?
                .add_field(&f_content)?
                .add_field(&f_search_text)?
                .add_field(&f_category)?
                .add_field(&f_timestamp)?
                .add_field(&f_session_id)?
                .add_field(&f_namespace)?
                .add_field(&f_importance)?
                .add_field(&f_turn_num)?
                .add_field(&f_embedding)?
                .add_field(&f_sparse)?
                .add_function(&bm25_fn)?;

            self.engine.create_collection(&schema)?;
            self.engine.create_index(
                &self.collection,
                "f_embedding",
                "vector_index",
                IndexType::Flat,
                MetricType::Cosine,
            )?;
            self.engine.create_index(
                &self.collection,
                "f_sparse",
                "sparse_vector_index",
                IndexType::SparseIvfFlat,
                MetricType::SparseIp,
            )?;

            tracing::debug!(
                "vectordb: created collection '{}' with {}-dim embeddings + BM25",
                self.collection,
                dims
            );
            // Write the version file last — only after collection + both indexes
            // are fully created. This makes the file a "completion marker": if
            // any prior step fails the file is absent, and the next startup will
            // correctly drop and recreate the collection.
            self.write_schema_version().map_err(|e| {
                anyhow::anyhow!(
                    "vectordb: failed to write schema version file for '{}': {e}",
                    self.collection
                )
            })?;
            Ok(())
        }

        pub(super) async fn delete_by_expression(&self, expr: &str) -> anyhow::Result<usize> {
            self.engine
                .delete(&self.collection, expr)
                .map_err(Into::into)
        }

        pub(super) async fn has_collection(&self) -> anyhow::Result<bool> {
            self.engine
                .has_collection(&self.collection)
                .map_err(Into::into)
        }

        pub(super) async fn drop_collection(&self) -> anyhow::Result<()> {
            self.engine
                .drop_collection(&self.collection)
                .map_err(Into::into)
        }

        pub(super) async fn show_collections(&self) -> anyhow::Result<Vec<String>> {
            self.engine.show_collections().map_err(Into::into)
        }

        pub(super) async fn describe_collection(
            &self,
        ) -> anyhow::Result<kart_vector_database_sys::CollectionDesc> {
            self.engine
                .describe_collection(&self.collection)
                .map_err(Into::into)
        }

        pub(super) async fn describe_index(
            &self,
            field: &str,
        ) -> anyhow::Result<kart_vector_database_sys::IndexDesc> {
            self.engine
                .describe_index(&self.collection, field)
                .map_err(Into::into)
        }

        pub(super) async fn collection_statistics(
            &self,
        ) -> anyhow::Result<kart_vector_database_sys::CollectionStat> {
            self.engine
                .get_collection_statistics(&self.collection)
                .map_err(Into::into)
        }

        pub(super) async fn drop_index(&self, index_name: &str) -> anyhow::Result<()> {
            self.engine
                .drop_index(&self.collection, index_name)
                .map_err(Into::into)
        }
    }

    #[async_trait]
    impl Memory for RealVectordbBackend {
        fn name(&self) -> &str {
            "vectordb"
        }

        async fn store(
            &self,
            key: &str,
            content: &str,
            category: MemoryCategory,
            session_id: Option<&str>,
        ) -> anyhow::Result<()> {
            self.store_with_metadata(key, content, category, session_id, None, None)
                .await
        }

        async fn store_with_metadata(
            &self,
            key: &str,
            content: &str,
            category: MemoryCategory,
            session_id: Option<&str>,
            namespace: Option<&str>,
            importance: Option<f64>,
        ) -> anyhow::Result<()> {
            self.initialized
                .get_or_try_init(|| async { self.ensure_collection().await })
                .await?;

            let search_text = search_text_for_storage(content, &category);
            let embedding = self.embedder.embed_one(&search_text).await?;
            let timestamp = chrono::Utc::now().to_rfc3339();
            let turn_num = extract_turn_num(&category, content);
            let resolved_importance = resolve_importance(content, &category, importance);

            let fields = vec![
                FieldData::VarChar {
                    name: "f_key".into(),
                    values: vec![key.to_string()],
                },
                FieldData::VarChar {
                    name: "f_content".into(),
                    values: vec![content.to_string()],
                },
                FieldData::VarChar {
                    name: "f_search_text".into(),
                    values: vec![search_text],
                },
                FieldData::VarChar {
                    name: "f_category".into(),
                    values: vec![category.to_string()],
                },
                FieldData::VarChar {
                    name: "f_timestamp".into(),
                    values: vec![timestamp],
                },
                FieldData::VarChar {
                    name: "f_session_id".into(),
                    values: vec![session_id.map(str::to_string).unwrap_or_default()],
                },
                FieldData::VarChar {
                    name: "f_namespace".into(),
                    values: vec![namespace.unwrap_or("default").to_string()],
                },
                FieldData::VarChar {
                    name: "f_importance".into(),
                    values: vec![resolved_importance.to_string()],
                },
                FieldData::Int64 {
                    name: "f_turn_num".into(),
                    values: vec![turn_num.map(i64::from).unwrap_or_default()],
                },
                FieldData::FloatVec {
                    name: "f_embedding".into(),
                    dim: embedding.len(),
                    values: vec![embedding],
                },
            ];

            tracing::debug!(
                key = %key,
                category = %category,
                session_id = ?session_id,
                "vectordb: storing memory entry"
            );
            self.engine.insert(&self.collection, fields)?;
            tracing::debug!("vectordb: successfully inserted memory entry");
            Ok(())
        }

        async fn recall(
            &self,
            query: &str,
            limit: usize,
            session_id: Option<&str>,
            since: Option<&str>,
            until: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            self.initialized
                .get_or_try_init(|| async { self.ensure_collection().await })
                .await?;

            tracing::debug!(
                query = %query,
                limit,
                session_id = ?session_id,
                "vectordb: recall started"
            );

            let q_vec = self.embedder.embed_one(query).await?;
            let fetch_limit = limit.saturating_mul(3).max(limit).max(1);
            let fetch_limit_i32 = i32::try_from(fetch_limit).unwrap_or(i32::MAX);
            let limit_i32 = i32::try_from(limit.max(1)).unwrap_or(i32::MAX);

            // RRF ranker with k=60. See fused_recall_score for normalization logic.
            const RRF_K: i32 = 60;

            let dense = AnnSearchRequestBuilder::dense("f_embedding", fetch_limit_i32, &q_vec)?;
            let sparse = AnnSearchRequestBuilder::sparse_text("f_sparse", fetch_limit_i32, query)?;
            let ranker = RrfRankerBuilder::new(RRF_K)?;
            let output_fields: &[&str] = &[
                "f_id",
                "f_key",
                "f_content",
                "f_category",
                "f_timestamp",
                "f_session_id",
                "f_namespace",
                "f_importance",
                "f_turn_num",
            ];

            let results = self.engine.hybrid_search_rrf(
                &self.collection,
                vec![dense, sparse],
                ranker,
                limit_i32,
                output_fields,
            )?;

            let scores = results.scores(0);
            tracing::debug!(
                scores_count = scores.len(),
                "vectordb: recall search completed"
            );
            if scores.is_empty() {
                return Ok(Vec::new());
            }

            let ids = results.int64_field(0, "f_id");
            let keys = results.string_field(0, "f_key");
            let contents = results.string_field(0, "f_content");
            let categories = results.string_field(0, "f_category");
            let timestamps = results.string_field(0, "f_timestamp");
            let session_ids = results.string_field(0, "f_session_id");
            let namespaces = results.string_field(0, "f_namespace");
            let importance_values = results.string_field(0, "f_importance");

            tracing::debug!(
                keys_count = keys.len(),
                contents_count = contents.len(),
                ids_count = ids.len(),
                "vectordb: recall field extraction completed"
            );

            let mut entries = Vec::new();
            for i in 0..keys.len().min(contents.len()) {
                let ts = timestamps.get(i).cloned().unwrap_or_default();
                let sid = session_ids.get(i).cloned().unwrap_or_default();
                let importance = parse_importance(importance_values.get(i).map(String::as_str));
                let raw_score = scores.get(i).copied().map(f64::from);
                let final_score = raw_score.map(|rs| fused_recall_score(rs, importance, &ts));

                tracing::debug!(
                    index = i,
                    key = %keys[i],
                    raw_score = ?raw_score,
                    importance = ?importance,
                    final_score = ?final_score,
                    session_id = %sid,
                    requested_session_id = ?session_id,
                    content_preview = %contents[i].chars().take(50).collect::<String>(),
                    "vectordb: recall entry score details"
                );

                let score = final_score;

                // Session filtering: session_id=None means global memory (visible to all sessions).
                // Only filter when both the requested session_id and the entry's session_id are non-empty.
                if let Some(sid_filter) = session_id
                    && !sid_filter.is_empty()
                    && !sid.is_empty()
                    && sid != sid_filter
                {
                    continue;
                }
                if let Some(since) = since
                    && ts.as_str() < since
                {
                    continue;
                }
                if let Some(until) = until
                    && ts.as_str() > until
                {
                    continue;
                }

                entries.push(MemoryEntry {
                    id: ids.get(i).copied().unwrap_or(i as i64).to_string(),
                    key: keys[i].clone(),
                    content: contents[i].clone(),
                    category: parse_category(categories.get(i).map(String::as_str)),
                    timestamp: ts,
                    session_id: if sid.is_empty() { None } else { Some(sid) },
                    score,
                    namespace: namespaces
                        .get(i)
                        .cloned()
                        .filter(|value| !value.is_empty())
                        .unwrap_or_else(|| "default".into()),
                    importance: parse_importance(importance_values.get(i).map(String::as_str)),
                    superseded_by: None,
                });
            }

            entries.sort_by(|left, right| {
                right
                    .score
                    .partial_cmp(&left.score)
                    .unwrap_or(std::cmp::Ordering::Equal)
            });
            entries.truncate(limit);

            Ok(entries)
        }

        async fn get(&self, key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            self.initialized
                .get_or_try_init(|| async { self.ensure_collection().await })
                .await?;

            let args = QueryArgumentsBuilder::new(&self.collection)?
                .expression(&format!("f_key == \"{}\"", escape_filter_value(key)))?
                .add_output_field("f_id")?
                .add_output_field("f_key")?
                .add_output_field("f_content")?
                .add_output_field("f_category")?
                .add_output_field("f_timestamp")?
                .add_output_field("f_session_id")?
                .add_output_field("f_namespace")?
                .add_output_field("f_importance")?;

            let results = match self.engine.query(args) {
                Ok(r) => r,
                Err(e) => {
                    tracing::warn!("vectordb: query failed for key '{}': {}", key, e);
                    return Ok(None);
                }
            };
            let keys = results.string_field("f_key");
            if keys.is_empty() {
                return Ok(None);
            }

            let ids = results.int64_field("f_id");
            let contents = results.string_field("f_content");
            let categories = results.string_field("f_category");
            let timestamps = results.string_field("f_timestamp");
            let session_ids = results.string_field("f_session_id");
            let namespaces = results.string_field("f_namespace");
            let importance_values = results.string_field("f_importance");

            Ok(Some(MemoryEntry {
                id: ids.first().copied().unwrap_or_default().to_string(),
                key: keys[0].clone(),
                content: contents.first().cloned().unwrap_or_default(),
                category: parse_category(categories.first().map(String::as_str)),
                timestamp: timestamps.first().cloned().unwrap_or_default(),
                session_id: session_ids
                    .first()
                    .cloned()
                    .filter(|value| !value.is_empty()),
                score: None,
                namespace: namespaces
                    .first()
                    .cloned()
                    .filter(|value| !value.is_empty())
                    .unwrap_or_else(|| "default".into()),
                importance: parse_importance(importance_values.first().map(String::as_str)),
                superseded_by: None,
            }))
        }

        async fn list(
            &self,
            category: Option<&MemoryCategory>,
            session_id: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            self.initialized
                .get_or_try_init(|| async { self.ensure_collection().await })
                .await?;

            let mut expr_parts = Vec::new();
            if let Some(category) = category {
                expr_parts.push(format!("f_category == \"{}\"", category));
            }
            if let Some(session_id) = session_id {
                expr_parts.push(format!(
                    "f_session_id == \"{}\"",
                    escape_filter_value(session_id)
                ));
            }
            let expr = if expr_parts.is_empty() {
                "f_id > 0".to_string()
            } else {
                expr_parts.join(" and ")
            };

            let args = QueryArgumentsBuilder::new(&self.collection)?
                .expression(&expr)?
                .add_output_field("f_id")?
                .add_output_field("f_key")?
                .add_output_field("f_content")?
                .add_output_field("f_category")?
                .add_output_field("f_timestamp")?
                .add_output_field("f_session_id")?
                .add_output_field("f_namespace")?
                .add_output_field("f_importance")?;

            // Handle query failures gracefully (e.g., empty collection)
            let results = match self.engine.query(args) {
                Ok(r) => r,
                Err(e) => {
                    tracing::warn!("vectordb: query failed, returning empty list: {}", e);
                    return Ok(Vec::new());
                }
            };
            let ids = results.int64_field("f_id");
            let keys = results.string_field("f_key");
            let contents = results.string_field("f_content");
            let categories = results.string_field("f_category");
            let timestamps = results.string_field("f_timestamp");
            let session_ids = results.string_field("f_session_id");
            let namespaces = results.string_field("f_namespace");
            let importance_values = results.string_field("f_importance");

            Ok(keys
                .into_iter()
                .enumerate()
                .map(|(i, key)| MemoryEntry {
                    id: ids.get(i).copied().unwrap_or(i as i64).to_string(),
                    key,
                    content: contents.get(i).cloned().unwrap_or_default(),
                    category: parse_category(categories.get(i).map(String::as_str)),
                    timestamp: timestamps.get(i).cloned().unwrap_or_default(),
                    session_id: session_ids
                        .get(i)
                        .cloned()
                        .filter(|value| !value.is_empty()),
                    score: None,
                    namespace: namespaces
                        .get(i)
                        .cloned()
                        .filter(|value| !value.is_empty())
                        .unwrap_or_else(|| "default".into()),
                    importance: parse_importance(importance_values.get(i).map(String::as_str)),
                    superseded_by: None,
                })
                .collect())
        }

        async fn forget(&self, key: &str) -> anyhow::Result<bool> {
            let count = self
                .delete_by_expression(&format!("f_key == \"{}\"", escape_filter_value(key)))
                .await?;
            Ok(count > 0)
        }

        async fn purge_namespace(&self, namespace: &str) -> anyhow::Result<usize> {
            self.delete_by_expression(&format!(
                "f_namespace == \"{}\"",
                escape_filter_value(namespace)
            ))
            .await
        }

        async fn purge_session(&self, session_id: &str) -> anyhow::Result<usize> {
            self.delete_by_expression(&format!(
                "f_session_id == \"{}\"",
                escape_filter_value(session_id)
            ))
            .await
        }

        async fn count(&self) -> anyhow::Result<usize> {
            self.initialized
                .get_or_try_init(|| async { self.ensure_collection().await })
                .await?;

            let args = QueryArgumentsBuilder::new(&self.collection)?
                .expression("f_id > 0")?
                .add_output_field("f_id")?;
            let results = self.engine.query(args)?;
            Ok(results.int64_field("f_id").len())
        }

        async fn health_check(&self) -> bool {
            kart_vector_database_sys::is_available()
                && self.engine.load_state(&self.collection).unwrap_or(false)
        }
    }

    fn parse_category(value: Option<&str>) -> MemoryCategory {
        match value {
            Some("core") => MemoryCategory::Core,
            Some("daily") => MemoryCategory::Daily,
            Some("conversation") => MemoryCategory::Conversation,
            Some("summary") => MemoryCategory::Summary,
            Some(other) => MemoryCategory::Custom(other.to_string()),
            None => MemoryCategory::Core,
        }
    }

    fn parse_importance(value: Option<&str>) -> Option<f64> {
        value.and_then(|raw| {
            let trimmed = raw.trim();
            if trimmed.is_empty() {
                None
            } else {
                trimmed.parse::<f64>().ok()
            }
        })
    }

    fn extract_turn_num(category: &MemoryCategory, content: &str) -> Option<i32> {
        if *category != MemoryCategory::Summary {
            return None;
        }

        serde_json::from_str::<SummaryItem>(content)
            .ok()
            .and_then(|summary| summary.turn_num)
    }

    fn search_text_for_storage(content: &str, category: &MemoryCategory) -> String {
        if *category != MemoryCategory::Summary {
            return content.to_string();
        }

        serde_json::from_str::<SummaryItem>(content)
            .map(|summary| summary.searchable_text())
            .unwrap_or_else(|_| content.to_string())
    }

    fn escape_filter_value(value: &str) -> String {
        value.replace('\\', "\\\\").replace('"', "\\\"")
    }
}
