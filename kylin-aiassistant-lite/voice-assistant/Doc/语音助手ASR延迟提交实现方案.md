# 语音助手 ASR 延迟提交实现方案

## 一、需求背景

当前 `voice-assistant` 使用 `kylin-ai coreai speech` SDK 的连续识别能力。SDK 回调 `SPEECH_RECOGNIZED`
只能说明 SDK 判断“当前识别片段”已经结束，不能直接等同于用户完整意图已经说完。

用户可能在短暂停顿后继续补充下一句，如果应用在每次 SDK final 后立即提交，就会把一次完整提问拆成多轮请求。

本次改动目标：

- SDK final 回调后不立即发送请求。
- final 后等待 1.5 秒。
- 1.5 秒内没有新的语音输入，再把累计文本发送给 Gateway。
- 1.5 秒内如果出现新的 partial/final，则取消或重置等待，继续累计本轮用户输入。

## 二、改动前逻辑

原流程：

```
SpeechRecognitionController::recognitionCallback()
  → reason == SPEECH_RECOGNIZED
  → emit finalText(text)
  → MainWindow::handleFinalText(text)
  → submitFinalText(text)
  → pauseRecognitionForPlayback()
  → GatewayChatClient::sendMessage(payload)
```

问题：

- 应用层直接信任 SDK final，未区分“片段结束”和“用户完整问题结束”。
- 连续说话中间出现短暂停顿时，可能过早提交。

## 三、设计方案

### 3.1 增加 final 后延迟提交窗口

在 `MainWindow` 中新增 single-shot timer：

```cpp
const int kAsrFinalSubmitDelayMs = 1500;
QTimer *m_asrFinalSubmitTimer = nullptr;
QString m_pendingFinalText;
```

收到 SDK final 后：

```
handleFinalText(text)
  → 合并到 m_pendingFinalText
  → 更新用户气泡显示
  → 启动 1500ms timer
```

timer 超时后：

```
submitPendingRecognitionText()
  → 停止 ASR
  → 丢弃旧 session 迟到回调
  → submitFinalText(累计文本)
```

### 3.2 1.5 秒内有新语音输入

收到 partial 时：

```
handlePartialText(text)
  → 如果 timer 正在等待，先 stop timer
  → m_lastPartialText = text
  → 用户气泡显示 m_pendingFinalText + text
```

收到下一次 final 时：

```
handleFinalText(text)
  → 将新 final 合并到 m_pendingFinalText
  → 清空 m_lastPartialText
  → 重新启动 1500ms timer
```

这样用户连续说：

```
“帮我查一下天气” [SDK final]
短暂停顿 1 秒
“明天北京的” [partial/final]
```

不会在第一句 final 后立即发送，而是累计成同一轮输入。

### 3.3 final 文本合并去重

不同 SDK 版本可能有两类 final 行为：

| SDK 行为 | 处理方式 |
| --- | --- |
| 每次 final 只返回新增片段 | 直接追加到 `m_pendingFinalText` |
| 后续 final 返回累计文本 | 用新的累计文本替换 `m_pendingFinalText` |
| 重复返回同一 final 片段 | 忽略重复片段 |

关键规则：

```cpp
if (m_pendingFinalText.trimmed().isEmpty()) {
    m_pendingFinalText = text;
} else if (text == m_pendingFinalText || m_pendingFinalText.endsWith(text)) {
    // 重复片段，保留一份
} else if (text.startsWith(m_pendingFinalText)) {
    // SDK 返回累计文本，直接替换
    m_pendingFinalText = text;
} else {
    // SDK 返回新增片段，追加
    m_pendingFinalText += text;
}
```

## 四、状态清理策略

为了避免旧 final 被延迟 timer 误发，以下场景会取消待提交状态：

| 场景 | 处理 |
| --- | --- |
| `stopRecognition()` | `cancelPendingFinalSubmit()` |
| `stopAudioForHide()` | `cancelPendingFinalSubmit()` |
| `handleRecognitionError()` | `cancelPendingFinalSubmit()` |
| 无 pending/partial 时点击静音 | `cancelPendingFinalSubmit()` |

`cancelPendingFinalSubmit()` 负责：

```cpp
m_asrFinalSubmitTimer->stop();
m_pendingFinalText.clear();
```

## 五、静音交互

静音按钮仍保持“用户正在说话时立即提交当前输入”的语义。

本次改动后，静音时提交内容从单纯 `m_lastPartialText` 扩展为：

```
m_pendingFinalText + m_lastPartialText
```

也就是说：

- 已有 SDK final 但还在 1.5 秒等待窗口内：会提交 pending final。
- final 后又出现 partial：会提交 pending final + 当前 partial。
- 没有任何有效 pending/partial：只停止 ASR，不发送请求。

## 六、文件改动详情

### 6.1 mainwindow.h

新增方法：

- `pendingRecognitionTextWith()`
- `cancelPendingFinalSubmit()`
- `schedulePendingFinalSubmit()`
- `submitPendingRecognitionText()`

新增成员：

- `QTimer *m_asrFinalSubmitTimer`
- `QString m_pendingFinalText`

### 6.2 mainwindow.cpp

核心改动：

1. 新增 `kAsrFinalSubmitDelayMs = 1500`。
2. 构造函数初始化 `m_asrFinalSubmitTimer`，timeout 后调用 `submitPendingRecognitionText()`。
3. `handleFinalText()` 从“立即提交”改为“缓存 final + 启动 1.5 秒 timer”。
4. `handlePartialText()` 在新语音到来时停止 timer，并显示累计文本。
5. 新增 pending 文本合并、取消、调度、提交函数。
6. `stopRecognition()`、`stopAudioForHide()`、`handleRecognitionError()` 清理 pending 状态。
7. 静音分支改为提交 `m_pendingFinalText + m_lastPartialText`。

## 七、验证

构建验证：

```bash
make -C build -j2
```

结果：编译通过。

## 八、当前行为总结

最终提交条件从：

```
SDK 返回 SPEECH_RECOGNIZED
```

调整为：

```
SDK 返回 SPEECH_RECOGNIZED
  且后续 1.5 秒内没有新的 partial/final 输入
```

应用层仍不直接做音频能量/VAD 判断，而是基于 SDK 的 partial/final 回调做一层会话级防抖合并。
