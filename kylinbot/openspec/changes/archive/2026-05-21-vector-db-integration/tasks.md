## 1. sys crate 基础设施（commit 1）

- [x] 1.0 **实现前必读**：阅读 `/usr/include/kylin-ai/vectordataengine.h` 与 `types/CollectionDesc.h` / `types/IndexDesc.h` / `types/CollectionStat.h`，记录所有 enum 命名与对应 i32 值；若与 demo `/home/pangyi-v11/cc-workspace/demo-for-vectorDB/main.cpp` 中使用的 magic number（DataType 5/21/101/104、FunctionType 1、IndexType 1201、MetricType 201/3）不符，**以头文件为准**并在此 task 的 commit message 或 PR 描述中记录差异
- [x] 1.1 新建 `crates/kart-vector-database-sys/Cargo.toml`：`libloading = "0.8"`、`thiserror = "2.0"`、`tracing = { version = "0.1", default-features = false }`；build-deps `bindgen = "0.71"`；feature `stub`（与 `kart-embedding-sys` 同形）；`publish = false`、`license = "MIT OR Apache-2.0"`、`repository.workspace = true`、**`edition = "2021"`**（与 `kart-embedding-sys` 一致；不要直接抄 workspace 的 `"2024"`）
- [x] 1.2 写 `crates/kart-vector-database-sys/wrapper.h`：仅一个 `#include <vectordataengine.h>`（如有附加 header 如 `kysdk-ai-common` 的错误码，按需补 include）
- [x] 1.3 写 `crates/kart-vector-database-sys/build.rs`：
  - `set_system_lib_path()` 由 `TARGET` 推 Debian triple，emit `cargo:rustc-env=KART_VECTOR_DB_SYSTEM_LIB=/usr/lib/<triple>/libkysdk-vector-engine-client.so`（非 Linux 设空）
  - `CARGO_FEATURE_STUB` 时跳过 bindgen 并 `cargo:warning=` 提示
  - 默认 `cargo:rustc-link-lib=dylib=kysdk-vector-engine-client`
  - bindgen `clang_arg("-I/usr/include/kylin-ai")`、`size_t_is_usize(true)`
  - `allowlist_function("vector_data_engine_.*")`
  - `allowlist_type("VectorDataEngine|CollectionSchema|FieldSchema|FunctionSchema|SearchArguments|AnnSearchRequest|RRFRanker|SearchResults|QueryArguments|QueryResults|.*FieldData")`
  - 输出到 `src/bindings.rs`
- [x] 1.4 写 `crates/kart-vector-database-sys/src/lib.rs` 顶层结构与错误类型：
  - `mod bindings;` gated by `#[cfg(not(feature = "stub"))]`，stub 路径用空 opaque 类型
  - `pub enum KylinVectorDBError { LibraryNotFound, EngineCreateFailed, ConnectFailed(i32), LoadDbFailed(i32), SchemaError(String), OpFailed { op: &'static str, code: i32 }, InvalidInput(String), FunctionNotFound(String) }` + `thiserror::Error`
  - `pub type Result<T> = std::result::Result<T, KylinVectorDBError>;`
- [x] 1.5 库加载与 `is_available()`：
  - `static VECTOR_LIB: OnceLock<Option<Library>>`
  - 候选顺序：`KART_VECTOR_DB_SDK_PATH` env → `env!("KART_VECTOR_DB_SYSTEM_LIB")` → exe 旁同名 .so → bare SONAME
  - 加载后校验 `vector_data_engine_create` 符号存在
  - stub 路径返回 `None`
- [x] 1.6 实现 `DataType / FunctionType / MetricType / IndexType` 枚举包装 SDK 数值常量（具名值由 `<vectordataengine.h>` 的 enum 决定；如头文件未定义命名 enum，sys crate 内用 `#[repr(i32)]` 枚举显式列出 demo 中的值并加 `// SDK magic number from vectordataengine.h:<line>` 注释）

## 2. sys crate 安全包装层（commit 1 续）

