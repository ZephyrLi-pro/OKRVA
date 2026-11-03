use kart_speech_sys::{KylinSpeechSynthesizer, is_available};
use std::fs::File;
use std::io::Write;

fn main() {
    eprintln!("=== TTS Debug Test ===");

    eprintln!("\n1. Calling is_available()...");
    let avail = is_available();
    eprintln!("   is_available() = {}", avail);

    eprintln!("\n2. Creating synthesizer...");
    let result = KylinSpeechSynthesizer::new("讯飞-语音大模型", "public_cloud");
    match result {
        Ok(mut synth) => {
            eprintln!("   SUCCESS!");

            eprintln!("\n3. Calling synthesize...");
            let rt = tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
                .unwrap();

            match rt.block_on(synth.synthesize("你好，请问有什么可以帮助你的吗？今天天气很好，我们出去走走吧。")) {
                Ok(output) => {
                    eprintln!("   SUCCESS: audio_bytes={}, sample_rate={}, channels={}",
                        output.audio_data.len(), output.sample_rate, output.channels);

                    // Save to WAV file
                    let path = "/home/kylin/Kylinproject/agent/kylinbot/tts_output.wav";
                    if let Err(e) = write_wav(path, &output.audio_data, output.sample_rate, output.channels) {
                        eprintln!("   FAILED to save WAV: {}", e);
                    } else {
                        eprintln!("   Saved to {}", path);
                    }
                }
                Err(e) => {
                    eprintln!("   FAILED: {}", e);
                }
            }
        }
        Err(e) => {
            eprintln!("   FAILED: {}", e);
        }
    }

    eprintln!("\n=== Test Complete ===");
}

fn write_wav(path: &str, pcm_data: &[u8], sample_rate: u32, channels: u16) -> std::io::Result<()> {
    let mut file = File::create(path)?;

    let bits_per_sample = 16u16;
    let byte_rate = sample_rate as u32 * channels as u32 * bits_per_sample as u32 / 8;
    let block_align = channels * bits_per_sample / 8;
    let data_size = pcm_data.len() as u32;

    // RIFF header
    file.write_all(b"RIFF")?;
    file.write_all(&(36 + data_size).to_le_bytes())?; // file size - 8
    file.write_all(b"WAVE")?;

    // fmt chunk
    file.write_all(b"fmt ")?;
    file.write_all(&16u32.to_le_bytes())?; // chunk size
    file.write_all(&1u16.to_le_bytes())?;  // PCM format
    file.write_all(&channels.to_le_bytes())?;
    file.write_all(&sample_rate.to_le_bytes())?;
    file.write_all(&byte_rate.to_le_bytes())?;
    file.write_all(&block_align.to_le_bytes())?;
    file.write_all(&bits_per_sample.to_le_bytes())?;

    // data chunk
    file.write_all(b"data")?;
    file.write_all(&data_size.to_le_bytes())?;
    file.write_all(pcm_data)?;

    Ok(())
}
