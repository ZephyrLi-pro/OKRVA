//! KylinVectorDB-path structured summary generation.
//!
//! Unlike `consolidation::consolidate_turn` (which outputs two bare strings
//! for Daily/Core categories), this module produces a structured `SummaryItem`
//! JSON and stores it directly under `MemoryCategory::Summary`.
//!
//! Each call processes only the **current turn** (user_msg + assistant_resp),
//! not the full session history. This avoids redundant summarization of
//! previously processed turns and keeps token usage bounded.

use crate::traits::{Memory, MemoryCategory};
use kylinbot_api::memory_traits::SummaryItem;
use kylinbot_api::provider::Provider;
use std::time::Duration;

/// Upper bound on the background summary LLM call. The summary is a detached,
/// fire-and-forget task with no other supervision, so without this a hung /
/// throttled upstream (e.g. DashScope) would leave the task silently pending
/// forever (only a "summary_turn started" log, no completion). A timeout turns
/// that silent hang into an observable `Err` that callers log.
const SUMMARY_CHAT_TIMEOUT: Duration = Duration::from_secs(30);


#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SummaryOutcome {
    Stored,
    Skipped,
}

const SUMMARY_SYSTEM_PROMPT: &str = r#"你是一个专业的对话分析师与记忆提取专家。你的任务是基于提供的对话材料，生成结构化摘要与记忆信息，供系统索引、上下文恢复及跨会话记忆使用。
## 核心约束（必须遵守）
1. **纯JSON输出**：仅输出一个合法的JSON对象，严禁输出Markdown代码块（如```json）、解释文本或额外注释，确保可被 `json.loads()` 直接解析。
2. **字段完整性**：所有字段必须存在。若某字段无内容，字符串类型留空 `""`，数组类型填空数组 `[]`。
3. **语言一致性**：输出的内容必须与对话材料的原始语言保持完全一致。
4. **客观性**：除 `summary_text` 需要叙事性描述外，其余数组字段必须客观陈述事实，严禁主观评价或臆测。

## 字段提取规范
### 1. summary (string)
- **提取要求**：用一句话概括核心会话主题，作为摘要标题。
- **长度约束**：10-20词（中文10-20字）。
### 2. summary_text (string)
- **提取要求**：第三人称叙事性完整摘要，需包含对话的重要细节与发展脉络，确保模型仅凭此段文本即可理解前因后果。
- **长度约束**：10-200词（中文10-200字）。
### 3. learned (array[string])
- **提取要求**：提取值得跨会话持久化的知识。包括但不限于：用户明确要求记住的内容（如"记住..."）、经过验证的技术决策、经验教训、用户偏好。每项为一个独立的原子化事实。
### 4. key_facts (array[string])
- **提取要求**：提取当次会话中的具体技术细节，如文件路径、参数、版本号、配置值等。
### 5. temporal_facts (array[string])
- **提取要求**：提取带精确时间的事实。**【关键规则】**：必须将相对时间（如"明天"、"下周"、"下个月"）结合当前对话发生的时间，转换为绝对时间（如"2024-05-20"、"下周二 2024-05-28"）。因为该记录可能在数周后才被读取，相对时间届时已失效。
### 6. buffer_info (array[string])
- **提取要求**：记录本次会话结束时的未完成状态，包括：进行中的任务、下一步行动、中断点、待确认事项。无需用户重新描述即可接续任务。
### 7. user_intents_and_slots (string)
- **提取要求**：记录用户明确表达的意图和本次完成的操作。仅作客观记录，不预测意图。使用动宾结构（如"配置了XX环境"、"修改了XX参数"）。注意：不要将"记住X"放入此字段，那属于 `learned` 字段。

## 输出模板
{
  "summary": "",
  "summary_text": "",
  "learned": [],
  "key_facts": [],
  "temporal_facts": [],
  "buffer_info": [],
  "user_intents_and_slots": []
}
"#;

#[derive(Debug, serde::Deserialize)]
struct SummaryLlmOutput {
    summary: String,
    summary_text: String,
    #[serde(default)]
    learned: Vec<String>,
    #[serde(default)]
    key_facts: Vec<String>,
    #[serde(default)]
    temporal_facts: Vec<String>,
    #[serde(default)]
    buffer_info: Vec<String>,
    #[serde(default)]
    user_intents_and_slots: Vec<String>,
}

fn strip_media_markers(text: &str) -> String {
    static RE: std::sync::LazyLock<regex::Regex> = std::sync::LazyLock::new(|| {
        regex::Regex::new(r"\[(?:IMAGE|DOCUMENT|FILE|VIDEO|VOICE|AUDIO):[^\]]*\]").unwrap()
    });
    RE.replace_all(text, "[media attachment]").into_owned()
}

fn parse_summary_response(raw: &str) -> SummaryLlmOutput {
    let cleaned = raw
        .trim()
        .trim_start_matches("```json")
        .trim_start_matches("```")
        .trim_end_matches("```")
        .trim();

    serde_json::from_str(cleaned).unwrap_or_else(|_| SummaryLlmOutput {
        summary: "Turn summary unavailable".into(),
        summary_text: raw.chars().take(150).collect(),
        learned: Vec::new(),
        key_facts: Vec::new(),
        temporal_facts: Vec::new(),
        buffer_info: Vec::new(),
        user_intents_and_slots: Vec::new(),
    })
}

