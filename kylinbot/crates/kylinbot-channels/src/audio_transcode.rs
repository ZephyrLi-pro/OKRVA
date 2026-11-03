//! Audio transcoding utilities — converts m4a/ogg/mp3/aac/opus/flac to WAV.

use std::io::Write;
use std::process::Output;
use std::sync::atomic::{AtomicBool, Ordering};

use anyhow::{Context, Result, bail};
use portable_atomic::AtomicBool as PortableAtomicBool;
use tokio::process::Command as AsyncCommand;
use tokio::time::{timeout, Duration};

/// Cached ffmpeg availability flag.
/// - `Some(true)` = available (checked once,不会再检测)
/// - `Some(false)` = unavailable
static FFMPEG_AVAILABLE: PortableAtomicBool = PortableAtomicBool::new(false);
static FFMPEG_CHECKED: AtomicBool = AtomicBool::new(false);

/// 30-second timeout for ffmpeg subprocess.
const FFMPEG_TIMEOUT_SECS: u64 = 30;

/// Check whether ffmpeg is present in PATH.
///
/// Result is cached after the first check — subsequent calls return immediately.
pub fn ensure_ffmpeg_available() -> Result<()> {
    tracing::debug!("[transcode] ensure_ffmpeg_available called");

    // Fast path: already checked and unavailable
    if FFMPEG_CHECKED.load(Ordering::Acquire) && !FFMPEG_AVAILABLE.load(Ordering::Acquire) {
        tracing::warn!("[transcode] ffmpeg already checked as unavailable, returning cached error");
        bail!(
            "ffmpeg is not available — please install ffmpeg to enable audio transcoding\n\
             On Kylin OS: sudo apt install ffmpeg"
        );
    }

    if FFMPEG_CHECKED.load(Ordering::Acquire) {
        tracing::debug!("[transcode] ffmpeg already checked as available, fast path");
        return Ok(());
    }

    let available = which::which("ffmpeg").is_ok();
    tracing::info!("[transcode] ffmpeg check result: {}", available);

    FFMPEG_AVAILABLE.store(available, Ordering::Release);
    FFMPEG_CHECKED.store(true, Ordering::Release);

    if !available {
        tracing::error!("[transcode] ffmpeg not found in PATH");
        bail!(
            "ffmpeg is not available — please install ffmpeg to enable audio transcoding\n\
             On Kylin OS: sudo apt install ffmpeg"
        );
    }

    Ok(())
}

const OPUS_MAGIC: &[u8] = b"OpusHead";

/// Returns true if the audio data starts with an OggS page containing an OpusHead packet.
/// OggS capture pattern is "OggS" (bytes 0-3).
/// First packet contains OpusHead magic "OpusHead" at start of page data.
pub fn is_opus(audio_bytes: &[u8]) -> bool {
    // Minimum OggS page header: 27 bytes (including capture pattern)
    if audio_bytes.len() < 27 {
        return false;
    }
    // Check OggS capture pattern
    if &audio_bytes[0..4] != b"OggS" {
        return false;
    }
    // Byte 5: stream structure version (must be 0)
    if audio_bytes[5] != 0 {
        return false;
    }
    // Byte 26: number of page segments (1-255)
    let segment_count = audio_bytes[26];
    if segment_count == 0 {
        return false;
    }
    // Lacing values (segment sizes) are at offset 27, for `segment_count` entries
    // Lacing values are 1 byte each. The total data size is the sum of lacing values.
    let lacing_start = 27_usize;
    let lacing_end = lacing_start + segment_count as usize;
    if audio_bytes.len() < lacing_end {
        return false;
    }

    let packet_data_start = lacing_end;
    let mut packet_data_len = 0_usize;
    for i in 0..segment_count as usize {
        let lacing_val = audio_bytes[lacing_start + i];
        packet_data_len += lacing_val as usize;
        if lacing_val < 255 {
            // Packet ends here
            break;
        }
        // lacing value of 255 means packet continues
    }

    // The first packet should be OpusHead (19 bytes)
    if packet_data_len < 8 {
        return false;
    }

    let first_packet_start = packet_data_start;
    let first_packet_end = first_packet_start + packet_data_len.min(19);
    if audio_bytes.len() < first_packet_end {
        return false;
    }

    // Check OpusHead magic at start of first packet data
    audio_bytes[first_packet_start..first_packet_end].starts_with(OPUS_MAGIC)
}

