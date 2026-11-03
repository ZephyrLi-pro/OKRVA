//! Live end-to-end memory test through the real Agent turn loop.
//!
//! Run explicitly with:
//! `cargo test --test memory_e2e --features vectordb,kart-embedding -- --ignored --nocapture`

#![cfg(all(feature = "vectordb", feature = "kart-embedding"))]

use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::fs::OpenOptions;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant};

use anyhow::{Context, Result, anyhow, bail};
use kylinbot::Config;
use kylinbot::agent::agent::{Agent, TurnEvent};
use kylinbot::memory::{self, Memory, MemoryCategory, SummaryItem};
use kylinbot::providers::ChatMessage;
use kylinbot::tools::{MemoryRecallTool, Tool};
use kylinbot_infra::session_store::SessionStore;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use tokio::time::sleep;

const DEFAULT_DATASET: &str =
    "/data/usershare/kylinbot/dataset/Locomo/locomo10_zh_top5_no_category5.json";
const DEFAULT_RESULT_PATH: &str = "/data/usershare/kylinbot/eval_results/result.jsonl";
const DEFAULT_WORKSPACE_ROOT: &str = "/data/usershare/kylinbot/eval_results/workspaces";
const STATE_TRANSCRIPT_MODE: &str = "locomo_source_per_turn_v2";

