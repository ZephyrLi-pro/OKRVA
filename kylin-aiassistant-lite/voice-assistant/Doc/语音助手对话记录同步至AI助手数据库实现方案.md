# 语音助手对话记录同步至AI助手数据库实现方案

## 一、需求来源

根据《V11-语音助手测试用例-最新.xlsx》中会话同步相关测试用例，提炼核心需求如下：

| 需求编号 | 描述 |
|---------|------|
| 1 | 通过语音助手与 AI 进行对话后，对话内容成功写入 AI 助手数据库 |
| 2 | 历史会话页面刷新后显示语音对话记录，内容和时间戳正确 |
| 3 | 多轮语音对话的问答内容、顺序和时间戳均正确，保持上下文顺序 |
| 4 | 对话内容在 voice-assistant 进程内按顺序缓存，用户点击 closeButton 关闭时集中写入 AI 助手数据库 |
| 5 | 进行中的对话内容在刷新后仅显示已完成的问答部分 |
| 6 | cutoff（挂断）后再次 ShowWindow 恢复窗口，继续同一会话 |
| 7 | closeButton 关闭后，下次启动分配新会话 |

## 二、改动前现状分析

### 已实现

- voice-assistant 作为独立进程运行，通过 `VADBusOperator`（`org.ukui.voice_assistant`）与 kylin-aiassistant 的 `VADBusService` 进行 D-Bus 通信
- kylin-aiassistant 的 `KADBusOperator`（`org.ukui.kylin_aiassistant`）已在主工程注册 D-Bus 服务
- 主工程的 `ChatDatabase` 已有普通历史 ID 管理方法：`selectMaxHistoryId()`、`updateMaxHistoryId()`，以及 `insertOneWithModeType()` 统一插入方法
- 主工程的历史会话 UI 已支持 `chatType=2`（语音通话）的图标显示和历史回看
- **注：当前实现已将 voice-assistant 历史会话类型改为 `chatType=0` + `modeType=1`（智能体），不再使用 `chatType=2`（语音通话）**
- 主工程的 `VoiceThread` 已有完整的语音通话数据库写入逻辑（`saveItemToDatabase()`），其 `sessionID="v_"+voiceID` 只适用于 AI 助手内部旧语音通话 `chatType=2`，不能复用于 voice-assistant 智能体历史

### 未实现/有问题

1. **无跨进程数据同步**：voice-assistant 独立进程产生的对话数据完全停留在内存中（`ChatWidget` 的 QLabel 气泡），无持久化，进程退出后数据丢失
2. **无 D-Bus 方法支持数据写入**：`KADBusOperator` 仅暴露 `showOrHideView()` 方法，无任何数据操作接口
3. **D-Bus 信号未导出**：`KADBusOperator` 注册对象时使用 `ExportAllProperties`，信号无法被 D-Bus 客户端接收
4. **无 UI 刷新通知机制**：主工程历史列表在启动、删除、收藏操作后刷新，但无法感知 voice-assistant 新增的记录

## 三、设计方案

### 3.1 核心思路：通过 D-Bus 扩展实现跨进程数据同步

选择 D-Bus 作为 IPC 通道的原因：

- 与现有架构一致（`KADBusOperator` + `VADBusOperator` 均基于 QDBus）
- 主工程拥有数据库 schema 所有权，voice-assistant 不需要了解表结构细节
- 避免多进程同时写 SQLite 的并发问题
- 架构文档明确建议"不直接复用主工程 ChatDatabase"

### 3.2 整体架构

```
voice-assistant (独立进程)                  kylin-aiassistant (主进程)
     │                                            │
     │  对话过程中构造 VoiceThread 兼容 JSON       │
     │  并追加到 m_pendingVoiceChatMessages        │
     │                                            │
     │── closeButton: hide() 后集中同步 ───────────>│
     │── D-Bus: AllocateVoiceSessionId() ────────>│  分配 sessionID
     │<──── return "142" ─────────────────────────│  (分配普通历史 ID，并推进 history-id 到下一个可用 ID)
     │                                            │
      │── D-Bus: SaveVoiceChatMessage(...) ───────>│  按 msgIndex 顺序批量 insertOneWithModeType()
      │                                            │  (chatType=0, modeType=1，智能体)
      │                                            │  + updateOperateTime()
     │                                            │
     │── D-Bus: NotifyHistoryChanged() ──────────>│  触发 getDialogueList() + QML 刷新
     │                                            │
```

