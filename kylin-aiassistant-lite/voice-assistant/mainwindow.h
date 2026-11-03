#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QString>
#include <QList>
#include <QElapsedTimer>
#include "externalvoicequeue.h"

class GatewayChatClient;
class SpeechRecognitionController;
class SpeechSynthesisController;
class NotificationManager;
class VADBusOperator;
class KADBusClient;
class QTimer;
class QLabel;

namespace Ui {
class MainWindow;
}

class QShowEvent;
class UkuiWindowHelper;

struct PendingVoiceChatMessage
{
    int msgIndex = 0;
    QString message;
    qlonglong operateTime = 0;
};

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    enum VoiceAssistantState {
        Idle,        // 空闲：无识别、无对话
        Listening,   // 聆听：ASR 激活中，等待语音输入
        Processing,  // 处理中：语句已发送给网关，等待回复
        Answering,   // 回答中：网关正在流式回复
        Reading      // 朗读中：TTS 朗读回答
    };

    explicit MainWindow(QWidget *parent = nullptr, bool autoStartRecognition = true);
    ~MainWindow();

    void setDbusOperator(VADBusOperator *op);
    void setKaDbusClient(KADBusClient *client);

    /** 将窗口移到当前屏工作区内右下角附近；会处理 size 尚未就绪（如 Wayland 首帧）的情况。 */
    void repositionNearScreenCorner();

public slots:
    void onActivationMessage(const QString &message);
    void onShowWindow();
    void onShowExternalListening();
    void onSubmitExternalText(const QString &text);
    void onExternalRecognitionFailed(const QString &code);
    QString prepareExternalListening();
    QString prepareFollowupListening();
    void endExternalConversation();
    QString externalInputStatus() const;
    bool setReadingContext(const QString &title, const QString &text, int page);
    void shutdownAndQuit();
    void ensureVoiceSessionIdAsync();

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_closeButton_clicked();
    void onAnswerClicked();