#[derive(Debug, Clone)]
struct RecordedToolCall {
    tool: String,
    arguments: Value,
    output: String,
    success: bool,
    error: Option<String>,
    duration_ms: f64,
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
#[ignore = "live provider, local vectordb/kart SDK, and LOCOMO dataset required"]
async fn memory_module_live_e2e() -> Result<()> {
    if !kart_vector_database_sys::is_available() {
        eprintln!("skip: kart_vector_database_sys not available");
        return Ok(());
    }
    if !kart_embedding_sys::is_available() {
        eprintln!("skip: kart_embedding_sys not available");
        return Ok(());
    }

    let settings = TestSettings::from_env();
    if !settings.dataset.exists() {
        eprintln!("skip: dataset not found: {}", settings.dataset.display());
        return Ok(());
    }

    let mut config = match Config::load_or_init().await {
        Ok(config) => config,
        Err(err) => {
            eprintln!("skip: config unavailable: {err:#}");
            return Ok(());
        }
    };
    config.memory.backend = "vectordb".into();
    config.memory.auto_save = true;

    let provider_name = match config.providers.fallback.as_deref() {
        Some(name) if !name.trim().is_empty() => name.to_string(),
        _ => {
            eprintln!("skip: providers.fallback is not configured");
            return Ok(());
        }
    };
    let fallback_provider = match config.providers.fallback_provider() {
        Some(provider) => provider,
        None => {
            eprintln!("skip: fallback provider '{provider_name}' has no provider profile");
            return Ok(());
        }
    };
    let model_name = match config.providers.resolve_default_model() {
        Some(model) => model,
        None => {
            eprintln!("skip: no model configured for fallback provider");
            return Ok(());
        }
    };
    let samples = load_dataset(&settings.dataset)?;
    if samples.is_empty() {
        eprintln!("skip: dataset has no samples");
        return Ok(());
    }
    prepare_result_file(&settings.result_path, settings.resume)?;
    std::fs::create_dir_all(&settings.workspace_root).with_context(|| {
        format!(
            "create workspace root {}",
            settings.workspace_root.display()
        )
    })?;
    let planned_qa = planned_qa_count(&samples, settings.samples, settings.qa_limit);
    let completed_qa = if settings.resume {
        load_completed_qa(&settings.result_path)?
    } else {
        CompletedQa::default()
    };
    if !settings.resume || !result_has_metadata(&settings.result_path)? {
        append_report(
            &settings.result_path,
            report_metadata_record(
                &settings,
                &model_name,
                planned_qa,
                total_dataset_qa_count(&samples),
            ),
        )?;
    }

    let sample_limit = settings.samples.unwrap_or(samples.len()).min(samples.len());
    eprintln!(
        "memory_e2e start: samples={sample_limit}/{}, planned_qa={planned_qa}, completed_qa={}, resume={}, dataset={}, result={}, workspace_root={}, provider={}, model={}",
        samples.len(),
        completed_qa.keys.len(),
        settings.resume,
        settings.dataset.display(),
        settings.result_path.display(),
        settings.workspace_root.display(),
        provider_name,
        model_name
    );

    let mut total_qa = 0usize;
    let mut nonempty_answers = 0usize;
    let mut exact_answer_hits = 0usize;
    let mut result_index = completed_qa.next_index;

    for (sample_index, sample) in samples.iter().take(sample_limit).enumerate() {
        let workspace = settings.workspace_root.join(&sample.sample_id);
        std::fs::create_dir_all(&workspace)
            .with_context(|| format!("create sample workspace {}", workspace.display()))?;
        let mut sample_config = config.clone();
        sample_config.workspace_dir = workspace.clone();
        sample_config.memory.backend = "vectordb".into();
        sample_config.memory.auto_save = true;
        let mem_poll = make_vectordb_memory(
            &sample_config,
            &workspace,
            fallback_provider.api_key.as_deref(),
        )?;
        let recall_tool =
            MemoryRecallTool::with_workspace(Arc::clone(&mem_poll), workspace.clone());

        let mut agent = Agent::from_config(&sample_config)
            .await
            .context("build production memory e2e agent from config")?;
        agent.set_memory_session_id(Some(sample.sample_id.clone()));

        let session_store = SessionStore::new(&workspace).context("create session store")?;
        let state_path = workspace.join("memory_e2e_state.json");
        let mut state = if settings.resume {
            load_compatible_sample_state(&state_path, &sample.sample_id)?
        } else {
            SampleState::new(&sample.sample_id)
        };
        let mut issued_turns = state.issued_turns.clone();
        let mut summary_wait_timeouts = 0usize;
        let session_numbers = sample.conversation.session_numbers();
        let qa_count = settings
            .qa_limit
            .unwrap_or(sample.qa.len())
            .min(sample.qa.len());
        eprintln!(
            "sample {}/{} {} start: sessions={}, qa={}, workspace={}",
            sample_index + 1,
            sample_limit,
            sample.sample_id,
            session_numbers.len(),
            qa_count,
            workspace.display()
        );

        if settings.resume && state.ingestion_complete {
            eprintln!(
                "sample {}/{} {} resume: ingestion already complete, completed_turns={}, issued_turns={}",
                sample_index + 1,
                sample_limit,
                sample.sample_id,
                state.completed_turns.len(),
                state.issued_turns.len()
            );
        }

        // Count total chunks (10 messages per chunk = 1 agent turn)
        const CHUNK_SIZE: usize = 10;
        let total_chunks: usize = session_numbers
            .iter()
            .map(|session_num| {
                sample.conversation.session_turns(*session_num)
                    .map(|turns| (turns.len() + CHUNK_SIZE - 1) / CHUNK_SIZE)
                    .unwrap_or(0)
            })
            .sum();
        let mut global_chunk_index = 0usize;

        for (_session_index, session_num) in session_numbers.iter().copied().enumerate() {
            let messages = sample.conversation.session_turns(session_num)?;
            for (chunk_index, chunk) in messages.chunks(CHUNK_SIZE).enumerate() {
                global_chunk_index += 1;
                let turn_key = (session_num, chunk_index);
                if settings.resume && state.completed_turns.contains(&turn_key) {
                    eprintln!(
                        "sample {}/{} {} ingest chunk {}/{} (session_{session_num}, chunk_{chunk_index}) skipped by state",
                        sample_index + 1,
                        sample_limit,
                        sample.sample_id,
                        global_chunk_index,
                        total_chunks
                    );
                    continue;
                }

                let before = summary_count(mem_poll.as_ref(), &sample.sample_id).await?;
                let message = sample.render_chunk_ingestion(session_num, chunk)?;
                let transcript = sample.render_chunk_transcript(session_num, chunk)?;
                let turn_num = next_eval_turn_num(&agent, &issued_turns);
                assert_monotonic(&issued_turns, turn_num)?;
                eprintln!(
                    "sample {}/{} {} ingest chunk {}/{} (session_{session_num}, chunk_{chunk_index}, agent_turn={turn_num})...",
                    sample_index + 1,
                    sample_limit,
                    sample.sample_id,
                    global_chunk_index,
                    total_chunks
                );

                let ingest_start = Instant::now();
                let answer = agent
                    .turn(&message, turn_num)
                    .await
                    .with_context(|| format!("ingest {} session {session_num} chunk {chunk_index}", sample.sample_id))?;
                let ingest_time_ms = millis_f64(ingest_start.elapsed());
                issued_turns.push(turn_num);
                append_source_transcript(&session_store, &sample.sample_id, turn_num, &transcript)?;

                let summary_start = Instant::now();
                let summary_stored = wait_for_summary(
                    mem_poll.as_ref(),
                    &sample.sample_id,
                    before,
                    settings.summary_timeout,
                )
                .await?;
                let after = summary_count(mem_poll.as_ref(), &sample.sample_id).await?;
                let summary_wait_ms = millis_f64(summary_start.elapsed());
                if !summary_stored {
                    summary_wait_timeouts += 1;
                }
                state.mark_turn_complete(session_num, chunk_index, turn_num, after);
                save_sample_state(&state_path, &state)?;
                eprintln!(
                    "sample {}/{} {} ingest chunk {}/{} done: answer_chars={}, ingest_ms={:.0}, summary_stored={}, summaries={} -> {}, summary_wait_ms={:.0}",
                    sample_index + 1,
                    sample_limit,
                    sample.sample_id,
                    global_chunk_index,
                    total_chunks,
                    answer.chars().count(),
                    ingest_time_ms,
                    summary_stored,
                    before,
                    after,
                    summary_wait_ms
                );
            }
        }

        let final_summary_start = Instant::now();
        let summaries_ready = wait_for_summary_turns(
            mem_poll.as_ref(),
            &sample.sample_id,
            &issued_turns,
            settings.final_summary_timeout,
        )
        .await?;
        let final_summary_wait_ms = millis_f64(final_summary_start.elapsed());
        let summaries = mem_poll
            .list(Some(&MemoryCategory::Summary), Some(&sample.sample_id))
            .await
            .context("list stored summaries")?;
        let missing_summary_turns = missing_summary_turns(&summaries, &issued_turns)?;
        if !summaries_ready || !missing_summary_turns.is_empty() {
            bail!(
                "sample {} has incomplete summaries before QA: summaries={}, missing_turns={:?}, per_turn_wait_timeouts={}, final_wait_ms={:.0}. Increase MEMORY_E2E_FINAL_SUMMARY_TIMEOUT_MS or fix summary generation before running QA.",
                sample.sample_id,
                summaries.len(),
                missing_summary_turns,
                summary_wait_timeouts,
                final_summary_wait_ms
            );
        }
        state.ingestion_complete = true;
        state.summary_count = summaries.len();
        state.updated_at = chrono::Utc::now().to_rfc3339();
        save_sample_state(&state_path, &state)?;
        assert_summary_invariants(&summaries, &issued_turns)?;
        assert_recall_turns_before_recap(&recall_tool, sample, &summaries).await?;

        let qa_iter = sample.qa.iter().take(qa_count);
        for (qa_index, qa) in qa_iter.enumerate() {
            let qa_key = qa_result_key(&sample.sample_id, &qa.question);
            if settings.resume && completed_qa.keys.contains(&qa_key) {
                eprintln!(
                    "sample {}/{} {} qa {}/{} skipped by result: {}",
                    sample_index + 1,
                    sample_limit,
                    sample.sample_id,
                    qa_index + 1,
                    qa_count,
                    qa.question
                );
                continue;
            }
            agent.clear_history();
            let turn_num = next_eval_turn_num(&agent, &issued_turns);
            assert_monotonic(&issued_turns, turn_num)?;
            eprintln!(
                "sample {}/{} {} qa {}/{} (global_index={}, turn={}): {}",
                sample_index + 1,
                sample_limit,
                sample.sample_id,
                qa_index + 1,
                qa_count,
                result_index,
                turn_num,
                qa.question
            );
            let qa_start = Instant::now();
            let qa_prompt = render_retrieval_required_qa_prompt(&qa.question);
            let (answer, tool_records) =
                run_agent_turn_with_trace(&mut agent, &qa_prompt, turn_num)
                    .await
                    .with_context(|| format!("QA turn for {}", sample.sample_id))?;
            let qa_time_ms = millis_f64(qa_start.elapsed());
            let qa_context = format_recorded_tool_context(&tool_records);
            let retrieved_keys = extract_retrieved_keys(&qa_context);
            let retrieval_time_ms = tool_records
                .iter()
                .filter(|record| record.tool == "memory_recall")
                .map(|record| record.duration_ms)
                .sum();
            let recall_success = tool_records
                .iter()
                .any(|record| record.tool == "memory_recall" && record.success);
            let recall_error = tool_records
                .iter()
                .find(|record| record.tool == "memory_recall" && record.error.is_some())
                .and_then(|record| record.error.as_deref());
            let qa_tool_invocations = format_tool_invocations(&tool_records);
            issued_turns.push(turn_num);
            state.mark_qa_turn(turn_num);
            save_sample_state(&state_path, &state)?;

            total_qa += 1;
            let answer_nonempty = !answer.trim().is_empty();
            if answer_nonempty {
                nonempty_answers += 1;
            }
            let expected = qa.answer_as_string();
            let exact_answer_hit = !expected.is_empty() && answer.contains(&expected);
            if exact_answer_hit {
                exact_answer_hits += 1;
            }
            append_report(
                &settings.result_path,
                per_query_record(PerQueryRecordInput {
                    index: result_index,
                    sample_id: &sample.sample_id,
                    question: &qa.question,
                    expected_answer: &expected,
                    generated_answer: &answer,
                    evidence: &qa.evidence,
                    category: &qa.category,
                    retrieved_keys,
                    qa_context,
                    qa_tool_invocations,
                    recall_success,
                    recall_error,
                    exact_answer_hit,
                    retrieval_time_ms,
                    qa_time_ms,
                    turn_num,
                }),
            )?;
            result_index += 1;
            eprintln!(
                "sample {}/{} {} qa {}/{} done: answer_chars={}, exact_answer_hit={}, retrieval_ms={:.0}, qa_ms={:.0}, report={}",
                sample_index + 1,
                sample_limit,
                sample.sample_id,
                qa_index + 1,
                qa_count,
                answer.chars().count(),
                exact_answer_hit,
                retrieval_time_ms,
                qa_time_ms,
                settings.result_path.display()
            );
        }

        eprintln!(
            "sample {} complete: summaries={}, summary_wait_timeouts={}, report={}",
            sample.sample_id,
            summaries.len(),
            summary_wait_timeouts,
            settings.result_path.display()
        );
    }

    if total_qa > 0 {
        assert!(
            nonempty_answers > 0,
            "expected at least one non-empty QA answer from live provider"
        );
    }
    eprintln!(
        "memory_e2e complete: qa={total_qa}, nonempty_answers={nonempty_answers}, exact_answer_hits={exact_answer_hits}"
    );
    eprintln!("result_jsonl={}", settings.result_path.display());
    eprintln!("workspace_root={}", settings.workspace_root.display());

    Ok(())
}

fn make_vectordb_memory(
    config: &Config,
    workspace: &Path,
    api_key: Option<&str>,
) -> Result<Arc<dyn Memory>> {
    let mem = memory::create_memory_with_storage_and_routes(
        &config.memory,
        &config.providers.embedding_routes,
        Some(&config.storage.provider.config),
        workspace,
        api_key,
    )?;
    if mem.name() != "vectordb" {
        bail!(
            "configured memory factory returned '{}' instead of vectordb",
            mem.name()
        );
    }
    Ok(Arc::from(mem))
}

fn next_eval_turn_num(agent: &Agent, issued_turns: &[u32]) -> u32 {
    let next_from_state = issued_turns.last().map_or(1, |turn| turn + 1);
    agent.next_turn_num().max(next_from_state)
}

async fn summary_count(memory: &dyn Memory, session_id: &str) -> Result<usize> {
    Ok(memory
        .list(Some(&MemoryCategory::Summary), Some(session_id))
        .await?
        .len())
}

async fn wait_for_summary(
    memory: &dyn Memory,
    session_id: &str,
    previous_count: usize,
    timeout: Duration,
) -> Result<bool> {
    let start = Instant::now();
    loop {
        if summary_count(memory, session_id).await? > previous_count {
            return Ok(true);
        }
        if start.elapsed() >= timeout {
            return Ok(false);
        }
        sleep(Duration::from_millis(100)).await;
    }
}

async fn wait_for_summary_turns(
    memory: &dyn Memory,
    session_id: &str,
    issued_turns: &[u32],
    timeout: Duration,
) -> Result<bool> {
    let start = Instant::now();
    loop {
        let summaries = memory
            .list(Some(&MemoryCategory::Summary), Some(session_id))
            .await?;
        if missing_summary_turns(&summaries, issued_turns)?.is_empty() {
            return Ok(true);
        }
        if start.elapsed() >= timeout {
            return Ok(false);
        }
        sleep(Duration::from_millis(250)).await;
    }
}

fn missing_summary_turns(
    entries: &[memory::MemoryEntry],
    issued_turns: &[u32],
) -> Result<Vec<u32>> {
    let summary_turns = entries
        .iter()
        .map(|entry| {
            let summary: SummaryItem = serde_json::from_str(&entry.content)
                .with_context(|| format!("parse summary entry {}", entry.key))?;
            Ok(summary.turn_num.and_then(|turn| u32::try_from(turn).ok()))
        })
        .collect::<Result<Vec<_>>>()?
        .into_iter()
        .flatten()
        .collect::<BTreeSet<_>>();
    Ok(issued_turns
        .iter()
        .copied()
        .filter(|turn| !summary_turns.contains(turn))
        .collect())
}

fn append_source_transcript(
    store: &SessionStore,
    session_id: &str,
    turn_num: u32,
    transcript: &str,
) -> Result<()> {
    store.append(
        session_id,
        &ChatMessage::user(transcript.to_string()).with_turn_num(turn_num),
    )?;
    Ok(())
}

fn render_retrieval_required_qa_prompt(question: &str) -> String {
    format!(
        "本轮必须先调用 memory_recall 检索相关记忆，再基于检索结果回答；如果第一次检索结果不足以回答问题，请用问题中的人物、事件、物品、日期线索或同义词再检索一次。\n\
         只依据检索结果中的原始对话、会话时间锚点、图片说明/caption 和摘要作答，不要凭常识或相近主题补充答案。\n\
         最终答案请尽量简短，优先直接给出答案，不要展开解释。\n\
         如果答案涉及时间，优先保留检索结果中的原文相对时间和会话时间锚点；只有在换算无歧义时才给出绝对日期或年份。不要直接用当前系统日期换算。\n\
         对“上周五/上周末/上周/最近/近期”等可能有歧义的表达，不要强行唯一化；可回答为“会话日期之前的上周五/上周末/那一周”。\n\
         如果问题要求列举多个对象、地点、活动、宠物、书籍、乐器或作品，请合并检索结果中的所有相关证据，避免只回答其中一个，也不要加入未被证据支持的项目。\n\
         如果检索结果只支持部分答案，请只回答有证据支持的部分；如果检索后仍没有相关信息，再回答：没有相关信息。\n\n\
         问题：{question}"
    )
}

async fn run_agent_turn_with_trace(
    agent: &mut Agent,
    message: &str,
    turn_num: u32,
) -> Result<(String, Vec<RecordedToolCall>)> {
    let (event_tx, mut event_rx) = tokio::sync::mpsc::channel(128);
    let collector = tokio::spawn(async move {
        let mut calls: HashMap<String, (String, Value)> = HashMap::new();
        let mut records = Vec::new();
        while let Some(event) = event_rx.recv().await {
            match event {
                TurnEvent::ToolCall { id, name, args } => {
                    calls.insert(id, (name, args));
                }
                TurnEvent::ToolResult { id, name, output } => {
                    let (tool, arguments) = calls
                        .remove(&id)
                        .unwrap_or_else(|| (name.clone(), Value::Null));
                    let success = !output.trim_start().starts_with("Error:");
                    let error = (!success).then(|| output.clone());
                    records.push(RecordedToolCall {
                        tool,
                        arguments,
                        output,
                        success,
                        error,
                        duration_ms: 0.0,
                    });
                }
                TurnEvent::Chunk { .. } | TurnEvent::Thinking { .. } => {}
            }
        }
        records
    });

    let answer = agent
        .turn_streamed(message, event_tx, None, turn_num)
        .await?;
    let tool_records = collector.await.context("collect QA turn events")?;
    Ok((answer, tool_records))
}

fn format_recorded_tool_context(records: &[RecordedToolCall]) -> String {
    let mut context = String::new();
    for record in records
        .iter()
        .filter(|record| record.tool == "memory_recall")
    {
        if !context.is_empty() {
            context.push_str("\n\n");
        }
        context.push_str("[memory_recall call]\n");
        context.push_str("arguments: ");
        context.push_str(&record.arguments.to_string());
        context.push('\n');
        if record.success {
            context.push_str("[memory_recall results]\n");
            context.push_str(&record.output);
        } else {
            context.push_str("[memory_recall error]\n");
            context.push_str(record.error.as_deref().unwrap_or("unknown error"));
        }
    }
    context
}

fn format_tool_invocations(records: &[RecordedToolCall]) -> Vec<String> {
    records
        .iter()
        .map(|record| format!("{}({})", record.tool, record.arguments))
        .collect()
}

fn assert_monotonic(issued_turns: &[u32], next: u32) -> Result<()> {
    if let Some(prev) = issued_turns.last() {
        assert!(
            next > *prev,
            "turn_num not monotonic: prev={prev}, next={next}"
        );
    }
    Ok(())
}

fn assert_summary_invariants(entries: &[memory::MemoryEntry], issued_turns: &[u32]) -> Result<()> {
    let issued: BTreeSet<i32> = issued_turns
        .iter()
        .map(|turn| i32::try_from(*turn).expect("turn number fits in i32"))
        .collect();

    let mut parsed = 0usize;
    for entry in entries {
        let summary: SummaryItem = serde_json::from_str(&entry.content)
            .with_context(|| format!("parse summary entry {}", entry.key))?;
        parsed += 1;
        if let Some(turn_num) = summary.turn_num {
            assert!(
                issued.contains(&turn_num),
                "summary turn_num {turn_num} was not issued by the test: {:?}",
                issued
            );
        }

        let searchable = summary.searchable_text();
        for forbidden in [
            "memory_id",
            "session_id",
            "transcript_session_key",
            "generated_at",
            "turn_num",
            "\"summary\"",
            "\"summary_text\"",
        ] {
            assert!(
                !searchable.contains(forbidden),
                "searchable_text leaked metadata/JSON token {forbidden}: {searchable}"
            );
        }
        if !summary.summary.trim().is_empty()
            || !summary.summary_text.trim().is_empty()
            || !summary.key_facts.is_empty()
            || !summary.temporal_facts.is_empty()
            || !summary.user_intents_and_slots.is_empty()
        {
            assert!(
                !searchable.trim().is_empty(),
                "semantic summary fields existed but searchable_text was empty"
            );
        }
    }

    assert!(parsed > 0, "no SummaryItem entries parsed");
    Ok(())
}

async fn assert_recall_turns_before_recap(
    recall_tool: &MemoryRecallTool,
    sample: &LocomoSample,
    summaries: &[memory::MemoryEntry],
) -> Result<()> {
    let query = summaries
        .iter()
        .filter_map(|entry| serde_json::from_str::<SummaryItem>(&entry.content).ok())
        .find_map(|summary| {
            let text = summary.searchable_text();
            (!text.trim().is_empty()).then_some(text)
        })
        .or_else(|| sample.qa.first().map(|qa| qa.question.clone()))
        .ok_or_else(|| anyhow!("no query text available for memory_recall assertion"))?;

    let result = recall_tool
        .execute(json!({
            "query": query,
            "limit": 5,
            "session_id": sample.sample_id,
        }))
        .await
        .context("execute memory_recall")?;

    assert!(
        result.success,
        "memory_recall failed: {:?}; output={}",
        result.error, result.output
    );

    let Some(turns_pos) = result.output.find("  Turns:") else {
        eprintln!(
            "skip recall order assertion for {}: no replayable Turns block in output",
            sample.sample_id
        );
        return Ok(());
    };
    let Some(recap_pos) = result.output.find("  Recap:") else {
        eprintln!(
            "skip recall order assertion for {}: no Recap block in output",
            sample.sample_id
        );
        return Ok(());
    };
    assert!(
        turns_pos < recap_pos,
        "expected Turns block before Recap block; output:\n{}",
        result.output
    );
    Ok(())
}

fn append_report(path: &Path, record: Value) -> Result<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)
            .with_context(|| format!("create report dir {}", parent.display()))?;
    }
    let mut file = OpenOptions::new().create(true).append(true).open(path)?;
    serde_json::to_writer(&mut file, &record)?;
    writeln!(file)?;
    Ok(())
}

