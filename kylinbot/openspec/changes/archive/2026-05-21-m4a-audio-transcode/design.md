## Context

KylinBot 的语音转写流程中，云服务端 STT（Groq、OpenAI Whisper 等）原生支持 m4a/ogg/mp3 等格式，但端侧 Kart Speech SDK 的 `KylinSpeechRecognizer::recognize()` 仅接受原始 PCM 数据。飞书渠道下载的语音消息几乎全是 m4a（AAC 编码），直接传给 Kart 会报格式不支持错误。

当前代码路径：
```
飞书下载音频 → TranscriptionManager::transcribe() → KartTranscriptionProvider::transcribe()
                                                    → 检查扩展名 → 非WAV/PCM → 报错
```

## Goals / Non-Goals

**Goals:**
- 将 m4a、ogg、mp3、aac、opus、flac 等常见音频格式转为 16kHz 16bit 单声道 PCM
- 转码逻辑对上游调用方透明，无需改动飞书渠道代码
- 转码仅在格式不被当前 provider 支持时触发，避免不必要的转换开销

**Non-Goals:**
- 不做云服务端 provider 的格式转换（它们已原生支持多种格式）
- 不做实时流式转码（当前场景是文件级离线转写）
- 不改变 Kart SDK 本身的 FFI 接口

## Decisions

### 1. 转码方案：调用系统 ffmpeg 子进程 vs 纯 Rust 解码库

**选择：调用系统 ffmpeg 子进程**

理由：
- ffmpeg 是 Kylin OS 预装的系统工具，无需额外安装
- 支持几乎所有音频格式，远超纯 Rust 库覆盖范围
- 纯 Rust 方案（如 symphonia）对 m4a/AAC 支持有限且不稳定
- ffmpeg 子进程方式代码量极小，维护成本低

替代方案：
- symphonia：纯 Rust，但 AAC 解码器不成熟，m4a 容器解析有已知问题
- rodio + minimp3：仅覆盖 mp3/wav/ogg，不支持 m4a
- 内嵌 ffmpeg C 库 via FFI：复杂度高，无必要

### 2. 转码插入位置：KartTranscriptionProvider 内部

**选择：在 KartTranscriptionProvider::transcribe() 中，格式检查失败后调用转码**

理由：
- 转码是 Kart provider 的特有需求，不应污染通用转录管道
- 其他 provider（云服务端）不需要转码，放在通用层会引入不必要的依赖
- 保持最小改动原则

### 3. 转码输出格式：WAV（内含 16kHz 16bit 单声道 PCM）

**选择：输出为 WAV 格式而非裸 PCM**

理由：
- WAV 带有采样率等元信息，与现有 `extract_pcm_from_wav()` 路径复用
- 避免硬编码采样率参数传递
- ffmpeg 输出 WAV 比 PCM 更标准

### 4. ffmpeg 可用性检测策略：惰性检测 + 缓存

**选择：首次转码时检测 ffmpeg，结果缓存在 `AtomicBool` 中，后续直接读缓存**

理由：
- 启动时无条件检测会拖慢不使用 Kart STT 的用户
- `AtomicBool` 无锁，并发读取安全
- 如果 ffmpeg 之前不可用但后来安装了，需要重启服务才能生效——可接受，因为 ffmpeg 是系统级依赖

### 5. 子进程安全性

- **路径安全**：使用 `tempfile::NamedTempFile` 生成随机路径，避免文件名注入
- **超时**：为 ffmpeg 子进程设置 30 秒超时，防止卡死。语音消息通常在 60 秒以内，转码耗时远小于此
- **并发**：每次转码创建独立的临时文件（`tempfile` 保证唯一性），多个并发请求互不干扰
- **命令构造**：使用 `std::process::Command` 的参数数组形式，不经 shell，杜绝命令注入

## Risks / Trade-offs

- **[系统依赖]** ffmpeg 未安装时转码会失败 → 惰性检测 + 缓存，不可用时返回明确错误信息并提示安装 ffmpeg
- **[性能]** ffmpeg 子进程启动有约 50ms 开销 → 对于秒级语音消息可接受；后续如需优化可改用 ffmpeg 子进程池或 FFI
- **[临时文件]** 需要存储转码中间产物 → 使用 `tempfile` crate 创建临时文件，RAII 析构时自动清理，即使 panic 也能清理
- **[超时]** 极端情况下 ffmpeg 可能卡住 → 30 秒超时 + kill 子进程