- [x] 2.1 `VectorDBEngine` 句柄：`Mutex<*mut bindings::VectorDataEngine>` + `Send + Sync` 标注 + `Drop` 调 `vector_data_engine_destroy`
  - 构造：`new(connection_id: &str) -> Result<Self>` 调 `vector_data_engine_create` + `vector_data_engine_connect`
  - `load_db_file(&self, path: &Path, encrypt: bool, key: Option<&str>) -> Result<()>`（签名对齐 `Database::LoadDBFile(file, encrypt=false, key="")`；本期固定传 `false / None`，签名预留为后续端侧加密留接口）
- [x] 2.2 Collection lifecycle 与内省：
  - `has_collection / drop_collection / show_collections / load_state`
  - `describe_collection(&self, name: &str) -> Result<CollectionDesc>`、`describe_index(&self, coll: &str, field: &str) -> Result<IndexDesc>`、`get_collection_statistics(&self, name: &str) -> Result<CollectionStat>`、`drop_index(&self, coll: &str, index_name: &str) -> Result<()>`
  - 新增安全包装结构体 `CollectionDesc / IndexDesc / CollectionStat`：字段对齐 `types/CollectionDesc.h` / `types/IndexDesc.h` / `types/CollectionStat.h` 的 KylinBot 使用子集（至少：collection name、字段列表、index name + type + metric、row_count）；裸指针由访问器复制后立即 free，结构体只持有 owned `String` / `Vec`
- [x] 2.3 Schema 构建器：
  - `CollectionSchema::new(name, desc, dynamic_field) -> Self`、`add_field(self, FieldSchema) -> Self`、`add_function(self, FunctionSchema) -> Self`
  - `FieldSchema::new(name, DataType) -> Self`、链式 `.description / .primary_key / .auto_id / .dimension / .max_len`
  - `FunctionSchema::bm25(name, in_field, out_field) -> Self`
  - Drop 路径释放 raw 句柄（注意 `create_collection` 后 function 已被 SDK 接管的所有权细节，参考 demo 第 65 行注释）
- [x] 2.4 `create_collection / create_index`：包 schema builders 转 raw 调用
- [x] 2.5 列式 insert：`FieldData` 枚举 + `insert(coll, Vec<FieldData>) -> Result<rows>`；内部把 `Vec<FieldData>` 翻成 SDK 的 `void* field_data[]`
- [x] 2.6 `SearchArguments` / `AnnSearchRequest` / `RrfRanker` builder + `search(args)` / `hybrid_search_rrf(coll, anns, ranker, top_k, outputs)`
- [x] 2.7 `QueryArguments` builder + `query(args)` / `delete(coll, expr)`
- [x] 2.8 `SearchResults` / `QueryResults` 访问器：`scores(batch)`、`int64_field(batch, name)`、`string_field(batch, name)`、`float_vec_field(batch, name)`；Drop 调对应 destroy；嵌套字段（`char**` / `float**`）按 demo 释放序列在访问器内复制后立即 free
- [x] 2.9 单元测试（不依赖 SDK 安装）：
  - `is_available_does_not_panic`
  - `error_display_*` 覆盖每个 enum 变体
  - schema builders 在 stub 模式下能构造与 drop 不 panic
- [x] 2.10 在 sys crate README/lib.rs 顶部加 module doc：库查找顺序、stub feature 用法、unsafe 边界声明

## 3. 工作空间注册（commit 2）

- [x] 3.1 修改根 `Cargo.toml` 的 `[workspace] members = [..., "crates/kart-vector-database-sys"]`
- [x] 3.2 `cargo build -p kart-vector-database-sys` 在 stub 与非 stub 默认 feature 下分别通过
- [x] 3.3 `cargo test -p kart-vector-database-sys` 在 stub feature 下通过

## 4. KylinVectorDBMemory 后端实现（commit 3）

