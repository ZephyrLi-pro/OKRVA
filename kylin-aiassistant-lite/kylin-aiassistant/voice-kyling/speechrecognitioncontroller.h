#ifndef SPEECHRECOGNITIONCONTROLLER_H
#define SPEECHRECOGNITIONCONTROLLER_H

#include <QObject>
#include <QAtomicInteger>

#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/recognizer.h>
#include <kylin-ai/coreai/speech/result.h>

#include <atomic>
#include <memory>

class SpeechRecognitionController : public QObject
{
    Q_OBJECT

public:
    explicit SpeechRecognitionController(QObject *parent = nullptr);
    ~SpeechRecognitionController();

    bool start();
    void stop();
    void cancelPendingStart();
    bool isRunning() const;
    bool isStopping() const;

signals:
    void recognitionStarted();
    void partialText(const QString &text);
    void finalText(const QString &text);
    void startFailed(const QString &message);
    void errorOccurred(const QString &message);

private:
    struct CallbackContext;

    void closeSession();
    void destroyAudioConfig();
    quint64 advanceGeneration();
    bool isCurrentGeneration(quint64 generation) const;
    static void recognitionCallback(SpeechRecognitionResult *result, void *userData);

    SpeechRecognitionSession *m_session;
    AudioConfig *m_audioConfig;
    std::shared_ptr<std::atomic_bool> m_alive;
    std::shared_ptr<CallbackContext> m_callbackContext;
    QAtomicInteger<quint64> m_generation;
    bool m_running;
    bool m_stopping;
    bool m_starting;
    bool m_pendingStart;
    bool m_stopRequestedWhileStarting;
};

#endif // SPEECHRECOGNITIONCONTROLLER_H