fn turn_has_substance(user_msg: &str) -> bool {
    !crate::should_skip_autosave_content(user_msg)
}

pub async fn summarize_turn(
    provider: &dyn Provider,
    model: &str,
    memory: &dyn Memory,
    user_msg: &str,
    assistant_resp: &str,
    session_id: &str,
    transcript_session_key: &str,
    turn_num: i32,
) -> anyhow::Result<SummaryOutcome> {
    tracing::debug!(
        session_id,
        transcript_session_key,
        turn_num,
        user_len = user_msg.len(),
        assistant_len = assistant_resp.len(),
        model,
        memory_backend = memory.name(),
        "summary_turn started"
    );

    let turn_text = format!(
        "User: {}\nAssistant: {}",
        strip_media_markers(user_msg),
        strip_media_markers(assistant_resp),
    );
    if !turn_has_substance(user_msg) {
        tracing::debug!(
            session_id,
            transcript_session_key,
            turn_num,
            user_chars = user_msg.chars().count(),
            "summary_turn skipped: user turn has no substantive content"
        );
        return Ok(SummaryOutcome::Skipped);
    }

    let truncated = if turn_text.len() > 2000 {
        let end = turn_text
            .char_indices()
            .map(|(i, _)| i)
            .take_while(|&i| i <= 2000)
            .last()
            .unwrap_or(0);
        format!("{}…", &turn_text[..end])
    } else {
        turn_text.clone()
    };

    const SUMMARY_TEMPERATURE: f64 = 0.1;
    let chat_fut = provider.chat_with_system(
        Some(SUMMARY_SYSTEM_PROMPT),
        &truncated,
        model,
        Some(SUMMARY_TEMPERATURE),
    );
    let raw = match tokio::time::timeout(SUMMARY_CHAT_TIMEOUT, chat_fut).await {
        Ok(result) => result?,
        Err(_elapsed) => {
            anyhow::bail!(
                "summary chat timed out after {}s (model={model}, session_id={session_id}, turn_num={turn_num})",
                SUMMARY_CHAT_TIMEOUT.as_secs()
            );
        }
    };
    tracing::debug!(
        session_id,
        turn_num,
        raw_len = raw.len(),
        "summary_turn provider response received"
    );

    let output = parse_summary_response(&raw);
    tracing::debug!(
        session_id,
        turn_num,
        summary = %output.summary,
        summary_text_len = output.summary_text.len(),
        learned_count = output.learned.len(),
        key_facts_count = output.key_facts.len(),
        intents_count = output.user_intents_and_slots.len(),
        "summary_turn parsed response"
    );

    let memory_id = uuid::Uuid::new_v4().to_string();
    let generated_at = chrono::Utc::now().to_rfc3339();
    let learned_facts = output.learned.clone();

    let item = SummaryItem {
        memory_id: memory_id.clone(),
        session_id: session_id.to_string(),
        transcript_session_key: Some(transcript_session_key.to_string()),
        generated_at: generated_at.clone(),
        summary: output.summary,
        summary_text: output.summary_text,
        learned: output.learned,
        key_facts: output.key_facts,
        temporal_facts: output.temporal_facts,
        buffer_info: output.buffer_info,
        user_intents_and_slots: output.user_intents_and_slots.clone(),
        turn_num: Some(turn_num),
    };

    if !item.has_structured_content() {
        tracing::debug!(
            session_id,
            transcript_session_key,
            turn_num,
            memory_id = %memory_id,
            "summary_turn skipped: parsed summary has no structured content"
        );
        return Ok(SummaryOutcome::Skipped);
    }

    let content = serde_json::to_string(&item)?;
    tracing::debug!(
        session_id,
        turn_num,
        memory_id = %memory_id,
        content_len = content.len(),
        "summary_turn storing summary item"
    );

    memory
        .store(
            &memory_id,
            &content,
            MemoryCategory::Summary,
            Some(session_id),
        )
        .await?;
    tracing::debug!(
        session_id,
        turn_num,
        memory_id = %memory_id,
        "summary_turn stored summary item"
    );

    // Store learned facts as global Core entries for cross-session recall.
    // This ensures user preferences and stable facts are visible across all sessions,
    // even if the model didn't explicitly call memory_store tool.
    if !learned_facts.is_empty() {
        for fact in &learned_facts {
            let learned_key = format!("learned_{}", uuid::Uuid::new_v4());
            tracing::debug!(
                session_id,
                turn_num,
                key = %learned_key,
                fact = %fact,
                "summary_turn storing learned fact as global Core"
            );
            memory
                .store(
                    &learned_key,
                    fact,
                    MemoryCategory::Core,
                    None, // Global - no session binding
                )
                .await?;
        }
        tracing::debug!(
            session_id,
            turn_num,
            learned_count = learned_facts.len(),
            "summary_turn stored all learned facts as global Core entries"
        );
    }

    Ok(SummaryOutcome::Stored)
}