- [x] 4.1 `crates/kylinbot-memory/Cargo.toml` 新增 `kart-vector-database-sys = { path = "../kart-vector-database-sys", optional = true }` 与 `[features] vectordb = ["dep:kart-vector-database-sys"]`
- [x] 4.2 新建 `crates/kylinbot-memory/src/vectordb.rs`：
  - `#[cfg(feature = "vectordb")]` 整个文件 gate
  - `pub struct KylinVectorDBMemory { engine, collection, embedder, initialized }`
  - `new_lazy(workspace_dir: &Path, db_name: &str, collection: &str, embedder: Arc<dyn EmbeddingProvider>) -> Result<Self>`（内部把 `.db` 解析为 `workspace_dir.join("memory").join(format!("{db_name}.db"))`，与 `SqliteMemory::new_named` 路径约定一致；必要时 `create_dir_all(parent)`）
- [x] 4.3 实现 `ensure_collection()`：
  - 若 `embedder.dimensions() == 0`（NoopEmbedding），WARN 报告向量检索不可用并直接 `Ok(())`，不创建 collection；后续 `store/recall` 由调用路径自然走空集
  - 否则检查 `has_collection`；不存在则构造 schema（`f_id` INT64 pk auto_id、`f_key` VARCHAR(128)、`f_content` VARCHAR(8192)、`f_category` VARCHAR(64)、`f_timestamp` VARCHAR(32)、`f_session_id` VARCHAR(64)、`f_embedding` FLOAT_VECTOR dim=`i32::try_from(embedder.dimensions())?`、`f_sparse` SPARSE_VECTOR），挂 BM25 FunctionSchema(content_bm25, f_content, f_sparse)，建 dense + sparse 两个索引
  - `usize → i32` 转换失败 MUST 返回 `KylinVectorDBError::InvalidInput`，不允许静默截断
- [x] 4.4 实现 `Memory::store(key, content, category, session_id)`：embed → 列式 insert（不带 `f_sparse`，SDK 自动产出）
- [x] 4.5 实现 `Memory::recall(query, limit, session_id, since, until)`（签名与 `crates/kylinbot-api/src/memory_traits.rs:128` 一致——参数是 `limit + session_id + since + until`，**没有 `category_filter`**）：embed query → `hybrid_search_rrf(dense + sparse_text, RRFRanker::new(60))` → 在 Rust 侧按 `session_id` 与 `since/until` 时间窗过滤后返回 `Vec<MemoryEntry>`
- [x] 4.6 实现 `Memory::get(key)`：`QueryArguments` with expression `f_key == "<key>"`，取首行
- [x] 4.7 实现 `Memory::list(category, session_id)`（签名与 `memory_traits.rs:141` 一致——参数是 `category + session_id`，**没有 `limit`**）：`QueryArguments` with expression 按 category 与 session_id 拼接，output fields 完整
- [x] 4.8 实现 `Memory::forget(key)`：内部委托 `self.delete_by_expression(&format!("f_key == \"{key}\""))`（见 4.13）
- [x] 4.9 实现 `Memory::count()`：query 全表取 `f_id` 长度（接受非热路径性能）
- [x] 4.10 实现 `Memory::health_check()`：`kart_vector_database_sys::is_available() && engine.load_state(&coll).is_ok()`
- [x] 4.11 实现 `Memory::name()` 返回 `"vectordb"`
- [x] 4.12 在 `crates/kylinbot-memory/src/lib.rs` 加 `#[cfg(feature = "vectordb")] pub mod vectordb;` 与 `#[cfg(feature = "vectordb")] pub use vectordb::KylinVectorDBMemory;`

### 4.x 专有 inherent methods（`impl KylinVectorDBMemory`）

- [x] 4.13 实现 `delete_by_expression(&self, expr: &str) -> Result<usize>`：直接调 `engine.delete(&coll, expr)`，返回删除行数；作为 4.8 `Memory::forget` 与 4.15 `purge_*` 的底层原语
- [x] 4.14 实现 `upsert(key, content, category, session_id)`：先 `self.forget(key).await?;` 再 `self.store(key, content, category, session_id).await?;`；`forget` 返回 `false`（key 不存在）时 MUST NOT 报错，继续 store
- [x] 4.15 override `Memory::purge_namespace(ns)` 与 `Memory::purge_session(sid)`：通过 `delete_by_expression` 按对应字段表达式批删（替换 trait 默认 `bail!`），返回删除行数
- [x] 4.16 实现 collection 生命周期专有方法（绑定当前 `self.collection`）：`has_collection() / drop_collection() / show_collections()`，内部调 sys crate 同名方法
- [x] 4.17 实现内省专有方法：`describe_collection() -> Result<CollectionDesc>`、`describe_index(field: &str) -> Result<IndexDesc>`、`collection_statistics() -> Result<CollectionStat>`、`drop_index(field: &str) -> Result<()>`
- [x] 4.18 文档注释明确：`drop_collection / drop_index` 仅用于测试/迁移；下一次 `Memory::store` 会通过 `ensure_collection()` 重建 schema / 索引

