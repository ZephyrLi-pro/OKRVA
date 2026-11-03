#include "speechsynthesiscontroller.h"
#include "speechtextsegments.h"
#include "speechpcmedges.h"

#include "sdl2audioplayerthread.h"
#include "speecherrormessage.h"
#include "ttswavdecoder.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <thread>
#include <algorithm>

namespace {
const int kMaxCompleteAnswerAudioBytes = 32 * 1024 * 1024;

QStringList completedSpeechParts(const QString &text)
{
    return SpeechText::splitFinal(text);
}

void destroySynthesisSessionAsync(SpeechSynthesizerSession *session,
                                  std::shared_ptr<void> context)
{
    if (!session) {
        return;
    }

    std::thread([session, context]() {
        Q_UNUSED(context);
        SpeechSynthesizerSession *sessionToDestroy = session;
        speech_synthesizer_destroy_session(&sessionToDestroy);
    }).detach();
}
} // namespace

struct SpeechSynthesisController::CallbackContext
{
    SpeechSynthesisController *controller = nullptr;
    std::shared_ptr<std::atomic_bool> alive;
    quint64 generation = 0;
};

struct SpeechSynthesisController::PendingAudioSegment
{
    QByteArray audioData;
    int sampleRate = 0;
    int channels = 0;
};

SpeechSynthesisController::SpeechSynthesisController(QObject *parent)
    : QObject(parent)
    , m_session(nullptr)
    , m_audioConfig(nullptr)
    , m_player(new SDL2AudioPlayerThread(this))
    , m_alive(std::make_shared<std::atomic_bool>(true))
    , m_generation(0)
    , m_acceptingStreamText(false)
    , m_synthesisInFlight(false)
    , m_audioPlaybackPending(false)
    , m_readingActive(false)
{
    m_speechClock.start();
    QSettings ttsSettings(QDir::homePath() + "/.config/kylin-aiassistant/tts.ini",
                          QSettings::IniFormat);
    m_adapterUrl = qEnvironmentVariable("KYLINBOT_TTS_URL",
                                        ttsSettings.value("TTS/url").toString());
    m_adapterModel = ttsSettings.value("TTS/model", "vits-zh").toString();
    m_adapterVoice = ttsSettings.value("TTS/voice", "zh-33").toString();
    m_network = new QNetworkAccessManager(this);
    connect(m_player, &SDL2AudioPlayerThread::playbackFinished,
            this, &SpeechSynthesisController::handlePlaybackFinished);
    connect(m_player, &SDL2AudioPlayerThread::initialized,
            this, &SpeechSynthesisController::handlePlayerInitialized);
}

SpeechSynthesisController::~SpeechSynthesisController()
{
    m_alive->store(false);
    close();
    m_retiredCallbackContexts.clear();
}

bool SpeechSynthesisController::speak(const QString &text)
{
    const QStringList parts = completedSpeechParts(text);
    if (parts.isEmpty()) { close(); return false; }
    if (!beginStream()) {
        return false;
    }
    m_bufferCompleteAnswer = true;
    // Queue every part before starting synthesis, so no early completion can
    // mistake a temporary empty text queue for the end of this answer.
    for (const QString &part : parts) m_pendingTexts.enqueue(part);
    m_acceptingStreamText = false;
    if (!startNextSynthesis()) {
        close();
        return false;
    }
    return true;
}

bool SpeechSynthesisController::beginStream()
{
    close();

    m_acceptingStreamText = true;
    m_streamPrefetching = true;
    m_player->setExpectingMoreData(true);
    return ensureSessionAsync();
}

bool SpeechSynthesisController::enqueueText(const QString &text)
{
    const QString trimmed = text.trimmed();
    const bool readable = std::any_of(trimmed.begin(), trimmed.end(), [](QChar ch) { return ch.isLetterOrNumber(); });
    if (!readable) {
        return true;
    }

    if (!m_acceptingStreamText && !m_readingActive) {
        if (!beginStream()) {
            return false;
        }
    }

    if (!m_textArrivalClock.isValid()) {
        m_textArrivalClock.start();
    } else {
        m_charactersAfterFirstArrival += trimmed.size();
    }
    for (const QString &part : SpeechText::splitFinal(trimmed)) m_pendingTexts.enqueue(part);
    if (!releaseStreamPrefetchIfReady()) return false;
    return startNextSynthesis();
}

