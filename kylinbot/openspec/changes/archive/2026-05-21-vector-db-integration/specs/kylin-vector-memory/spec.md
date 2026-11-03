## ADDED Requirements

### Requirement: kart-vector-database-sys FFI 包装层 (FFI wrapper crate)

KylinBot SHALL 提供独立 sys crate `kart-vector-database-sys`，作为 `libkysdk-vector-engine-client` 的唯一 `unsafe` 边界。该 crate MUST 使用 `libloading` 在运行时加载 `libkysdk-vector-engine-client.so`（而非 link 时绑定），MUST 提供 `stub` Cargo feature 跳过 bindgen 与符号查找，并 MUST 以具名枚举（`DataType` / `FunctionType` / `MetricType` / `IndexType`）包装 SDK 的整数常量，使下游 crate 不出现裸 magic number。库查找顺序 MUST 为：`KART_VECTOR_DB_SDK_PATH` 环境变量 → 编译期多架构系统路径 → 与运行二进制同目录 → bare SONAME。

#### Scenario: 工作站未安装 SDK 但开启 stub feature
- **WHEN** 在没有装 `libkysdk-vector-engine-client-dev` 的机器上运行 `cargo build -p kart-vector-database-sys --features stub`
- **THEN** 构建 MUST 成功
- **AND** `kart_vector_database_sys::is_available()` MUST 返回 `false`
- **AND** 所有公开方法 MUST 返回 `KylinVectorDBError::LibraryNotFound`，不允许 panic 或未定义行为

#### Scenario: SDK 可用时加载成功
- **WHEN** 运行环境中 `libkysdk-vector-engine-client.so` 可通过任一查找路径定位且导出 `vector_data_engine_create`
- **THEN** `kart_vector_database_sys::is_available()` MUST 返回 `true`
- **AND** `VectorDBEngine::new("<id>")` MUST 成功返回句柄

#### Scenario: SDK 函数缺失
- **WHEN** 运行时加载的 `.so` 缺少某个被调用的符号
- **THEN** 对应方法 MUST 返回 `KylinVectorDBError::FunctionNotFound(name)`
- **AND** MUST NOT 触发 panic 或未定义行为

#### Scenario: 构造失败时不泄漏 engine 句柄
- **WHEN** `VectorDBEngine::new` 在 `vector_data_engine_create` 成功后 `vector_data_engine_connect` 返回非零
- **THEN** 已分配的 engine 句柄 MUST 通过 RAII 守卫被 `vector_data_engine_destroy` 释放
- **AND** 错误 MUST 以 `KylinVectorDBError::ConnectFailed(code)` 返回

#### Scenario: SearchResults 离开作用域时自动释放
- **WHEN** `VectorDBEngine::search` / `hybrid_search_rrf` / `query` 返回的 `SearchResults` 或 `QueryResults` 离开作用域
- **THEN** `Drop` MUST 调对应的 `vector_data_engine_destroy_search_results` / `vector_data_engine_destroy_query_results`
- **AND** 通过访问器读取的嵌套字段（`char**` / `float**`）MUST 在内部完成复制后立即按 demo 的释放顺序 free，应用层 MUST NOT 接触原始裸指针

### Requirement: KylinVectorDBMemory 后端实现 (Backend implementation)

KylinBot SHALL 在 `crates/kylinbot-memory/` 提供 `KylinVectorDBMemory`，实现 `kylinbot_api::memory_traits::Memory` 的所有方法。该后端 MUST 通过 Cargo feature `vectordb = ["dep:kart-vector-database-sys"]` 启用，feature 未启用时整个 `KylinVectorDBMemory` 类型 MUST 在 `#[cfg(feature = "vectordb")]` 下不编译。`Memory::name()` MUST 返回 `"vectordb"`。

