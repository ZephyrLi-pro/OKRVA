#include "speechrecognitioncontroller.h"
#include "speecherrormessage.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QString>
#include <QTimer>
#include <thread>

namespace {
void destroyRecognitionSessionAsync(SpeechRecognitionSession *session,
                                    std::shared_ptr<void> context,
                                    bool stopFirst)
{
    if (!session) {
        return;
    }

    std::thread([session, context, stopFirst]() {
        Q_UNUSED(context);
        SpeechRecognitionSession *sessionToDestroy = session;
        if (stopFirst) {
            speech_recognizer_stop_continuous_recognition_async(sessionToDestroy);
        }
        speech_recognizer_destroy_session(&sessionToDestroy);
    }).detach();
}

void stopRecognitionSessionAsync(SpeechRecognitionSession *session)
{
    if (!session) {
        return;
    }

    std::thread([session]() {
        speech_recognizer_stop_continuous_recognition_async(session);
    }).detach();
}

QString recognitionText(const char *source)
{
    return QCoreApplication::translate("SpeechRecognitionController", source);
}
} // namespace

struct SpeechRecognitionController::CallbackContext
{
    SpeechRecognitionController *controller = nullptr;
    std::shared_ptr<std::atomic_bool> alive;
    quint64 generation = 0;
};

SpeechRecognitionController::SpeechRecognitionController(QObject *parent)
    : QObject(parent)
    , m_session(nullptr)
    , m_audioConfig(nullptr)
    , m_alive(std::make_shared<std::atomic_bool>(true))
    , m_generation(0)
    , m_running(false)
    , m_stopping(false)
    , m_starting(false)
    , m_pendingStart(false)
    , m_stopRequestedWhileStarting(false)
{
}

SpeechRecognitionController::~SpeechRecognitionController()
{
    m_alive->store(false);
    closeSession();
}

bool SpeechRecognitionController::start()
{
    if (m_running || m_starting) {
        return true;
    }

    if (m_stopping) {
        qWarning() << "voice-assistant speech recognition still stopping, deferring start";
        m_pendingStart = true;
        return false;
    }

    const quint64 generation = advanceGeneration();
    auto callbackContext = std::make_shared<CallbackContext>();
    callbackContext->controller = this;
    callbackContext->alive = m_alive;
    callbackContext->generation = generation;

    m_starting = true;
    m_stopRequestedWhileStarting = false;
    std::shared_ptr<std::atomic_bool> alive = m_alive;
    std::thread([this, alive, generation, callbackContext]() {
        QString errorMessage;
        SpeechRecognitionSession *session = speech_recognizer_create_session();
        AudioConfig *audioConfig = nullptr;
        bool ok = false;

        if (!session) {
            errorMessage = recognitionText("Failed to create speech recognition session.");
        } else {
            const int errorCode = speech_recognizer_init_session(session);
            if (errorCode != 0) {
                const QString mapped = speechErrorMessageForCode(errorCode);
                errorMessage = mapped.isEmpty()
                        ? recognitionText("Failed to initialize speech recognition session: %1").arg(errorCode)
                        : mapped;
                SpeechRecognitionSession *sessionToDestroy = session;
                speech_recognizer_destroy_session(&sessionToDestroy);
                session = nullptr;
            }
        }

        if (session) {
            audioConfig = audio_config_create_continuous_audio_input_from_default_microphone();
            if (!audioConfig) {
                errorMessage = speechMicrophoneUnavailableMessage();
                SpeechRecognitionSession *sessionToDestroy = session;
                speech_recognizer_destroy_session(&sessionToDestroy);
                session = nullptr;
            }
        }

        if (session && audioConfig) {
            speech_recognizer_set_audio_config(session, audioConfig);
            speech_recognizer_result_set_callback(session, recognitionCallback, callbackContext.get());
            speech_recognizer_start_continuous_recognition_async(session);
            ok = true;
        }

        QObject *app = QCoreApplication::instance();
        if (!app || !alive->load()) {
            if (audioConfig) {
                AudioConfig *configToDestroy = audioConfig;
                audio_config_destroy(&configToDestroy);
            }
            destroyRecognitionSessionAsync(session, callbackContext, ok);
            return;
        }

        QMetaObject::invokeMethod(app, [this, alive, generation, callbackContext, session, audioConfig, ok, errorMessage]() {
            if (!alive->load() || !isCurrentGeneration(generation)) {
                if (audioConfig) {
                    AudioConfig *configToDestroy = audioConfig;
                    audio_config_destroy(&configToDestroy);
                }
                destroyRecognitionSessionAsync(session, callbackContext, ok);
                return;
            }

            m_starting = false;
            if (m_stopRequestedWhileStarting) {
                m_stopRequestedWhileStarting = false;
                if (audioConfig) {
                    AudioConfig *configToDestroy = audioConfig;
                    audio_config_destroy(&configToDestroy);
                }
                if (session) {
                    destroyRecognitionSessionAsync(session, callbackContext, ok);
                }
                if (m_pendingStart) {
                    m_pendingStart = false;
                    start();
                }
                return;
            }

            if (!ok) {
                Q_EMIT startFailed(errorMessage);
                return;
            }

            m_session = session;
            m_audioConfig = audioConfig;
            m_callbackContext = callbackContext;
            m_running = true;
            Q_EMIT recognitionStarted();
        }, Qt::QueuedConnection);
    }).detach();
    return true;
}