### 3.3 D-Bus 方法设计

在 `org.ukui.kylin_aiassistant` 接口新增 3 个方法：

| 方法 | 签名 | 说明 |
|------|------|------|
| `AllocateVoiceSessionId` | `QString AllocateVoiceSessionId()` | 分配新的智能体历史 sessionID：取 `max(HISTORY_ID[1], ConfigManager["history-id"])` 作为当前 ID，返回纯数字字符串，并将 `HISTORY_ID[1]` 和 `history-id` 推进到下一个可用 ID |
| `SaveVoiceChatMessage` | `bool SaveVoiceChatMessage(QString sessionID, int msgIndex, QString message, int chatType, qlonglong operateTime)` | 将 VoiceThread 风格 JSON 规范化为聊天页可渲染的 `author=User/Bot` 格式，再调用 `ChatDatabase::insertOneWithModeType()`（chatType=0, modeType=1），成功后刷新 `msgIndex=0` 的 `operateTime` 并同步普通历史 ID，写入失败返回 `false` |
| `NotifyHistoryChanged` | `void NotifyHistoryChanged()` | 发射 `sigNotifyHistoryChanged()` 信号，触发主工程历史列表刷新 |

### 3.4 消息 JSON 格式

voice-assistant 进程内缓存阶段仍构造兼容主工程 `VoiceThread::saveItemToDatabase()` 的 JSON 格式：

**用户消息（首条用户消息为 msgIndex=0，同时用于历史列表预览）：**

```json
{
  "message": "用户说的话...",
  "sentByUser": true,
  "reasonMessage": "",
  "modelMsg": "",
  "colorMsg": "white",
  "errorCode": 0,
  "isEnd": true,
  "isReading": false,
  "uniqueIndex": 0
}
```

**AI 回复：**

```json
{
  "message": "AI 的回复文本...",
  "sentByUser": false,
  "reasonMessage": "",
  "modelMsg": "",
  "colorMsg": "black",
  "errorCode": 0,
  "isEnd": true,
  "isReading": false,
  "uniqueIndex": 2
}
```

注意：主工程聊天页历史恢复使用的是普通聊天气泡模型，依赖 `author` 字段区分 `User` / `Bot`。因此 `KADBusOperator::SaveVoiceChatMessage()` 在写库前会对 voice-assistant 消息做一次格式规范化：

- `sentByUser=true` → `author="User"`
- `sentByUser=false` → `author="Bot"`
- 补齐 `isRefreshButtonVisiable`、`showRead`、`msgHead`、`promptId`、`reasonMessage`、`isGenerateToDo=false` 等聊天页默认字段
- 保留 `message` 字段用于历史列表预览

已有旧数据仍可能是 `sentByUser` 格式。写库侧规范化后，新数据不需要依赖加载时再转换。

`selectAllDialogue()` 查询 `msgIndex=0` 的记录时，从 JSON 中取 `message` 字段作为历史列表预览文字，此字段在两种格式中均存在，兼容无问题。

### 3.4.1 sessionID 与会话恢复规则

voice-assistant 历史记录写入为普通数字 `sessionID` + `chatType=0` + `modeType=1`。这里不能使用 `v_*`，因为 `v_*` 是 AI 助手旧语音通话 `chatType=2` 的会话 ID 体系，而智能体历史恢复、选中态、任务会话管理等链路都默认 `sessionID` 可转换为普通历史数字 ID。

- `main.qml` / `HistoryPage.qml`：打开 `chatType=0` + `modeType=1` 会话时切换到智能体 UI（上方显示“聊天、通道、技能、任务、模型”，输入区使用智能体模式按钮布局）
- `CMsgPane::loadHistoryBySessionId()`：数字 `sessionID` + `modeType=1` 按智能体历史恢复路径处理
- `Chat.qml`：添加日程提示气泡只允许 `model.isGenerateToDo === true` 的消息触发，普通语音回答不能因为文本被解析出待办内容而显示“是否添加日程”

### 3.5 UI 刷新链路

