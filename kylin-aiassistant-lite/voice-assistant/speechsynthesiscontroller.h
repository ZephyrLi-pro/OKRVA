#ifndef SPEECHSYNTHESISCONTROLLER_H
#define SPEECHSYNTHESISCONTROLLER_H

#include <QObject>
#include <QQueue>
#include <QByteArray>
#include <QMutex>
#include <QAtomicInteger>
#include <QVector>
#include <QElapsedTimer>

#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/result.h>
#include <kylin-ai/coreai/speech/synthesizer.h>

#include <atomic>
#include <memory>

class SDL2AudioPlayerThread;
class QNetworkAccessManager;
class QNetworkReply;

class SpeechSynthesisController : public QObject
{
    Q_OBJECT

public:
    explicit SpeechSynthesisController(QObject *parent = nullptr);
    ~SpeechSynthesisController();

    bool speak(const QString &text);
    bool beginStream();
    bool enqueueText(const QString &text);
    void finishStream();
    void close();
    bool isReadingActive() const { return m_readingActive; }

signals:
    void readingStarted();
    void readingFinished();
    void streamTextNeeded();
    void errorOccurred(const QString &message);

private:
    friend class VoiceInputRegression;
    struct CallbackContext;
    struct PendingAudioSegment;

    bool ensureSessionAsync();
    void handleSessionReady(quint64 generation, SpeechSynthesizerSession *session, int errorCode);
    void destroyAudioConfig();
    static void synthesisCallback(SpeechSynthesisResult *result, void *userData);
    void handleSynthesisError(int errorCode);
    bool handleSynthesizedSegment(const QByteArray &audioData, int sampleRate, int channels);
    void handleSynthesisCompleted();
    void handlePlaybackFinished();
    bool startNextSynthesis();
    QString takeNextSynthesisText();
    void noteAudioQueued(const QByteArray &audioData, int sampleRate, int channels);
    bool releaseStreamPrefetchIfReady();
    bool startAdapterSynthesis();
    void handleAdapterError(const QString &message);
    void maybeFinishReading();
    void resetCurrentSegmentBuffer();
    void handlePlayerInitialized(bool success);
    quint64 advanceGeneration();
    bool isCurrentGeneration(quint64 generation) const;
    void retireCallbackContext();

    SpeechSynthesizerSession *m_session;
    AudioConfig *m_audioConfig;
    SDL2AudioPlayerThread *m_player;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_httpReply = nullptr;
    QByteArray m_httpAudio;
    QString m_adapterUrl;
    QString m_adapterModel;
    QString m_adapterVoice;
    QQueue<QString> m_pendingTexts;
    QQueue<PendingAudioSegment> m_pendingAudioSegments;
    std::shared_ptr<std::atomic_bool> m_alive;
    std::shared_ptr<CallbackContext> m_callbackContext;
    QVector<std::shared_ptr<CallbackContext>> m_retiredCallbackContexts;
    QByteArray m_currentSegmentBuffer;
    QMutex m_callbackMutex;
    int m_cachedSampleRate = 0;
    int m_cachedChannels = 0;
    QAtomicInteger<quint64> m_generation;
    bool m_sessionStarting = false;
    bool m_playerInitInFlight = false;
    bool m_acceptingStreamText;
    bool m_synthesisInFlight;
    bool m_audioPlaybackPending = false;
    bool m_readingActive;
    // Completed answers are synthesized in bounded requests, then played as
    // one PCM buffer. No speaker output may begin while another part is pending.
    bool m_bufferCompleteAnswer = false;
    QByteArray m_completeAnswerAudio;
    int m_completeAnswerRate = 0;
    int m_completeAnswerChannels = 0;
    int m_completeAnswerSegments = 0;
    QElapsedTimer m_speechClock;
    QElapsedTimer m_requestClock;
    int m_requestCharacters = 0;
    double m_synthesisMsPerCharacter = 0.0;
    qint64 m_audioQueuedUntilMs = 0;
    QQueue<PendingAudioSegment> m_streamPrefetch;
    bool m_streamPrefetching = false;
    qint64 m_streamPrefetchMs = 0;
    int m_streamPrefetchBytes = 0;
    QElapsedTimer m_textArrivalClock;
    int m_charactersAfterFirstArrival = 0;
    bool m_streamPlaybackLogged = false;
};

#endif // SPEECHSYNTHESISCONTROLLER_H
