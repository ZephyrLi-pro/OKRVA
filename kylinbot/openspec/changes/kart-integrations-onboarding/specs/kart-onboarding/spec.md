## 概述

在 Provider 环节中集成 Kart，选中后一站式配置所有模型类能力。各能力根据 SDK 实际能力采取不同配置方式。

## 需求

### 需求：Provider 列表中的 Kart
当 `kart-nlp` feature 启用时，provider 选择列表必须包含 "Kart" 选项。

#### 场景：Kart 出现在 provider 列表中
- **给定** `kart-nlp` feature 已启用
- **当** 用户运行 `kylinbot onboard` 并到达 provider 选择
- **那么** "Kart" 在列表中可见，带有 `local: true` 标记

#### 场景：没有 feature 时 Kart 不可见
- **给定** `kart-nlp` feature 未启用
- **当** 用户运行 `kylinbot onboard`
- **那么** "Kart" 不出现在 provider 列表中

### 需求：跳过 API Key
当选择 Kart 作为 provider 时，必须跳过 API key 提示。

#### 场景：Kart 无 API key
- **给定** 用户选择 "Kart" 作为 provider
- **当** 向导到达认证步骤
- **那么** 跳过 API key 提示，直接进入模型选择

### 需求：NLP 模型选择（动态列表）
选中 Kart 后，必须通过 SDK 的 `get_model_list()` 动态获取模型列表供用户选择。

#### 场景：展示 NLP 模型列表
- **给定** 用户已选择 "Kart" 作为 provider
- **当** 向导到达模型选择步骤
- **那么** 调用 `kart_nlp_sys::get_model_list()` 获取可用模型
- **并且** 以 select 菜单展示模型列表（显示 display_name）

#### 场景：SDK 不可用时的回退
- **给定** NLP SDK 未安装或 `get_model_list()` 失败
- **当** 向导到达模型选择步骤
- **那么** 回退到手动输入模型名

### 需求：TTS 配置
选中 Kart 后，如果 `kart-speech` feature 启用，必须让用户配置 TTS。

#### 场景：TTS 配置流程
- **给定** 用户已选择 "Kart" 且 `kart-speech` feature 已启用
- **当** NLP 模型选择完成后
- **那么** 向导依次提示：
  1. "启用 TTS（文本转语音）？"（默认：否）
  2. 如果启用，选择 deploy_type（on_device / public_cloud / private_cloud，默认：on_device）
  3. 输入 TTS 模型名（placeholder 提示"留空使用默认模型"）
- **并且** 写入父级配置 `[tts]`：`default_provider = "kart"`
- **并且** 写入子级配置 `[tts.kart]`（model_name、deploy_type）

#### 场景：speech feature 未启用时跳过
- **给定** `kart-speech` feature 未启用
- **那么** 跳过 TTS 和 STT 配置

### 需求：STT 配置
选中 Kart 后，如果 `kart-speech` feature 启用，必须让用户配置 STT。

#### 场景：STT 配置流程
- **给定** 用户已选择 "Kart" 且 `kart-speech` feature 已启用
- **当** TTS 配置完成后
- **那么** 向导依次提示：
  1. "启用 STT（语音转文本）？"（默认：否）
  2. 如果启用，选择 deploy_type（on_device / public_cloud / private_cloud，默认：on_device）
  3. 输入 STT 模型名（placeholder 提示"留空使用默认模型"）
- **并且** 写入父级配置 `[transcription]`：`enabled = true`、`default_provider = "kart"`
- **并且** 写入子级配置 `[transcription.kart]`（model_name、deploy_type、sample_rate=16000）

### 需求：文生图配置
选中 Kart 后，如果 `kart-vision` feature 启用，必须让用户配置文生图。

#### 场景：文生图配置流程
- **给定** 用户已选择 "Kart" 且 `kart-vision` feature 已启用
- **当** 语音配置完成后（或跳过后）
- **那么** 向导提示："启用文生图能力？"（默认：否）
- **并且** 启用时写入 `[image-gen-kart]`（style/尺寸使用 SDK 默认值，不在向导中配置）

#### 场景：vision feature 未启用时跳过
- **给定** `kart-vision` feature 未启用
- **那么** 跳过文生图配置

### 需求：纯工具自动启用
OCR 和主体分割在 SDK 可用时自动启用，无需配置。

#### 场景：OCR 工具自动可用
- **给定** `kart-coreai-vision` feature 已启用且 SDK 已安装
- **当** agent 启动
- **那么** `ocr` 工具无需显式配置即可使用

#### 场景：主体分割工具自动可用
- **给定** `kart-coreai-vision` feature 已启用且 SDK 已安装
- **当** agent 启动
- **那么** `subject_segmentation` 工具无需显式配置即可使用

### 需求：向后兼容
现有配置必须继续正常工作。

#### 场景：旧配置加载
- **给定** 没有 Kart 相关配置的配置文件
- **当** agent 加载它
- **那么** agent 正常启动，Kart 各能力使用默认值