```
KADBusOperator::NotifyHistoryChanged()
  → Q_EMIT sigNotifyHistoryChanged()                          // D-Bus 信号
  → WindowController::historyChanged()                         // 中继信号
  → CMsgPane::getDialogueList()                                // 查询数据库
  → Q_EMIT loadDialogueList(list)                              // 发送给 QML
  → HistoryShowPage 重建 dialogueListModel                     // 刷新 UI
```

### 3.6 会话生命周期

```
voice-assistant 进程启动
  → m_voiceSessionId = ""（未分配）
  → m_pendingVoiceChatMessages = 空

用户第一次说话 (submitFinalText)
  → saveChatMessageToDatabase(text, true)
  → 构造用户消息 JSON，追加到 m_pendingVoiceChatMessages
  → m_msgIndex = 1

AI 回复完成 (handleAiReplyFinished)
  → saveChatMessageToDatabase(textToRead, false)
  → 构造 AI 回复 JSON，追加到 m_pendingVoiceChatMessages
  → m_msgIndex = 2

用户继续说话（第2轮）
  → saveChatMessageToDatabase(text, true)   // 缓存 msgIndex=2
  → m_msgIndex = 3
  → AI 回复: saveChatMessageToDatabase(textToRead, false)  // 缓存 msgIndex=3
  → m_msgIndex = 4

挂断 (cutoff) → hide()
  → m_pendingVoiceChatMessages 和 m_msgIndex 保留
  → 下次说话继续同一会话

恢复窗口 (onShowWindow / ShowWindow)
  → 继续使用当前内存队列和 msgIndex

关闭 (closeButton → shutdownAndQuit)
  → hide() 隐藏窗口
  → stopRecognition() / 关闭 TTS / 取消 Gateway
  → startVoiceHistorySyncBeforeQuit()
  → 如 m_voiceSessionId 为空，异步 AllocateVoiceSessionId()
  → flushPendingVoiceChatMessagesForQuit()
  → 按 msgIndex 批量 SaveVoiceChatMessage()
  → 所有保存回调返回后 NotifyHistoryChanged()
  → ProcessExiting(0)
  → 进程退出

下次启动新进程
  → m_voiceSessionId = "" → 首次说话分配新 sessionID
```

## 四、文件改动详情

### 4.1 新增文件

#### voice-assistant/dbus/kadbusclient.h

D-Bus 客户端头文件，声明 `KADBusClient` 类，封装对 `org.ukui.kylin_aiassistant` 新方法的调用：

```cpp
class KADBusClient : public QObject
{
    Q_OBJECT
public:
    explicit KADBusClient(QObject *parent = nullptr);
    ~KADBusClient();

    void allocateVoiceSessionIdAsync();
    void saveVoiceChatMessageAsync(const QString &sessionID, int msgIndex,
                                   const QString &message, int chatType,
                                   qlonglong operateTime);
    void notifyHistoryChangedAsync();
    void notifyVoiceTaskCompletedAsync(const QString &sessionID, const QString &message);

signals:
    void voiceSessionIdAllocated(const QString &sessionID);
    void voiceChatMessageSaved(int msgIndex, bool success);

private:
    static const QString s_serviceName;
    static const QString s_objectPath;
    static const QString s_interfaceName;
};
```

#### voice-assistant/dbus/kadbusclient.cpp

D-Bus 客户端实现，每个方法内部创建 `QDBusInterface`，通过 `asyncCall()` 调用主工程的 D-Bus 方法，并用 `QDBusPendingCallWatcher` 在返回后发出本地信号，避免关闭保存阶段阻塞 UI 事件循环。

### 4.2 修改文件

#### kylin-aiassistant/dbus/kadbusoperator.h

新增内容：

| 改动点 | 改动内容 |
|--------|---------|
| signals | 新增 `void sigNotifyHistoryChanged()` |
| public slots | 新增 `QString AllocateVoiceSessionId()` |
| public slots | 新增 `bool SaveVoiceChatMessage(QString sessionID, int msgIndex, QString message, int chatType, qlonglong operateTime)` |
| public slots | 新增 `void NotifyHistoryChanged()` |

#### kylin-aiassistant/dbus/kadbusoperator.cpp