#[cfg(test)]
mod tests {
    use super::*;
    use async_trait::async_trait;
    use kylinbot_api::memory_traits::MemoryEntry;
    use std::sync::atomic::{AtomicUsize, Ordering};

    struct StaticProvider {
        response: &'static str,
        calls: AtomicUsize,
    }

    #[async_trait]
    impl Provider for StaticProvider {
        async fn chat_with_system(
            &self,
            _system_prompt: Option<&str>,
            _message: &str,
            _model: &str,
            _temperature: Option<f64>,
        ) -> anyhow::Result<String> {
            self.calls.fetch_add(1, Ordering::SeqCst);
            Ok(self.response.to_string())
        }
    }

    #[derive(Default)]
    struct RecordingMemory {
        store_calls: AtomicUsize,
    }

    #[async_trait]
    impl Memory for RecordingMemory {
        fn name(&self) -> &str {
            "test"
        }

        async fn store(
            &self,
            _key: &str,
            _content: &str,
            _category: MemoryCategory,
            _session_id: Option<&str>,
        ) -> anyhow::Result<()> {
            self.store_calls.fetch_add(1, Ordering::SeqCst);
            Ok(())
        }

        async fn recall(
            &self,
            _query: &str,
            _limit: usize,
            _session_id: Option<&str>,
            _since: Option<&str>,
            _until: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(Vec::new())
        }

        async fn get(&self, _key: &str) -> anyhow::Result<Option<MemoryEntry>> {
            Ok(None)
        }

