# Proposal: 向量库记忆模块综合优化

## Why

KylinBot 向量库记忆模块存在多维度问题，经评测报告 `vectordb-memory-issues-and-priority.md` 系统识别如下：

### P1: 摘要检索噪声

写入 `MemoryCategory::Summary` 的记录把整个 `SummaryItem` 序列化成 JSON 当 `content` 存储。该 JSON 既被直接送进 dense embedding，又通过 BM25 从 `f_content` 字段自动建索引。结果：

- `memory_id`(UUID)、`session_id`、`generated_at`(时间戳)、`transcript_session_key`
- JSON 字段名（`"summary"`、`"key_facts"`…）

全部进入向量与词表，成为噪声，稀释语义信号、让相似摘要难以区分，并随库增大持续恶化检索相关性。

### P2: 默认召回丢原文

`RetrievalMode` 默认 `SummaryOnly`，且 `memory_recall` 在 agent 未显式传参时取默认值。vectordb 后端虽已实现"摘要 + 原始 turn 回放"，但因默认不开启，召回结果只剩结构化摘要，丢失时间锚点与对话细节。

即便开启 `SummaryWithTurns`，呈现顺序颠倒——摘要正文在前、原始 turn 追加在末尾，不利于以原文为锚点还原上下文。

### P3: 空壳摘要污染

三条摘要触发路径（agent.rs / loop_.rs / orchestrator）都无条件为每一轮生成摘要。寒暄、确认（"好的"）等无信息量对话会产生空壳 `SummaryItem` 污染向量库。

现成的过滤钩子 `SummaryItem::has_structured_content()` 定义了却从未被调用。

### P4: 摘要阻塞主流程

两套 agent 大脑（`agent.rs` 与 `loop_.rs`）都在主流程上同步 `.await` 等待摘要 LLM 往返，给用户每轮额外增加约 5s 延迟。只有 orchestrator 已用 `tokio::spawn` 异步。

### P5: turn_num 漂移

系统中"对话轮次"`turn_num` 存在两套语义、三种计算公式：

- **消息级轮次**：`max(已有 turn_num)+1`
- **摘要级轮次**：三个不同公式（agent.rs 自维护计数器、loop_.rs 的 assistant 消息条数、orchestrator 的 assistant 消息条数）

漂移的后果：向量库摘要排序 `ByTurnNumDesc` 取"最新摘要"取错，`transcript_window_for_turn` 回放窗口定位错位。

### P6: importance 缺失

通过 `store()` 存储的记忆条目缺少 importance 分数（传入了 `None`）。只有 consolidation 显式计算，导致约 90% 写入路径 importance 为空。召回时空 importance 默认为 0.5，破坏了按类别区分的排序。

## What Changes

### 摘要检索文本清洗（P1）

- 新增 `SummaryItem::searchable_text()`：仅拼接语义字段
- 向量库新增 `f_search_text` 字段，Dense/BM25 索引均基于该字段
- `f_content` 仍存完整 payload，下游反序列化不受影响

### 召回默认模式与顺序修正（P2）

- `memory_recall` 缺省值 backend-aware：vectordb → `SummaryWithTurns`
- `SummaryWithTurns` 召回：原始 turn 在前、摘要在后
- 原始 turn 补 `generated_at` 时间锚点
- 自动注入收窄为仅 Recap

### 摘要价值判断过滤（P3）

- LLM 前过滤系统合成内容（cron 任务、心跳、distilled 等）
- LLM 后 `has_structured_content()` 作为存储门
- 集中实现在 `summarize_turn` 内部，三路生效

### 摘要异步非阻塞生成（P4）

- `agent.rs` 与 `loop_.rs` 改为 `tokio::spawn` fire-and-forget
- `provider` 由 `Box` 改为 `Arc` 以支持跨任务共享

### turn_num 全链路统一（P5）

- 统一规则：`turn_num = max(消息池 turn_num) + 1`
- 删除 Agent 自维护计数器，改为只读派生
- 消除 assistant 计数歪算法
- 持久化写入点统一 stamp

### importance 默认计算（P6）

- `store_with_metadata()` 在 importance 为 None 时自动计算

### 端到端测试

- 新增 `tests/memory_e2e.rs`，真实 Agent loop + 真实后端

## Capabilities

### New Capabilities

- `turn-num-consistency`：全链路对话轮次号唯一计算契约
- `turn-summary-generation`：轮次摘要生成契约（价值过滤 + 异步执行）
- `summary-recall-presentation`：摘要记忆检索呈现契约
- `importance-default-computation`：记忆存储 importance 自动计算
- `memory-e2e-testing`：端到端测试覆盖

### Modified Capabilities

- `kylin-vector-memory`：collection schema 变更（新增 f_search_text），存储时计算纯净搜索文本

## Impact

- **代码**：涉及 agent.rs、loop_.rs、summary_turn.rs、vectordb.rs、memory_recall.rs、memory_loader.rs、ws.rs、orchestrator/mod.rs 等
- **API**：`Agent::turn`/`turn_streamed` 新增 `turn_num` 入参
- **数据**：向量库 collection 需重建
- **行为**：摘要更精准、召回更完整、延迟更低、turn_num 一致