| 改动点 | 改动内容 |
|--------|---------|
| #include | 新增 `#include "chatdatabase.h"`、`#include "configmanager.h"`、`#include <ukui-log4qt.h>` |
| registerObject | `ExportAllProperties` 改为 `ExportAllSignals`，使 D-Bus 信号可被客户端接收 |
| 新增 `AllocateVoiceSessionId()` | 取 `max(HISTORY_ID[1], ConfigManager["history-id"])` 分配普通数字历史 ID，并将 `HISTORY_ID[1]` / `history-id` 推进到下一个可用 ID |
| 新增 `SaveVoiceChatMessage()` | 调用 `normalizeVoiceAssistantMessageForChat()` 规范化 JSON，再调用 `ChatDatabase::insertOneWithModeType()`（chatType=0, modeType=1），成功后刷新 `msgIndex=0` 的 `operateTime`，同步普通历史 ID，返回写入结果 |
| 新增 `NotifyHistoryChanged()` | 发射 `sigNotifyHistoryChanged()` 信号 |

关键代码片段 —— `AllocateVoiceSessionId()`：

```cpp
QString KADBusOperator::AllocateVoiceSessionId()
{
    ChatDatabase *db = ChatDatabase::getChatDatabase();
    int historyIdDb = db->selectMaxHistoryId();
    int historyIdSettings = ConfigManager::getInstance()->getConfigValue("history-id").toInt();
    int newId = qMax(historyIdDb, historyIdSettings);
    db->updateMaxHistoryId(newId + 1);
    ConfigManager::getInstance()->updateConfig("history-id", newId + 1);
    QString sessionId = QString::number(newId);
    KyInfo() << "KADBusOperator::AllocateVoiceSessionId:" << sessionId;
    return sessionId;
}
```

关键代码片段 —— `SaveVoiceChatMessage()`（写库前规范化 voice-assistant 消息，使用 `insertOneWithModeType`，`chatType=0`, `modeType=1`）：

```cpp
bool KADBusOperator::SaveVoiceChatMessage(QString sessionID, int msgIndex,
                                           QString message, int chatType, qlonglong operateTime)
{
    ChatDatabase *db = ChatDatabase::getChatDatabase();
    const QString normalizedMessage = normalizeVoiceAssistantMessageForChat(message);
    if (!db->insertOneWithModeType(sessionID, msgIndex, normalizedMessage, chatType, 1)) {
        KyInfo() << "KADBusOperator::SaveVoiceChatMessage: insert failed, sessionID=" << sessionID
                 << "msgIndex=" << msgIndex << "chatType=" << chatType;
        return false;
    }

    db->updateOperateTime(sessionID, 0, operateTime);

    bool ok = false;
    int sessionHistoryId = sessionID.toInt(&ok);
    if (ok) {
        int historyIdDb = db->selectMaxHistoryId();
        int historyIdSettings = ConfigManager::getInstance()->getConfigValue("history-id").toInt();
        if (sessionHistoryId >= historyIdDb) {
            db->updateMaxHistoryId(sessionHistoryId + 1);
        }
        if (sessionHistoryId + 1 > historyIdSettings) {
            ConfigManager::getInstance()->updateConfig("history-id", sessionHistoryId + 1);
        }
    }

    KyInfo() << "KADBusOperator::SaveVoiceChatMessage: sessionID=" << sessionID
             << "msgIndex=" << msgIndex << "chatType=" << chatType;
    return true;
}
```

关键代码片段 —— `flushPendingVoiceChatMessagesForQuit()`（voice-assistant 侧，`chatType=0` 传递给 D-Bus）：

```cpp
void MainWindow::flushPendingVoiceChatMessagesForQuit()
{
    ...
    const QList<PendingVoiceChatMessage> pending = m_pendingVoiceChatMessages;
    m_pendingVoiceChatMessages.clear();
    m_voiceHistoryPendingSaveCount = pending.size();
    for (const PendingVoiceChatMessage &item : pending) {
        m_kaDbusClient->saveVoiceChatMessageAsync(m_voiceSessionId, item.msgIndex,
                                                  item.message, 0, item.operateTime);
    }
}
```

#### kylin-aiassistant/window/windowcontroller.h

新增信号 `void historyChanged()`。

#### kylin-aiassistant/window/windowcontroller.cpp

`initDbusWathcer()` 中新增连接：

```cpp
connect(m_dbusOperator, &KADBusOperator::sigNotifyHistoryChanged,
        this, &WindowController::historyChanged);
```

#### kylin-aiassistant/main.cpp

新增连接：

