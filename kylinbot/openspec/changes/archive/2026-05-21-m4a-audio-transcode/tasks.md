## 1. 音频转码模块

- [x] 1.1 在 `crates/kylinbot-channels/src/` 中新增 `audio_transcode.rs` 模块，定义 `transcode_to_wav(input_data: &[u8], input_format: &str) -> Result<Vec<u8>>` 函数签名
- [x] 1.2 实现 ffmpeg 可用性检测：惰性检测 + `AtomicBool` 缓存，`ensure_ffmpeg_available() -> Result<()>`
- [x] 1.3 实现转码核心逻辑：将输入音频写入 `tempfile::NamedTempFile` → 构造 `Command::new("ffmpeg")` 参数数组（不经 shell）→ 读取输出 WAV → RAII 析构清理临时文件
- [x] 1.4 为 ffmpeg 子进程添加 30 秒超时，超时后 kill 子进程
- [x] 1.5 在 `mod.rs` 中注册新模块

## 2. 集成到 Kart STT Provider

- [x] 2.1 修改 `KartTranscriptionProvider::transcribe()` 方法，在现有 WAV/PCM 分支后增加 else 分支，调用 `transcode_to_wav()` 进行转码
- [x] 2.2 转码失败时返回明确错误信息：区分 ffmpeg 不可用、转码失败、超时三种情况
- [x] 2.3 确保转码后走现有 `extract_pcm_from_wav()` 路径复用 PCM 提取逻辑
- [x] 2.4 在转码关键路径添加 tracing 日志（转码开始、格式、耗时、结果）

## 3. 依赖与配置

- [x] 3.1 在 `crates/kylinbot-channels/Cargo.toml` 中添加 `tempfile` 依赖
- [x] 3.2 确认 `which` crate 已在依赖中（用于 ffmpeg 路径检测），如缺失则添加
- [x] 3.3 确认 `tracing` 已在依赖中，如缺失则添加

## 4. 测试

- [ ] 4.1 为 `transcode_to_wav()` 编写单元测试（mock ffmpeg 调用）
- [ ] 4.2 为 `ensure_ffmpeg_available()` 编写单元测试（含缓存行为验证）
- [ ] 4.3 为超时机制编写测试（模拟长时间运行的 ffmpeg）
- [ ] 4.4 为 Kart provider 的转码分支编写集成测试（使用真实 m4a 样本文件）