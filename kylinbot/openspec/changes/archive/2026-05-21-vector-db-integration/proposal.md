## 为什么 (Why)

KylinBot 的长程记忆当前依赖 `QdrantMemory`（远程 REST 向量库）或 `SqliteMemory`（本地 SQL + 应用层余弦相似度）作为后端。两者各有约束：Qdrant 要求一个外部服务进程；`SqliteMemory` 在 `sqlite.rs:680` 通过 `vector.rs::hybrid_merge` 做向量 + BM25 的应用层加权融合，且无内置向量索引，召回成本随条目数线性增长。Kylin 提供的 `libkysdk-vector-engine-client-dev`（pkg-config 名 `kysdk-vector-engine-client`）是一个**进程内嵌入式**向量库，自带 ANN 索引、SPARSE_VECTOR、内置 BM25 `FunctionSchema` 与 `hybrid_search_rrf`，从签名上看正好对应 KylinBot 的 hybrid recall 需求。本变更把该 SDK 作为一个新的 `Memory` 后端 `KylinVectorDBMemory` 接入，与 Qdrant/SQLite 平级共存，让端侧部署的 KylinBot 可以在无外部服务的前提下获得带索引的向量 + 关键词 hybrid 召回。

参考来源：用户提供的 C demo `/home/pangyi-v11/cc-workspace/demo-for-vectorDB/main.cpp` 已验证 SDK 的 connect / load_db_file / create_collection / create_index / insert / search / hybrid_search_rrf / query / delete 全链路。

## 变更内容 (What Changes)

