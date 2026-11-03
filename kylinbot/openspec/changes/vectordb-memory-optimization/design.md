# Design: 向量库记忆模块综合优化

## Context

本变更涉及六个相互关联但各自独立的子问题，核心围绕"摘要记忆"的写入、检索、呈现全链路。

### 三条摘要触发路径

| 路径 | 入口 | 摘要触发现状 |
|------|------|-------------|
| `loop_.rs` | 交互式 CLI、渠道消息、daemon、cron | 同步 `.await` |
| `agent.rs` | gateway WebSocket | 同步 `.await` |
| `orchestrator/mod.rs` | 多渠道编排器 | 已异步 `tokio::spawn` |

三者最终都调用同一个 `kylinbot_memory::summary_turn::summarize_turn`。

### turn_num 的三种计算源

| 来源 | 公式 | 问题 |
|------|------|------|
| orchestrator `mod.rs:2752` | `max(cache.turn_num)+1` | 权威源 |
| agent.rs `current_turn_num` | 可变计数器 `+=1` | 取消跳号、与消息池脱钩 |
| assistant 计数 | `filter(assistant).count()` | 工具循环虚高 |

### 向量库索引结构

- Dense embedding：`embed_one(content)`
- BM25：从 `f_content` 列自动建索引

Summary 类记录存的是 JSON，导致噪声进入向量与词表。

## Goals / Non-Goals

**Goals:**

- P1: Dense/BM25 索引建立在纯净语义文本上
- P2: vectordb 默认召回带原始 turn 回放，且原文在前
- P3: 无实质内容轮次不生成摘要
- P4: agent.rs/loop_.rs 摘要异步非阻塞
- P5: turn_num 全链路统一为 `max+1`
- P6: 所有存储自动获得 importance 分数
- 端到端测试覆盖核心改动

**Non-Goals:**

- 不改 recall 查询侧（用户 query 本就是纯文本）
- 不改 `Memory` trait 签名
- 不改数据库 Schema（仅向量库 collection schema）
- 不做存量数据迁移

## Decisions

### D1: 摘要检索文本清洗

**新增 `f_search_text` 列**，BM25 改读该列；`f_content` 仍存完整 payload。

```rust
// SummaryItem::searchable_text()
pub fn searchable_text(&self) -> String {
    let mut parts = vec![];
    if !self.summary.is_empty() { parts.push(&self.summary); }
    if !self.summary_text.is_empty() { parts.push(&self.summary_text); }
    // ... key_facts, temporal_facts, buffer_info, user_intents_and_slots
    parts.join("\n")
}
```

**纳入字段**：summary, summary_text, key_facts, temporal_facts, buffer_info, user_intents_and_slots
**排除字段**：learned（已另作 Core 条目入库）、元数据（UUID/时间戳）

> **设计决策**：`buffer_info` 被纳入索引的原因：
> 1. 虽为短期上下文，但可能包含与当前对话相关的关键补充信息
> 2. 完整语义信息有助于召回时更好地匹配用户查询意图
> 3. 相比于丢失可能相关的上下文，保留的收益大于噪声成本

### D2: 召回默认值与顺序

**backend-aware 默认值**在调用点解析，枚举 `#[default]` 保持 `SummaryOnly`：

```rust
let retrieval_mode = match args.get("retrieval_mode") {
    Some(v) => parse(v),
    None if self.is_vectordb() => RetrievalMode::SummaryWithTurns,
    None => RetrievalMode::SummaryOnly,
};
```

**顺序修正**：原始 turn 提到 Recap 之前。

**时间锚点**：用 `SummaryItem.generated_at` 作近似。

### D3: 摘要价值过滤

**两层过滤集中在 `summarize_turn` 内部**：

- **Layer A（LLM 前）**：`turn_has_substance()` 复用 `should_skip_autosave_content()` 过滤系统合成内容（cron 任务、心跳、distilled 等自动生成消息）
- **Layer B（LLM 后）**：`has_structured_content()` 存储门，五个结构化字段全空时跳过存储

返回 `SummaryOutcome { Stored, Skipped }`。

> **设计决策**：Layer A 未实现长度门或寒暄短语过滤的原因：
> 1. 长度门阈值难以确定，不同语言/场景的合理阈值差异大
> 2. 寒暄短语集合难以穷尽，硬编码容易遗漏或误伤
> 3. Layer B 的 `has_structured_content()` 已能有效过滤无实质内容的摘要
> 4. 让 LLM 判断内容价值比启发式规则更准确，即使多一次 LLM 调用，空壳摘要也会被 Layer B 拦截不入库

### D4: 异步摘要

**provider `Box`→`Arc`** 以支持 `tokio::spawn`：

```rust
// agent.rs 字段
pub struct Agent { provider: Arc<dyn Provider>, ... }

// spawn
tokio::spawn(async move {
    let provider = Arc::clone(&provider);
    let memory = Arc::clone(&memory);
    summarize_turn(provider.as_ref(), memory.as_ref(), ...).await;
});
```

### D5: turn_num 统一

**唯一权威 = 消息池 `max+1`**：

```rust
// Agent 新增派生方法
pub fn next_turn_num(&self) -> u32 {
    self.history.iter()
        .filter_map(|m| m.turn_num)
        .max()
        .map_or(1, |m| m + 1)
}

// 删除 current_turn_num 字段与全部 +=1
// turn()/turn_streamed() 增 turn_num 入参
```

**消除 assistant 计数**：喂摘要改用透传的 `turn_num`。

**持久化写入点补 stamp**：gateway ws.rs、orchestrator mod.rs。

### D6: importance 默认计算

```rust
// store_with_metadata() in vectordb.rs
let imp = importance.unwrap_or_else(|| {
    compute_importance(content, &category)
});
```

> **设计决策**：仅 `vectordb.rs` 应用此逻辑，`sqlite.rs` 保持原有 `unwrap_or(0.5)` 的原因：
> 1. vectordb 是主力生产后端，需要更精细的 importance 区分以优化召回排序
> 2. 保持 sqlite 行为不变可减少回归风险，避免影响现有依赖该行为的使用场景
> 3. 如后续有统一需求，可单独立项处理

## Risks / Trade-offs

- **新旧向量不可比**：直接 drop 重建 collection（eval 场景可接受）
- **Layer B 误杀叙述型摘要**：仅有 summary_text、无结构化列表的轮次被丢弃 → 与 P3"空壳"定义一致
- **fire-and-forget 丢失背压**：进程退出可能丢任务 → 摘要为派生数据，可接受
- **backend 耦合 SummaryItem**：解析失败有原文兜底
- **sqlite/vectordb importance 不一致**：两后端默认值不同，后续可按需统一

## Migration Plan

1. 合入代码
2. Drop 现有向量库 collection（或依赖自动迁移）
3. 重新跑摘要写入 / eval 数据准备
4. 回滚：还原代码 + 再次 drop 重建

## Implementation Tasks

1. P1: 新增 `f_search_text` + `searchable_text()`
2. P2: backend-aware 默认值 + 顺序修正
3. P3: 两层价值过滤 + `SummaryOutcome`
4. P4: `Box`→`Arc` + 异步 spawn
5. P5: 删除计数器 + 派生方法 + 持久化 stamp
6. P6: importance 默认计算
7. 端到端测试