```cpp
QObject::connect(windowCtrl, &WindowController::historyChanged,
                 msgPane, &CMsgPane::getDialogueList);
```

#### kylin-aiassistant/msgpane.cpp

| 改动点 | 改动内容 |
|--------|---------|
| 新增 `normalizeVoiceAssistantHistoryMessage()` | 将旧的 voice-assistant `sentByUser` JSON 转换为聊天页需要的 `author=User/Bot` JSON，并补齐默认字段 |
| 修改 `loadHistoryBySessionId()` | 旧版错误写入的 `v_` + `chatType=0` 数据保留兼容分支；新 voice-assistant 数据使用普通数字 `sessionID`，按标准智能体历史恢复 |

#### kylin-aiassistant/chatdatabase.cpp

| 改动点 | 改动内容 |
|--------|---------|
| 修改 `selectAllDialogue()` | 新数据通过数字 `sessionID` + `mode_type=1` 进入智能体历史；保留对旧版错误 `v_` + `chat_type=0` 数据的模式兼容 |
| 修改 `getSessionModeType()` | 实时读取数据库中的 `mode_type`，历史入口不再信任 QML 列表缓存值；旧 `chat_type=2` 语音通话历史不受影响 |

关键代码片段 —— 历史恢复分支：

```cpp
int modeType = m_database->getSessionModeType(sessionId);
const bool isVoiceAssistantHistory = sessionId.startsWith(QStringLiteral("v_")) && modeType == 1;
if (modeType == 1 && !isVoiceAssistantHistory) {
    loadTaskSessionHistory(sessionId);
    return;
}
if (isVoiceAssistantHistory) {
    // QML 保持智能体 UI；这里不恢复 Gateway 任务会话，只回放历史消息。
    m_chatType = NORMAL;
    Q_EMIT loadConversation(normalizeVoiceAssistantHistoryMessages(m_database->selectBySessionID(sessionId)));
    Q_EMIT sessionReplyStatusChanged(sessionId, false, false);
    Q_EMIT modifyDialogueListFlag("");
    return;
}
```

#### kylin-aiassistant/res/qml/main.qml、HistoryPage.qml、Chat.qml

| 文件 | 改动内容 |
|------|---------|
| `main.qml` | 打开 `chatType=0` 历史时通过 `msgPane.getSessionModeType(sessionId)` 重新读取会话模式，避免使用历史列表缓存的旧 `modeType`；数字 `sessionID` + `modeType=1` 会话会定位到智能体“聊天”页签 |
| `HistoryPage.qml` | 历史页列表内点击会话时同样实时读取 `modeType`，使用智能体 UI 切换规则，并同步 `ButtonState.chatMode=1` |
| `Chat.qml` | `AddSchedulePromptBubble` 的 `onOutputCompletedChanged` 增加 `model.isGenerateToDo` 保护，普通语音回复不触发添加日程提示 |

#### voice-assistant/mainwindow.h

| 改动点 | 改动内容 |
|--------|---------|
| 前向声明 | `class QTimer;` → `class KADBusClient;`（QTimer 已在成员中直接使用，前向声明不再需要） |
| 公有方法 | 新增 `void setKaDbusClient(KADBusClient *client)` |
| 成员变量 | 新增 `KADBusClient *m_kaDbusClient = nullptr` |
| 成员变量 | 新增 `QString m_voiceSessionId` |
| 成员变量 | 新增 `int m_msgIndex = 0` |
| 成员变量 | 新增 `QList<PendingVoiceChatMessage> m_pendingVoiceChatMessages`，运行中缓存待同步消息 |
| 成员变量 | 新增 `bool m_voiceSessionAllocating` / `bool m_voiceHistorySyncInProgress` / `int m_voiceHistoryPendingSaveCount`，管理关闭时批量同步状态 |
| 私有方法 | 新增 `void saveChatMessageToDatabase(const QString &message, bool sentByUser, const QString &modelMsg)` |
| 私有方法 | 新增 `startVoiceHistorySyncBeforeQuit()` / `flushPendingVoiceChatMessagesForQuit()` / `finishVoiceHistorySyncAndQuit()` |

#### voice-assistant/mainwindow.cpp