### 4.y 测试

- [x] 4.19 单元测试（`#[cfg(all(test, feature = "vectordb"))]`，仅在 SDK 可用时跑）：
  - `store_then_recall_roundtrip`（用 `tempfile::TempDir` 写隔离 .db）
  - `get_returns_inserted`
  - `forget_removes_entry`
  - `recall_respects_session_and_time_window`（验证 4.5 的 session_id + since/until 过滤）
  - `noop_embedder_does_not_create_collection`（验证 4.3 的 dim==0 边界）
  - 在 SDK 缺失的 runner 上加 `if !is_available() { eprintln!("skip"); return; }` skip 守门
- [x] 4.20 `cargo build -p kylinbot-memory -F vectordb` 通过（在装了 SDK 的 runner 上）
- [x] 4.21 专有 inherent methods 单元测试（同样 SDK 可用时才跑）：
  - `upsert_replaces_existing_entry`（先 `store`，再 `upsert` 同 key 不同 content，`get` 拿到新值）
  - `upsert_inserts_when_key_absent`
  - `delete_by_expression_batch_removes`
  - `purge_namespace_removes_all_in_namespace_and_returns_count`
  - `has_collection_drop_collection_roundtrip`（创建 → has=true → drop → has=false → 再次 store 会自动 ensure_collection）
  - `collection_statistics_row_count_matches_inserts`
  - `describe_collection_lists_schema_fields`

## 5. 工厂路由与配置（commit 4）

> 本节落地的是 design §3 描述的四步改造——`MemoryBackendKind` 枚举注册、`VECTORDB_PROFILE`、`create_memory_with_storage_and_routes` early-return、`create_memory_with_builders` match arm。**不要照抄"在 `create_memory_backend` 加分支"那种简化模板**——`create_memory_backend` 在仓库中不存在；真实工厂是 `create_memory` / `create_memory_with_storage` / `create_memory_with_storage_and_routes` / `create_memory_with_builders`。降级类型是 `NoneMemory`（`crates/kylinbot-memory/src/none.rs:9`），**不是** `NoopMemory`（该类型不存在）。

- [x] 5.1 在 `crates/kylinbot-config/` 的 memory schema 中新增 `KylinVectorDBMemoryConfig { db_name, collection, embedder }`
- [x] 5.2 定义默认值 helper：`default_vectordb_db_name() = "vector"`（最终拼成 `${workspace_dir}/memory/vector.db`）、`default_vectordb_collection() = "kylinbot_memory"`、`default_vectordb_embedder() = "kart-embedding"`
- [x] 5.3 在 `MemoryConfig`（`crates/kylinbot-config/src/schema.rs:5294`）上挂 `pub vectordb: KylinVectorDBMemoryConfig`（与现有 `qdrant: QdrantConfig` 同级位置，加 `#[serde(default)]`）
- [x] 5.4 在 `crates/kylinbot-memory/src/backend.rs` 注册新 backend 类别：
  - `MemoryBackendKind::Vectordb` 变体
  - `classify_memory_backend("vectordb") -> MemoryBackendKind::Vectordb`
  - `VECTORDB_PROFILE`（`key="vectordb"`、`optional_dependency=true`、`sqlite_based=false`、`uses_sqlite_hygiene=false`、`auto_save_default=true`）
  - `memory_backend_profile` match 中新增 `MemoryBackendKind::Vectordb => VECTORDB_PROFILE`
  - 暂不进 `SELECTABLE_MEMORY_BACKENDS`（onboarding 不展示，等 wizard 检测 SDK 逻辑就绪后再加）
  - `backend.rs::tests` 加 `classify_memory_backend("vectordb")` 与 `memory_backend_profile("vectordb")` 的断言
