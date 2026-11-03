//! End-to-end test for `KylinVectorDBMemory` paired with the real
//! on-device `kart-embedding` provider. Stores several knowledge entries
//! and verifies that semantically related questions retrieve them.
//!
//! Requires both the `vectordb` and `kart-embedding` features, plus the
//! corresponding Kylin SDK packages and the default embedding model on
//! disk. When any prerequisite is missing the test prints a skip message
//! and exits successfully.

#![cfg(all(feature = "vectordb", feature = "kart-embedding"))]

use std::sync::Arc;

use kylinbot_memory::KylinVectorDBMemory;
use kylinbot_memory::embeddings::{EmbeddingProvider, KartEmbedding};
use kylinbot_memory::traits::{Memory, MemoryCategory};
use tempfile::TempDir;

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn kart_embedding_store_and_semantic_recall() {
    if !kart_vector_database_sys::is_available() {
        eprintln!("skip: kart_vector_database_sys not available");
        return;
    }
    if !kart_embedding_sys::is_available() {
        eprintln!("skip: kart_embedding_sys not available");
        return;
    }

    let embedder: Arc<dyn EmbeddingProvider> =
        match KartEmbedding::new(kart_embedding_sys::DEFAULT_TEXT_MODEL) {
            Ok(e) => Arc::new(e),
            Err(e) => {
                eprintln!("skip: KartEmbedding init failed: {e}");
                return;
            }
        };

    eprintln!(
        "embedder: {} ({} dims)",
        embedder.name(),
        embedder.dimensions()
    );

    let tmp = TempDir::new().expect("tempdir");
    let mem = KylinVectorDBMemory::new_lazy(tmp.path(), "kart_e2e", "kart_e2e", embedder)
        .expect("construct backend");

    let entries: &[(&str, &str, MemoryCategory)] = &[
        (
            "fav_color",
            "我最喜欢的颜色是蓝色。",
            MemoryCategory::Core,
        ),
        (
            "fav_food",
            "我最爱吃的食物是麻辣火锅。",
            MemoryCategory::Core,
        ),
        (
            "pet",
            "我家养了一只名叫旺财的金毛犬。",
            MemoryCategory::Core,
        ),
        (
            "job",
            "我是一名在北京工作的软件工程师,主要使用 Rust 语言。",
            MemoryCategory::Core,
        ),
        (
            "hobby",
            "周末喜欢去爬山和打羽毛球。",
            MemoryCategory::Daily,
        ),
    ];

    for (k, c, cat) in entries {
        mem.store(k, c, cat.clone(), Some("session-e2e"))
            .await
            .unwrap_or_else(|e| panic!("store {k}: {e}"));
    }

    eprintln!("stored {} entries", entries.len());
    let total = mem.count().await.expect("count");
    eprintln!("count() = {total}");
    assert!(
        total >= entries.len(),
        "expected at least {} rows, got {total}",
        entries.len()
    );

    // (question, expected key that should appear in top-3)
    let cases: &[(&str, &str)] = &[
        ("我喜欢什么颜色?", "fav_color"),
        ("我平常吃什么?", "fav_food"),
        ("我家有什么宠物?", "pet"),
        ("我做什么工作?", "job"),
        ("周末通常做哪些运动?", "hobby"),
    ];

    let mut top1_hits = 0;
    let mut top3_hits = 0;

    for (question, expected_key) in cases {
        let hits = mem
            .recall(question, 3, None, None, None)
            .await
            .unwrap_or_else(|e| panic!("recall {question}: {e}"));

        eprintln!("\nQ: {question}");
        for (i, h) in hits.iter().enumerate() {
            eprintln!(
                "  #{}: key={} score={:?} content={}",
                i + 1,
                h.key,
                h.score,
                h.content
            );
        }

        if hits.first().map(|h| h.key.as_str()) == Some(expected_key) {
            top1_hits += 1;
        }
        if hits.iter().any(|h| h.key == *expected_key) {
            top3_hits += 1;
        }
    }

    eprintln!(
        "\nresult: top1={}/{}, top3={}/{}",
        top1_hits,
        cases.len(),
        top3_hits,
        cases.len()
    );

    // Be lenient: BM25 + dense fusion on tiny corpora can shuffle ranks.
    // Require all expected keys to appear in top-3, and a majority at top-1.
    assert_eq!(
        top3_hits,
        cases.len(),
        "every expected key should appear in top-3"
    );
    assert!(
        top1_hits * 2 >= cases.len() as i32,
        "at least half of questions should hit the expected key at rank #1 (got {top1_hits}/{})",
        cases.len()
    );
}