void SpeechSynthesisController::finishStream()
{
    m_acceptingStreamText = false;
    if (!releaseStreamPrefetchIfReady()) return;
    maybeFinishReading();
}

bool SpeechSynthesisController::startNextSynthesis()
{
    if (m_synthesisInFlight || m_pendingTexts.isEmpty()) {
        return true;
    }

    if (!m_adapterUrl.isEmpty()) {
        return startAdapterSynthesis();
    }

    if (!m_session) {
        return ensureSessionAsync();
    }

    destroyAudioConfig();
    m_audioConfig = audio_config_create_audio_output_from_pcm_data();
    if (!m_audioConfig) {
        Q_EMIT errorOccurred(tr("Failed to create speech synthesis audio config."));
        return false;
    }

    audio_config_set_input_audio_rate(m_audioConfig, 16000);
    speech_synthesizer_set_audio_config(m_session, m_audioConfig);

    const QString trimmed = takeNextSynthesisText();
    resetCurrentSegmentBuffer();
    const QByteArray utf8Text = trimmed.toUtf8();
    speech_synthesizer_synthesize_text_async(m_session,
                                             utf8Text.constData(),
                                             static_cast<uint32_t>(utf8Text.size()));
    m_synthesisInFlight = true;
    if (!m_readingActive) {
        m_readingActive = true;
        Q_EMIT readingStarted();
    }
    return true;
}

QString SpeechSynthesisController::takeNextSynthesisText()
{
    if (m_bufferCompleteAnswer) return m_pendingTexts.dequeue();
    int budget = 24;
    const qint64 bufferedMs = qMax<qint64>(0, m_audioQueuedUntilMs - m_speechClock.elapsed());
    if (bufferedMs > 0 && m_synthesisMsPerCharacter > 0.0) {
        // Leave a margin for scheduling/network variation. Do not batch the
        // entire visible backlog into a synthesis that outlasts current audio.
        budget = qBound(8, int(bufferedMs * 0.75 / m_synthesisMsPerCharacter), 48);
    }
    // A complete clause remains indivisible even when longer than the target
    // budget. The budget only controls combining *whole* queued clauses.
    QString text = m_pendingTexts.dequeue();
    while (!m_pendingTexts.isEmpty() && text.size() + m_pendingTexts.head().size() <= budget) {
        text += m_pendingTexts.dequeue();
    }
    return text;
}