| 改动点 | 改动内容 |
|--------|---------|
| #include | 新增 `#include "dbus/kadbusclient.h"`、`<QDateTime>`、`<QJsonDocument>`、`<QJsonObject>` |
| 新增 `setKaDbusClient()` | 保存 KADBusClient 指针 |
| `submitFinalText()` | 调用 `saveChatMessageToDatabase()` 缓存用户消息，不立即写库 |
| `handleAiReplyFinished()` | 调用 `saveChatMessageToDatabase()` 缓存 AI 回复，不立即刷新历史 |
| `shutdownAndQuit()` | 先 `hide()`，停止 ASR/TTS/Gateway 后调用 `startVoiceHistorySyncBeforeQuit()`，保存完成后再退出进程 |
| 新增 `saveChatMessageToDatabase()` | 构造 VoiceThread 兼容 JSON，按当前 `m_msgIndex` 追加到 `m_pendingVoiceChatMessages`，然后递增 `m_msgIndex` |
| 新增 `startVoiceHistorySyncBeforeQuit()` | 关闭窗口时启动集中同步；无消息则直接退出，有消息则分配 session 并保存 |
| 新增 `flushPendingVoiceChatMessagesForQuit()` | 将内存队列中的消息按 `msgIndex` 批量发送到主工程 D-Bus |
| 新增 `finishVoiceHistorySyncAndQuit()` | 同步结束后通知历史刷新、发送 `ProcessExiting(0)` 并退出 |

关键代码片段 —— `saveChatMessageToDatabase()` 缓存消息：

```cpp
PendingVoiceChatMessage item;
item.msgIndex = msgIndex;
item.message = jsonStr;
item.operateTime = operateTime;
m_pendingVoiceChatMessages.append(item);
```

关键代码片段 —— closeButton 关闭时集中同步：

```cpp
void MainWindow::startVoiceHistorySyncBeforeQuit()
{
    m_voiceHistorySyncInProgress = true;
    if (!m_kaDbusClient || m_pendingVoiceChatMessages.isEmpty()) {
        finishVoiceHistorySyncAndQuit();
        return;
    }

    if (m_voiceSessionId.isEmpty()) {
        ensureVoiceSessionIdAsync();
        return;
    }
    flushPendingVoiceChatMessagesForQuit();
}
```

#### voice-assistant/main.cpp

| 改动点 | 改动内容 |
|--------|---------|
| #include | 新增 `#include "dbus/kadbusclient.h"` |
| main() | 创建 `KADBusClient kaDbusClient` 并调用 `w.setKaDbusClient(&kaDbusClient)` |

#### voice-assistant/voice-assistant.pro

SOURCES 和 HEADERS 新增 `dbus/kadbusclient.cpp` / `dbus/kadbusclient.h`。

## 五、关键流程图

### 5.1 首次语音对话缓存

```
voice-assistant                          kylin-aiassistant
     |                                          |
     |-- submitFinalText("你好")                 |
     |   saveChatMessageToDatabase(text, true)   |
     |   构造 user_json                          |
     |   append m_pendingVoiceChatMessages        |
     |   m_msgIndex = 1                          |
```

### 5.2 AI 回复完成缓存

```
voice-assistant                          kylin-aiassistant
     |                                          |
     |-- handleAiReplyFinished("回复文本")        |
     |   saveChatMessageToDatabase(text, false)  |
     |   构造 ai_json                            |
     |   append m_pendingVoiceChatMessages        |
     |   m_msgIndex = 2                          |
```

### 5.3 cutoff 后继续同一会话

```
voice-assistant                          kylin-aiassistant
     |                                          |
     |<------- 用户点 cutoff --------------------|
     |   stopAudioForHide() + hide()             |
     |   m_pendingVoiceChatMessages 保留          |
     |                                          |
     |-- ShowWindow() (从主助手点击) ------------>|
     |   onShowWindow()                          |
     |   show() + startRecognition()             |
     |                                          |
     |-- 用户再次说话                             |
     |   saveChatMessageToDatabase(text, true)   |
     |   append m_pendingVoiceChatMessages        |
     |   (msgIndex 继续递增)                      |
```

### 5.4 closeButton 后集中同步并退出

