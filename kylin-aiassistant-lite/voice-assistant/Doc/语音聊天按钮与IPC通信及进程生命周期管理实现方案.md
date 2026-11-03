# 语音聊天按钮与 IPC 通信及进程生命周期管理实现方案

## 一、需求来源

在 kylin-aiassistant 标题栏新增"语音聊天"按钮，实现与 voice-assistant 子工程的双向 IPC 通信及进程生命周期管理，具体需求如下：

| 需求编号 | 描述 |
|---------|------|
| 1 | 新增语音聊天按钮，点击后启动 voice-assistant 工程，打开语音通话窗口 |
| 2 | 两个工程间建立 IPC 通信机制，实现双向通信 |
| 3 | kylin-aiassistant 退出时（正常或意外），voice-assistant 进程跟随退出 |
| 4 | kylin-aiassistant 自启后，点击语音聊天按钮唤起 voice-assistant |
| 5 | closeButton 关闭窗口后，再次点击语音聊天按钮打开全新窗口 |
| 6 | pushButton_cutoff 关闭窗口后，再次点击语音聊天按钮恢复隐藏的窗口（后台进程保留） |

## 二、改动前现状分析

### 已实现

- `voiceChatButton` 按钮已在 TitleBar.qml 中添加，但 `onClicked` 为空
- `voice-chat.svg` 图标已注册到 `msgpane.qrc`
- `CMsgPane::startVoiceAssistantApp()` 通过 `QProcess::startDetached()` 启动 voice-assistant，但进程完全脱离，无 IPC 通信
- voice-assistant 使用 `kdk::QtSingleApplication` 保证单实例
- kylin-aiassistant 通过 `KADBusOperator` 注册 D-Bus 服务 `org.ukui.kylin_aiassistant`

### 未实现/有问题

1. **无 IPC 通信**：两个工程之间无任何通信通道，`startDetached` 后即失去联系
2. **无进程生命周期管理**：voice-assistant 作为 detached 进程，父进程退出后仍独立运行
3. **closeButton 无清理**：只调用 `close()`，不做 ASR/TTS/Gateway 清理，也不通知主进程
4. **pushButton_cutoff 直接 close()**：挂断后窗口关闭，进程退出，无法恢复隐藏窗口
5. **无窗口恢复机制**：主进程无法得知 voice-assistant 是否在运行，无法区分"恢复窗口"和"启动新实例"

## 三、设计方案

### 3.1 IPC 通信机制：QDBus

选择 QDBus 作为 IPC 通信机制，原因：
- 与 kylin-aiassistant 现有 `KADBusOperator`（`org.ukui.kylin_aiassistant`）架构一致
- 支持方法调用 + 信号，满足双向通信需求
- 可通过 `isServiceRegistered()` 检测对方进程是否存活

#### D-Bus 服务注册

| 项目 | 服务名 | 对象路径 | 接口名 |
|------|--------|---------|--------|
| voice-assistant | `org.ukui.voice_assistant` | `/org/ukui/voice_assistant` | `org.ukui.voice_assistant` |

#### D-Bus 方法与信号

**voice-assistant 暴露的方法（供 kylin-aiassistant 调用）：**

| 方法 | 说明 |
|------|------|
| `ShowWindow()` | 显示/激活隐藏的窗口，重新开始 ASR |

**voice-assistant 暴露的信号（供 kylin-aiassistant 监听）：**

| 信号 | 说明 |
|------|------|
| `ProcessExiting(int reason)` | voice-assistant 即将退出，`reason`: 0=closeButton 退出, 1=异常退出 |

### 3.2 进程生命周期管理：QDBusServiceWatcher

**核心思路**：voice-assistant 启动后通过 `QDBusServiceWatcher` 监控 `org.ukui.kylin_aiassistant` 服务名。当 kylin-aiassistant 退出后，该服务名被 D-Bus daemon 自动注销，voice-assistant 感知到后自动退出。

**优点**：无论 kylin-aiassistant 正常退出还是 crash，D-Bus 服务名都会被 bus daemon 注销，voice-assistant 均能感知到。

### 3.3 语音聊天按钮逻辑