- 新增独立 sys crate `crates/kart-vector-database-sys/`：libloading 运行时加载 `libkysdk-vector-engine-client.so`，bindgen 从 `<vectordataengine.h>` 生成绑定，allowlist 仅暴露文本/向量必需的子集；提供 `stub` feature 让没有装 SDK 的工作站和 CI 仍能编译，所有公开 API 在 stub 下返回 `KylinVectorDBError::LibraryNotFound`。安全包装层覆盖：`VectorDBEngine`（连接 + 加载 .db 文件 + RAII Drop）、`CollectionSchemaBuilder` / `FieldSchemaBuilder` / `FunctionSchemaBuilder`、列式 insert 的 `FieldData` 枚举包装、`SearchArguments` / `AnnSearchRequest` / `RRFRanker` 构建器、`SearchResults` / `QueryResults` 安全访问器（按字段名取 i64 / 字符串 / float vec，自动 destroy）。库查找顺序复用 `kart-embedding-sys` 的约定：`KART_VECTOR_DB_SDK_PATH` → multiarch system path → exe 旁路径 → bare SONAME。
- 在工作空间 `Cargo.toml` 的 `workspace.members` 注册 `crates/kart-vector-database-sys`（机械步骤，单独 commit）。
- 在 `crates/kylinbot-memory/` 新增 `src/vectordb.rs`，实现 `Memory` trait 的 `KylinVectorDBMemory` 后端：与现有 `QdrantMemory` 平级；schema 在 `ensure_collection()` 中编程式创建（字段：`f_id` INT64 pk auto，`f_key` VARCHAR，`f_content` VARCHAR，`f_category` VARCHAR，`f_timestamp` VARCHAR，`f_session_id` VARCHAR，`f_embedding` FLOAT_VECTOR dim=embedder.dimensions()，`f_sparse` SPARSE_VECTOR；外加一个 BM25 `FunctionSchema` 把 `f_content` 自动映射成 `f_sparse`）；`recall()` 通过 `vector_data_engine_hybrid_search_rrf` 用 `AnnSearchRequest(f_embedding, query_embedding) + AnnSearchRequest(f_sparse, query_text) + RRFRanker(k=60)` 一次拿到融合结果，**不再走应用层 `hybrid_merge`**。Cargo feature `vectordb = ["dep:kart-vector-database-sys"]`，未启用 feature 时 `KylinVectorDBMemory` 整体被 `#[cfg(feature = "vectordb")]` gate 掉。
- 在 `crates/kylinbot-memory/` 工厂中把 `"vectordb"` 字符串接入现有的 `MemoryBackendKind` 路由（`crates/kylinbot-memory/src/backend.rs::classify_memory_backend`）：新增 `MemoryBackendKind::Vectordb` 变体与对应 `VECTORDB_PROFILE`，并在 `lib.rs::create_memory_with_storage_and_routes` 中加 early-return 分支（与 `Qdrant` 分支同形）、在 `lib.rs::create_memory_with_builders` 的 match 中加 `Vectordb` arm，避免落入 `Unknown → MarkdownMemory` fallback。SDK 缺失（`kart_vector_database_sys::is_available()` 为 `false`）时降级为 `NoneMemory` 并 WARN 日志（与 `embeddings::create_embedding_provider("kart-embedding", ...)` 在 SDK 缺失下降级到 `NoopEmbedding` 的行为对称——注意两边的 "noop" 命名不同：memory 侧是 `NoneMemory`，embedding 侧是 `NoopEmbedding`）。
- 在 `crates/kylinbot-config/` 的 memory schema 中新增 `[memory.vectordb]` 段：`db_name: String`（默认 `"vector"`，最终落盘到 `${workspace_dir}/memory/vector.db`，与 `SqliteMemory::new_named` 的 `workspace_dir/memory/{db_name}.db` 约定一致，不再使用 `$XDG_DATA_HOME`）、`collection: String`（默认 `kylinbot_memory`）、`embedder: String`（默认 `"kart-embedding"`，复用现有 embedding provider 工厂）；不引入新的 secrets。`MemoryConfig.backend: String` 字段（schema.rs:5296）通过取值 `"vectordb"` 启用本后端，与现有 `backend = "qdrant"` / `"sqlite"` / `"postgres"` 形态一致。
- 测试：`kart-vector-database-sys` 自带 `is_available_does_not_panic` 一类不依赖 SDK 加载的单元测试；`KylinVectorDBMemory` 单元测试使用 `tempfile::TempDir` 写一个临时 `.db` 文件，仅在 `vectordb` feature 与 SDK 可用时跑 store/recall/get/forget happy path。
- 文档：`AGENTS.md` 的 memory backend 段落新增 `vectordb` 行；README 的 quickstart 不动（端侧 SDK 不是默认依赖）。

## 能力 (Capabilities)

### 新增能力 (New Capabilities)
- `kylin-vector-memory`：基于 `libkysdk-vector-engine-client` 的端侧嵌入式向量 + BM25 hybrid 记忆后端、与之配套的 sys 层安全包装、collection schema 约定、SDK 缺失降级策略、配置形态。

### 修改能力 (Modified Capabilities)
- （无 —— 现有 `QdrantMemory`、`SqliteMemory`、`PostgresMemory` 与 `kart-embedding` provider 均不修改。`vector.rs::hybrid_merge` 工具继续供 `SqliteMemory` 路径（`sqlite.rs:680`）使用——Qdrant 路径目前未调用 `hybrid_merge`。）

## 影响范围 (Impact)

- **代码**：
  - 新增 crate：`crates/kart-vector-database-sys/{Cargo.toml,build.rs,wrapper.h,src/{lib.rs,bindings.rs}}`。
  - `crates/kylinbot-memory/src/vectordb.rs`（新）、`crates/kylinbot-memory/src/lib.rs`（加 `pub mod vectordb;` 与工厂分支）、`crates/kylinbot-memory/Cargo.toml`（新增 `kart-vector-database-sys = { path = "...", optional = true }` 与 feature `vectordb`）。
  - `crates/kylinbot-config/`：memory schema 新增 `[memory.vectordb]` 段（结构体 + 默认值 + 反序列化测试）。
  - 工作空间 `Cargo.toml`：`workspace.members` 加 `crates/kart-vector-database-sys`。