```
voice-assistant                          kylin-aiassistant
     |                                          |
     |-- shutdownAndQuit()                       |
     |   hide()                                  |
     |   stopRecognition/TTS/Gateway             |
     |-- D-Bus: AllocateVoiceSessionId() ------>|
     |<-- return "142" -------------------------|
      |-- D-Bus: SaveVoiceChatMessage(msg 0) ---->| insertOneWithModeType("142", 0, user_json, 0, 1)
      |-- D-Bus: SaveVoiceChatMessage(msg 1) ---->| insertOneWithModeType("142", 1, ai_json, 0, 1)
     |-- D-Bus: NotifyHistoryChanged() --------->|
     |   ProcessExiting(0) -------------------->|
     |   qApp->quit()                            |
     |                                          |
     |   (下次启动)                              |
     |   m_voiceSessionId = ""                   |
     |-- 用户说话                                |
     |   m_voiceSessionId.isEmpty() → true        |
     |   关闭时重新分配新 ID                      |
```

## 六、测试用例覆盖

| 测试用例 | 覆盖情况 |
|----------|----------|
| 单轮对话写入数据库验证 | `submitFinalText()` 和 `handleAiReplyFinished()` 先缓存消息；closeButton 后批量 `SaveVoiceChatMessage()` 写入 |
| 历史会话页面刷新后显示记录 | 关闭同步完成后 `NotifyHistoryChanged()` → `sigNotifyHistoryChanged()` → `getDialogueList()` → QML 刷新 |
| 多轮对话顺序和时间戳正确 | `m_msgIndex` 在缓存时逐条递增，关闭时按缓存的 `msgIndex` 批量写入，`operateTime` 使用缓存时刻 |
| 语音历史回看进入智能体界面且不显示异常日程控件 | voice-assistant 会话使用普通数字 `sessionID` + `chatType=0` + `modeType=1`，按标准智能体历史恢复；普通语音回复不会触发 `AddSchedulePromptBubble` |
| 旧错误数据兼容 | 早期误写成 `v_` + `chatType=0` 的数据仅保留兼容处理；新数据不再写入 `v_*` |
| 关闭时集中写入 | 用户点击 closeButton 后窗口先隐藏，后台保存完成后进程再退出 |
| 进行中对话仅显示已完成部分 | closeButton 会取消 Gateway 未完成响应，仅已调用 `saveChatMessageToDatabase()` 的用户消息和完整 AI 回复进入缓存 |
| cutoff 后继续同一会话 | cutoff 不清空 `m_pendingVoiceChatMessages` 和 `m_msgIndex` |
| closeButton 后新会话 | 进程退出，下次启动内存队列为空，关闭时重新分配新的 voice sessionID |

## 七、风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| D-Bus 调用阻塞 voice-assistant 事件循环 | 使用 `QDBusPendingCall` 异步保存，窗口已隐藏，不阻塞用户交互 |
| voice-assistant crash 导致会话消息丢失 | 当前策略在 closeButton 前只保存在进程内存中，进程异常退出会丢失未集中同步的消息；这是“关闭时集中同步”需求带来的取舍 |
| 主工程未启动时 D-Bus 调用失败 | `QDBusServiceWatcher` 保证 voice-assistant 跟随主工程退出，不会出现此场景 |
| `ExportAllProperties` 改为 `ExportAllSignals` 影响现有功能 | `ExportAllProperties` 导出的是 Q_PROPERTY，现有代码无 Q_PROPERTY 定义，改动无副作用 |
| cutoff 后恢复继续同一会话 | cutoff 只隐藏窗口，不清空内存队列；再次 ShowWindow 后继续追加到同一批待同步消息 |
| D-Bus 写入失败导致进程迟迟不退出 | `voiceChatMessageSaved` 失败也会计入完成并继续退出；另有 10 秒超时兜底 |
| sessionID 分配失败 | 关闭同步阶段若 `AllocateVoiceSessionId` 失败，会放弃本批同步并继续退出，避免窗口进程残留 |
| JSON 格式与 `selectAllDialogue()` 兼容性 | `selectAllDialogue()` 仅取 `message` 字段做历史预览，原始 `sentByUser` 格式和规范化后的 `author` 格式均包含此字段 |
| `sessionID` 使用 `v_*` 导致智能体历史异常 | `v_*` 只用于 AI 助手旧语音通话 `chatType=2`；voice-assistant 统一使用普通数字历史 ID |
| 普通语音回答被识别为待办并显示添加日程提示 | `Chat.qml` 只有 `model.isGenerateToDo === true` 时才检查并显示添加日程提示 |