        async fn list(
            &self,
            _category: Option<&MemoryCategory>,
            _session_id: Option<&str>,
        ) -> anyhow::Result<Vec<MemoryEntry>> {
            Ok(Vec::new())
        }

        async fn forget(&self, _key: &str) -> anyhow::Result<bool> {
            Ok(false)
        }

        async fn count(&self) -> anyhow::Result<usize> {
            Ok(self.store_calls.load(Ordering::SeqCst))
        }

        async fn health_check(&self) -> bool {
            true
        }
    }

    #[test]
    fn parse_summary_response_valid_json() {
        let raw = r#"{"summary":"topic","summary_text":"narrative","learned":["pref A"],"key_facts":[],"temporal_facts":[],"buffer_info":[],"user_intents_and_slots":[]}"#;
        let out = parse_summary_response(raw);
        assert_eq!(out.summary, "topic");
        assert_eq!(out.learned, vec!["pref A"]);
    }

    #[test]
    fn parse_summary_response_falls_back_on_invalid() {
        let out = parse_summary_response("not json at all");
        assert_eq!(out.summary, "Turn summary unavailable");
        assert!(out.learned.is_empty());
    }

    #[test]
    fn parse_summary_response_handles_code_block_wrapper() {
        let raw = "```json\n{\"summary\":\"t\",\"summary_text\":\"n\",\"learned\":[],\"key_facts\":[],\"temporal_facts\":[],\"buffer_info\":[],\"user_intents_and_slots\":[]}\n```";
        let out = parse_summary_response(raw);
        assert_eq!(out.summary, "t");
    }

    #[test]
    fn turn_text_strips_media_markers() {
        let user_msg = "look [IMAGE:/tmp/photo.jpg] at this";
        let assistant_resp = "I see it";
        let turn_text = format!(
            "User: {}\nAssistant: {}",
            strip_media_markers(user_msg),
            strip_media_markers(assistant_resp),
        );
        assert!(!turn_text.contains("/tmp/photo.jpg"));
        assert!(turn_text.contains("[media attachment]"));
    }

    #[test]
    fn truncation_is_char_boundary_safe() {
        let long_user = "你好世界".repeat(500);
        let turn_text = format!("User: {}\nAssistant: ok", long_user);
        let truncated = if turn_text.len() > 2000 {
            let end = turn_text
                .char_indices()
                .map(|(i, _)| i)
                .take_while(|&i| i <= 2000)
                .last()
                .unwrap_or(0);
            format!("{}…", &turn_text[..end])
        } else {
            turn_text.clone()
        };
        assert!(truncated.is_char_boundary(truncated.len()));
        assert!(truncated.ends_with('…'));
    }

    #[test]
    fn turn_has_substance_filters_short_blank_and_synthetic_noise() {
        assert!(!turn_has_substance("   "));
        assert!(!turn_has_substance(
            "[cron:auto] patrol check with enough characters"
        ));
        assert!(turn_has_substance(
            "请帮我分析这个配置为什么启动失败，并给出排查步骤"
        ));
    }

    #[tokio::test]
    async fn summarize_turn_skips_empty_structured_summary() {
        let provider = StaticProvider {
            response: r#"{"summary":"配置排查","summary_text":"用户询问配置启动失败的排查。","learned":[],"key_facts":[],"temporal_facts":[],"buffer_info":[],"user_intents_and_slots":[]}"#,
            calls: AtomicUsize::new(0),
        };
        let memory = RecordingMemory::default();

        let outcome = summarize_turn(
            &provider,
            "test-model",
            &memory,
            "请帮我分析这个配置为什么启动失败，并给出排查步骤",
            "可以从日志、端口和权限三个方向排查。",
            "session",
            "session",
            1,
        )
        .await
        .unwrap();

        assert_eq!(outcome, SummaryOutcome::Skipped);
        assert_eq!(provider.calls.load(Ordering::SeqCst), 1);
        assert_eq!(memory.store_calls.load(Ordering::SeqCst), 0);
    }
}