private:
    friend class VoiceInputRegression;
    Ui::MainWindow *ui = nullptr;
    VADBusOperator *m_dbusOperator = nullptr;
    KADBusClient *m_kaDbusClient = nullptr;
    QString m_voiceSessionId;
    int m_msgIndex = 0;
    bool m_voiceSessionAllocating = false;
    bool m_shuttingDown = false;
    QList<PendingVoiceChatMessage> m_pendingVoiceChatMessages;

    void setState(VoiceAssistantState newState);
    void updateVoiceStateUi();
    void updateConversationStatusUi();
    void updateCloseButtonIcon(bool white);
    void setCallbackMode(bool enabled);
    void setScreenShareEnabled(bool enabled);
    void markScreenShareUnavailable();
    void restoreScreenShareAvailability();
    void beginCallbackAttempts();
    void startCallbackAttempt();
    void handleCallbackRingTimeout();
    void scheduleCallbackRetry();
    void finishCallbackFailure();
    void sendCallbackFailureNotification();
    void cancelCallbackFlow();
    void setMiniMode(bool enabled);
    void resizeKeepingBottomRight(const QSize &newSize);
    bool isRoundCompleted() const;

    void applyWindowStates();
    void presentForUserRequest();
    void beginExternalListening(bool userRequested);

    void startRecognition();
    void stopRecognition();
    void stopAudioForHide();
    void scheduleStartRecognition(int delayMs);
    void invalidateScheduledRecognitionStarts();
    bool canStartRecognition() const;
    void maybeStartRecognitionForCurrentState();
    void startRecognitionForLauncherRequest();
    void updateMiddlePage();
    void ensureUserBubble();
    void handlePartialText(const QString &text);
    void handleFinalText(const QString &text);
    QString pendingRecognitionTextWith(const QString &tailText) const;
    void cancelPendingFinalSubmit();
    void schedulePendingFinalSubmit();
    bool submitPendingRecognitionText(const QString &tailText = QString());
    void submitFinalText(const QString &text);
    void handleRecognitionStartFailed(const QString &message);
    void handleRecognitionError(const QString &message);
    void beginAiReply();
    void appendAiReply(const QString &text);
    void handleAiReplyFinished(const QString &fullResponse);
    QString filterModelThinkingFromReplyChunk(const QString &text);
    void resetReplyThinkingFilter();
    void startStreamingReadIfNeeded();
    void enqueueStreamingReadText(const QString &text);
    void flushStreamingReadBuffer();
    void dispatchPendingStreamTail();
    void reconcileStreamReadBufferWithFullResponse(const QString &chunkedReplyBeforeFinish,
                                                   const QString &authoritativeFullText);
    bool extractReadableSentence(QString *sentenceOut);
    void handleChatError(const QString &message);
    void pauseRecognitionForPlayback();
    void resumeRecognitionAfterPlayback();
    void saveChatMessageToDatabase(const QString &message, bool sentByUser, const QString &modelMsg, const QString &imageBase64 = QString(), const QString &colorMsg = QString(), int errorCode = 0);
    void savePendingUserMessageIfNeeded(const QString &message);
    void saveAssistantErrorMessageIfNeeded(const QString &message);
    void saveErrorMessageToHistory(const QString &message);
    void persistPendingConversationBeforeQuit();
    bool shouldPersistErrorHistory(bool useChatBubble) const;
    void startVoiceHistorySyncBeforeQuit();
    void flushPendingVoiceChatMessagesForQuit();
    void finishVoiceHistorySyncAndQuit();

    /** 是否处于「对话相关」上下文：字幕页、用户气泡、AI 流式、等网关，或本轮已收到 partial。 */
    bool inChatErrorContext() const;
    void presentErrorToUser(const QString &message, bool useChatBubble);
    void ensureChatVisibleForBubbleError();
    void appendStandaloneAiBubble(const QString &text);
    void clearActiveAiErrorBubble();
    void showInlineError(const QString &text);
    void clearInlineError(bool refreshUi);
    void notifyVoiceTaskCompleted(const QString &message);
    bool isCallbackFlowActive() const;

    GatewayChatClient *m_gatewayClient = nullptr;
    SpeechRecognitionController *m_speechController = nullptr;
    SpeechSynthesisController *m_synthesisController = nullptr;
    NotificationManager *m_notificationManager = nullptr;
    QTimer *m_callbackRingTimer = nullptr;
    QTimer *m_callbackRetryTimer = nullptr;
    QTimer *m_asrFinalSubmitTimer = nullptr;
    QTimer *m_gatewayReplyTimer = nullptr;
    QLabel *m_conversationStatus = nullptr;

    VoiceAssistantState m_state = Idle;
    QString m_currentAiReplyText;
    bool m_hasActiveUserBubble;
    bool m_hasActiveAiBubble;
    bool m_showChatPage;
    bool m_resumeRecognitionAfterPlayback;
    bool m_hookedWindowScreenChanged = false;
    /** 用户已发出本轮提问，等待网关 chunk/done 或 error。 */
    bool m_waitingGatewayReply = false;
    /** 本轮网关错误是否已写入历史，避免 type=error 与 done 内嵌错误重复落库。 */
    bool m_chatErrorSavedThisRound = false;
    /** 本轮已收到识别 partial（仅聆听页亦视为对话进行中，直至 final 或出错或暂停）。 */
    bool m_asrListeningRoundActive = false;
    /** 静音已手动提交 partial 后，丢弃旧 ASR session 迟到的 partial/final，避免重复气泡。 */
    bool m_discardLateAsrResults = false;
    /** 对应开发方案文档中的屏幕共享开关，与 MiniPanel 按钮状态同步 */
    bool m_screenShare = false;
    bool m_screenShareAvailable = true;
    /** 静音状态，true 表示麦克风已静音，与 MiniPanel 静音按钮状态同步 */
    bool m_muted = false;
    /** 最近一次 ASR partial 文本，用于静音时作为 final 发送 */
    QString m_lastPartialText;
    /** SDK final 之后等待用户是否继续说话的待提交文本。 */
    QString m_pendingFinalText;
    int m_recognitionStartGeneration = 0;
    int m_inlineErrorGeneration = 0;
    bool m_inlineErrorActive = false;
    bool m_callbackPending = false;
    bool m_callbackMode = false;
    int m_callbackAttemptCount = 0;
    bool m_miniMode = false;
    bool m_windowHelperReady = false;
    UkuiWindowHelper *m_windowHelper = nullptr;
    /** 从本地配置文件加载的系统提示词。 */
    QString m_systemPrompt;
    QString m_systemPromptError;
    QString m_streamReadBuffer;
    bool m_streamReadingStarted = false;
    bool m_streamReadingFinished = false;
    bool m_replyThinkBlockActive = false;
    QString m_replyThinkTagBuffer;
    bool m_currentAiReplySaved = true;
    QString m_activeAiErrorBubbleText;
    bool m_needsInitialPosition = true;
    /** true 时由 KylinBot/SenseVoice 提供 ASR，本窗口不得再打开旧语音 SDK。 */
    bool m_externalRecognitionMode = false;
    ExternalVoiceQueue m_externalVoiceQueue;
    bool m_externalCaptureActive = false;
    bool m_externalCaptureExpired = false;
    bool m_suppressCurrentReplySpeech = false;
    QTimer *m_externalCaptureTimer = nullptr;
    QString m_currentExternalContext;
    QString m_readingContext;
    qint64 m_readingContextExpires = 0;
    void requestExternalContext(quint64 inputId, const QString &question);
    void scheduleNextExternalInput();
    void submitNextExternalInput();
    void finishExternalCapture();
    void maybeOfferFollowup();
    void cancelFollowup();
    bool m_followupRequested = false;
    bool m_followupCaptureActive = false;
    bool m_externalConversationEnabled = false;
    QElapsedTimer m_followupElapsed;
    QTimer *m_followupTimer = nullptr;

    void skipTaskbarOnX11();
    bool m_voiceHistorySyncInProgress = false;
    int m_voiceHistoryPendingSaveCount = 0;
};

#endif // MAINWINDOW_H