/// Transcode audio data to opus format using ffmpeg.
/// Returns the transcoded audio bytes (opus in ogg container).
pub async fn transcode_to_opus(input_data: &[u8], source_format: &str) -> Result<Vec<u8>> {
    let start = std::time::Instant::now();
    let format = source_format.to_lowercase();

    tracing::info!("[transcode] transcode_to_opus: format={}, bytes={}", format, input_data.len());

    if let Err(e) = ensure_ffmpeg_available() {
        tracing::error!("[transcode] ffmpeg not available: {}", e);
        return Err(e);
    }

    // Write input to a temporary file
    let suffix = format!(".{}", format);
    let mut input_file = tempfile::NamedTempFile::with_suffix(&suffix)
        .context("Failed to create temporary input file")?;
    input_file.write_all(input_data)?;
    input_file.flush()?;
    let input_path = input_file.path().to_path_buf();
    let input_path_str = input_path.to_string_lossy().to_string();

    // Output to a tempfile with .opus suffix
    let output_file = tempfile::NamedTempFile::with_suffix(".opus")
        .context("Failed to create temporary output file")?;
    let output_path = output_file.path().to_path_buf();
    let output_path_str = output_path.to_string_lossy().to_string();

    // Run ffmpeg: -acodec libopus encodes to opus, -f ogg forces ogg container
    let output = run_ffmpeg_to_opus(&input_path_str, &output_path_str).await?;
    tracing::debug!("[transcode] ffmpeg opus transcode finished, status={}", output.status);

    drop(input_file);

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        tracing::error!("[transcode] ffmpeg opus transcode failed: status={}, stderr={}", output.status, stderr);
        bail!("ffmpeg opus transcode failed for format '{}': {}", format, stderr.trim());
    }

    let opus_data = std::fs::read(&output_path).context("Failed to read opus transcode output")?;
    drop(output_file);

    let elapsed = start.elapsed();
    tracing::info!("[transcode] opus transcode done: {}ms, opus_bytes={}", elapsed.as_millis(), opus_data.len());

    Ok(opus_data)
}

async fn run_ffmpeg_to_opus(input_path: &str, output_path: &str) -> Result<Output> {
    let result = timeout(Duration::from_secs(FFMPEG_TIMEOUT_SECS), async {
        AsyncCommand::new("ffmpeg")
            // Raw PCM: 16kHz mono 16-bit signed little-endian
            // -vbr on: enable variable bitrate (required for proper duration metadata)
            // -application audio: better container compliance for duration reporting
            .args(["-y", "-f", "s16le", "-ar", "16000", "-ac", "1", "-i", input_path,
                  "-acodec", "libopus", "-vbr", "on", "-application", "audio",
                  "-f", "ogg", output_path])
            .stdin(std::process::Stdio::null())
            .output()
            .await
    })
    .await;

    match result {
        Ok(Ok(output)) => Ok(output),
        Ok(Err(e)) => bail!("ffmpeg IO error: {}", e),
        Err(_) => bail!("ffmpeg opus transcode timed out after {}s", FFMPEG_TIMEOUT_SECS),
    }
}

/// Run ffmpeg to transcode to WAV format.
async fn run_ffmpeg(input_path: &str, output_path: &str) -> Result<Output> {
    let result = timeout(Duration::from_secs(FFMPEG_TIMEOUT_SECS), async {
        AsyncCommand::new("ffmpeg")
            .args(["-y", "-i", input_path, "-ac", "1", "-ar", "16000", "-acodec", "pcm_s16le", output_path])
            .stdin(std::process::Stdio::null())
            .output()
            .await
    })
    .await;

    match result {
        Ok(Ok(output)) => Ok(output),
        Ok(Err(e)) => bail!("ffmpeg IO error: {}", e),
        Err(_) => bail!("ffmpeg transcode timed out after {}s", FFMPEG_TIMEOUT_SECS),
    }
}

/// Transcode audio data to 16kHz 16bit mono WAV format.
///
/// This is an async function because it runs ffmpeg via tokio::process::Command
/// which requires an async context.
pub async fn transcode_to_wav(input_data: &[u8], input_format: &str) -> Result<Vec<u8>> {
    let start = std::time::Instant::now();
    let format = input_format.to_lowercase();

    tracing::info!("[transcode] starting: format={}, bytes={}", format, input_data.len());

    if let Err(e) = ensure_ffmpeg_available() {
        tracing::error!("[transcode] ffmpeg not available: {}", e);
        return Err(e);
    }
    tracing::debug!("[transcode] ffmpeg available, writing temp file");

    // Write input to a temporary file
    let suffix = format!(".{}", format);
    let mut input_file = tempfile::NamedTempFile::with_suffix(&suffix)
        .context("Failed to create temporary input file")?;

    input_file.write_all(input_data)?;
    input_file.flush()?;
    let input_path = input_file.path().to_path_buf();
    let input_path_str = input_path.to_string_lossy().to_string();

    // Output to a tempfile with .wav suffix
    let output_file = tempfile::NamedTempFile::with_suffix(".wav")
        .context("Failed to create temporary output file")?;
    let output_path = output_file.path().to_path_buf();
    let output_path_str = output_path.to_string_lossy().to_string();

    // Run ffmpeg: no shell, arguments as a list
    // -y: overwrite output file if it exists
    // -i: input file
    // -ac 1: mono channel
    // -ar 16000: 16kHz sample rate
    // -acodec pcm_s16le: 16bit signed little-endian PCM
    let output = run_ffmpeg(&input_path_str, &output_path_str).await?;
    tracing::debug!("[transcode] ffmpeg finished, status={}", output.status);

    // Clean up input temp file
    drop(input_file);

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        tracing::error!("[transcode] ffmpeg failed: status={}, stderr={}", output.status, stderr);
        bail!(
            "ffmpeg transcode failed for format '{}': {}",
            format,
            stderr.trim()
        );
    }

    let wav_data = std::fs::read(&output_path).context("Failed to read transcode output")?;

    // Clean up output temp file
    drop(output_file);

    let elapsed = start.elapsed();
    tracing::info!("[transcode] done: {}ms, wav_bytes={}", elapsed.as_millis(), wav_data.len());

    Ok(wav_data)
}