```
点击 voiceChatButton
  → msgPane.startVoiceChat()
    → VADBusService::isRunning() 检查 D-Bus 服务是否已注册
      ├─ 已注册（进程在运行，窗口隐藏）→ 调用 ShowWindow() 恢复窗口
      └─ 未注册（进程未运行）→ QProcess::startDetached 启动新进程
```

### 3.4 closeButton 行为（彻底退出）

```
用户点击 closeButton
  → on_closeButton_clicked()
    → shutdownAndQuit()
      1. hide() 立即隐藏窗口
      2. 停止 ASR
      3. 关闭 TTS
      4. 取消 Gateway 响应
      5. stopRecognition() 重置语音状态
      6. 将内存中缓存的语音会话消息集中同步到 AI 助手历史数据库
      7. 同步完成后通过 D-Bus 发送 ProcessExiting(0) 信号
      8. qApp->quit() 退出进程
```

主进程收到 `ProcessExiting` 信号后，状态标记清除，下次点击按钮走启动新进程路径。

### 3.5 pushButton_cutoff 行为（挂断但保留进程）

```
用户点击 pushButton_cutoff
  → cutoffClicked 信号
    1. 停止 ASR
    2. 关闭 TTS
    3. 取消 Gateway 响应
    4. stopRecognition() 重置所有状态
    5. hide() 隐藏窗口（进程保留，D-Bus 服务保持注册）
```

下次点击语音聊天按钮时，主进程检测到 `org.ukui.voice_assistant` 仍在运行，调用 `ShowWindow()`：
```
ShowWindow()
  → onShowWindow()
    1. repositionNearScreenCorner()
    2. show() + raise() + activateWindow()
    3. startRecognition() 重新开始语音识别
```

## 四、文件改动详情

### 4.1 新增文件

#### voice-assistant/dbus/vadbusoperator.h

D-Bus 服务端头文件，声明 `VADBusOperator` 类：

```cpp
class VADBusOperator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ukui.voice_assistant")
public:
    enum ExitReason { CloseButton = 0, Abnormal = 1 };
signals:
    void ProcessExiting(int reason);
    void sigShowWindow();
public slots:
    void ShowWindow();
};
```

#### voice-assistant/dbus/vadbusoperator.cpp

D-Bus 服务端实现：
- 构造函数中注册 `org.ukui.voice_assistant` 服务和 `/org/ukui/voice_assistant` 对象
- `ShowWindow()` 发射 `sigShowWindow` 信号，由 MainWindow 的 `onShowWindow()` 响应

#### voice-assistant/data/org.ukui.voice_assistant.service

D-Bus 服务激活文件，允许 D-Bus 按需启动 voice-assistant：

```ini
[D-BUS Service]
Name=org.ukui.voice_assistant
Exec=/usr/bin/voice-assistant
```

#### kylin-aiassistant/dbus/vadbusservice.h

D-Bus 客户端头文件，声明 `VADBusService` 类：

```cpp
class VADBusService : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE bool isRunning();
    Q_INVOKABLE bool showWindow();
    Q_INVOKABLE bool startProcess();
signals:
    void sigVoiceAssistantExiting(int reason);
private slots:
    void onProcessExiting(int reason);
};
```

#### kylin-aiassistant/dbus/vadbusservice.cpp

D-Bus 客户端实现：
- `isRunning()`：通过 `QDBusConnectionInterface::isServiceRegistered()` 检测
- `showWindow()`：通过 `QDBusInterface::call("ShowWindow")` 远程调用
- `startProcess()`：`QProcess::startDetached()` 启动 `/usr/bin/voice-assistant`
- 构造函数中 `QDBusConnection::sessionBus().connect()` 监听 `ProcessExiting` 信号

### 4.2 修改文件

#### voice-assistant/mainwindow.h

新增：
- 前向声明 `class VADBusOperator`
- 成员 `VADBusOperator *m_dbusOperator = nullptr`
- 方法 `void setDbusOperator(VADBusOperator *op)`
- 槽 `void onShowWindow()`、`void shutdownAndQuit()`

#### voice-assistant/mainwindow.cpp

| 改动点 | 改动内容 |
|--------|---------|
| `cutoffClicked` 槽 | `close()` → `stopRecognition() + hide()` |
| `on_closeButton_clicked()` | `close()` → `shutdownAndQuit()` |
| 新增 `setDbusOperator()` | 保存 D-Bus 操作符指针 |
| 新增 `onShowWindow()` | 恢复窗口；如果 ASR 已在运行则保持当前聆听状态，如果处于空闲隐藏态才重新启动 ASR |
| 新增 `shutdownAndQuit()` | 隐藏窗口 + 停止一切 + 触发历史集中同步 + 同步完成后发 ProcessExiting 信号 + quit |