fn report_metadata_record(
    settings: &TestSettings,
    model_name: &str,
    planned_qa: usize,
    total_dataset_qa: usize,
) -> Value {
    let now = chrono::Utc::now().to_rfc3339();
    json!({
        "record_type": "report_metadata",
        "metadata": {
            "backend": "vectordb",
            "dataset_path": settings.dataset,
            "workspace_dir": settings.workspace_root,
            "output_dir": settings.result_path,
            "qa_count": planned_qa,
            "k": 5,
            "limit": settings.qa_limit,
            "timestamp": now,
            "model": model_name,
        },
        "progress": {
            "status": "running",
            "phase": "agent_e2e",
            "processed_queries": 0,
            "total_queries": total_dataset_qa,
            "last_completed_question": null,
            "updated_at": now,
        },
        "import_stats": {
            "total_turns": 0,
            "total_sessions": 0,
            "import_time_secs": 0.0,
            "total_tokens": 0,
            "summary_turn_tokens": null,
        },
        "summary_stats": null,
        "retrieval_metrics": {
            "recall_at_k": 0.0,
            "precision_at_k": 0.0,
            "mrr": 0.0,
            "hit_rate": 0.0,
            "avg_retrieval_time_ms": 0.0,
            "matching_mode": "agent_e2e_record_only",
            "similarity_threshold": 0.75,
            "kart_embedding_enabled": true,
        },
        "qa_evidence_path_metrics": {
            "memory_recall_trigger_rate": 0.0,
            "source_turn_expansion_rate": 0.0,
            "avg_tool_calls_per_query": 0.0,
        },
        "qa_metrics": {
            "enabled": false,
            "evaluation_mode": "record_only",
            "total_input_tokens": 0,
            "total_output_tokens": 0,
        },
        "error_cases": [],
    })
}