void SpeechRecognitionController::stop()
{
    m_pendingStart = false;

    if (m_starting) {
        m_stopRequestedWhileStarting = true;
        return;
    }

    if (m_stopping) {
        return;
    }

    if (!m_session || !m_running) {
        m_running = false;
        return;
    }

    const quint64 stoppedGeneration = advanceGeneration();
    Q_UNUSED(stoppedGeneration);
    stopRecognitionSessionAsync(m_session);
    m_running = false;
    m_stopping = true;
    destroyAudioConfig();

    // stop_continuous_recognition_async 是异步操作，延迟销毁 session
    // 以避免在底层仍在使用 session 时释放资源。
    // 外部调用方（MainWindow）也已通过状态机 + kAsrRestartDelayMs 确保不会
    // 在 stop 完成前调用 start。
    QTimer::singleShot(2000, this, [this]() {
        m_stopping = false;
        if (m_session) {
            SpeechRecognitionSession *s = m_session;
            auto context = m_callbackContext;
            m_session = nullptr;
            m_callbackContext.reset();
            destroyRecognitionSessionAsync(s, context, false);
        }
        if (m_pendingStart) {
            m_pendingStart = false;
            start();
        }
    });
}

void SpeechRecognitionController::cancelPendingStart()
{
    m_pendingStart = false;
}

bool SpeechRecognitionController::isRunning() const
{
    return m_running;
}

bool SpeechRecognitionController::isStopping() const
{
    return m_stopping;
}

void SpeechRecognitionController::closeSession()
{
    // 直接停止并立即销毁：用于析构等不需要考虑后续 start 的场景。
    // stop_continuous_recognition_async 在此场景下仍会发出，但对象即将销毁
    // 因此我们直接同步清理。
    if (m_session && m_running) {
        advanceGeneration();
    }

    m_running = false;
    m_stopping = false;
    m_starting = false;
    m_stopRequestedWhileStarting = true;
    destroyAudioConfig();

    if (m_session) {
        SpeechRecognitionSession *sessionToDestroy = m_session;
        auto context = m_callbackContext;
        m_session = nullptr;
        m_callbackContext.reset();
        destroyRecognitionSessionAsync(sessionToDestroy, context, true);
    }
}

void SpeechRecognitionController::destroyAudioConfig()
{
    if (!m_audioConfig) {
        return;
    }

    AudioConfig *configToDestroy = m_audioConfig;
    audio_config_destroy(&configToDestroy);
    m_audioConfig = nullptr;
}

quint64 SpeechRecognitionController::advanceGeneration()
{
    return ++m_generation;
}

bool SpeechRecognitionController::isCurrentGeneration(quint64 generation) const
{
    return generation == m_generation.loadAcquire();
}

void SpeechRecognitionController::recognitionCallback(SpeechRecognitionResult *result, void *userData)
{
    CallbackContext *context = static_cast<CallbackContext *>(userData);
    if (!context || !context->alive || !context->alive->load() || !context->controller || !result) {
        return;
    }

    SpeechRecognitionController *controller = context->controller;
    const auto alive = context->alive;
    const quint64 generation = context->generation;
    const int errorCode = speech_recognition_result_get_error_code(result);
    if (errorCode != 0 && errorCode != -1) {
        const QString mapped = speechErrorMessageForCode(errorCode);
        const QString message = mapped.isEmpty()
                ? recognitionText("Speech recognition error: %1").arg(errorCode)
                : mapped;
        QObject *app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [controller, alive, generation, message]() {
                if (!alive->load() || !controller->isCurrentGeneration(generation)) {
                    return;
                }
                Q_EMIT controller->errorOccurred(message);
            }, Qt::QueuedConnection);
        }
        return;
    }

    const char *rawText = speech_recognition_result_get_text(result);
    const QString text = rawText ? QString::fromUtf8(rawText) : QString();
    if (text.isEmpty()) {
        return;
    }

    const SpeechResultReason reason = speech_recognition_result_get_reason(result);
    QObject *app = QCoreApplication::instance();
    if (!app) {
        return;
    }
    if (reason == SPEECH_RECOGNIZING) {
        QMetaObject::invokeMethod(app, [controller, alive, generation, text]() {
            if (!alive->load() || !controller->isCurrentGeneration(generation)) {
                return;
            }
            Q_EMIT controller->partialText(text);
        }, Qt::QueuedConnection);
    } else if (reason == SPEECH_RECOGNIZED) {
        QMetaObject::invokeMethod(app, [controller, alive, generation, text]() {
            if (!alive->load() || !controller->isCurrentGeneration(generation)) {
                return;
            }
            Q_EMIT controller->finalText(text);
        }, Qt::QueuedConnection);
    }
}
