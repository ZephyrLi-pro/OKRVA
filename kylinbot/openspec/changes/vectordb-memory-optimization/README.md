# 向量库记忆模块综合优化

## 概述

本变更对向量库记忆模块进行综合优化，解决评测报告 `vectordb-memory-issues-and-priority.md` 中识别的 P1-P5 问题，并增加端到端测试覆盖。

## 解决的问题

| 优先级 | 问题 | 描述 |
|--------|------|------|
| P1 | 摘要检索噪声 | Summary 记录把整个 JSON 当 content 存，UUID/时间戳/字段名进入向量与词表 |
| P2 | 默认召回丢原文 | `RetrievalMode` 默认 `SummaryOnly`，丢失时间锚点与对话细节 |
| P3 | 空壳摘要污染 | 无信息量对话（寒暄、确认）仍生成摘要入库 |
| P4 | 摘要阻塞主流程 | agent.rs/loop_.rs 同步等待摘要 LLM 往返，每轮额外约 5s 延迟 |
| P5 | turn_num 漂移 | 消息级与摘要级轮次号不一致，导致排序/回放定位错位 |
| P6 | importance 缺失 | 约 90% 写入路径 importance 为空，破坏召回排序 |

## 变更内容

### 1. 摘要检索文本清洗（P1）

- 新增 `SummaryItem::searchable_text()`：拼接语义字段（summary/summary_text/key_facts/temporal_facts/buffer_info/user_intents_and_slots）
- 向量库新增 `f_search_text` 字段，Dense embedding 与 BM25 索引均基于该字段
- `f_content` 仍存完整 JSON，下游反序列化不受影响

### 2. 召回默认模式与顺序修正（P2）

- `memory_recall` 缺省值改为 backend-aware：vectordb → `SummaryWithTurns`，其余 → `SummaryOnly`
- `SummaryWithTurns` 召回结果：原始对话轮次在前、结构化摘要在后
- 原始 turn 补 `generated_at` 时间锚点

### 3. 摘要价值判断过滤（P3）

- LLM 调用前：`turn_has_substance()` 复用 `should_skip_autosave_content()` 过滤系统合成内容（cron 任务、心跳、distilled 等）
- LLM 调用后：`has_structured_content()` 作为存储门，五个结构化字段全空时不持久化
- 集中实现在 `summarize_turn` 内部，三条路径（agent.rs/loop_.rs/orchestrator）一次性生效

### 4. 摘要异步非阻塞生成（P4）

- `agent.rs` 与 `loop_.rs` 的摘要调用改为 `tokio::spawn` fire-and-forget
- `provider` 由 `Box<dyn Provider>` 改为 `Arc<dyn Provider>` 以支持跨任务共享
- 后台任务失败以 `error!` 记录，不阻塞主流程

### 5. turn_num 全链路统一（P5）

- 统一规则：`turn_num = max(消息池中已有 turn_num) + 1`
- 删除 `Agent.current_turn_num` 可变计数器，新增 `next_turn_num()` 只读派生方法
- `turn()`/`turn_streamed()` 增加 `turn_num` 入参，由调用方传入
- 消除"assistant 计数"歪算法
- 持久化写入点统一 stamp（gateway/orchestrator）

### 6. importance 默认计算（P6）

- `vectordb.rs` 的 `store_with_metadata()` 在 importance 为 None 时自动计算默认值
- `sqlite.rs` 保持原有 `unwrap_or(0.5)` 行为（两后端不一致，后续可按需统一）

### 7. 端到端测试覆盖

- 新增 ignored live integration test `tests/memory_e2e.rs`
- 以 `Agent::turn()` 为入口，真实 LLM + 真实 vectordb/kart 后端
- 基于 LOCOMO 数据集验证摘要入库、turn_num、searchable_text 清洗、召回格式

## 新增能力

- `turn-num-consistency`：全链路对话轮次号唯一计算契约
- `turn-summary-generation`：轮次摘要生成契约（价值过滤 + 异步执行）
- `summary-recall-presentation`：摘要记忆检索呈现契约
- `importance-default-computation`：记忆存储 importance 自动计算
- `memory-e2e-testing`：端到端测试覆盖

## 修改的能力

- `kylin-vector-memory`：collection schema 变更（新增 f_search_text），存储时计算纯净搜索文本

## 影响范围

### 代码改动

| 文件 | 改动说明 |
|------|----------|
| `crates/kylinbot-api/src/memory_traits.rs` | 新增 `searchable_text()`，`SummaryOutcome` |
| `crates/kylinbot-memory/src/summary_turn.rs` | 两层价值过滤，返回 `SummaryOutcome` |
| `crates/kylinbot-memory/src/vectordb.rs` | `f_search_text` 字段，importance 默认计算 |
| `crates/kylinbot-memory/src/importance.rs` | 基础分调整 |
| `crates/kylinbot-runtime/src/agent/agent.rs` | 删除计数器，`Box`→`Arc`，异步摘要 |
| `crates/kylinbot-runtime/src/agent/loop_.rs` | 异步摘要，消除 assistant 计数 |
| `crates/kylinbot-runtime/src/agent/memory_loader.rs` | 自动注入收窄为仅 Recap |
| `crates/kylinbot-tools/src/memory_recall.rs` | backend-aware 默认值，顺序修正 |
| `crates/kylinbot-gateway/src/ws.rs` | 调用方传入 turn_num，持久化 stamp |
| `crates/kylinbot-channels/src/orchestrator/mod.rs` | turn_num 统一，持久化 stamp |
| `tests/memory_e2e.rs` | 新增端到端测试 |

### API 变更

- `Agent::turn` / `Agent::turn_streamed` 新增 `turn_num: u32` 入参
- `summarize_turn` 返回类型改为 `anyhow::Result<SummaryOutcome>`

### 数据迁移

- 向量库 collection 需重建（drop 后首次 store 自动重建）

## 验证

- 现有单测与集成测试通过
- 新增端到端测试 `tests/memory_e2e.rs` 验证核心流程

## 设计决策说明

以下为关键实现决策的说明：

| 决策点 | 实现方式 | 原因 |
|--------|---------|------|
| `searchable_text()` 包含 `buffer_info` | 保留短期上下文 | 完整语义信息有助于召回匹配，保留收益大于噪声成本 |
| Layer A 仅过滤系统合成内容 | 不加长度门/寒暄过滤 | 阈值难确定、短语难穷尽；Layer B 已能有效拦截空壳摘要 |
| `sqlite.rs` 保持原 importance 逻辑 | 仅 vectordb 应用新逻辑 | vectordb 是主力生产后端；sqlite 保持原行为减少回归风险 |