struct PerQueryRecordInput<'a> {
    index: usize,
    sample_id: &'a str,
    question: &'a str,
    expected_answer: &'a str,
    generated_answer: &'a str,
    evidence: &'a [String],
    category: &'a Value,
    retrieved_keys: Vec<String>,
    qa_context: String,
    qa_tool_invocations: Vec<String>,
    recall_success: bool,
    recall_error: Option<&'a str>,
    exact_answer_hit: bool,
    retrieval_time_ms: f64,
    qa_time_ms: f64,
    turn_num: u32,
}

fn per_query_record(input: PerQueryRecordInput<'_>) -> Value {
    json!({
        "record_type": "per_query_result",
        "index": input.index,
        "result": {
            "sample_id": input.sample_id,
            "question": input.question,
            "expected_answer": input.expected_answer,
            "generated_answer": input.generated_answer,
            "evidence": input.evidence,
            "retrieved_keys": input.retrieved_keys,
            "qa_context": input.qa_context,
            "qa_tool_invocations": input.qa_tool_invocations,
            "qa_evidence_path": {
                "memory_loader_context": "",
                "memory_loader_fact_only": true,
                "tool_invocations": input.qa_tool_invocations,
                "expanded_source_turns": extract_source_turn_lines(&input.qa_context),
                "final_context": input.qa_context,
            },
            "recall": if input.exact_answer_hit { 1.0 } else { 0.0 },
            "precision": 0.0,
            "mrr": 0.0,
            "hit": input.exact_answer_hit,
            "exact_match": null,
            "token_f1": null,
            "semantic_similarity": null,
            "retrieval_time_ms": input.retrieval_time_ms,
            "qa_time_ms": input.qa_time_ms,
            "category": input.category,
            "turn_num": input.turn_num,
        }
    })
}

