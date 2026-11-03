//! Smoke test for `KylinVectorDBMemory`. Requires the `vectordb` feature
//! and `libkysdk-vector-engine-client` SDK to be present at runtime. If the
//! SDK is missing the test prints a skip message and returns success.

#![cfg(feature = "vectordb")]

use std::sync::Arc;

use kylinbot_memory::KylinVectorDBMemory;
use kylinbot_memory::embeddings::EmbeddingProvider;
use kylinbot_memory::traits::{Memory, MemoryCategory};
use tempfile::TempDir;

/// Deterministic stand-in embedder so the test does not depend on a real
/// model being available. Maps each input char to a slot in a 32-dim vector.
struct HashEmbedder;

#[async_trait::async_trait]
impl EmbeddingProvider for HashEmbedder {
    fn dimensions(&self) -> usize {
        32
    }
    async fn embed(&self, texts: &[&str]) -> anyhow::Result<Vec<Vec<f32>>> {
        Ok(texts
            .iter()
            .map(|text| {
                let mut v = vec![0.0f32; 32];
                for (i, c) in text.chars().enumerate() {
                    v[(c as usize + i) % 32] += 1.0;
                }
                let norm: f32 = v.iter().map(|x| x * x).sum::<f32>().sqrt().max(1e-6);
                for x in &mut v {
                    *x /= norm;
                }
                v
            })
            .collect())
    }
    fn name(&self) -> &str {
        "hash-32"
    }
}

#[tokio::test(flavor = "current_thread")]
async fn smoke_store_recall_get_forget_count() {
    if !kart_vector_database_sys::is_available() {
        eprintln!("skip: kart_vector_database_sys::is_available() == false");
        return;
    }

    let tmp = TempDir::new().expect("tempdir");
    let embedder: Arc<dyn EmbeddingProvider> = Arc::new(HashEmbedder);
    let mem = KylinVectorDBMemory::new_lazy(tmp.path(), "smoke", "kylinbot_smoke", embedder)
        .expect("construct backend");

    assert_eq!(mem.name(), "vectordb");

    // store three entries
    mem.store(
        "k1",
        "the quick brown fox",
        MemoryCategory::Core,
        Some("sess-a"),
    )
    .await
    .expect("store k1");
    mem.store(
        "k2",
        "jumps over the lazy dog",
        MemoryCategory::Core,
        Some("sess-a"),
    )
    .await
    .expect("store k2");
    mem.store(
        "k3",
        "pangram with quick fox",
        MemoryCategory::Daily,
        Some("sess-b"),
    )
    .await
    .expect("store k3");

    // get a known key
    let got = mem.get("k1").await.expect("get k1");
    let got = got.expect("k1 present");
    assert_eq!(
        got.content, "the quick brown fox",
        "get returns stored content"
    );

    // recall — should retrieve something for "quick fox"
    let hits = mem
        .recall("quick fox", 5, None, None, None)
        .await
        .expect("recall");
    eprintln!("recall returned {} hits", hits.len());
    assert!(!hits.is_empty(), "recall should produce at least one hit");

    // count after 3 inserts
    let c = mem.count().await.expect("count");
    assert!(c >= 3, "count >= 3 after three stores, got {c}");

    // forget removes k1
    let removed = mem.forget("k1").await.expect("forget");
    assert!(removed, "forget should report success for existing key");
    let got = mem.get("k1").await.expect("get after forget");
    assert!(got.is_none(), "k1 should be gone after forget");

    // health_check returns true when SDK present and collection loaded
    let healthy = mem.health_check().await;
    eprintln!("health_check -> {healthy}");
    assert!(healthy, "health_check should be true with SDK available");
}