#### Scenario: 首次 store 触发 collection 初始化
- **WHEN** 调用方第一次调用 `store(key, content, category, session_id)` 且 collection 不存在
- **THEN** 后端 MUST 按以下 schema 创建 collection：字段 `f_id` (INT64, primary_key=true, auto_id=true)、`f_key` (VARCHAR 128)、`f_content` (VARCHAR 8192)、`f_category` (VARCHAR 64)、`f_timestamp` (VARCHAR 32)、`f_session_id` (VARCHAR 64)、`f_embedding` (FLOAT_VECTOR, dimension=embedder.dimensions())、`f_sparse` (SPARSE_VECTOR)
- **AND** MUST 在 collection 上挂一个 BM25 `FunctionSchema`，输入字段 `f_content`，输出字段 `f_sparse`
- **AND** MUST 为 `f_embedding` 与 `f_sparse` 分别建立 dense 与 sparse 索引
- **AND** MUST 把 `f_key / f_content / f_category / f_timestamp / f_session_id / f_embedding` 列式 insert 写入
- **AND** MUST NOT 显式写入 `f_sparse`（由 SDK 通过 BM25 FunctionSchema 自动写入）

#### Scenario: recall 走 SDK 内置 hybrid
- **WHEN** 调用方调用 `recall(query, limit, session_id, since, until)`
- **THEN** 后端 MUST embed `query` 得到 dense 查询向量
- **AND** MUST 构造两个 `AnnSearchRequest`：一个对 `f_embedding` 用 dense 向量、一个对 `f_sparse` 用 BM25 文本路径（`query` 文本作为输入）
- **AND** MUST 以 `RRFRanker::new(60)` 调用 `vector_data_engine_hybrid_search_rrf` 一次性获取融合 top-k（`k = limit`）
- **AND** MUST NOT 调用 `vector.rs::hybrid_merge`
- **AND** MUST 在 Rust 侧按 `session_id` 与 `since` / `until` 时间窗过滤后返回 `Vec<MemoryEntry>`（与 `Memory` trait 的 `recall` 实际签名一致——`recall` 没有 `category_filter` 参数，类别筛选属于 `list` 的语义）

#### Scenario: get 按业务键定位
- **WHEN** 调用方调用 `get(key)`
- **THEN** 后端 MUST 通过 `QueryArguments` 表达式 `f_key == "<key>"` 查询
- **AND** 若有结果 MUST 返回 `Some(MemoryEntry)`，否则 `None`

#### Scenario: forget 按业务键删除
- **WHEN** 调用方调用 `forget(key)`
- **THEN** 后端 MUST 调 `vector_data_engine_delete`，表达式 `f_key == "<key>"`
- **AND** MUST 返回是否删除了至少一行

#### Scenario: embedder 维度为 0 时不创建向量字段
- **WHEN** 注入的 `embedder` 是 `NoopEmbedding`（`dimensions() == 0`）
- **THEN** `ensure_collection()` MUST 以 WARN 日志报告向量检索不可用
- **AND** MUST NOT 创建 collection
- **AND** 后续 `store / recall` MUST 与 `QdrantMemory` 在 noop embedder 下的行为一致（不崩溃，返回空结果）

### Requirement: KylinVectorDBMemory 专有管理与内省接口 (Proprietary admin & inspection surface)

`KylinVectorDBMemory` SHALL 在 `impl KylinVectorDBMemory`（inherent method，非 `Memory` trait 方法）上暴露 `Database.h` 中 `Memory` trait 未覆盖的能力，用于测试、迁移、运维诊断与原生表达式批操作。这些方法 MUST 仅在 `#[cfg(feature = "vectordb")]` 下编译，trait object（`Box<dyn Memory>`）视图 MUST NOT 受影响。

凡 `Memory` trait 已覆盖的 `Database.h` 方法（`Insert` / `Search` / `HybridSearch` / 按主键的 `Query` / 按表达式列表的 `Query` / 按单主键的 `Delete` / `GetLoadState` / `Connect` / `Disconnect` / `LoadDBFile` / `CreateCollection*` / `CreateIndex`）MUST NOT 在 inherent impl 上重复暴露，避免双入口。