fn extract_retrieved_keys(output: &str) -> Vec<String> {
    output
        .lines()
        .filter_map(|line| {
            let trimmed = line.trim_start();
            let (_, after_dot) = trimmed.split_once(". [")?;
            let (key, _) = after_dot.split_once(']')?;
            Some(key.to_string())
        })
        .collect()
}

fn extract_source_turn_lines(context: &str) -> Vec<String> {
    context
        .lines()
        .filter(|line| line.contains("User:") || line.contains("Assistant:"))
        .map(ToString::to_string)
        .collect()
}

fn millis_f64(duration: Duration) -> f64 {
    duration.as_secs_f64() * 1000.0
}

fn prepare_result_file(path: &Path, resume: bool) -> Result<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)
            .with_context(|| format!("create report dir {}", parent.display()))?;
    }
    let mut options = OpenOptions::new();
    options.create(true).write(true);
    if resume {
        options.append(true);
    } else {
        options.truncate(true);
    }
    options
        .open(path)
        .with_context(|| format!("prepare result file {}", path.display()))?;
    Ok(())
}

#[derive(Default)]
struct CompletedQa {
    keys: HashSet<String>,
    next_index: usize,
}

fn load_completed_qa(path: &Path) -> Result<CompletedQa> {
    if !path.exists() {
        return Ok(CompletedQa::default());
    }

    let content = std::fs::read_to_string(path)
        .with_context(|| format!("read existing result file {}", path.display()))?;
    let mut completed = CompletedQa::default();
    for line in content
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty())
    {
        let record: Value = match serde_json::from_str(line) {
            Ok(record) => record,
            Err(err) => {
                eprintln!("resume: ignoring invalid result JSONL line: {err}");
                continue;
            }
        };
        if record.get("record_type").and_then(Value::as_str) != Some("per_query_result") {
            continue;
        }
        if let Some(index) = record.get("index").and_then(Value::as_u64) {
            completed.next_index = completed.next_index.max(index as usize + 1);
        }
        let Some(result) = record.get("result") else {
            continue;
        };
        let Some(sample_id) = result.get("sample_id").and_then(Value::as_str) else {
            continue;
        };
        let Some(question) = result.get("question").and_then(Value::as_str) else {
            continue;
        };
        completed.keys.insert(qa_result_key(sample_id, question));
    }
    Ok(completed)
}