- **构建依赖**：仅当显式启用 `vectordb` feature 且 SDK 已安装时才链接 `kysdk-vector-engine-client`；默认 feature 集不引入 debian 包依赖。bindgen 通过 `wrapper.h` + `-I/usr/include/kylin-ai` 生成绑定；CI 在 stub feature 下编译，不要求安装 `libkysdk-vector-engine-client-dev`。
- **配置**：新增 `[memory.vectordb]` 段；现有 `[memory.qdrant]`、`[memory.sqlite]`、`[memory.postgres]` 字段语义不变。
- **环境变量**：新增 `KART_VECTOR_DB_SDK_PATH`（与 `KART_EMBEDDING_SDK_PATH` 对称的库路径覆盖）。
- **测试**：`kart-vector-database-sys` 单元测试（不依赖 SDK 加载）、`KylinVectorDBMemory` 在 `vectordb` feature 下的端到端单元测试（用 tempfile 隔离 .db 文件）。
- **稳定性等级**：`kart-vector-database-sys` 为新增 Experimental crate（沿用 `kart-embedding-sys` / `aardvark-sys` 的归类）；`kylinbot-memory` / `kylinbot-config` 已在 stable 表内，本次仅为加法变更（新增 feature、新增字段，无 BREAKING）。
- **不在范围内**：
  - **不抽新 trait**：`KylinVectorDBMemory` 与 `QdrantMemory` / `SqliteMemory` 一样直接 impl `kylinbot_api::memory_traits::Memory`，不引入 `VectorStore` 中间层。如果未来出现第三个嵌入式向量库后端再考虑抽象。
  - **不下沉 SQLite 的 hybrid 检索到 SDK**：本次只为 `KylinVectorDBMemory` 启用 SDK 内置 BM25 + RRF；`SqliteMemory` 继续用应用层 `vector.rs::hybrid_merge`。
  - **不接入 image embedding / dense-only 模式**：BM25 + hybrid 是默认形态；纯 dense 检索可在未来 follow-up 中加 feature gate。
  - **不修改 Memory trait**：现有 `store / recall / get / list / forget / count / health_check` 等签名保持不变；hybrid 用 RRF 是 backend 内部实现细节，不向 trait 表面渗透。
  - **不替换 Qdrant 为默认**：默认 memory backend 路由保持现状（`MemoryConfig.backend` 默认 `"sqlite"`）；用户需通过 `[memory] backend = "vectordb"` 显式启用。

## 能力边界 (Capability Boundaries)

| 能力                          | QdrantMemory          | SqliteMemory       | KylinVectorDBMemory (新) |
|------------------------------|-----------------------|--------------------|------------------------|
| 部署形态                      | 远程 HTTP 服务         | 进程内 SQLite      | 进程内嵌入式 + 本地 .db |
| ANN 向量索引                  | ✓                     | ✗（线性扫描）       | ✓                      |
| 关键词检索                    | 应用层 BM25 over text  | 应用层 BM25         | SDK 内置 BM25 (FunctionSchema) |
| Hybrid 融合策略               | （当前未启用 hybrid_merge） | 应用层 `hybrid_merge` 加权 | SDK `hybrid_search_rrf` + `RRFRanker` |
| 表达式过滤                    | Qdrant filter JSON    | SQL WHERE          | SDK 表达式字符串（如 `f_id > 1`）|
| 是否依赖外部进程              | 是                    | 否                  | 否                     |
| 是否依赖 debian 包            | 否                    | 否                  | `libkysdk-vector-engine-client-dev` |
| 启用方式                      | `[memory] backend = "qdrant"`（无 Cargo feature gate，直接编进 `kylinbot-memory`）| `[memory] backend = "sqlite"`（默认）| `[memory] backend = "vectordb"` + Cargo feature `vectordb`（默认关）|