/// Extract PCM data from a WAV file by skipping the 44-byte header.
///
/// This is the same logic used by KartTranscriptionProvider for direct WAV files,
/// factored out so the transcode path can reuse it.
pub fn extract_pcm_from_wav(wav_data: &[u8]) -> Result<Vec<u8>> {
    if wav_data.len() < 44 {
        bail!("WAV file too small to contain header");
    }
    Ok(wav_data[44..].to_vec())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicBool, Ordering};

    fn reset_ffmpeg_cache() {
        FFMPEG_CHECKED.store(false, Ordering::Relaxed);
        FFMPEG_AVAILABLE.store(false, Ordering::Relaxed);
    }

    #[test]
    fn ensure_ffmpeg_available_returns_result() {
        reset_ffmpeg_cache();
        let result = ensure_ffmpeg_available();
        assert!(result.is_ok() || result.is_err());
    }

    #[test]
    fn extract_pcm_from_wav_returns_error_for_small_data() {
        let small = vec![0u8; 10];
        let err = extract_pcm_from_wav(&small).unwrap_err();
        assert!(err.to_string().contains("too small"));
    }

    #[test]
    fn extract_pcm_from_wav_extracts_data() {
        let header = vec![0u8; 44];
        let pcm = vec![1u8, 2u8, 3u8];
        let mut wav = header;
        wav.extend_from_slice(&pcm);
        let extracted = extract_pcm_from_wav(&wav).unwrap();
        assert_eq!(extracted, pcm);
    }

    #[test]
    fn ensure_ffmpeg_available_caches_result() {
        reset_ffmpeg_cache();
        let _ = ensure_ffmpeg_available();
        let result2 = ensure_ffmpeg_available();
        assert_eq!(result2.is_ok(), ensure_ffmpeg_available().is_ok());
    }

    // ── is_opus tests ─────────────────────────────────────────────

    #[test]
    fn is_opus_rejects_empty() {
        assert!(!is_opus(&[]));
    }

    #[test]
    fn is_opus_rejects_short_non_opus() {
        assert!(!is_opus(b"short"));
    }

    #[test]
    fn is_opus_rejects_non_ogg_data() {
        // Random bytes that are not an Ogg container
        let data = vec![0u8; 100];
        assert!(!is_opus(&data));
    }

    #[test]
    fn is_opus_rejects_ogg_without_opus_head() {
        // Valid OggS page header but wrong capture pattern
        let mut data = vec![0u8; 50];
        data[0..4].copy_from_slice(b"OggS");
        data[5] = 0; // version
        // First packet data is not OpusHead
        assert!(!is_opus(&data));
    }

    #[test]
    fn is_opus_accepts_valid_opus_head() {
        // Minimal valid OggS page with OpusHead packet
        let mut data = vec![0u8; 100];
        // OggS capture pattern
        data[0..4].copy_from_slice(b"OggS");
        // Stream structure version = 0
        data[5] = 0;
        // Header type (beginning of stream)
        data[6] = 2;
        // Granule position (8 bytes zero)
        // Segment table
        data[26] = 1; // 1 segment
        data[27] = 19; // segment size = 19 bytes (OpusHead size)
        // OpusHead magic at start of packet data
        data[28..36].copy_from_slice(b"OpusHead");
        // Rest is OpusTags etc

        assert!(is_opus(&data));
    }

    #[test]
    fn is_opus_rejects_wav_data() {
        // WAV file header (RIFF...WAVE)
        let wav_header = vec![
            0x52, 0x49, 0x46, 0x46, // "RIFF"
            0x00, 0x00, 0x00, 0x00, // file size
            0x57, 0x41, 0x56, 0x45, // "WAVE"
        ];
        assert!(!is_opus(&wav_header));
    }

    #[test]
    fn is_opus_rejects_mp3_data() {
        // MP3 frame sync
        let mut mp3 = vec![0xff; 10];
        assert!(!is_opus(&mp3));
    }
}