#### Scenario: collection 生命周期管理
- **WHEN** 调用方调用 `has_collection()` / `drop_collection()` / `show_collections()`
- **THEN** 后端 MUST 分别返回当前 collection 是否存在、删除当前 collection、返回同一 .db 文件中所有 collection 名
- **AND** `drop_collection()` 调用后下一次 `Memory::store` MUST 重新触发 `ensure_collection()` 重建 schema

#### Scenario: schema 与统计信息内省
- **WHEN** 调用方调用 `describe_collection()` / `describe_index(field)` / `collection_statistics()` / `drop_index(field)`
- **THEN** 后端 MUST 通过 SDK 对应函数返回安全包装结构体（`CollectionDesc` / `IndexDesc` / `CollectionStat`），不暴露 SDK 裸指针
- **AND** `CollectionStat` MUST 至少包含 row_count；其余字段按 SDK 头文件 mirror 必要子集
- **AND** `drop_index(field)` 后下一次 `Memory::store` MUST 通过 `ensure_collection()` 重建索引

#### Scenario: Upsert 通过 forget+store 委托
- **WHEN** 调用方调用 `upsert(key, content, category, session_id)`
- **THEN** 后端 MUST 先调 `Memory::forget(key)` 再调 `Memory::store(key, content, category, session_id)`
- **AND** MUST NOT 改变 `Memory::store` 自身的语义（store 仍是纯插入）
- **AND** 即使 `forget` 返回 `false`（key 不存在）也 MUST 继续 store，不报错

#### Scenario: delete_by_expression 作为 Memory trait 删除路径的底层原语
- **WHEN** 调用方调用 `delete_by_expression(expr)`
- **THEN** 后端 MUST 调 `vector_data_engine_delete`，原样透传表达式，返回删除行数
- **AND** `Memory::forget(key)` MUST 实现为 `delete_by_expression(&format!("f_key == \"{key}\""))`
- **AND** `Memory::purge_namespace(ns)` 与 `Memory::purge_session(sid)` MUST 通过对应字段表达式委托给 `delete_by_expression`，返回删除行数

#### Scenario: feature 未启用时专有方法不存在
- **WHEN** 在未启用 `vectordb` feature 的构建中尝试引用 `KylinVectorDBMemory::has_collection` 等专有方法
- **THEN** 编译 MUST 失败（整个 `KylinVectorDBMemory` 类型在 `#[cfg(feature = "vectordb")]` 下不存在）
- **AND** 默认 feature 集的 `Box<dyn Memory>` 调用方 MUST NOT 受影响

### Requirement: 工厂路由与 SDK 缺失降级 (Factory routing & graceful fallback)

`crates/kylinbot-memory` 的工厂函数（`create_memory_with_storage_and_routes` 与 `create_memory_with_builders`，经由 `MemoryBackendKind` / `classify_memory_backend` 分派）SHALL 识别配置字符串 `"vectordb"`，并在 SDK 缺失、feature 关闭或构造失败时降级为 `NoneMemory`（`crates/kylinbot-memory/src/none.rs`）。MUST NOT panic 或拒绝启动。

实现路径：
- `MemoryBackendKind` 新增 `Vectordb` 变体；`classify_memory_backend("vectordb") -> MemoryBackendKind::Vectordb`
- `backend.rs` 新增 `VECTORDB_PROFILE`（`key="vectordb"`、`optional_dependency=true`、`sqlite_based=false`、`uses_sqlite_hygiene=false`）；`memory_backend_profile` match 加 `Vectordb` arm
- `create_memory_with_storage_and_routes` 加 `if matches!(backend_kind, MemoryBackendKind::Vectordb) { ... early return }` 分支（与 `Qdrant` 分支同形）
- `create_memory_with_builders` 的 `match` 加 `MemoryBackendKind::Vectordb` arm（避免落入 `Unknown → MarkdownMemory`）

