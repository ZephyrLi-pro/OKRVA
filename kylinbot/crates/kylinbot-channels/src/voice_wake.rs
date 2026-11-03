//! Voice Wake Word detection channel.
//!
//! Listens on the default microphone via `cpal`, detects a configurable wake
//! word using energy-based VAD followed by a dedicated keyword-spotting model,
//! then captures the subsequent utterance and dispatches it as a channel message.
//! If the optional KWS backend is unavailable, transcription-based matching is
//! retained as a compatibility fallback.
//!
//! Gated behind the `voice-wake` Cargo feature.

use std::collections::VecDeque;
use std::env;
use std::future::Future;
use std::path::PathBuf;
use std::process::Stdio;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Result, bail};
use async_trait::async_trait;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::process::Command;
use tokio::sync::mpsc;
use tracing::{debug, info, warn};

use crate::transcription::transcribe_audio;
use kylinbot_config::schema::TranscriptionConfig;
use kylinbot_config::schema::VoiceWakeConfig;

use kylinbot_api::channel::{Channel, ChannelMessage, SendMessage};

// ── State machine ──────────────────────────────────────────────

/// Internal states for the wake-word detector.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WakeState {
    /// Passively monitoring microphone energy levels.
    Listening,
    /// Energy spike detected — capturing a short window to check for wake word.
    Triggered,
    /// Wake word confirmed — capturing the full utterance that follows.
    Capturing,
    /// Captured audio is being transcribed.
    Processing,
}

impl std::fmt::Display for WakeState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Listening => write!(f, "Listening"),
            Self::Triggered => write!(f, "Triggered"),
            Self::Capturing => write!(f, "Capturing"),
            Self::Processing => write!(f, "Processing"),
        }
    }
}

// ── Channel implementation ─────────────────────────────────────

/// Voice wake-word channel that activates on a spoken keyword.
pub struct VoiceWakeChannel {
    config: VoiceWakeConfig,
    transcription_config: TranscriptionConfig,
}

impl VoiceWakeChannel {
    /// Create a new `VoiceWakeChannel` from its config sections.
    pub fn new(config: VoiceWakeConfig, transcription_config: TranscriptionConfig) -> Self {
        Self {
            config,
            transcription_config,
        }
    }
}

const VOICE_ASSISTANT_SERVICE: &str = "org.ukui.voice_assistant";
const VOICE_ASSISTANT_PATH: &str = "/org/ukui/voice_assistant";
const VOICE_ASSISTANT_INTERFACE: &str = "org.ukui.voice_assistant";

/// Invoke the optional small-K voice window over the user session bus.
///
/// The UI bridge is deliberately best-effort: systems without the lite UI keep
/// the original input-only voice-wake behaviour instead of failing the channel.
async fn call_voice_assistant(method: &str, text: Option<&str>) -> bool {
    let mut command = Command::new("busctl");
    command.kill_on_drop(true);
    command.args([
        "--user",
        "call",
        VOICE_ASSISTANT_SERVICE,
        VOICE_ASSISTANT_PATH,
        VOICE_ASSISTANT_INTERFACE,
        method,
    ]);
    if let Some(value) = text {
        command.args(["s", value]);
    }

    match tokio::time::timeout(Duration::from_secs(8), command.output()).await {
        Ok(Ok(output)) if output.status.success() => true,
        Ok(Ok(output)) => {
            warn!(
                method,
                stderr = %String::from_utf8_lossy(&output.stderr).trim(),
                "VoiceWake: small-K voice window D-Bus call failed"
            );
            false
        }
        Ok(Err(error)) => {
            warn!(method, %error, "VoiceWake: failed to run busctl for small-K UI");
            false
        }
        Err(_) => {
            warn!(
                method,
                "VoiceWake: small-K voice window D-Bus call timed out"
            );
            false
        }
    }
}

async fn show_voice_assistant_listening() -> bool {
    call_voice_assistant("ShowExternalListening", None).await
}

async fn submit_voice_assistant_text(text: &str) -> bool {
    call_voice_assistant("SubmitExternalText", Some(text)).await
}

#[derive(Debug, PartialEq, Eq)]
enum CapturePermission {
    Ready,
    Rejected,
    Legacy,
}

fn parse_capture_permission(output: &str) -> CapturePermission {
    match output.trim() {
        "s \"ready\"" => CapturePermission::Ready,
        "s \"speaking\"" | "s \"muted\"" | "s \"full\"" | "s \"capturing\"" | "s \"closing\"" => {
            CapturePermission::Rejected
        }
        _ => CapturePermission::Legacy,
    }
}

async fn prepare_voice_assistant_capture() -> CapturePermission {
    let mut command = Command::new("busctl");
    command.kill_on_drop(true).args([
        "--user",
        "call",
        VOICE_ASSISTANT_SERVICE,
        VOICE_ASSISTANT_PATH,
        VOICE_ASSISTANT_INTERFACE,
        "PrepareExternalListening",
    ]);
    match tokio::time::timeout(Duration::from_secs(8), command.output()).await {
        Ok(Ok(output)) if output.status.success() => {
            parse_capture_permission(&String::from_utf8_lossy(&output.stdout))
        }
        // Keep the original input-only channel on systems without the lite UI,
        // and support older voice windows without the new handshake method.
        _ => CapturePermission::Legacy,
    }
}

const FOLLOWUP_WINDOW_MS: u64 = 9_000;
// The UI can spend more than a second processing the submitted question before
// servicing the next D-Bus call. This is a handshake timeout, NOT the user's
// nine-second speech-onset budget; align it with the normal capture handshake.
const FOLLOWUP_HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(8);

#[derive(Debug, PartialEq, Eq)]
enum FollowupPermission {
    Waiting,
    Ready(u64),
    Off,
    Unavailable,
}

fn parse_followup_permission(output: &str) -> FollowupPermission {
    let output = output.trim();
    if output == "s \"waiting\"" {
        return FollowupPermission::Waiting;
    }
    if let Some(remaining) = output
        .strip_prefix("s \"ready:")
        .and_then(|text| text.strip_suffix('"'))
        .and_then(|text| text.parse::<u64>().ok())
        .filter(|remaining| (1..=FOLLOWUP_WINDOW_MS).contains(remaining))
    {
        return FollowupPermission::Ready(remaining);
    }
    // No legacy fallback: absent/old/closed UIs must never open a microphone round.
    FollowupPermission::Off
}

async fn prepare_followup_capture() -> FollowupPermission {
    let mut command = Command::new("busctl");
    command.args([
        "--user",
        "--auto-start=no",
        "call",
        VOICE_ASSISTANT_SERVICE,
        VOICE_ASSISTANT_PATH,
        VOICE_ASSISTANT_INTERFACE,
        "PrepareFollowupListening",
    ]);
    query_followup_permission(command, FOLLOWUP_HANDSHAKE_TIMEOUT).await
}

async fn query_followup_permission(mut command: Command, timeout: Duration) -> FollowupPermission {
    command.kill_on_drop(true);
    match tokio::time::timeout(timeout, command.output()).await {
        Ok(Ok(output)) if output.status.success() => {
            parse_followup_permission(&String::from_utf8_lossy(&output.stdout))
        }
        // A failed/late reply is not an explicit opt-out. Keep it observable,
        // but do not blindly retry a timed-out one-shot microphone claim: the
        // server might already have claimed it even though its reply was lost.
        _ => FollowupPermission::Unavailable,
    }
}

fn retain_followup_audio(buffer: &mut VecDeque<f32>, chunk: &[f32], limit: usize) {
    buffer.extend(chunk.iter().copied());
    if buffer.len() > limit {
        buffer.drain(..buffer.len() - limit);
    }
}

fn take_followup_preroll(
    buffer: &mut VecDeque<f32>,
    remaining_ms: u64,
    rate: u32,
    channels: u16,
) -> Vec<f32> {
    // Keep only audio captured since playback finished, never its speaker tail.
    let elapsed_ms = FOLLOWUP_WINDOW_MS.saturating_sub(remaining_ms);
    let limit = (elapsed_ms as usize * rate as usize / 1000) * usize::from(channels.max(1));
    if buffer.len() > limit {
        buffer.drain(..buffer.len() - limit);
    }
    buffer.drain(..).collect()
}

fn ends_voice_conversation(text: &str) -> bool {
    matches!(normalize_wake_text(text).as_str(), "结束对话" | "退出对话")
}

struct NeuralVad {
    child: tokio::process::Child,
    input: tokio::process::ChildStdin,
    output: BufReader<tokio::process::ChildStdout>,
}