bool SpeechSynthesisController::startAdapterSynthesis()
{
    const QUrl url(m_adapterUrl);
    // This deployment is local-only. Never send assistant replies to a cloud
    // endpoint because a user config value was mistyped or redirected.
    if (!url.isValid() || url.scheme() != "http" || !url.userInfo().isEmpty()
            || (url.host() != "127.0.0.1" && url.host() != "localhost")) {
        handleAdapterError(tr("Local speech endpoint must use loopback HTTP."));
        return false;
    }
    const QString text = takeNextSynthesisText();
    m_requestCharacters = text.size();
    m_requestClock.restart();
    int pendingChars = 0;
    for (const QString &pending : m_pendingTexts) pendingChars += pending.size();
    qInfo() << "voice-assistant TTS request:" << "characters=" << text.size()
            << "queued_characters=" << pendingChars << "stream_open=" << m_acceptingStreamText;
    QJsonObject payload;
    payload.insert("model", m_adapterModel);
    payload.insert("voice", m_adapterVoice);
    payload.insert("input", text);
    payload.insert("response_format", "wav");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "audio/wav");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    const QByteArray token = qgetenv("KYLINBOT_TTS_TOKEN");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + token);
    }
    const quint64 generation = m_generation.loadAcquire();
    m_httpAudio.clear();
    m_synthesisInFlight = true;
    QNetworkReply *reply = m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_httpReply = reply;
    if (!m_readingActive) {
        m_readingActive = true;
        Q_EMIT readingStarted();
    }
    QTimer::singleShot(130000, reply, [this, reply, generation]() {
        if (isCurrentGeneration(generation) && m_httpReply == reply && !reply->isFinished()) {
            reply->setProperty("tts_timed_out", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, generation]() {
        if (!isCurrentGeneration(generation) || m_httpReply != reply) {
            return;
        }
        m_httpAudio.append(reply->readAll());
        if (m_httpAudio.size() > 32 * 1024 * 1024) {
            reply->setProperty("tts_audio_oversized", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, text]() {
        reply->deleteLater();
        if (!isCurrentGeneration(generation) || m_httpReply != reply) {
            return;
        }
        m_httpReply = nullptr;
        m_httpAudio.append(reply->readAll());
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray type = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        if (reply->error() != QNetworkReply::NoError || status != 200
                || !type.startsWith("audio/wav") || m_httpAudio.size() > 32 * 1024 * 1024) {
            const QString message = reply->property("tts_timed_out").toBool()
                    ? tr("Local speech synthesis timed out.")
                    : tr("Local speech synthesis failed (HTTP %1): %2").arg(status).arg(reply->errorString());
            handleAdapterError(message);
            return;
        }
        QByteArray pcm;
        int sampleRate = 0;
        int channels = 0;
        if (!TtsWav::decode(m_httpAudio, &pcm, &sampleRate, &channels)) {
            handleAdapterError(tr("Local speech service returned invalid PCM16 WAV."));
            return;
        }
        m_httpAudio.clear();
        const int originalBytes = pcm.size();
        const bool comma = !text.isEmpty() && QStringLiteral("，,、").contains(text.back());
        pcm = SpeechPcm::trimEdges(pcm, sampleRate, channels, comma ? 120 : 250);
        qInfo() << "voice-assistant TTS edge padding removed:" << originalBytes - pcm.size() << "bytes";
        if (m_requestCharacters > 0 && m_requestClock.isValid()) {
            const double observed = qMax(5.0, double(m_requestClock.elapsed()) / m_requestCharacters);
            m_synthesisMsPerCharacter = m_synthesisMsPerCharacter > 0.0
                    ? (m_synthesisMsPerCharacter + observed) * 0.5 : observed;
        }
        qInfo() << "voice-assistant local TTS audio ready:" << "bytes=" << pcm.size()
                << "rate=" << sampleRate << "channels=" << channels;
        if (!handleSynthesizedSegment(pcm, sampleRate, channels)) {
            close();
            return;
        }
        handleSynthesisCompleted();
    });
    return true;
}

void SpeechSynthesisController::handleAdapterError(const QString &message)
{
    close();
    Q_EMIT errorOccurred(message);
}

void SpeechSynthesisController::close()
{
    advanceGeneration();
    QNetworkReply *reply = m_httpReply;
    m_httpReply = nullptr;
    if (reply) {
        reply->abort();
    }
    m_httpAudio.clear();
    m_pendingTexts.clear();
    m_audioQueuedUntilMs = 0;
    m_streamPrefetch.clear();
    m_streamPrefetching = false;
    m_streamPrefetchMs = 0;
    m_streamPrefetchBytes = 0;
    m_textArrivalClock.invalidate();
    m_charactersAfterFirstArrival = 0;
    m_streamPlaybackLogged = false;
    m_requestClock.invalidate();
    m_requestCharacters = 0;
    m_bufferCompleteAnswer = false;
    m_completeAnswerAudio.clear();
    m_completeAnswerRate = 0;
    m_completeAnswerChannels = 0;
    m_completeAnswerSegments = 0;
    m_acceptingStreamText = false;
    m_synthesisInFlight = false;
    m_audioPlaybackPending = false;
    m_readingActive = false;
    m_sessionStarting = false;
    m_playerInitInFlight = false;
    m_pendingAudioSegments.clear();
    resetCurrentSegmentBuffer();

    destroyAudioConfig();

    if (m_session) {
        SpeechSynthesizerSession *sessionToDestroy = m_session;
        auto context = m_callbackContext;
        m_session = nullptr;
        retireCallbackContext();
        destroySynthesisSessionAsync(sessionToDestroy, context);
    } else {
        retireCallbackContext();
    }

    if (m_player) {
        m_player->setExpectingMoreData(false);
        m_player->clearQueue();
        m_player->stop();
    }
}

bool SpeechSynthesisController::ensureSessionAsync()
{
    if (!m_adapterUrl.isEmpty()) {
        return true;
    }
    if (m_session) {
        return true;
    }
    if (m_sessionStarting) {
        return true;
    }

    const quint64 generation = m_generation.loadAcquire();
    m_sessionStarting = true;
    std::shared_ptr<std::atomic_bool> alive = m_alive;
    std::thread([this, alive, generation]() {
        SpeechSynthesizerSession *session = speech_synthesizer_create_session();
        int errorCode = 0;
        if (!session) {
            errorCode = -10000;
        } else {
            errorCode = speech_synthesizer_init_session(session);
        }

        QObject *app = QCoreApplication::instance();
        if (!app || !alive->load()) {
            destroySynthesisSessionAsync(session, nullptr);
            return;
        }

        QMetaObject::invokeMethod(app, [this, alive, generation, session, errorCode]() {
            if (!alive->load() || !isCurrentGeneration(generation)) {
                destroySynthesisSessionAsync(session, nullptr);
                return;
            }
            handleSessionReady(generation, session, errorCode);
        }, Qt::QueuedConnection);
    }).detach();

    return true;
}

void SpeechSynthesisController::handleSessionReady(quint64 generation,
                                                   SpeechSynthesizerSession *session,
                                                   int errorCode)
{
    m_sessionStarting = false;
    if (!isCurrentGeneration(generation)) {
        destroySynthesisSessionAsync(session, nullptr);
        return;
    }

    if (!session) {
        m_synthesisInFlight = false;
        m_readingActive = false;
        m_acceptingStreamText = false;
        m_audioPlaybackPending = false;
        m_pendingTexts.clear();
        if (m_player) {
            m_player->setExpectingMoreData(false);
            m_player->clearQueue();
            m_player->stop();
        }
        Q_EMIT errorOccurred(tr("Failed to create speech synthesis session."));
        return;
    }

    if (errorCode != 0) {
        if (errorCode == -10000) {
            Q_EMIT errorOccurred(tr("Failed to create speech synthesis session."));
        } else {
            handleSynthesisError(errorCode);
        }
        destroySynthesisSessionAsync(session, nullptr);
        return;
    }

    m_session = session;
    retireCallbackContext();
    m_callbackContext = std::make_shared<CallbackContext>();
    m_callbackContext->controller = this;
    m_callbackContext->alive = m_alive;
    m_callbackContext->generation = m_generation.loadAcquire();
    speech_synthesizer_result_set_callback(m_session, synthesisCallback, m_callbackContext.get());
    startNextSynthesis();
}

void SpeechSynthesisController::destroyAudioConfig()
{
    if (!m_audioConfig) {
        return;
    }

    AudioConfig *configToDestroy = m_audioConfig;
    audio_config_destroy(&configToDestroy);
    m_audioConfig = nullptr;
}

void SpeechSynthesisController::synthesisCallback(SpeechSynthesisResult *result, void *userData)
{
    CallbackContext *context = static_cast<CallbackContext *>(userData);
    SpeechSynthesisController *controller = context ? context->controller : nullptr;
    if (!context || !context->alive || !context->alive->load() || !controller || !result) {
        return;
    }
    const quint64 generation = context->generation;
    const auto alive = context->alive;
    if (!controller->isCurrentGeneration(generation)) {
        return;
    }

    const int errorCode = speech_synthesis_result_get_error_code(result);
    if (errorCode != 0 && errorCode != -1) {
        QObject *app = QCoreApplication::instance();
        if (!app) {
            return;
        }
        QMetaObject::invokeMethod(app, [controller, alive, generation, errorCode]() {
            if (!alive->load() || !controller->isCurrentGeneration(generation)) {
                return;
            }
            controller->handleSynthesisError(errorCode);
        }, Qt::QueuedConnection);
        return;
    }

    const SpeechResultReason reason = speech_synthesis_result_get_reason(result);
    if (reason == SPEECH_SYNTHESIZING
            || reason == SPEECH_SYNTHESIS_STARTED
            || reason == SPEECH_SYNTHESIS_COMPLETED) {
        const int sampleRate = speech_synthesis_result_get_audio_rate(result);
        const int channels = speech_synthesis_result_get_audio_channel(result);
        if (sampleRate > 0 && channels > 0) {
            QMutexLocker locker(&controller->m_callbackMutex);
            if (!controller->isCurrentGeneration(generation)) {
                return;
            }
            if (controller->m_cachedSampleRate <= 0 || controller->m_cachedChannels <= 0) {
                controller->m_cachedSampleRate = sampleRate;
                controller->m_cachedChannels = channels;
            }
        }
    }

    uint32_t dataLength = 0;
    const uint8_t *pcmData = speech_synthesis_result_get_data(result, &dataLength);
    if (pcmData && dataLength > 0) {
        QMutexLocker locker(&controller->m_callbackMutex);
        if (!controller->isCurrentGeneration(generation)) {
            return;
        }
        controller->m_currentSegmentBuffer.append(reinterpret_cast<const char *>(pcmData),
                                                  static_cast<int>(dataLength));
    }

    if (reason == SPEECH_SYNTHESIS_COMPLETED) {
        QByteArray segmentData;
        int sampleRate = 0;
        int channels = 0;
        {
            QMutexLocker locker(&controller->m_callbackMutex);
            if (!controller->isCurrentGeneration(generation)) {
                return;
            }
            segmentData = controller->m_currentSegmentBuffer;
            controller->m_currentSegmentBuffer.clear();
            sampleRate = controller->m_cachedSampleRate;
            channels = controller->m_cachedChannels;
            controller->m_cachedSampleRate = 0;
            controller->m_cachedChannels = 0;
        }

        QObject *app = QCoreApplication::instance();
        if (!app) {
            return;
        }
        QMetaObject::invokeMethod(app, [controller, alive, generation, segmentData, sampleRate, channels]() {
            if (!alive->load() || !controller->isCurrentGeneration(generation)) {
                return;
            }
            if (!controller->handleSynthesizedSegment(segmentData, sampleRate, channels)) {
                controller->m_synthesisInFlight = false;
                controller->m_readingActive = false;
                controller->m_acceptingStreamText = false;
                controller->m_audioPlaybackPending = false;
                controller->m_pendingTexts.clear();
                controller->maybeFinishReading();
                return;
            }
            controller->handleSynthesisCompleted();
        }, Qt::QueuedConnection);
    }
}

void SpeechSynthesisController::handleSynthesisError(int errorCode)
{
    // Invalidate late SDK callbacks and discard the rolling prefix as well as
    // already queued output before reporting the synthesis failure.
    close();
    m_bufferCompleteAnswer = false;
    m_completeAnswerAudio.clear();
    m_completeAnswerRate = 0;
    m_completeAnswerChannels = 0;
    m_completeAnswerSegments = 0;
    m_synthesisInFlight = false;
    m_readingActive = false;
    m_acceptingStreamText = false;
    m_audioPlaybackPending = false;
    m_pendingTexts.clear();
    resetCurrentSegmentBuffer();
    if (m_player) {
        m_player->setExpectingMoreData(false);
        m_player->clearQueue();
        m_player->stop();
    }
    const QString mapped = speechErrorMessageForCode(errorCode);
    Q_EMIT errorOccurred(mapped.isEmpty()
                                 ? tr("Speech synthesis error: %1").arg(errorCode)
                                 : mapped);
}

bool SpeechSynthesisController::handleSynthesizedSegment(const QByteArray &audioData, int sampleRate, int channels)
{
    if (audioData.isEmpty()) {
        return true;
    }

    if (sampleRate <= 0 || channels <= 0) {
        Q_EMIT errorOccurred(tr("Failed to read speech synthesis audio format."));
        return false;
    }

    if (m_bufferCompleteAnswer) {
        if ((m_completeAnswerRate && (m_completeAnswerRate != sampleRate || m_completeAnswerChannels != channels))
                || audioData.size() > kMaxCompleteAnswerAudioBytes - m_completeAnswerAudio.size()) {
            handleAdapterError(tr("Complete speech audio exceeds its buffer limit or changes format."));
            return false;
        }
        m_completeAnswerRate = sampleRate;
        m_completeAnswerChannels = channels;
        m_completeAnswerAudio.append(audioData);
        ++m_completeAnswerSegments;
        return true;
    }

    if (m_streamPrefetching) {
        if (audioData.size() > kMaxCompleteAnswerAudioBytes - m_streamPrefetchBytes) {
            handleAdapterError(tr("Speech prefetch buffer limit exceeded."));
            return false;
        }
        PendingAudioSegment segment;
        segment.audioData = audioData; segment.sampleRate = sampleRate; segment.channels = channels;
        m_streamPrefetch.enqueue(segment);
        m_streamPrefetchBytes += audioData.size();
        m_streamPrefetchMs += qint64(audioData.size()) * 1000 / (qint64(sampleRate) * channels * 2);
        return releaseStreamPrefetchIfReady();
    }

    if (!m_player->isInitialized()) {
        PendingAudioSegment segment;
        segment.audioData = audioData;
        segment.sampleRate = sampleRate;
        segment.channels = channels;
        m_pendingAudioSegments.enqueue(segment);
        m_audioPlaybackPending = true;

        if (!m_playerInitInFlight && !m_player->isInitializing()) {
            m_playerInitInFlight = true;
            m_player->initAsync(sampleRate, channels, AUDIO_S16LSB);
        }
        return true;
    }

    noteAudioQueued(audioData, sampleRate, channels);
    m_player->enqueueData(audioData);
    m_audioPlaybackPending = true;
    if (!m_player->isPlaying()) {
        m_player->play();
    }
    return true;
}

void SpeechSynthesisController::handlePlayerInitialized(bool success)
{
    if (!m_playerInitInFlight && m_pendingAudioSegments.isEmpty()) {
        return;
    }

    m_playerInitInFlight = false;
    if (!success) {
        m_pendingAudioSegments.clear();
        m_synthesisInFlight = false;
        m_readingActive = false;
        m_acceptingStreamText = false;
        m_audioPlaybackPending = false;
        m_pendingTexts.clear();
        Q_EMIT errorOccurred(tr("Failed to initialize SDL audio output."));
        maybeFinishReading();
        return;
    }

    while (!m_pendingAudioSegments.isEmpty()) {
        const PendingAudioSegment segment = m_pendingAudioSegments.dequeue();
        noteAudioQueued(segment.audioData, segment.sampleRate, segment.channels);
        m_player->enqueueData(segment.audioData);
    }

    m_audioPlaybackPending = true;
    if (!m_player->isPlaying()) {
        m_player->play();
    }
    maybeFinishReading();
}

void SpeechSynthesisController::handleSynthesisCompleted()
{
    m_synthesisInFlight = false;
    if (m_acceptingStreamText && m_pendingTexts.isEmpty()) {
        // Request already generated UI text now, without waiting for another
        // Gateway chunk. The slot only enqueues; synthesis stays asynchronous.
        Q_EMIT streamTextNeeded();
    }
    if (m_bufferCompleteAnswer && !m_acceptingStreamText && m_pendingTexts.isEmpty()) {
        QByteArray completeAudio;
        completeAudio.swap(m_completeAnswerAudio);
        const int rate = m_completeAnswerRate, channels = m_completeAnswerChannels;
        qInfo() << "voice-assistant complete speech audio ready:" << "segments=" << m_completeAnswerSegments
                << "bytes=" << completeAudio.size() << "rate=" << rate << "channels=" << channels;
        m_bufferCompleteAnswer = false;
        m_completeAnswerRate = 0;
        m_completeAnswerChannels = 0;
        m_completeAnswerSegments = 0;
        if (completeAudio.isEmpty()) {
            handleAdapterError(tr("Speech synthesis returned no audio for the complete answer."));
            return;
        }
        if (!handleSynthesizedSegment(completeAudio, rate, channels)) {
            close();
            return;
        }
    }
    startNextSynthesis();
    maybeFinishReading();
}

void SpeechSynthesisController::noteAudioQueued(const QByteArray &data, int rate, int channels)
{
    if (rate <= 0 || channels <= 0) return;
    const qint64 duration = qint64(data.size()) * 1000 / (qint64(rate) * channels * 2);
    if (!m_streamPlaybackLogged) {
        qInfo() << "voice-assistant stream playback begins:" << "buffered_milliseconds=" << duration;
        m_streamPlaybackLogged = true;
    }
    m_audioQueuedUntilMs = qMax(m_audioQueuedUntilMs, m_speechClock.elapsed()) + duration;
}

bool SpeechSynthesisController::releaseStreamPrefetchIfReady()
{
    if (!m_streamPrefetching || m_streamPrefetch.isEmpty()) return true;
    // Fast/burst input needs little lead. Slow decoding needs a longer rolling
    // prefix because speech consumes text faster than the model produces it.
    double rate = 0.0;
    if (m_textArrivalClock.isValid() && m_textArrivalClock.elapsed() > 0)
        rate = m_charactersAfterFirstArrival * 1000.0 / m_textArrivalClock.elapsed();
    const qint64 target = rate >= 4.0 ? 1500 : rate >= 2.0 ? 6000 : 10000;
    if (m_acceptingStreamText && m_streamPrefetchMs < target) return true;
    const int sampleRate = m_streamPrefetch.head().sampleRate;
    const int channels = m_streamPrefetch.head().channels;
    QByteArray audio;
    audio.reserve(m_streamPrefetchBytes);
    while (!m_streamPrefetch.isEmpty()) {
        const PendingAudioSegment segment = m_streamPrefetch.dequeue();
        if (segment.sampleRate != sampleRate || segment.channels != channels) {
            handleAdapterError(tr("Speech format changed inside the prefetch buffer."));
            return false;
        }
        audio.append(segment.audioData);
    }
    qInfo() << "voice-assistant rolling speech prefix ready:" << "milliseconds=" << m_streamPrefetchMs
            << "input_characters_per_second=" << rate << "stream_open=" << m_acceptingStreamText;
    m_streamPrefetching = false;
    m_streamPrefetchMs = 0;
    m_streamPrefetchBytes = 0;
    return handleSynthesizedSegment(audio, sampleRate, channels);
}

void SpeechSynthesisController::handlePlaybackFinished()
{
    if (m_player) {
        m_player->stop();
    }
    m_audioPlaybackPending = false;
    const bool waitingForText = m_acceptingStreamText || m_synthesisInFlight || !m_pendingTexts.isEmpty();
    if (waitingForText || !m_readingActive) {
        return;
    }

    m_readingActive = false;
    Q_EMIT readingFinished();
}

void SpeechSynthesisController::resetCurrentSegmentBuffer()
{
    QMutexLocker locker(&m_callbackMutex);
    m_currentSegmentBuffer.clear();
    m_cachedSampleRate = 0;
    m_cachedChannels = 0;
}

void SpeechSynthesisController::maybeFinishReading()
{
    const bool waitingForText = m_acceptingStreamText || m_synthesisInFlight || !m_pendingTexts.isEmpty();
    const bool hasBufferedAudio = m_player && m_player->hasBufferedAudio();
    if (m_player) {
        m_player->setExpectingMoreData(waitingForText);
    }
    if (waitingForText || m_audioPlaybackPending || hasBufferedAudio || !m_readingActive) {
        return;
    }
    if (m_player && m_player->isPlaying()) {
        return;
    }

    m_readingActive = false;
    Q_EMIT readingFinished();
}

quint64 SpeechSynthesisController::advanceGeneration()
{
    return ++m_generation;
}

bool SpeechSynthesisController::isCurrentGeneration(quint64 generation) const
{
    return generation == m_generation.loadAcquire();
}

void SpeechSynthesisController::retireCallbackContext()
{
    if (!m_callbackContext) {
        return;
    }

    m_callbackContext->controller = nullptr;
    m_retiredCallbackContexts.append(m_callbackContext);
    m_callbackContext.reset();
}