#### Scenario: SDK 可用且 feature 启用
- **WHEN** 配置 `[memory] backend = "vectordb"`、二进制以 `--features vectordb` 构建、`kart_vector_database_sys::is_available()` 返回 `true`
- **THEN** 工厂 MUST 返回 `Box<KylinVectorDBMemory>`（通过 `new_lazy(workspace_dir, &cfg.db_name, &cfg.collection, embedder)` 构造；内部把 `.db` 解析为 `${workspace_dir}/memory/${db_name}.db`）

#### Scenario: feature 启用但 SDK 缺失
- **WHEN** 二进制以 `--features vectordb` 构建，但运行环境无法定位 `.so`
- **THEN** 工厂 MUST 以 `tracing::warn!` 记录降级原因
- **AND** MUST 返回 `NoneMemory`
- **AND** 进程 MUST NOT 崩溃

#### Scenario: feature 未启用
- **WHEN** 配置 `[memory] backend = "vectordb"` 但二进制未启用 `vectordb` feature
- **THEN** 工厂 MUST 以 `tracing::warn!` 记录 "feature 未启用"
- **AND** MUST 返回 `NoneMemory`

#### Scenario: 构造失败降级
- **WHEN** `KylinVectorDBMemory::new_lazy(...)` 返回 `Err`（例如 `.db` 路径不可写）
- **THEN** 工厂 MUST 以 `tracing::error!` 记录原因
- **AND** MUST 返回 `NoneMemory`

### Requirement: 配置 schema (Configuration schema)

KylinBot SHALL 在 `crates/kylinbot-config/` 暴露 `[memory.vectordb]` 配置段。所有字段 MUST 有 serde 默认值，使缺省 TOML 仍能解析。

字段约束：
- `db_name: String` 默认 `"vector"`；`.db` 文件最终位于 `${workspace_dir}/memory/${db_name}.db`，与 `SqliteMemory::new_named` 的路径约定一致，**不再使用** `$XDG_DATA_HOME`
- `collection: String` 默认 `"kylinbot_memory"`
- `embedder: String` 默认 `"kart-embedding"`（复用现有 embedding provider 工厂）

#### Scenario: 缺省配置加载
- **WHEN** TOML 中没有 `[memory.vectordb]` 段
- **THEN** `MemoryConfig` MUST 反序列化成功
- **AND** `vectordb` 字段 MUST 使用上述全部默认值
- **AND** `KylinVectorDBMemory` MUST 把 `.db` 文件写到 `${workspace_dir}/memory/vector.db`

#### Scenario: 显式覆盖
- **WHEN** TOML 中提供 `[memory.vectordb]` 含 `db_name = "test"`、`collection = "test_coll"`、`embedder = "noop"`
- **THEN** `MemoryConfig` MUST 反序列化为对应字段的覆盖值
- **AND** `KylinVectorDBMemory` MUST 把 `.db` 文件写到 `${workspace_dir}/memory/test.db`

### Requirement: 不回归现有后端 (No regression on existing backends)

本变更 SHALL NOT 修改 `QdrantMemory`、`SqliteMemory`、`PostgresMemory` 或 `kart-embedding` provider 的任何行为；`vector.rs::hybrid_merge` 工具 MUST 继续供 `SqliteMemory`（`sqlite.rs:680`）使用（Qdrant 路径目前不调用 `hybrid_merge`，本变更不改变这一点）。

#### Scenario: 默认 feature 集
- **WHEN** 用户在不开启 `vectordb` feature 的情况下执行 `cargo build` 与 `cargo test -p kylinbot-memory`
- **THEN** 所有现有测试 MUST 通过
- **AND** MUST NOT 链接 `libkysdk-vector-engine-client`
- **AND** MUST NOT 要求安装 `libkysdk-vector-engine-client-dev`

#### Scenario: 现有后端不回归
- **WHEN** 用户配置 `[memory] backend = "qdrant"`、`backend = "sqlite"` 或 `backend = "postgres"`
- **THEN** 行为 MUST 与本变更合入前完全一致（含 `SqliteMemory` 的应用层 `hybrid_merge` 路径）