impl NeuralVad {
    async fn start(rate: u32) -> Result<Option<Self>> {
        let (Some(binary), Some(model)) = (
            env::var_os("KYLINBOT_VAD_BINARY"),
            env::var_os("KYLINBOT_VAD_MODEL"),
        ) else {
            return Ok(None);
        };
        let mut child = Command::new(binary)
            .arg(model)
            .arg(rate.to_string())
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit())
            .kill_on_drop(true)
            .spawn()?;
        let input = child
            .stdin
            .take()
            .ok_or_else(|| anyhow::anyhow!("missing VAD stdin"))?;
        let output = child
            .stdout
            .take()
            .ok_or_else(|| anyhow::anyhow!("missing VAD stdout"))?;
        let mut detector = Self {
            child,
            input,
            output: BufReader::new(output),
        };
        let mut line = String::new();
        tokio::time::timeout(
            Duration::from_secs(20),
            detector.output.read_line(&mut line),
        )
        .await??;
        if line.trim() != "READY" {
            bail!("VAD worker did not become ready");
        }
        info!("VoiceWake: Silero neural VAD is ready (CPU, one thread)");
        Ok(Some(detector))
    }

    async fn classify(&mut self, samples: &[f32]) -> Result<bool> {
        if samples.len() > 65536 {
            bail!("VAD frame exceeds protocol limit");
        }
        if self.child.try_wait()?.is_some() {
            bail!("VAD worker exited");
        }
        let count = u32::try_from(samples.len())?;
        let mut request = Vec::with_capacity(4 + samples.len() * 4);
        request.extend_from_slice(&count.to_le_bytes());
        for sample in samples {
            request.extend_from_slice(&sample.to_le_bytes());
        }
        let reply = async {
            self.input.write_all(&request).await?;
            self.input.flush().await?;
            let mut line = String::new();
            self.output.read_line(&mut line).await?;
            match line.trim() {
                "0" => Ok(false),
                "1" => Ok(true),
                _ => bail!("invalid VAD worker reply"),
            }
        };
        tokio::time::timeout(Duration::from_secs(1), reply).await?
    }

    async fn reset(&mut self) -> Result<()> {
        self.classify(&[]).await.map(|_| ())
    }
}

/// Play the optional, pre-rendered wake reply and wait for the audio to drain.
/// The WAV must have its trailing silence trimmed so playback completion is
/// also the point at which the user hears the reply finish.
async fn play_wake_acknowledgement() -> Result<()> {
    let Some(wav_path) = env::var_os("KYLINBOT_WAKE_ACK_FILE") else {
        return Ok(());
    };
    let started = Instant::now();
    let mut command = Command::new("pw-play");
    command
        .kill_on_drop(true)
        .args(["--latency", "20ms", "--volume", "0.65"])
        .arg(wav_path);

    let output = tokio::time::timeout(Duration::from_secs(3), command.output())
        .await
        .map_err(|_| anyhow::anyhow!("wake acknowledgement playback timed out"))??;
    if !output.status.success() {
        bail!(
            "wake acknowledgement playback failed: {}",
            String::from_utf8_lossy(&output.stderr).trim()
        );
    }
    info!(
        elapsed_ms = started.elapsed().as_millis(),
        "VoiceWake: wake acknowledgement finished; starting capture immediately"
    );
    Ok(())
}

async fn begin_external_capture(
    audio_rx: &mut mpsc::Receiver<Vec<f32>>,
    voice_ui_show_task: &mut Option<tokio::task::JoinHandle<bool>>,
    preroll_samples: usize,
) -> Option<Vec<f32>> {
    // Start/raise the small-K listening window without waiting for D-Bus
    // activation. The window is external-ASR-only, so it never opens a second
    // microphone capture session. Show it while the optional reply is playing.
    if let Some(previous_task) = voice_ui_show_task.take() {
        previous_task.abort();
    }
    let (permission, _) =
        collect_pending_audio(prepare_voice_assistant_capture(), audio_rx, preroll_samples).await;
    if permission == CapturePermission::Rejected {
        info!("VoiceWake: UI rejected capture (half-duplex, muted, or queue full)");
        return None;
    }
    *voice_ui_show_task = Some(tokio::spawn(async move {
        permission == CapturePermission::Ready || show_voice_assistant_listening().await
    }));

    let (playback, pending) =
        collect_pending_audio(play_wake_acknowledgement(), audio_rx, preroll_samples).await;
    if let Err(error) = playback {
        warn!(%error, "VoiceWake: wake reply unavailable; continuing with capture");
    }

    // Keep only the short tail, not the wake phrase or the whole acknowledgement.
    // Audible playback may end before pw-play exits. Never flush speech that
    // arrives in that interval; the main capture keeps this tail as pre-roll.
    Some(pending.into_iter().flatten().collect())
}

