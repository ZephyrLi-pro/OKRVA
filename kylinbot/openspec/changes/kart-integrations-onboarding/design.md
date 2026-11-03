## 背景

KylinBot 当前已集成多个麒麟本地 AI SDK（统称为 "Kart"）：
- **Kart NLP**：本地 LLM 推理（feature `kart-nlp`），SDK 提供 `get_model_list()` API
- **Kart Speech**：TTS/STT（feature `kart-speech`），SDK **无** list_models API，模型名为手动字符串
- **Kart Vision**：文生图（feature `kart-vision`），SDK **无模型概念**，通过 style/尺寸配置
- **Kart CoreAI Vision**：OCR 和主体分割（feature `kart-coreai-vision`），SDK 内嵌模型，无需配置

当前各 Kart 能力的配置已分散在不同的 config 节：
- TTS：`[tts.kart]`（model_name、deploy_type）
- STT：`[transcription.kart]`（model_name、deploy_type）
- 文生图：`[image-gen-kart]`（style、width、height）

但初始化向导中没有暴露这些配置，用户只能手动编辑配置文件。

## 目标/非目标

**目标**：
- 在 Providers 环节选择列表中加入 Kart
- 选中 Kart 后，一站式配置所有能力
- NLP 模型通过 SDK 的 `get_model_list()` 动态获取供用户选择
- TTS/STT 模型通过手动输入或留空使用 SDK 默认值，加 deploy_type 选择
- 文生图通过启用/禁用开关控制
- 纯工具（OCR、主体分割）自动启用，不暴露
- 保持向后兼容

**非目标**：
- 不新增独立的 Kart section（所有配置在 Provider 环节内完成）
- 不为 Kart 能力要求 API key
- 不为语音/视觉 SDK 实现模型列表发现（SDK 不支持）

## 设计决策

### 1. 全部在 Provider 环节内完成

用户选择 Kart provider 后的流程：

```
1. [跳过 API key]
2. NLP 模型选择  ← SDK get_model_list() 动态列表
3. TTS 配置      ← 启用？→ deploy_type → 模型名（手动输入/留空）
4. STT 配置      ← 启用？→ deploy_type → 模型名（手动输入/留空）
5. 文生图配置    ← 启用？
```

理由：选一次 Kart，一个地方配完所有能力。

### 2. 各能力的模型配置方式

根据各 SDK 的实际能力采取不同策略：

| 能力 | SDK 能力 | onboarding 方式 |
|------|---------|----------------|
| NLP | `get_model_list()` 返回模型列表 | select 菜单动态列出模型 |
| TTS | 无 list API，model_name 字符串 | deploy_type 选择 + 模型名手动输入（留空=默认） |
| STT | 无 list API，model_name 字符串 | 同 TTS |
| 文生图 | 无模型概念 | 启用/禁用（style/尺寸使用 SDK 默认值） |
| OCR | SDK 内嵌 | 自动启用，不配置 |
| 主体分割 | SDK 内嵌 | 自动启用，不配置 |

### 3. 配置写入位置

Kart 的各能力配置写入**现有的**配置节，不新增顶级 `[kart]` 节：
- NLP：写入 `providers.models.kart.model`（与其他 provider 一致，通过 `Config::set_prop` 写入）
- TTS：
  - 父级 `[tts]`：`default_provider = "kart"`（channel 层已内置 PCM→opus 转码，无需额外配置格式）
  - 子级 `[tts.kart]`：model_name、deploy_type（默认 `on_device`）
- STT：
  - 父级 `[transcription]`：`enabled = true`、`default_provider = "kart"`
  - 子级 `[transcription.kart]`：model_name、deploy_type（默认 `on_device`）、sample_rate（默认 16000）
- 文生图：启用时写入 `[image-gen-kart]`（使用 SDK 默认值，不在向导中暴露 style/尺寸配置）

理由：复用现有 schema，不引入新的顶级配置结构。

> **转码说明**：Kart TTS 仅支持 PCM 格式输出（16kHz 16-bit mono），各 channel（如 Lark、QQ）已在发送端通过 `is_opus()` 字节检测自动触发 ffmpeg PCM→opus 转码，无需通过配置项控制。

### 4. Feature 开关

- `#[cfg(feature = "kart-nlp")]`：Kart 出现在 provider 列表中
- `#[cfg(feature = "kart-speech")]`：显示 TTS/STT 配置子步骤
- `#[cfg(feature = "kart-vision")]`：显示文生图配置子步骤

如果只启用了 `kart-nlp`，则只出现 NLP 模型选择，不出现语音/视觉配置。

> **已知限制**：`kart-nlp` 是 Kart 在 provider 列表中出现的唯一前提。仅启用 `kart-speech` 或 `kart-vision` 而未启用 `kart-nlp` 的场景下，用户无法通过 onboarding 配置这些能力，仍需手动编辑配置文件。

### 5. deploy_type 选项

TTS 和 STT 共用 deploy_type 选择，选项为：
- `on_device`（端侧部署）
- `public_cloud`（公有云）
- `private_cloud`（私有云）

## 风险/权衡

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 运行时未安装 SDK | `get_model_list()` 失败，用户无法选择模型 | 优雅降级：catch 错误后回退到手动输入，并展示安装 SDK 的提示信息 |
| 用户不知道填什么模型名（TTS/STT） | 配置困难 | 留空使用 SDK 默认值；输入框 placeholder 提示"留空使用默认模型" |
| Provider 环节变长 | 用户体验 | 每个子能力有启用/禁用开关（默认：否），可快速跳过 |
