#include "aiballcontroller.h"

#include "speechrecognitioncontroller.h"

#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QCoreApplication>
#include <QtMath>

namespace {
const int kFinalWaitMs = 650;

qreal clamp01(qreal value)
{
    return qMax<qreal>(0.0, qMin<qreal>(1.0, value));
}

}

AudioLevelDevice::AudioLevelDevice(QObject *parent)
    : QIODevice(parent)
{
}

qreal AudioLevelDevice::level() const
{
    return m_level;
}

void AudioLevelDevice::resetLevel()
{
    m_level = 0.0;
}

void AudioLevelDevice::setFormat(const QAudioFormat &format)
{
    m_format = format;
}

qint64 AudioLevelDevice::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data);
    Q_UNUSED(maxSize);
    return 0;
}

qint64 AudioLevelDevice::writeData(const char *data, qint64 maxSize)
{
    if (!data || maxSize <= 0 || !m_format.isValid()) {
        m_level *= 0.82;
        return maxSize;
    }

    qreal peak = 0.0;
    const int sampleSize = m_format.sampleSize();
    if (sampleSize == 16) {
        const qint16 *samples = reinterpret_cast<const qint16 *>(data);
        const int count = static_cast<int>(maxSize / sizeof(qint16));
        for (int i = 0; i < count; ++i) {
            peak = qMax(peak, qAbs(samples[i]) / 32768.0);
        }
    } else if (sampleSize == 8) {
        const uchar *samples = reinterpret_cast<const uchar *>(data);
        for (qint64 i = 0; i < maxSize; ++i) {
            peak = qMax(peak, qAbs(static_cast<int>(samples[i]) - 128) / 128.0);
        }
    }

    m_level = qMax(peak, m_level * 0.78);
    return maxSize;
}

AIBallController::AIBallController(QObject *parent)
    : QObject(parent)
    , m_speechController(new SpeechRecognitionController(this))
    , m_audioDevice(new AudioLevelDevice(this))
{
    connect(m_speechController, &SpeechRecognitionController::recognitionStarted, this, [this]() {
        setUiState(QStringLiteral("listening"));
    });
    connect(m_speechController, &SpeechRecognitionController::partialText, this, [this](const QString &text) {
        m_lastPartialText = text;
        updateSpeechPulseFromText(text);
        setRecognizedText(m_finalText + text);
    });
    connect(m_speechController, &SpeechRecognitionController::finalText, this, [this](const QString &text) {
        appendFinalText(text);
        m_lastPartialText.clear();
    });
    connect(m_speechController, &SpeechRecognitionController::startFailed, this, [this](const QString &message) {
        Q_UNUSED(message);
        setUiState(QStringLiteral("idle"));
        stopAudioLevelMeter();
    });
    connect(m_speechController, &SpeechRecognitionController::errorOccurred, this, [this](const QString &message) {
        Q_UNUSED(message);
        setUiState(QStringLiteral("idle"));
        stopAudioLevelMeter();
    });

    m_audioLevelTimer.setInterval(50);
    connect(&m_audioLevelTimer, &QTimer::timeout, this, &AIBallController::refreshAudioLevel);

    m_finishTimer.setSingleShot(true);
    connect(&m_finishTimer, &QTimer::timeout, this, &AIBallController::finishRecognitionAndHide);
}

AIBallController::~AIBallController()
{
    stopAudioLevelMeter();
}

bool AIBallController::ballVisible() const
{
    return m_ballVisible;
}

QString AIBallController::uiState() const
{
    return m_uiState;
}

qreal AIBallController::audioLevel() const
{
    return m_audioLevel;
}

qreal AIBallController::speechPulse() const
{
    return m_speechPulse;
}

QString AIBallController::recognizedText() const
{
    return m_recognizedText;
}

void AIBallController::showBall()
{
    m_finishTimer.stop();
    m_submitReason = NoSubmit;
    m_pendingSubmitText.clear();
    resetRecognitionText();
    setUiState(QStringLiteral("waking"));
    setBallVisible(true);
    startRecognition();
}

void AIBallController::hideBall()
{
    if (!m_ballVisible && m_uiState == QStringLiteral("idle")) {
        return;
    }

    stopRecognitionForSubmit();
    setUiState(QStringLiteral("recognizing"));
    m_finishTimer.start(kFinalWaitMs);
}

void AIBallController::hideAnimationFinished()
{
    if (m_submitReason != SubmitAfterHide) {
        return;
    }

    m_submitReason = NoSubmit;
    const QString text = m_pendingSubmitText.trimmed();
    m_pendingSubmitText.clear();
    setUiState(QStringLiteral("idle"));

    if (!text.isEmpty()) {
        submitTextToAssistant(text);
    }
}