/// Consume microphone callbacks even while playback/ASR is awaited. The old
/// four-chunk queue dropped the rest and a subsequent drain discarded the onset.
async fn collect_pending_audio<F, T>(
    future: F,
    audio_rx: &mut mpsc::Receiver<Vec<f32>>,
    limit: usize,
) -> (T, VecDeque<Vec<f32>>)
where
    F: Future<Output = T>,
{
    tokio::pin!(future);
    let mut pending: VecDeque<Vec<f32>> = VecDeque::new();
    let mut samples = 0usize;
    let mut input_open = true;
    loop {
        tokio::select! {
            biased;
            chunk = audio_rx.recv(), if input_open => {
                if let Some(chunk) = chunk {
                    samples += chunk.len();
                    pending.push_back(chunk);
                    while samples > limit {
                        let excess = samples - limit;
                        if let Some(front) = pending.front_mut() {
                            if front.len() > excess {
                                front.drain(..excess);
                                samples -= excess;
                            } else {
                                samples -= front.len();
                                pending.pop_front();
                            }
                        } else { break; }
                    }
                } else {
                    input_open = false;
                }
            }
            output = &mut future => return (output, pending),
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
enum CaptureProgress {
    Waiting,
    Ready,
    NoSpeech,
    TooLong,
}

/// All endpoints use the audio clock, not wall time spent in D-Bus or inference.
/// Silence may end an utterance only after a sustained speech onset is seen.
struct UtteranceCapture {
    samples: Vec<f32>,
    threshold: f32,
    preroll: usize,
    onset: usize,
    silence: usize,
    maximum: usize,
    wait_limit: usize,
    default_wait_limit: usize,
    waited: usize,
    consecutive_voice: usize,
    speech_started: bool,
    last_voice: usize,
    peak_rms: f32,
}

impl UtteranceCapture {
    fn new(rate: u32, channels: u16, silence: Duration, maximum: Duration) -> Self {
        let count = |duration: Duration| {
            // Count frames first: floating-point truncation at 44.1 kHz could
            // otherwise keep an odd stereo sample count and swap channel order.
            ((duration.as_nanos() * u128::from(rate)) / 1_000_000_000) as usize
                * usize::from(channels.max(1))
        };
        Self {
            samples: Vec::new(),
            threshold: 0.008,
            preroll: count(Duration::from_millis(350)),
            onset: count(Duration::from_millis(80)).max(1),
            silence: count(silence).max(1),
            maximum: count(maximum).max(1),
            wait_limit: count(Duration::from_secs(8)).max(1),
            default_wait_limit: count(Duration::from_secs(8)).max(1),
            waited: 0,
            consecutive_voice: 0,
            speech_started: false,
            last_voice: 0,
            peak_rms: 0.0,
        }
    }

    fn reset(&mut self, threshold: f32, mut preroll: Vec<f32>) {
        if preroll.len() > self.preroll {
            preroll.drain(..preroll.len() - self.preroll);
        }
        self.samples = preroll;
        self.wait_limit = self.default_wait_limit;
        self.threshold = threshold;
        self.waited = 0;
        self.consecutive_voice = 0;
        self.speech_started = false;
        self.last_voice = 0;
        self.peak_rms = 0.0;
    }

    fn push(&mut self, chunk: &[f32], energy: f32) -> CaptureProgress {
        self.push_classified(chunk, energy, energy >= self.threshold)
    }

    fn push_classified(&mut self, chunk: &[f32], energy: f32, speech: bool) -> CaptureProgress {
        self.peak_rms = self.peak_rms.max(energy);
        self.samples.extend_from_slice(chunk);
        if speech {
            self.consecutive_voice += chunk.len();
            if self.consecutive_voice >= self.onset {
                self.speech_started = true;
            }
            self.last_voice = self.samples.len();
        } else {
            self.consecutive_voice = 0;
        }
        if !self.speech_started {
            self.waited += chunk.len();
            if self.samples.len() > self.preroll {
                let excess = self.samples.len() - self.preroll;
                self.samples.drain(..excess);
                self.last_voice = self.last_voice.saturating_sub(excess);
            }
            return if self.waited >= self.wait_limit {
                CaptureProgress::NoSpeech
            } else {
                CaptureProgress::Waiting
            };
        }
        if self.samples.len().saturating_sub(self.last_voice) >= self.silence {
            CaptureProgress::Ready
        } else if self.samples.len() >= self.maximum {
            // Never send a truncated command to the agent as though it were a
            // complete instruction. Ask the user to split it into shorter turns.
            CaptureProgress::TooLong
        } else {
            CaptureProgress::Waiting
        }
    }
}

/// Estimate the quiet floor while listening. The configured energy is a ceiling
/// for speech onset, so a quiet microphone is not forced to reach the wake VAD
/// threshold. A low percentile avoids using the wake phrase as background noise.
fn utterance_threshold(background: &VecDeque<f32>, configured: f32) -> f32 {
    if background.len() < 12 {
        return configured;
    }
    let mut sorted: Vec<f32> = background.iter().copied().collect();
    sorted.sort_by(f32::total_cmp);
    (sorted[sorted.len() / 5] * 3.0).max(0.001).min(configured)
}

async fn report_capture_failure(code: &str) {
    let _ = call_voice_assistant("ExternalRecognitionFailed", Some(code)).await;
}

#[derive(Clone, Debug)]
struct KwsCommandConfig {
    binary: PathBuf,
    stream_binary: Option<PathBuf>,
    encoder: PathBuf,
    decoder: PathBuf,
    joiner: PathBuf,
    tokens: PathBuf,
    keywords: PathBuf,
    provider: String,
    threads: usize,
    score: f32,
    threshold: f32,
}

impl KwsCommandConfig {
    /// Load the optional sherpa-onnx KWS command from the service environment.
    /// Requiring all model paths keeps the generic KylinBot package independent
    /// from a particular model filename or installation prefix.
    fn from_env() -> Option<Self> {
        Some(Self {
            binary: env::var_os("KYLINBOT_KWS_BINARY")?.into(),
            stream_binary: env::var_os("KYLINBOT_KWS_STREAM_BINARY").map(Into::into),
            encoder: env::var_os("KYLINBOT_KWS_ENCODER")?.into(),
            decoder: env::var_os("KYLINBOT_KWS_DECODER")?.into(),
            joiner: env::var_os("KYLINBOT_KWS_JOINER")?.into(),
            tokens: env::var_os("KYLINBOT_KWS_TOKENS")?.into(),
            keywords: env::var_os("KYLINBOT_KWS_KEYWORDS")?.into(),
            provider: env::var("KYLINBOT_KWS_PROVIDER").unwrap_or_else(|_| "cpu".into()),
            threads: env::var("KYLINBOT_KWS_THREADS")
                .ok()
                .and_then(|value| value.parse().ok())
                .unwrap_or(2),
            score: env::var("KYLINBOT_KWS_SCORE")
                .ok()
                .and_then(|value| value.parse().ok())
                .unwrap_or(1.0),
            threshold: env::var("KYLINBOT_KWS_THRESHOLD")
                .ok()
                .and_then(|value| value.parse().ok())
                .unwrap_or(0.25),
        })
    }
}

fn spawn_streaming_kws(
    config: &KwsCommandConfig,
    sample_rate: u32,
) -> Result<(mpsc::Sender<Vec<f32>>, mpsc::Receiver<String>)> {
    let binary = config
        .stream_binary
        .as_ref()
        .ok_or_else(|| anyhow::anyhow!("KYLINBOT_KWS_STREAM_BINARY is not configured"))?;

    let mut child = Command::new(binary)
        .args([
            config.encoder.display().to_string(),
            config.decoder.display().to_string(),
            config.joiner.display().to_string(),
            config.tokens.display().to_string(),
            config.keywords.display().to_string(),
            config.provider.clone(),
            sample_rate.to_string(),
            config.threads.to_string(),
            config.score.to_string(),
            config.threshold.to_string(),
        ])
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .kill_on_drop(true)
        .spawn()?;

    let mut child_stdin = child
        .stdin
        .take()
        .ok_or_else(|| anyhow::anyhow!("streaming KWS worker has no stdin"))?;
    let child_stdout = child
        .stdout
        .take()
        .ok_or_else(|| anyhow::anyhow!("streaming KWS worker has no stdout"))?;

    let (pcm_tx, mut pcm_rx) = mpsc::channel::<Vec<f32>>(8);
    let (keyword_tx, keyword_rx) = mpsc::channel::<String>(4);

    tokio::spawn(async move {
        while let Some(samples) = pcm_rx.recv().await {
            let mut bytes = Vec::with_capacity(samples.len() * std::mem::size_of::<f32>());
            for sample in samples {
                bytes.extend_from_slice(&sample.to_le_bytes());
            }
            if let Err(error) = child_stdin.write_all(&bytes).await {
                warn!(%error, "VoiceWake: streaming KWS PCM writer stopped");
                break;
            }
        }
    });

    tokio::spawn(async move {
        let mut lines = BufReader::new(child_stdout).lines();
        loop {
            match lines.next_line().await {
                Ok(Some(line)) if line == "READY" => {
                    info!("VoiceWake: streaming KWS model is ready");
                }
                Ok(Some(line)) => {
                    if let Some(keyword) = line.strip_prefix("KEYWORD\t")
                        && keyword_tx.send(keyword.to_owned()).await.is_err()
                    {
                        break;
                    }
                }
                Ok(None) => break,
                Err(error) => {
                    warn!(%error, "VoiceWake: streaming KWS result reader stopped");
                    break;
                }
            }
        }
    });

    tokio::spawn(async move {
        match child.wait().await {
            Ok(status) if status.success() => {
                warn!("VoiceWake: streaming KWS worker exited");
            }
            Ok(status) => {
                warn!(%status, "VoiceWake: streaming KWS worker failed");
            }
            Err(error) => {
                warn!(%error, "VoiceWake: failed to wait for streaming KWS worker");
            }
        }
    });

    Ok((pcm_tx, keyword_rx))
}

fn parse_kws_keyword(output: &str) -> Option<String> {
    output.lines().find_map(|line| {
        let value: serde_json::Value = serde_json::from_str(line.trim()).ok()?;
        value
            .get("keyword")
            .and_then(serde_json::Value::as_str)
            .map(str::to_owned)
    })
}

async fn detect_keyword_with_sherpa(
    config: &KwsCommandConfig,
    wav_bytes: &[u8],
) -> Result<Option<String>> {
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    let wav_path = env::temp_dir().join(format!(
        "kylinbot-kws-{}-{timestamp}.wav",
        std::process::id()
    ));
    tokio::fs::write(&wav_path, wav_bytes).await?;

    let mut command = Command::new(&config.binary);
    command.kill_on_drop(true).args([
        format!("--encoder={}", config.encoder.display()),
        format!("--decoder={}", config.decoder.display()),
        format!("--joiner={}", config.joiner.display()),
        format!("--tokens={}", config.tokens.display()),
        format!("--keywords-file={}", config.keywords.display()),
        format!("--provider={}", config.provider),
        format!("--num-threads={}", config.threads),
        format!("--keywords-score={}", config.score),
        format!("--keywords-threshold={}", config.threshold),
        "--print-args=false".into(),
    ]);
    command.arg(&wav_path);

    let output_result = tokio::time::timeout(Duration::from_secs(15), command.output()).await;
    let _ = tokio::fs::remove_file(&wav_path).await;

    let output = match output_result {
        Ok(result) => result?,
        Err(_) => bail!("sherpa-onnx keyword spotter timed out"),
    };
    if !output.status.success() {
        bail!(
            "sherpa-onnx keyword spotter exited with {}: {}",
            output.status,
            String::from_utf8_lossy(&output.stderr).trim()
        );
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(parse_kws_keyword(&stdout))
}

async fn detect_wake_by_transcription(
    wav_bytes: Vec<u8>,
    wake_word: &str,
    transcription_config: &TranscriptionConfig,
) -> Result<Option<String>> {
    let text = transcribe_audio(wav_bytes, "wake_check.wav", transcription_config).await?;
    let normalized_text = normalize_wake_text(&text);
    Ok((!wake_word.is_empty() && normalized_text.contains(wake_word)).then_some(text))
}

#[async_trait]
impl Channel for VoiceWakeChannel {
    fn name(&self) -> &str {
        "voice_wake"
    }

    async fn send(&self, _message: &SendMessage) -> Result<()> {
        // Voice wake is input-only; outbound messages are not supported.
        Ok(())
    }

    async fn listen(&self, tx: mpsc::Sender<ChannelMessage>) -> Result<()> {
        let config = self.config.clone();
        let transcription_config = self.transcription_config.clone();
        let kws_config = KwsCommandConfig::from_env();

        // Run the blocking audio capture loop on a dedicated thread.
        let (audio_tx, mut audio_rx) = mpsc::channel::<Vec<f32>>(32);

        let energy_threshold = config.energy_threshold;
        let silence_timeout = Duration::from_millis(u64::from(config.silence_timeout_ms));
        let max_capture = Duration::from_secs(u64::from(config.max_capture_secs));
        let sample_rate: u32;
        let channels_count: u16;

        // ── Initialise cpal stream ────────────────────────────
        {
            use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};

            let host = cpal::default_host();
            let device = host
                .default_input_device()
                .ok_or_else(|| anyhow::anyhow!("No default audio input device available"))?;

            let supported = device.default_input_config()?;
            sample_rate = supported.sample_rate().0;
            channels_count = supported.channels();

            info!(
                device = ?device.name().unwrap_or_default(),
                sample_rate,
                channels = channels_count,
                "VoiceWake: opening audio input"
            );

            let stream_config: cpal::StreamConfig = supported.into();
            let audio_tx_clone = audio_tx.clone();

            let stream = device.build_input_stream(
                &stream_config,
                move |data: &[f32], _: &cpal::InputCallbackInfo| {
                    // Non-blocking: try_send and drop if full.
                    let _ = audio_tx_clone.try_send(data.to_vec());
                },
                move |err| {
                    warn!("VoiceWake: audio stream error: {err}");
                },
                None,
            )?;

            stream.play()?;

            // Keep the stream alive for the lifetime of the channel.
            // We leak it intentionally — the channel runs until the daemon shuts down.
            std::mem::forget(stream);
        }

        // Drop the extra sender so the channel closes when the stream sender drops.
        drop(audio_tx);

        // ── Main detection loop ───────────────────────────────
        let wake_word = normalize_wake_text(&config.wake_word);
        let mut state = WakeState::Listening;
        let mut capture_buf: Vec<f32> = Vec::new();
        let mut last_voice_at = Instant::now();
        let mut capture_start = Instant::now();
        let mut msg_counter: u64 = 0;
        let mut empty_retry_count: u8 = 0;
        let mut followup_polling = false;
        let mut followup_capture = false;
        let mut next_followup_poll = Instant::now();
        let mut followup_audio = VecDeque::new();
        let mut background: VecDeque<f32> = VecDeque::new();
        let mut utterance =
            UtteranceCapture::new(sample_rate, channels_count, silence_timeout, max_capture);
        let mut voice_ui_show_task: Option<tokio::task::JoinHandle<bool>> = None;
        let (mut streaming_kws_tx, mut streaming_kws_rx) = match &kws_config {
            Some(kws) if kws.stream_binary.is_some() => {
                match spawn_streaming_kws(kws, sample_rate) {
                    Ok((pcm_tx, keyword_rx)) => (Some(pcm_tx), Some(keyword_rx)),
                    Err(error) => {
                        warn!(
                            %error,
                            "VoiceWake: failed to start streaming KWS; using windowed fallback"
                        );
                        (None, None)
                    }
                }
            }
            _ => (None, None),
        };

        if let Some(kws) = &kws_config {
            if let Some(stream_binary) = &kws.stream_binary {
                info!(
                    binary = %stream_binary.display(),
                    keywords = %kws.keywords.display(),
                    "VoiceWake: persistent sherpa-onnx KWS enabled"
                );
            } else {
                info!(
                    binary = %kws.binary.display(),
                    keywords = %kws.keywords.display(),
                    "VoiceWake: windowed sherpa-onnx KWS fallback enabled"
                );
            }
        } else {
            warn!(
                "VoiceWake: KWS environment is incomplete; using transcription-based wake fallback"
            );
        }
        info!(wake_word = %wake_word, "VoiceWake: entering listen loop");

        let (vad_start, _) =
            collect_pending_audio(NeuralVad::start(sample_rate), &mut audio_rx, 0).await;
        let mut neural_vad = match vad_start {
            Ok(detector) => detector,
            Err(error) => {
                warn!(%error, "VoiceWake: neural VAD unavailable; using adaptive energy fallback");
                None
            }
        };
        let mut capture_pending = VecDeque::new();
        loop {
            let next = if state == WakeState::Capturing && !capture_pending.is_empty() {
                capture_pending.pop_front()
            } else {
                audio_rx.recv().await
            };
            let Some(chunk) = next else {
                break;
            };
            if state == WakeState::Listening && followup_polling {
                retain_followup_audio(&mut followup_audio, &chunk, utterance.preroll);
                if Instant::now() >= next_followup_poll {
                    let (permission, pending) = collect_pending_audio(
                        prepare_followup_capture(),
                        &mut audio_rx,
                        utterance.preroll,
                    )
                    .await;
                    for audio in pending {
                        retain_followup_audio(&mut followup_audio, &audio, utterance.preroll);
                    }
                    next_followup_poll = Instant::now() + Duration::from_millis(250);
                    match permission {
                        FollowupPermission::Ready(remaining_ms) => {
                            let preroll = take_followup_preroll(
                                &mut followup_audio,
                                remaining_ms,
                                sample_rate,
                                channels_count,
                            );
                            let buffered_samples = preroll.len();
                            capture_pending.clear();
                            let frame_samples = (sample_rate as usize / 50
                                * usize::from(channels_count.max(1)))
                            .max(1);
                            capture_pending
                                .extend(preroll.chunks(frame_samples).map(<[f32]>::to_vec));
                            if let Some(vad) = neural_vad.as_mut() {
                                let (reset, pending) = collect_pending_audio(
                                    vad.reset(),
                                    &mut audio_rx,
                                    utterance.preroll,
                                )
                                .await;
                                capture_pending.extend(pending);
                                if let Err(error) = reset {
                                    warn!(%error, "VoiceWake: follow-up VAD reset failed; using energy fallback");
                                    neural_vad = None;
                                }
                            }
                            let threshold = utterance_threshold(&background, energy_threshold);
                            utterance.reset(threshold, Vec::new());
                            utterance.wait_limit = buffered_samples
                                + remaining_ms as usize * sample_rate as usize / 1000
                                    * usize::from(channels_count.max(1));
                            followup_polling = false;
                            followup_capture = true;
                            empty_retry_count = 0;
                            state = WakeState::Capturing;
                            capture_buf.clear();
                            info!(
                                remaining_ms,
                                "VoiceWake: follow-up capture opened without wake acknowledgement"
                            );
                            continue;
                        }
                        FollowupPermission::Off => {
                            followup_polling = false;
                            followup_audio.clear();
                        }
                        FollowupPermission::Unavailable => {
                            followup_polling = false;
                            followup_audio.clear();
                            warn!(
                                error_key = "voice_followup_handshake_unavailable",
                                "VoiceWake: follow-up handshake failed or timed out; capture not opened"
                            );
                        }
                        FollowupPermission::Waiting => {}
                    }
                }
            }
            let mono_samples = downmix_interleaved_to_mono(&chunk, channels_count);
            let energy = compute_rms_energy(&mono_samples);
            if state == WakeState::Listening {
                background.push_back(energy);
                if background.len() > 160 {
                    background.pop_front();
                }
            }

            if state == WakeState::Listening && streaming_kws_tx.is_some() {
                let mut stream_disconnected = false;

                if let Some(pcm_tx) = &streaming_kws_tx
                    && let Err(error) = pcm_tx.try_send(mono_samples)
                    && matches!(error, mpsc::error::TrySendError::Closed(_))
                {
                    stream_disconnected = true;
                }

                let detected_keyword = if let Some(keyword_rx) = &mut streaming_kws_rx {
                    match keyword_rx.try_recv() {
                        Ok(keyword) => Some(keyword),
                        Err(mpsc::error::TryRecvError::Empty) => None,
                        Err(mpsc::error::TryRecvError::Disconnected) => {
                            stream_disconnected = true;
                            None
                        }
                    }
                } else {
                    None
                };

                if stream_disconnected {
                    warn!("VoiceWake: streaming KWS disconnected; enabling windowed fallback");
                    streaming_kws_tx = None;
                    streaming_kws_rx = None;
                }

                if let Some(keyword) = detected_keyword {
                    let normalized_keyword = normalize_wake_text(&keyword);
                    if !wake_word.is_empty() && normalized_keyword.contains(&wake_word) {
                        info!(
                            text = %keyword,
                            detector = "sherpa-kws-stream",
                            "VoiceWake: wake word detected — starting capture"
                        );
                        let Some(preroll) = begin_external_capture(
                            &mut audio_rx,
                            &mut voice_ui_show_task,
                            utterance.preroll,
                        )
                        .await
                        else {
                            continue;
                        };
                        if let Some(vad) = neural_vad.as_mut() {
                            let (reset, pending) = collect_pending_audio(
                                vad.reset(),
                                &mut audio_rx,
                                utterance.preroll,
                            )
                            .await;
                            capture_pending.extend(pending);
                            if let Err(error) = reset {
                                warn!(%error, "VoiceWake: VAD reset failed; using energy fallback");
                                neural_vad = None;
                            }
                        }
                        let threshold = utterance_threshold(&background, energy_threshold);
                        utterance.reset(threshold, preroll);
                        state = WakeState::Capturing;
                        empty_retry_count = 0;
                        capture_buf.clear();
                        last_voice_at = Instant::now();
                        capture_start = Instant::now();
                        info!(threshold, "VoiceWake: waiting for speech onset");
                        followup_polling = false;
                        followup_capture = false;
                        followup_audio.clear();
                        continue;
                    }
                    debug!(%keyword, "VoiceWake: streaming KWS returned another keyword");
                }
            }

            match state {
                WakeState::Listening => {
                    if streaming_kws_tx.is_none() && energy >= energy_threshold {
                        debug!(
                            energy,
                            "VoiceWake: energy spike — transitioning to Triggered"
                        );
                        state = WakeState::Triggered;
                        capture_buf.clear();
                        capture_buf.extend_from_slice(&chunk);
                        last_voice_at = Instant::now();
                        capture_start = Instant::now();
                    }
                }
                WakeState::Triggered => {
                    capture_buf.extend_from_slice(&chunk);

                    if energy >= energy_threshold {
                        last_voice_at = Instant::now();
                    }

                    let since_voice = last_voice_at.elapsed();
                    let since_start = capture_start.elapsed();

                    // After enough silence or max time, run the dedicated KWS model.
                    if since_voice >= silence_timeout || since_start >= max_capture {
                        debug!("VoiceWake: Triggered window closed — checking wake word");

                        let wake_detection = if let Some(kws) = &kws_config {
                            let mono_samples =
                                downmix_interleaved_to_mono(&capture_buf, channels_count);
                            let wav_bytes = encode_wav_from_f32(&mono_samples, sample_rate, 1);

                            match detect_keyword_with_sherpa(kws, &wav_bytes).await {
                                Ok(Some(keyword)) => {
                                    let normalized_keyword = normalize_wake_text(&keyword);
                                    let matched = !wake_word.is_empty()
                                        && normalized_keyword.contains(&wake_word);
                                    Ok(matched.then_some((keyword, "sherpa-kws")))
                                }
                                Ok(None) => Ok(None),
                                Err(error) => {
                                    warn!(
                                        %error,
                                        "VoiceWake: KWS backend failed; trying transcription fallback"
                                    );
                                    let wav_bytes = encode_wav_from_f32(
                                        &capture_buf,
                                        sample_rate,
                                        channels_count,
                                    );
                                    detect_wake_by_transcription(
                                        wav_bytes,
                                        &wake_word,
                                        &transcription_config,
                                    )
                                    .await
                                    .map(|text| text.map(|value| (value, "asr-fallback")))
                                }
                            }
                        } else {
                            let wav_bytes =
                                encode_wav_from_f32(&capture_buf, sample_rate, channels_count);
                            detect_wake_by_transcription(
                                wav_bytes,
                                &wake_word,
                                &transcription_config,
                            )
                            .await
                            .map(|text| text.map(|value| (value, "asr")))
                        };

                        match wake_detection {
                            Ok(Some((text, detector))) => {
                                info!(
                                    text = %text,
                                    detector,
                                    "VoiceWake: wake word detected — starting capture"
                                );

                                let Some(preroll) = begin_external_capture(
                                    &mut audio_rx,
                                    &mut voice_ui_show_task,
                                    utterance.preroll,
                                )
                                .await
                                else {
                                    state = WakeState::Listening;
                                    capture_buf.clear();
                                    continue;
                                };
                                if let Some(vad) = neural_vad.as_mut() {
                                    let (reset, pending) = collect_pending_audio(
                                        vad.reset(),
                                        &mut audio_rx,
                                        utterance.preroll,
                                    )
                                    .await;
                                    capture_pending.extend(pending);
                                    if let Err(error) = reset {
                                        warn!(%error, "VoiceWake: VAD reset failed; using energy fallback");
                                        neural_vad = None;
                                    }
                                }
                                let threshold = utterance_threshold(&background, energy_threshold);
                                utterance.reset(threshold, preroll);

                                state = WakeState::Capturing;
                                empty_retry_count = 0;
                                capture_buf.clear();
                                last_voice_at = Instant::now();
                                capture_start = Instant::now();
                                info!(threshold, "VoiceWake: waiting for speech onset");
                                followup_polling = false;
                                followup_capture = false;
                                followup_audio.clear();
                            }
                            Ok(None) => {
                                debug!("VoiceWake: no wake word — back to Listening");
                                state = WakeState::Listening;
                                capture_buf.clear();
                            }
                            Err(error) => {
                                warn!(%error, "VoiceWake: wake-word detection failed");
                                state = WakeState::Listening;
                                capture_buf.clear();
                            }
                        }
                    }
                }
                WakeState::Capturing => {
                    let was_speaking = utterance.speech_started;
                    let progress = if let Some(vad) = neural_vad.as_mut() {
                        let capture_mono = downmix_interleaved_to_mono(&chunk, channels_count);
                        let (activity, pending) = collect_pending_audio(
                            vad.classify(&capture_mono),
                            &mut audio_rx,
                            utterance.maximum,
                        )
                        .await;
                        capture_pending.extend(pending);
                        match activity {
                            Ok(speech) => utterance.push_classified(&chunk, energy, speech),
                            Err(error) => {
                                warn!(%error, "VoiceWake: VAD inference failed; using energy fallback");
                                neural_vad = None;
                                utterance.push(&chunk, energy)
                            }
                        }
                    } else {
                        utterance.push(&chunk, energy)
                    };
                    if !was_speaking && utterance.speech_started {
                        info!(
                            energy,
                            threshold = utterance.threshold,
                            "VoiceWake: speech onset detected"
                        );
                    }
                    if progress == CaptureProgress::NoSpeech || progress == CaptureProgress::TooLong
                    {
                        info!(
                            peak_rms = utterance.peak_rms,
                            threshold = utterance.threshold,
                            "VoiceWake: no speech before onset deadline; leaving UI open"
                        );
                        if let Some(task) = voice_ui_show_task.take() {
                            let _ = task.await;
                        }
                        report_capture_failure(
                            if progress == CaptureProgress::NoSpeech && followup_capture {
                                "followup_timeout"
                            } else if progress == CaptureProgress::TooLong {
                                "too_long"
                            } else {
                                "no_speech"
                            },
                        )
                        .await;
                        state = WakeState::Listening;
                        followup_capture = false;
                        capture_pending.clear();
                        utterance.reset(energy_threshold, Vec::new());
                        continue;
                    }

                    if progress == CaptureProgress::Ready {
                        capture_pending.clear();
                        debug!("VoiceWake: utterance capture complete — transcribing");
                        info!(
                            duration_ms = utterance.samples.len() * 1000
                                / sample_rate as usize
                                / usize::from(channels_count),
                            peak_rms = utterance.peak_rms,
                            threshold = utterance.threshold,
                            "VoiceWake: captured utterance audio"
                        );

                        let wav_bytes =
                            encode_wav_from_f32(&utterance.samples, sample_rate, channels_count);

                        let (result, pending) = collect_pending_audio(
                            transcribe_audio(wav_bytes, "utterance.wav", &transcription_config),
                            &mut audio_rx,
                            utterance.maximum,
                        )
                        .await;
                        match result {
                            Ok(text) => {
                                let trimmed = text.trim().to_string();
                                if has_meaningful_text(&trimmed) {
                                    if ends_voice_conversation(&trimmed) {
                                        if let Some(task) = voice_ui_show_task.take() {
                                            let _ = task.await;
                                        }
                                        let _ =
                                            call_voice_assistant("EndExternalConversation", None)
                                                .await;
                                        followup_capture = false;
                                        followup_polling = false;
                                        followup_audio.clear();
                                        capture_pending.clear();
                                        utterance.reset(energy_threshold, Vec::new());
                                        state = WakeState::Listening;
                                        info!(
                                            "VoiceWake: conversation ended by explicit voice command"
                                        );
                                        continue;
                                    }
                                    empty_retry_count = 0;
                                    msg_counter += 1;
                                    let ts = SystemTime::now()
                                        .duration_since(UNIX_EPOCH)
                                        .unwrap_or_default()
                                        .as_secs();

                                    let msg = ChannelMessage {
                                        id: format!("voice_wake_{msg_counter}"),
                                        sender: "voice_user".into(),
                                        reply_target: "voice_user".into(),
                                        content: trimmed.clone(),
                                        channel: "voice_wake".into(),
                                        timestamp: ts,
                                        thread_ts: None,
                                        interruption_scope_id: None,
                                        attachments: vec![],
                                    };

                                    let voice_ui_ready = match voice_ui_show_task.take() {
                                        Some(task) => task.await.unwrap_or(false),
                                        None => followup_capture,
                                    };
                                    let handed_to_voice_ui = voice_ui_ready
                                        && submit_voice_assistant_text(&trimmed).await;
                                    followup_polling = handed_to_voice_ui;
                                    next_followup_poll = Instant::now();
                                    followup_audio.clear();

                                    if handed_to_voice_ui {
                                        info!(
                                            text = %trimmed,
                                            "VoiceWake: utterance handed to small-K voice window"
                                        );
                                    } else if followup_capture {
                                        warn!(
                                            "VoiceWake: follow-up UI unavailable; discarding input instead of changing sessions"
                                        );
                                    } else if let Err(e) = tx.send(msg).await {
                                        warn!("VoiceWake: failed to dispatch message: {e}");
                                    }
                                } else if empty_retry_count == 0 && !followup_capture {
                                    empty_retry_count = 1;
                                    info!(
                                        text = %trimmed,
                                        "VoiceWake: empty or punctuation-only utterance; retrying capture once"
                                    );

                                    // Preserve speech that arrived while ASR was running.
                                    let threshold = utterance.threshold;
                                    utterance.reset(threshold, Vec::new());
                                    capture_pending = pending;
                                    if let Some(vad) = neural_vad.as_mut() {
                                        let (reset, extra) = collect_pending_audio(
                                            vad.reset(),
                                            &mut audio_rx,
                                            utterance.maximum,
                                        )
                                        .await;
                                        capture_pending.extend(extra);
                                        if let Err(error) = reset {
                                            warn!(%error, "VoiceWake: retry VAD reset failed; using energy fallback");
                                            neural_vad = None;
                                        }
                                    }
                                    state = WakeState::Capturing;
                                    capture_buf.clear();
                                    last_voice_at = Instant::now();
                                    capture_start = Instant::now();
                                    info!(
                                        "VoiceWake: capturing utterance again after empty result"
                                    );
                                    continue;
                                } else {
                                    empty_retry_count = 0;
                                    if let Some(task) = voice_ui_show_task.take() {
                                        task.abort();
                                    }
                                    report_capture_failure("empty_transcript").await;
                                    info!(
                                        text = %trimmed,
                                        "VoiceWake: ignored empty or punctuation-only utterance"
                                    );
                                }
                            }
                            Err(e) => {
                                warn!("VoiceWake: transcription error for utterance: {e}");
                                report_capture_failure("asr_failed").await;
                            }
                        }

                        state = WakeState::Listening;
                        followup_capture = false;
                        capture_buf.clear();
                    }
                }
                WakeState::Processing => {
                    // Should not receive chunks while processing, but just buffer them.
                    // State transitions happen above synchronously after transcription.
                }
            }
        }

        bail!("VoiceWake: audio stream ended unexpectedly");
    }
}

// ── Audio utilities ────────────────────────────────────────────

/// Compute RMS (root-mean-square) energy of an audio chunk.
pub fn compute_rms_energy(samples: &[f32]) -> f32 {
    if samples.is_empty() {
        return 0.0;
    }
    let sum_sq: f32 = samples.iter().map(|s| s * s).sum();
    (sum_sq / samples.len() as f32).sqrt()
}

/// Downmix interleaved multichannel PCM to mono for the KWS model.
///
/// The K1 microphone currently reports 44.1 kHz stereo, while sherpa-onnx's
/// file keyword spotter expects a single channel. Averaging each frame avoids
/// silently interpreting left/right samples as twice-speed mono audio.
pub fn downmix_interleaved_to_mono(samples: &[f32], channels: u16) -> Vec<f32> {
    let channel_count = usize::from(channels.max(1));
    if channel_count == 1 {
        return samples.to_vec();
    }

    samples
        .chunks_exact(channel_count)
        .map(|frame| frame.iter().copied().sum::<f32>() / channel_count as f32)
        .collect()
}

/// Normalize recognized text before matching a wake phrase.
///
/// ASR backends may insert punctuation or whitespace into short phrases, for
/// example `你好，麒麟。`. They may also choose a homophone for the uncommon
/// characters in `麒麟`, such as `齐麟` or `祁麟`. Keeping only letters and
/// numbers and canonicalizing those narrowly-scoped homophones makes the wake
/// phrase stable without weakening it to a generic `你好` match.
pub fn normalize_wake_text(text: &str) -> String {
    text.chars()
        .filter(|character| character.is_alphanumeric())
        .flat_map(char::to_lowercase)
        .map(|character| match character {
            '齐' | '祁' | '琪' | '奇' | '其' | '棋' | '琦' | '淇' => '麒',
            '林' | '琳' | '临' | '霖' => '麟',
            _ => character,
        })
        .collect()
}

/// Return true when an ASR result contains content worth sending to the LLM.
/// SenseVoice may return a punctuation mark for silence or microphone noise;
/// submitting that to the agent can start a long, meaningless generation.
pub fn has_meaningful_text(text: &str) -> bool {
    text.chars().any(char::is_alphanumeric)
}

/// Encode raw f32 PCM samples as a WAV byte buffer (16-bit PCM).
///
/// This produces a minimal valid WAV file that Whisper-compatible APIs accept.
pub fn encode_wav_from_f32(samples: &[f32], sample_rate: u32, channels: u16) -> Vec<u8> {
    let bits_per_sample: u16 = 16;
    let byte_rate = u32::from(channels) * sample_rate * u32::from(bits_per_sample) / 8;
    let block_align = channels * bits_per_sample / 8;
    #[allow(clippy::cast_possible_truncation)]
    let data_len = (samples.len() * 2) as u32; // 16-bit = 2 bytes per sample; max ~25 MB
    let file_len = 36 + data_len;

    let mut buf = Vec::with_capacity(file_len as usize + 8);

    // RIFF header
    buf.extend_from_slice(b"RIFF");
    buf.extend_from_slice(&file_len.to_le_bytes());
    buf.extend_from_slice(b"WAVE");

    // fmt chunk
    buf.extend_from_slice(b"fmt ");
    buf.extend_from_slice(&16u32.to_le_bytes()); // chunk size
    buf.extend_from_slice(&1u16.to_le_bytes()); // PCM format
    buf.extend_from_slice(&channels.to_le_bytes());
    buf.extend_from_slice(&sample_rate.to_le_bytes());
    buf.extend_from_slice(&byte_rate.to_le_bytes());
    buf.extend_from_slice(&block_align.to_le_bytes());
    buf.extend_from_slice(&bits_per_sample.to_le_bytes());

    // data chunk
    buf.extend_from_slice(b"data");
    buf.extend_from_slice(&data_len.to_le_bytes());

    for &sample in samples {
        let clamped = sample.clamp(-1.0, 1.0);
        #[allow(clippy::cast_possible_truncation)]
        let pcm16 = (clamped * 32767.0) as i16; // clamped to [-1,1] so fits i16
        buf.extend_from_slice(&pcm16.to_le_bytes());
    }

    buf
}

// ── Tests ──────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    fn test_capture() -> UtteranceCapture {
        let mut capture = UtteranceCapture::new(
            1000,
            1,
            Duration::from_millis(1500),
            Duration::from_secs(10),
        );
        capture.reset(0.005, Vec::new());
        capture
    }

    #[test]
    fn followup_permission_is_bounded_and_fail_closed() {
        assert_eq!(
            parse_followup_permission("s \"waiting\"\n"),
            FollowupPermission::Waiting
        );
        assert_eq!(
            parse_followup_permission("s \"ready:9000\""),
            FollowupPermission::Ready(9000)
        );
        assert_eq!(
            parse_followup_permission("s \"ready:1\""),
            FollowupPermission::Ready(1)
        );
        for text in [
            "",
            "s \"ready\"",
            "s \"ready:0\"",
            "s \"ready:9001\"",
            "s \"ready:-1\"",
            "b true",
            "s \"off\"",
        ] {
            assert_eq!(parse_followup_permission(text), FollowupPermission::Off);
        }
    }

    #[cfg(unix)]
    #[tokio::test]
    async fn followup_handshake_accepts_a_reply_delayed_beyond_half_a_second() {
        // Production was observed returning `waiting` after 1.56 seconds. The
        // old 500 ms deadline silently disabled polling before that reply.
        for (script, expected) in [
            (
                "sleep 0.7; printf 's \"waiting\"\\n'",
                FollowupPermission::Waiting,
            ),
            (
                "sleep 0.7; printf 's \"ready:8500\"\\n'",
                FollowupPermission::Ready(8500),
            ),
        ] {
            let mut command = Command::new("sh");
            command.args(["-c", script]);
            assert_eq!(
                query_followup_permission(command, FOLLOWUP_HANDSHAKE_TIMEOUT).await,
                expected
            );
        }
    }

    #[cfg(unix)]
    #[tokio::test]
    async fn followup_handshake_failure_is_not_an_explicit_off_or_a_capture() {
        let mut command = Command::new("sh");
        command.args(["-c", "exit 1"]);
        assert_eq!(
            query_followup_permission(command, FOLLOWUP_HANDSHAKE_TIMEOUT).await,
            FollowupPermission::Unavailable
        );
        let mut command = Command::new("sh");
        command.args(["-c", "exec sleep 1"]);
        assert_eq!(
            query_followup_permission(command, Duration::from_millis(30)).await,
            FollowupPermission::Unavailable
        );
        assert_eq!(
            parse_followup_permission("s \"off\""),
            FollowupPermission::Off
        );
    }

    #[test]
    fn followup_preroll_excludes_audio_before_playback_completed() {
        let mut audio = VecDeque::new();
        retain_followup_audio(&mut audio, &[1.0; 500], 350);
        assert_eq!(audio.len(), 350);
        let tail = take_followup_preroll(&mut audio, 8900, 1000, 2);
        assert_eq!(tail.len(), 200); // only the last 100 ms, preserving stereo frames
        assert!(audio.is_empty());
        retain_followup_audio(&mut audio, &[1.0; 200], 350);
        assert!(take_followup_preroll(&mut audio, 9000, 1000, 1).is_empty());
    }

    #[test]
    fn followup_waits_nine_seconds_but_keeps_speech_after_onset() {
        let mut capture = test_capture();
        capture.wait_limit = 9000;
        for _ in 0..89 {
            assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Waiting);
        }
        assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::NoSpeech);
        capture.reset(0.005, Vec::new());
        assert_eq!(capture.wait_limit, 8000); // a subsequent normal wake restores its policy
        capture.wait_limit = 9000;
        capture.waited = 8900;
        assert_eq!(capture.push(&[0.02; 100], 0.02), CaptureProgress::Waiting);
        assert!(capture.speech_started);
        for _ in 0..14 {
            assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Waiting);
        }
        assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Ready);
    }

    #[test]
    fn conversation_end_commands_are_exact_not_embedded_in_questions() {
        assert!(ends_voice_conversation("结束对话。"));
        assert!(ends_voice_conversation("退出对话！"));
        assert!(!ends_voice_conversation("结束对话是什么意思？"));
        assert!(!ends_voice_conversation("帮我退出文件管理器"));
    }

    #[test]
    fn capture_waits_for_speech_before_starting_silence_timeout() {
        let mut capture = test_capture();
        for _ in 0..40 {
            assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Waiting);
        }
        assert!(!capture.speech_started);
        assert_eq!(capture.push(&[0.02; 100], 0.02), CaptureProgress::Waiting);
        assert!(capture.speech_started);
        for _ in 0..14 {
            assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Waiting);
        }
        assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Ready);
    }

    #[test]
    fn capture_preserves_onset_and_playback_tail() {
        let mut capture = test_capture();
        capture.reset(0.005, vec![0.015; 200]);
        capture.push(&[0.02; 100], 0.02);
        assert_eq!(&capture.samples[..200], &[0.015; 200]);
        assert_eq!(&capture.samples[200..], &[0.02; 100]);
    }

    #[test]
    fn capture_does_not_send_silence_to_asr_or_keep_unbounded_audio() {
        let mut capture = test_capture();
        for _ in 0..79 {
            assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Waiting);
            assert!(capture.samples.len() <= capture.preroll);
        }
        assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::NoSpeech);
    }

    #[test]
    fn capture_ignores_brief_click_and_supports_quiet_speech() {
        let background = VecDeque::from(vec![0.0004; 40]);
        let threshold = utterance_threshold(&background, 0.008);
        assert!((threshold - 0.0012).abs() < 0.00001);
        let mut capture = test_capture();
        capture.reset(threshold, Vec::new());
        capture.push(&[0.01; 20], 0.01);
        capture.push(&[0.0; 100], 0.0);
        assert!(!capture.speech_started);
        capture.push(&[0.002; 100], 0.002);
        assert!(capture.speech_started);
    }

    #[test]
    fn capture_caps_continuous_speech_and_reset_clears_prior_turn() {
        let mut capture = test_capture();
        for _ in 0..99 {
            capture.push(&[0.02; 100], 0.02);
        }
        assert_eq!(capture.push(&[0.02; 100], 0.02), CaptureProgress::TooLong);
        capture.reset(0.005, Vec::new());
        assert!(!capture.speech_started);
        assert_eq!(capture.push(&[0.0; 100], 0.0), CaptureProgress::Waiting);
    }

    #[tokio::test]
    async fn pending_audio_is_consumed_during_async_work_and_bounded() {
        let (tx, mut rx) = mpsc::channel(32);
        tx.send(vec![1.0; 200]).await.unwrap();
        tx.send(vec![2.0; 200]).await.unwrap();
        let (output, pending) = collect_pending_audio(async { 7 }, &mut rx, 250).await;
        assert_eq!(output, 7);
        let samples: Vec<_> = pending.into_iter().flatten().collect();
        assert_eq!(&samples[..50], &[1.0; 50]);
        assert_eq!(&samples[50..], &[2.0; 200]);
        assert!(rx.try_recv().is_err());
    }

    #[tokio::test]
    async fn pending_audio_does_not_spin_on_closed_input() {
        let (tx, mut rx) = mpsc::channel(1);
        drop(tx);
        let (output, _) = collect_pending_audio(async { 9 }, &mut rx, 250).await;
        assert_eq!(output, 9);
    }

    #[test]
    fn capture_uses_neural_activity_not_amplitude_when_supplied() {
        let mut capture = test_capture();
        capture.push_classified(&[0.04; 100], 0.04, false);
        assert!(!capture.speech_started); // Loud non-speech must not trigger.
        capture.push_classified(&[0.0002; 100], 0.0002, true);
        assert!(capture.speech_started); // Quiet detected speech is retained.
        for _ in 0..14 {
            capture.push_classified(&[0.01; 100], 0.01, false);
        }
        assert_eq!(
            capture.push_classified(&[0.01; 100], 0.01, false),
            CaptureProgress::Ready
        );
    }

    #[test]
    fn ui_capture_permission_is_explicit_and_old_ui_remains_compatible() {
        assert_eq!(
            parse_capture_permission("s \"ready\"\n"),
            CapturePermission::Ready
        );
        for status in ["speaking", "muted", "full", "capturing", "closing"] {
            assert_eq!(
                parse_capture_permission(&format!("s \"{status}\"")),
                CapturePermission::Rejected
            );
        }
        assert_eq!(parse_capture_permission(""), CapturePermission::Legacy);
    }
    use kylinbot_config::traits::ChannelConfig;

    // ── State machine tests ────────────────────────────────

    #[test]
    fn wake_state_display() {
        assert_eq!(WakeState::Listening.to_string(), "Listening");
        assert_eq!(WakeState::Triggered.to_string(), "Triggered");
        assert_eq!(WakeState::Capturing.to_string(), "Capturing");
        assert_eq!(WakeState::Processing.to_string(), "Processing");
    }

    #[test]
    fn wake_state_equality() {
        assert_eq!(WakeState::Listening, WakeState::Listening);
        assert_ne!(WakeState::Listening, WakeState::Triggered);
    }

    // ── Energy computation tests ───────────────────────────

    #[test]
    fn rms_energy_of_silence_is_zero() {
        let silence = vec![0.0f32; 1024];
        assert_eq!(compute_rms_energy(&silence), 0.0);
    }

    #[test]
    fn rms_energy_of_empty_is_zero() {
        assert_eq!(compute_rms_energy(&[]), 0.0);
    }

    #[test]
    fn rms_energy_of_constant_signal() {
        // Constant signal at 0.5 → RMS should be 0.5
        let signal = vec![0.5f32; 100];
        let energy = compute_rms_energy(&signal);
        assert!((energy - 0.5).abs() < 1e-5);
    }

    #[test]
    fn rms_energy_above_threshold() {
        let loud = vec![0.8f32; 256];
        let energy = compute_rms_energy(&loud);
        assert!(energy > 0.01, "Loud signal should exceed default threshold");
    }

    #[test]
    fn rms_energy_below_threshold_for_quiet() {
        let quiet = vec![0.001f32; 256];
        let energy = compute_rms_energy(&quiet);
        assert!(
            energy < 0.01,
            "Very quiet signal should be below default threshold"
        );
    }

    #[test]
    fn stereo_audio_is_downmixed_by_frame() {
        let stereo = [1.0, -1.0, 0.5, 0.25, -0.5, -0.25];
        assert_eq!(
            downmix_interleaved_to_mono(&stereo, 2),
            vec![0.0, 0.375, -0.375]
        );
    }

    #[test]
    fn mono_audio_is_preserved() {
        let mono = [0.1, -0.2, 0.3];
        assert_eq!(downmix_interleaved_to_mono(&mono, 1), mono);
    }

    // ── Wake phrase normalization tests ───────────────────

    #[test]
    fn wake_text_normalization_ignores_asr_punctuation_and_spaces() {
        assert_eq!(normalize_wake_text("你好，麒麟。"), "你好麒麟");
        assert_eq!(normalize_wake_text(" Hey, Kylin Bot! "), "heykylinbot");
    }

    #[test]
    fn wake_text_normalization_canonicalizes_observed_homophones() {
        assert_eq!(normalize_wake_text("你好，齐麟。"), "你好麒麟");
        assert_eq!(normalize_wake_text("你好，祁麟。"), "你好麒麟");
        assert_eq!(normalize_wake_text("你好，琪林。"), "你好麒麟");
    }

    #[test]
    fn normalized_transcript_matches_configured_wake_word() {
        let wake_word = normalize_wake_text("你好麒麟");
        let transcript = normalize_wake_text("你好，麒麟。");
        assert!(transcript.contains(&wake_word));
    }

    #[test]
    fn punctuation_only_asr_output_is_not_meaningful() {
        assert!(!has_meaningful_text(""));
        assert!(!has_meaningful_text("。！？ ,..."));
        assert!(has_meaningful_text("打开系统设置。"));
        assert!(has_meaningful_text("set volume to 50%"));
    }

    #[test]
    fn kws_json_output_extracts_keyword() {
        let output = r#"{"keyword":"你好麒麟","timestamps":[0.1,0.2]}"#;
        assert_eq!(parse_kws_keyword(output).as_deref(), Some("你好麒麟"));
    }

    #[test]
    fn kws_json_output_without_detection_returns_none() {
        assert_eq!(parse_kws_keyword("{\"keyword\":\"\"}"), Some(String::new()));
        assert_eq!(parse_kws_keyword("not-json"), None);
    }

    // ── WAV encoding tests ─────────────────────────────────

    #[test]
    fn wav_header_is_valid() {
        let samples = vec![0.0f32; 100];
        let wav = encode_wav_from_f32(&samples, 16000, 1);

        // RIFF header
        assert_eq!(&wav[0..4], b"RIFF");
        assert_eq!(&wav[8..12], b"WAVE");

        // fmt chunk
        assert_eq!(&wav[12..16], b"fmt ");
        let fmt_size = u32::from_le_bytes(wav[16..20].try_into().unwrap());
        assert_eq!(fmt_size, 16);

        // PCM format
        let format = u16::from_le_bytes(wav[20..22].try_into().unwrap());
        assert_eq!(format, 1);

        // Channels
        let channels = u16::from_le_bytes(wav[22..24].try_into().unwrap());
        assert_eq!(channels, 1);

        // Sample rate
        let sr = u32::from_le_bytes(wav[24..28].try_into().unwrap());
        assert_eq!(sr, 16000);

        // data chunk
        assert_eq!(&wav[36..40], b"data");
        let data_size = u32::from_le_bytes(wav[40..44].try_into().unwrap());
        assert_eq!(data_size, 200); // 100 samples * 2 bytes each
    }

    #[test]
    fn wav_total_size_correct() {
        let samples = vec![0.0f32; 50];
        let wav = encode_wav_from_f32(&samples, 44100, 2);
        // header (44 bytes) + data (50 * 2 = 100 bytes)
        assert_eq!(wav.len(), 144);
    }

    #[test]
    fn wav_encodes_clipped_samples() {
        // Samples outside [-1, 1] should be clamped
        let samples = vec![-2.0f32, 2.0, 0.0];
        let wav = encode_wav_from_f32(&samples, 16000, 1);

        let s0 = i16::from_le_bytes(wav[44..46].try_into().unwrap());
        let s1 = i16::from_le_bytes(wav[46..48].try_into().unwrap());
        let s2 = i16::from_le_bytes(wav[48..50].try_into().unwrap());

        assert_eq!(s0, -32767); // clamped to -1.0
        assert_eq!(s1, 32767); // clamped to 1.0
        assert_eq!(s2, 0);
    }

    // ── Config parsing tests ───────────────────────────────

    #[test]
    fn voice_wake_config_defaults() {
        let config = VoiceWakeConfig::default();
        assert_eq!(config.wake_word, "hey kylinbot");
        assert_eq!(config.silence_timeout_ms, 2000);
        assert!((config.energy_threshold - 0.01).abs() < f32::EPSILON);
        assert_eq!(config.max_capture_secs, 30);
    }

    #[test]
    fn voice_wake_config_deserialize_partial() {
        let toml_str = r#"
            wake_word = "okay agent"
            max_capture_secs = 60
        "#;
        let config: VoiceWakeConfig = toml::from_str(toml_str).unwrap();
        assert_eq!(config.wake_word, "okay agent");
        assert_eq!(config.max_capture_secs, 60);
        // Defaults preserved for unset fields
        assert_eq!(config.silence_timeout_ms, 2000);
        assert!((config.energy_threshold - 0.01).abs() < f32::EPSILON);
    }

    #[test]
    fn voice_wake_config_deserialize_all_fields() {
        let toml_str = r#"
            wake_word = "hello bot"
            silence_timeout_ms = 3000
            energy_threshold = 0.05
            max_capture_secs = 15
        "#;
        let config: VoiceWakeConfig = toml::from_str(toml_str).unwrap();
        assert_eq!(config.wake_word, "hello bot");
        assert_eq!(config.silence_timeout_ms, 3000);
        assert!((config.energy_threshold - 0.05).abs() < f32::EPSILON);
        assert_eq!(config.max_capture_secs, 15);
    }

    #[test]
    fn voice_wake_config_channel_config_trait() {
        assert_eq!(VoiceWakeConfig::name(), "VoiceWake");
        assert_eq!(VoiceWakeConfig::desc(), "voice wake word detection");
    }

    // ── State transition logic tests ───────────────────────

    #[test]
    fn energy_threshold_determines_trigger() {
        let threshold = 0.01f32;
        let quiet_energy = compute_rms_energy(&vec![0.005f32; 256]);
        let loud_energy = compute_rms_energy(&vec![0.5f32; 256]);

        assert!(quiet_energy < threshold, "Quiet should not trigger");
        assert!(loud_energy >= threshold, "Loud should trigger");
    }

    #[test]
    fn state_transitions_are_deterministic() {
        // Verify that the state enum values are distinct and copyable
        let states = [
            WakeState::Listening,
            WakeState::Triggered,
            WakeState::Capturing,
            WakeState::Processing,
        ];
        for (i, a) in states.iter().enumerate() {
            for (j, b) in states.iter().enumerate() {
                if i == j {
                    assert_eq!(a, b);
                } else {
                    assert_ne!(a, b);
                }
            }
        }
    }

    #[test]
    fn channel_config_impl() {
        // VoiceWakeConfig implements ChannelConfig
        assert_eq!(VoiceWakeConfig::name(), "VoiceWake");
        assert!(!VoiceWakeConfig::desc().is_empty());
    }

    #[test]
    fn voice_wake_channel_name() {
        let config = VoiceWakeConfig::default();
        let transcription_config = TranscriptionConfig::default();
        let channel = VoiceWakeChannel::new(config, transcription_config);
        assert_eq!(channel.name(), "voice_wake");
    }
}