fn result_has_metadata(path: &Path) -> Result<bool> {
    if !path.exists() {
        return Ok(false);
    }
    let content = std::fs::read_to_string(path)
        .with_context(|| format!("read result file {}", path.display()))?;
    Ok(content.lines().any(|line| {
        serde_json::from_str::<Value>(line)
            .ok()
            .and_then(|value| {
                value
                    .get("record_type")
                    .and_then(Value::as_str)
                    .map(|record_type| record_type == "report_metadata")
            })
            .unwrap_or(false)
    }))
}

fn qa_result_key(sample_id: &str, question: &str) -> String {
    format!("{sample_id}\n{}", question.trim())
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct SampleState {
    #[serde(default = "state_transcript_mode")]
    transcript_mode: String,
    sample_id: String,
    ingestion_complete: bool,
    /// (session_num, turn_index_within_session) for each completed turn
    completed_turns: Vec<(u32, usize)>,
    issued_turns: Vec<u32>,
    summary_count: usize,
    updated_at: String,
}

impl SampleState {
    fn new(sample_id: &str) -> Self {
        Self {
            transcript_mode: STATE_TRANSCRIPT_MODE.to_string(),
            sample_id: sample_id.to_string(),
            ingestion_complete: false,
            completed_turns: Vec::new(),
            issued_turns: Vec::new(),
            summary_count: 0,
            updated_at: chrono::Utc::now().to_rfc3339(),
        }
    }

    fn mark_turn_complete(&mut self, session_num: u32, turn_index: usize, turn_num: u32, summary_count: usize) {
        let key = (session_num, turn_index);
        if !self.completed_turns.contains(&key) {
            self.completed_turns.push(key);
            self.completed_turns.sort();
        }
        self.mark_qa_turn(turn_num);
        self.summary_count = summary_count;
        self.updated_at = chrono::Utc::now().to_rfc3339();
    }

    fn mark_qa_turn(&mut self, turn_num: u32) {
        if !self.issued_turns.contains(&turn_num) {
            self.issued_turns.push(turn_num);
            self.issued_turns.sort_unstable();
        }
        self.updated_at = chrono::Utc::now().to_rfc3339();
    }
}

fn state_transcript_mode() -> String {
    String::new()
}

fn load_compatible_sample_state(path: &Path, sample_id: &str) -> Result<SampleState> {
    let Some(state) = load_sample_state(path)? else {
        return Ok(SampleState::new(sample_id));
    };
    if state.transcript_mode == STATE_TRANSCRIPT_MODE {
        return Ok(state);
    }
    eprintln!(
        "resume: ignoring incompatible sample state at {} (transcript_mode='{}', expected='{}'); sample will be re-ingested",
        path.display(),
        state.transcript_mode,
        STATE_TRANSCRIPT_MODE
    );
    Ok(SampleState::new(sample_id))
}

fn load_sample_state(path: &Path) -> Result<Option<SampleState>> {
    if !path.exists() {
        return Ok(None);
    }
    let bytes = std::fs::read(path).with_context(|| format!("read {}", path.display()))?;
    serde_json::from_slice(&bytes)
        .map(Some)
        .with_context(|| format!("parse {}", path.display()))
}

fn save_sample_state(path: &Path, state: &SampleState) -> Result<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)
            .with_context(|| format!("create state dir {}", parent.display()))?;
    }
    let json = serde_json::to_vec_pretty(state)?;
    std::fs::write(path, json).with_context(|| format!("write {}", path.display()))
}