关键代码片段 —— `shutdownAndQuit()`：

```cpp
void MainWindow::shutdownAndQuit()
{
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    hide();

    if (m_speechController->isRunning()) {
        m_speechController->stop();
    }
    if (m_synthesisController) {
        m_synthesisController->close();
    }
    if (m_gatewayClient) {
        m_gatewayClient->cancelResponse();
    }
    stopRecognition();
    startVoiceHistorySyncBeforeQuit();
}
```

`startVoiceHistorySyncBeforeQuit()` 会在后台分配语音 sessionID、批量保存缓存消息，并在保存完成或超时兜底后调用 `finishVoiceHistorySyncAndQuit()` 发送 `ProcessExiting(0)` 和 `qApp->quit()`。

关键代码片段 —— `cutoffClicked` 改动：

```cpp
connect(ui->bottomWidget, &MiniPanel::cutoffClicked, this, [this]() {
    if (m_speechController->isRunning()) {
        m_speechController->stop();
    }
    if (m_synthesisController) {
        m_synthesisController->close();
    }
    if (m_gatewayClient) {
        m_gatewayClient->cancelResponse();
    }
    stopRecognition();  // 重置所有状态
    hide();             // 隐藏窗口而非关闭进程
});
```

#### voice-assistant/main.cpp

新增：
- `#include <QDBusConnection>`、`#include <QDBusServiceWatcher>`、`#include "dbus/vadbusoperator.h"`
- 创建 `VADBusOperator` 实例并通过 `setDbusOperator()` 传给 MainWindow
- 连接 `sigShowWindow → onShowWindow`
- 创建 `QDBusServiceWatcher` 监控 `org.ukui.kylin_aiassistant`

关键代码片段 —— 父进程监控：

```cpp
QDBusServiceWatcher *parentWatcher = new QDBusServiceWatcher(
    "org.ukui.kylin_aiassistant",
    QDBusConnection::sessionBus(),
    QDBusServiceWatcher::WatchForUnregistration);
QObject::connect(parentWatcher, &QDBusServiceWatcher::serviceUnregistered,
    &w, [&w]() {
        qWarning() << "voice-assistant: parent service unregistered, shutting down";
        w.shutdownAndQuit();
    });
```

#### voice-assistant/voice-assistant.pro

- SOURCES/HEADERS 增加 `dbus/vadbusoperator.cpp/h`
- INSTALLS 增加 `service`（D-Bus 服务文件安装到 `$PREFIX/share/dbus-1/services`）

#### kylin-aiassistant/msgpane.h

新增：
- 前向声明 `class VADBusService`
- `Q_INVOKABLE void startVoiceChat()`
- `void setVADBusService(VADBusService *service)`
- 成员 `VADBusService *m_vaDbusService = nullptr`

#### kylin-aiassistant/msgpane.cpp

新增：
- `#include "dbus/vadbusservice.h"`
- `setVADBusService()` 实现
- `startVoiceChat()` 实现
- `startVoiceChat()` 不检查主助手 NLP/语音模型列表；独立语音聊天的问答后端走 kybot 链路，入口只负责打断主窗口内语音占用、D-Bus 恢复/健康检查和必要时启动进程

关键代码片段 —— `startVoiceChat()`：

```cpp
void CMsgPane::startVoiceChat()
{
    if (!m_vaDbusService) {
        notifyVoiceAssistantMessage(tr("Failed to start voice assistant."));
        return;
    }
    if (m_vaDbusService->isRunning()) {
        m_vaDbusService->checkHealthyAsync(...);
    } else {
        m_vaDbusService->startProcess();
    }
}
```

#### kylin-aiassistant/main.cpp

新增：
- `#include "dbus/vadbusservice.h"`
- 创建 `VADBusService` 实例并通过 `setVADBusService()` 传给 CMsgPane

#### kylin-aiassistant/kylin-aiassistant.pro

- SOURCES 增加 `dbus/vadbusservice.cpp`
- HEADERS 增加 `dbus/vadbusservice.h`

