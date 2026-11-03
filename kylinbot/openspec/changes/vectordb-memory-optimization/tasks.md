# Tasks: 向量库记忆模块综合优化

## P1: 摘要检索文本清洗

- [x] `memory_traits.rs`: 新增 `SummaryItem::searchable_text()`
- [x] `vectordb.rs`: collection 新增 `f_search_text` 字段
- [x] `vectordb.rs`: BM25 输入字段改为 `f_search_text`
- [x] `vectordb.rs`: store 时对 Summary 类记录计算纯净文本

## P2: 召回默认模式与顺序修正

- [x] `memory_recall.rs`: 缺省值 backend-aware 解析
- [x] `memory_recall.rs`: `render_summary_entry` 原始 turn 提前
- [x] `memory_recall.rs`: `append_turn_replay` 补 `generated_at` 锚点
- [x] `memory_loader.rs`: 自动注入收窄为仅 `summary_text`

## P3: 摘要价值判断过滤

- [x] `summary_turn.rs`: 新增 `turn_has_substance()` 启发式
- [x] `summary_turn.rs`: 接入 `has_structured_content()` 存储门
- [x] `summary_turn.rs`: 返回 `SummaryOutcome`

## P4: 摘要异步非阻塞生成

- [x] `agent.rs`: `provider` 字段 `Box`→`Arc`
- [x] `agent.rs`: `maybe_store_turn_summary` spawn 后台任务
- [x] `loop_.rs`: 局部 `provider` 绑定 `Box`→`Arc`
- [x] `loop_.rs`: 三处摘要调用 spawn

## P5: turn_num 全链路统一

- [x] `agent.rs`: 删除 `current_turn_num` 字段与全部 `+=1`
- [x] `agent.rs`: 新增 `next_turn_num()` 派生方法
- [x] `agent.rs`: `turn()`/`turn_streamed()` 增 `turn_num` 入参
- [x] `ws.rs`/`acp_server.rs`: 调用前取 `next_turn_num()` 并传入
- [x] `loop_.rs`: 删除 `assistant_turn_count` 作摘要轮次之用
- [x] `orchestrator/mod.rs`: 删除内联 assistant 计数
- [x] `ws.rs`: 持久化写入点补 stamp `turn_num`
- [x] `orchestrator/mod.rs`: assistant 侧补 stamp `turn_num`

## P6: importance 默认计算

- [x] `vectordb.rs`: `store_with_metadata` importance 为 None 时自动计算
- [x] `importance.rs`: 调整 Conversation/Summary 基础分

## 端到端测试

- [x] 新增 `tests/memory_e2e.rs`
- [x] 基于 LOCOMO 数据集验证摘要入库
- [x] 验证 turn_num 一致性
- [x] 验证 searchable_text 清洗
- [x] 验证 memory_recall 召回格式