fn load_dataset(path: &Path) -> Result<Vec<LocomoSample>> {
    let bytes = std::fs::read(path).with_context(|| format!("read {}", path.display()))?;
    serde_json::from_slice(&bytes).with_context(|| format!("parse {}", path.display()))
}

fn planned_qa_count(
    samples: &[LocomoSample],
    sample_limit: Option<usize>,
    qa_limit: Option<usize>,
) -> usize {
    samples
        .iter()
        .take(sample_limit.unwrap_or(samples.len()))
        .map(|sample| qa_limit.unwrap_or(sample.qa.len()).min(sample.qa.len()))
        .sum()
}

fn total_dataset_qa_count(samples: &[LocomoSample]) -> usize {
    samples.iter().map(|sample| sample.qa.len()).sum()
}

#[derive(Debug)]
struct TestSettings {
    dataset: PathBuf,
    result_path: PathBuf,
    workspace_root: PathBuf,
    resume: bool,
    samples: Option<usize>,
    qa_limit: Option<usize>,
    summary_timeout: Duration,
    final_summary_timeout: Duration,
}

impl TestSettings {
    fn from_env() -> Self {
        Self {
            dataset: std::env::var_os("MEMORY_E2E_DATASET")
                .map(PathBuf::from)
                .unwrap_or_else(|| PathBuf::from(DEFAULT_DATASET)),
            result_path: std::env::var_os("MEMORY_E2E_RESULT")
                .map(PathBuf::from)
                .unwrap_or_else(|| PathBuf::from(DEFAULT_RESULT_PATH)),
            workspace_root: std::env::var_os("MEMORY_E2E_WORKSPACE_ROOT")
                .map(PathBuf::from)
                .unwrap_or_else(|| PathBuf::from(DEFAULT_WORKSPACE_ROOT).join(run_id())),
            resume: parse_bool_env("MEMORY_E2E_RESUME"),
            samples: parse_optional_usize("MEMORY_E2E_SAMPLES"),
            qa_limit: parse_optional_usize("MEMORY_E2E_QA_LIMIT"),
            summary_timeout: Duration::from_millis(
                parse_optional_u64("MEMORY_E2E_SUMMARY_TIMEOUT_MS").unwrap_or(35_000),
            ),
            final_summary_timeout: Duration::from_millis(
                parse_optional_u64("MEMORY_E2E_FINAL_SUMMARY_TIMEOUT_MS").unwrap_or(300_000),
            ),
        }
    }
}