#### kylin-aiassistant/res/qml/TitleBar.qml

- `voiceChatButton.onClicked` 填充 `msgPane.startVoiceChat()`

## 五、关键流程图

### 5.1 首次点击语音聊天

```
kylin-aiassistant                          voice-assistant
     |                                          |
     |-- startVoiceChat()                       |
     |   isRunning()? → 否                       |
     |-- startDetached("/usr/bin/voice-assistant")
     |                                          |--> 启动
     |                                          |--> 注册 D-Bus: org.ukui.voice_assistant
     |                                          |--> WatchFor: org.ukui.kylin_aiassistant
     |                                          |--> show(), startRecognition()
```

### 5.2 pushButton_cutoff 后再次点击

```
kylin-aiassistant                          voice-assistant
     |                                          |
     |<------- 用户点 cutoff --------------------|
     |                                          | stop ASR/TTS, stopRecognition()
     |                                          | hide() (进程保留, D-Bus 服务保留)
     |                                          |
     |-- startVoiceChat()                       |
     |   isRunning()? → 是                       |
     |-- D-Bus: ShowWindow() ------------------>|
     |                                          | onShowWindow()
     |                                          | show() + raise() + activateWindow()
     |                                          | startRecognition()
```

### 5.3 closeButton 后再次点击

```
kylin-aiassistant                          voice-assistant
     |                                          |
     |<------- 用户点 closeButton ---------------|
     |                                          | shutdownAndQuit()
     |                                          | hide()
     |                                          | stop ASR/TTS/Gateway
     |                                          | batch sync voice history
     |<-- D-Bus signal: ProcessExiting(0) ------|
     |                                          | qApp->quit() (进程退出, D-Bus 注销)
     |                                          |
     |-- startVoiceChat()                       |
     |   isRunning()? → 否                       |
     |-- startDetached("/usr/bin/voice-assistant")
     |                                          |--> 新实例启动
```

### 5.4 kylin-aiassistant 退出

```
kylin-aiassistant                          voice-assistant
     |                                          |
     |-- quit()                                 |
     |  (D-Bus 服务名 org.ukui.kylin_aiassistant 注销)
     |                                          |--> serviceUnregistered signal
     |                                          |--> shutdownAndQuit()
     |                                          |--> qApp->quit()
```

## 六、设计决策与理由

| 决策 | 理由 |
|------|------|
| 选择 QDBus 而非 QLocalSocket | 与现有 KADBusOperator 架构一致；D-Bus 原生支持方法调用+信号+服务名存活检测 |
| 用 QDBusServiceWatcher 而非 QProcess 管理子进程 | kylin-aiassistant 使用 startDetached 启动 voice-assistant，无 QProcess 句柄；D-Bus 服务名注销是更可靠的存活检测方式，覆盖 crash 场景 |
| closeButton 先 hide 再异步 qApp->quit() | 用户视角窗口立即关闭；后台完成语音历史集中同步后再真正退出并注销 D-Bus 服务 |
| pushButton_cutoff 用 hide() 替代 close() | close() 会触发窗口关闭流程；hide() 仅隐藏窗口，进程保留，D-Bus 服务保持注册，支持后续 ShowWindow() 恢复 |
| onShowWindow() 中调用 startRecognitionForLauncherRequest() | 该函数需要幂等：回拨流程中直接忽略外部唤起；已处于 Listening/Processing/Answering/Reading 时不重置会话；cutoff 后的 Idle 隐藏态才重新启动 ASR |

## 七、风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| D-Bus 服务名冲突（多用户登录） | D-Bus session bus 按用户隔离，不同用户有不同的 session bus |
| voice-assistant 进程在 D-Bus 注册前主进程调用 isRunning() | isRunning() 返回 false，走 startDetached 路径，QtSingleApplication 保证单实例 |
| ProcessExiting 信号丢失 | 主进程通过 isServiceRegistered() 做实时检测，不依赖信号做状态同步 |
| shutdownAndQuit() 中历史同步耗时 | 窗口已隐藏，用户无感；同步完成后退出，并设置超时兜底避免进程残留 |
| ProcessExiting 信号发出后进程退出 | `finishVoiceHistorySyncAndQuit()` 在历史同步结束后发送信号，再调用异步 `qApp->quit()`，信号有时间发出 |