void AIBallController::refreshAudioLevel()
{
    qreal level = m_audioDevice ? m_audioDevice->level() : 0.0;
    if (m_lastTextPulseAt.isValid() && m_lastTextPulseAt.msecsTo(QDateTime::currentDateTime()) < 220) {
        level = qMax(level, m_speechPulse);
    } else {
        setSpeechPulse(m_speechPulse * 0.86);
    }

    setAudioLevel(level);
}

void AIBallController::setBallVisible(bool visible)
{
    if (m_ballVisible == visible) {
        return;
    }
    m_ballVisible = visible;
    Q_EMIT ballVisibleChanged();
}

void AIBallController::setUiState(const QString &state)
{
    if (m_uiState == state) {
        return;
    }
    m_uiState = state;
    Q_EMIT uiStateChanged();
}

void AIBallController::setAudioLevel(qreal level)
{
    level = clamp01(level);
    if (qAbs(m_audioLevel - level) < 0.01) {
        return;
    }
    m_audioLevel = level;
    Q_EMIT audioLevelChanged();
}

void AIBallController::setSpeechPulse(qreal pulse)
{
    pulse = clamp01(pulse);
    if (qAbs(m_speechPulse - pulse) < 0.01) {
        return;
    }
    m_speechPulse = pulse;
    Q_EMIT speechPulseChanged();
}

void AIBallController::setRecognizedText(const QString &text)
{
    if (m_recognizedText == text) {
        return;
    }
    m_recognizedText = text;
    Q_EMIT recognizedTextChanged();
}

void AIBallController::appendFinalText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (m_finalText.trimmed().isEmpty()) {
        m_finalText = trimmed;
    } else if (m_finalText.endsWith(trimmed) || trimmed == m_finalText) {
        return;
    } else if (trimmed.startsWith(m_finalText)) {
        m_finalText = trimmed;
    } else {
        m_finalText += trimmed;
    }
    setRecognizedText(m_finalText);
    updateSpeechPulseFromText(trimmed);
}

void AIBallController::resetRecognitionText()
{
    m_finalText.clear();
    m_lastPartialText.clear();
    setRecognizedText(QString());
}

void AIBallController::startRecognition()
{
    startAudioLevelMeter();
    m_speechController->cancelPendingStart();
    m_speechController->start();
}

void AIBallController::stopRecognitionForSubmit()
{
    m_speechController->cancelPendingStart();
    if (m_speechController->isRunning()) {
        m_speechController->stop();
    }
    stopAudioLevelMeter();
}

void AIBallController::finishRecognitionAndHide()
{
    QString text = m_recognizedText.trimmed();
    if (text.isEmpty()) {
        text = m_lastPartialText.trimmed();
    }

    m_pendingSubmitText = text;
    m_submitReason = SubmitAfterHide;
    setUiState(QStringLiteral("done"));
    setBallVisible(false);
}

void AIBallController::submitTextToAssistant(const QString &text)
{
    Q_EMIT textSubmitted(text);
}

void AIBallController::startAudioLevelMeter()
{
    stopAudioLevelMeter();

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    const QAudioDeviceInfo inputDevice = QAudioDeviceInfo::defaultInputDevice();
    if (!inputDevice.isNull() && !inputDevice.isFormatSupported(format)) {
        format = inputDevice.nearestFormat(format);
    }

    m_audioDevice->setFormat(format);
    m_audioDevice->resetLevel();
    m_audioDevice->open(QIODevice::WriteOnly);

    m_audioInput = new QAudioInput(format, this);
    m_audioInput->setBufferSize(2048);
    m_audioInput->start(m_audioDevice);
    m_audioLevelTimer.start();
}

void AIBallController::stopAudioLevelMeter()
{
    m_audioLevelTimer.stop();
    if (m_audioInput) {
        m_audioInput->stop();
        m_audioInput->deleteLater();
        m_audioInput = nullptr;
    }
    if (m_audioDevice) {
        m_audioDevice->close();
        m_audioDevice->resetLevel();
    }
    setAudioLevel(0.0);
    setSpeechPulse(0.0);
}

void AIBallController::updateSpeechPulseFromText(const QString &text)
{
    const qreal pulse = qMin<qreal>(1.0, 0.35 + text.trimmed().length() / 24.0);
    m_lastTextPulseAt = QDateTime::currentDateTime();
    setSpeechPulse(pulse);
}