fn parse_bool_env(name: &str) -> bool {
    std::env::var(name)
        .ok()
        .map(|value| {
            matches!(
                value.trim().to_ascii_lowercase().as_str(),
                "1" | "true" | "yes" | "on"
            )
        })
        .unwrap_or(false)
}

fn run_id() -> String {
    let secs = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |duration| duration.as_secs());
    format!("run-{secs}")
}

fn parse_optional_usize(name: &str) -> Option<usize> {
    std::env::var(name)
        .ok()
        .and_then(|value| value.trim().parse::<usize>().ok())
}

fn parse_optional_u64(name: &str) -> Option<u64> {
    std::env::var(name)
        .ok()
        .and_then(|value| value.trim().parse::<u64>().ok())
}

#[derive(Debug, Deserialize)]
struct LocomoSample {
    sample_id: String,
    qa: Vec<QaPair>,
    conversation: Conversation,
}

impl LocomoSample {
    #[allow(dead_code)]
    fn render_session_ingestion(&self, session_num: u32) -> Result<String> {
        let block = self.render_session_transcript(session_num)?;
        Ok(format!("{}", block))
    }

    #[allow(dead_code)]
    fn render_session_transcript(&self, session_num: u32) -> Result<String> {
        let turns = self.conversation.session_turns(session_num)?;
        let datetime = self
            .conversation
            .session_datetime(session_num)
            .unwrap_or_else(|| "unknown time".to_string());
        let mut block = format!("Session {session_num} ({datetime})\n");
        for turn in turns {
            block.push_str(&format!(
                "- {datetime} | {} [{}]: {}\n",
                turn.speaker, turn.dia_id, turn.text
            ));
        }
        Ok(block)
    }

    /// Render a single turn for per-turn ingestion mode.
    #[allow(dead_code)]
    fn render_turn_ingestion(&self, session_num: u32, turn: &Turn) -> Result<String> {
        let datetime = self
            .conversation
            .session_datetime(session_num)
            .unwrap_or_else(|| "unknown time".to_string());
        Ok(format!(
            "Session {session_num} ({datetime})\n- {datetime} | {} [{}]: {}",
            turn.speaker, turn.dia_id, turn.text
        ))
    }

    /// Render a single turn transcript for session store.
    #[allow(dead_code)]
    fn render_turn_transcript(&self, session_num: u32, turn: &Turn) -> Result<String> {
        let datetime = self
            .conversation
            .session_datetime(session_num)
            .unwrap_or_else(|| "unknown time".to_string());
        Ok(format!(
            "Session {session_num} ({datetime})\n- {datetime} | {} [{}]: {}",
            turn.speaker, turn.dia_id, turn.text
        ))
    }

    /// Render a chunk of messages for per-chunk ingestion mode.
    fn render_chunk_ingestion(&self, session_num: u32, chunk: &[Turn]) -> Result<String> {
        let datetime = self
            .conversation
            .session_datetime(session_num)
            .unwrap_or_else(|| "unknown time".to_string());
        let mut block = format!("Session {session_num} ({datetime})\n");
        for turn in chunk {
            block.push_str(&format!(
                "- {datetime} | {} [{}]: {}\n",
                turn.speaker, turn.dia_id, turn.text
            ));
        }
        Ok(block.trim_end().to_string())
    }

    /// Render a chunk of messages transcript for session store.
    fn render_chunk_transcript(&self, session_num: u32, chunk: &[Turn]) -> Result<String> {
        self.render_chunk_ingestion(session_num, chunk)
    }
}

#[derive(Debug, Deserialize)]
struct QaPair {
    question: String,
    answer: Value,
    #[serde(default)]
    evidence: Vec<String>,
    #[serde(default)]
    category: Value,
}

impl QaPair {
    fn answer_as_string(&self) -> String {
        match &self.answer {
            Value::String(value) => value.clone(),
            Value::Number(value) => value.to_string(),
            Value::Bool(value) => value.to_string(),
            Value::Array(values) => values
                .iter()
                .map(|value| match value {
                    Value::String(inner) => inner.clone(),
                    other => other.to_string(),
                })
                .collect::<Vec<_>>()
                .join(", "),
            Value::Null => String::new(),
            other => other.to_string(),
        }
    }
}

#[derive(Debug, Deserialize)]
struct Conversation {
    /// speaker_a: String,
    /// speaker_b: String,
    #[serde(flatten)]
    sessions: BTreeMap<String, Value>,
}

impl Conversation {
    fn session_numbers(&self) -> Vec<u32> {
        self.sessions
            .keys()
            .filter_map(|key| {
                key.strip_prefix("session_")
                    .and_then(|rest| rest.parse::<u32>().ok())
            })
            .collect()
    }

    fn session_datetime(&self, session_num: u32) -> Option<String> {
        self.sessions
            .get(&format!("session_{session_num}_date_time"))
            .and_then(Value::as_str)
            .map(ToString::to_string)
    }

    fn session_turns(&self, session_num: u32) -> Result<Vec<Turn>> {
        let value = self
            .sessions
            .get(&format!("session_{session_num}"))
            .ok_or_else(|| anyhow!("missing session_{session_num}"))?;
        serde_json::from_value(value.clone())
            .with_context(|| format!("parse session_{session_num}"))
    }
}

#[derive(Debug, Deserialize)]
struct Turn {
    speaker: String,
    dia_id: String,
    text: String,
}