- [x] 5.5 在 `crates/kylinbot-memory/src/lib.rs::create_memory_with_storage_and_routes` 加 early-return 分支（与现有 `Qdrant` 分支同形，见 design §3 Step 2 伪代码）：feature 关时 WARN 后返回 `Box::new(NoneMemory::new())`；feature 开 + `is_available()` 为 false 时同上 WARN 降级；`KylinVectorDBMemory::new_lazy(...)` 构造失败 `tracing::error!` 后降级；成功时返回 `Box::new(KylinVectorDBMemory::new_lazy(workspace_dir, &config.vectordb.db_name, &config.vectordb.collection, embedder)?)`
- [x] 5.6 在 `create_memory_with_builders` 的 `match classify_memory_backend(backend_name)` 中加 `MemoryBackendKind::Vectordb` arm：与 `Postgres` arm 一致 `anyhow::bail!("vectordb backend requires MemoryConfig + embedder; call create_memory_with_storage_and_routes instead of create_memory_with_builders")`，避免落入 `Unknown → MarkdownMemory` fallback
- [x] 5.7 **降级类型 cheat sheet**：memory 侧降级目标是 `NoneMemory`（`crates/kylinbot-memory/src/none.rs:9`，`name()` 返回 `"none"`）；embedding 侧降级目标是 `embeddings::NoopEmbedding`（`embeddings.rs:26`）；两者命名不同，工厂代码里**绝不**写 `NoopMemory`（不存在）。
- [x] 5.8 反序列化测试：`MemoryConfig` 默认值能从空 toml 加载；`[memory.vectordb]` 段能解析；缺字段使用默认；`backend = "vectordb"` 字符串能被 `classify_memory_backend` 识别为 `MemoryBackendKind::Vectordb`
- [x] 5.9 更新 `AGENTS.md` 第 74 行 `crates/kylinbot-memory/` 的 callout（当前文本：`memory backends (markdown, sqlite, embeddings, vector merge)`），把 `vectordb` 写入其中。如果 `AGENTS.md` 没有独立的 "memory backends" 段落，本 task 同时新增一段简短说明：依赖 `libkysdk-vector-engine-client-dev` 与 `vectordb` feature；hybrid 由 SDK 内置；默认不启用

## 6. 测试与验证

- [x] 6.1 `cargo build` 默认 feature 集通过（无需安装 SDK）
- [x] 6.2 `cargo build -p kart-vector-database-sys --features stub` 通过
- [x] 6.3 `cargo test -p kart-vector-database-sys` 通过（不依赖 SDK 加载）
- [x] 6.4 `cargo test -p kylinbot-memory` 通过（默认 feature 不含 `vectordb`，所有现有测试不动）
- [x] 6.5 在装了 `libkysdk-vector-engine-client-dev` 的开发机：`cargo test -p kylinbot-memory -F vectordb` 通过
- [x] 6.6 在装了 SDK 的开发机：配 `[memory] backend = "vectordb"` 启动 KylinBot；通过 CLI / API 触发 `memory store` / `memory recall`，验证 hybrid 路径返回结果（命中 `KylinVectorDBMemory` 而非 `NoneMemory`，并核对日志中 backend kind 解析正确）
- [x] 6.7 SDK 缺失场景：在没装 SDK 的机器上配 `[memory] backend = "vectordb"` 启动；确认降级为 `NoneMemory` 且打印 WARN，进程不崩溃

## 7. 文档（可选 commit 5）

- [x] 7.1 `docs/` 下 memory backend 章节新增 `vectordb.md`：前置依赖（debian 包名、`libclang`）、配置示例（`[memory] backend = "vectordb"` + `[memory.vectordb]` 段）、hybrid 行为说明、与 Qdrant 的对比表
- [x] 7.2 `CHANGELOG-next.md` 加一条：`Added: vectordb memory backend (on-device VectorDataEngine, opt-in via vectordb feature).`
- [x] 7.3 `AGENTS.md` 已在 5.9 更新；如有 README quickstart 表，加一行注脚
