#ifndef AIBALLCONTROLLER_H
#define AIBALLCONTROLLER_H

#include <QAudioFormat>
#include <QBuffer>
#include <QDateTime>
#include <QIODevice>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QAudioInput;
class SpeechRecognitionController;

class AudioLevelDevice : public QIODevice
{
    Q_OBJECT

public:
    explicit AudioLevelDevice(QObject *parent = nullptr);
    qreal level() const;
    void resetLevel();

    void setFormat(const QAudioFormat &format);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    QAudioFormat m_format;
    qreal m_level = 0.0;
};

class AIBallController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ballVisible READ ballVisible NOTIFY ballVisibleChanged)
    Q_PROPERTY(QString uiState READ uiState NOTIFY uiStateChanged)
    Q_PROPERTY(qreal audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(qreal speechPulse READ speechPulse NOTIFY speechPulseChanged)
    Q_PROPERTY(QString recognizedText READ recognizedText NOTIFY recognizedTextChanged)

public:
    explicit AIBallController(QObject *parent = nullptr);
    ~AIBallController();

    bool ballVisible() const;
    QString uiState() const;
    qreal audioLevel() const;
    qreal speechPulse() const;
    QString recognizedText() const;

    Q_INVOKABLE void hideAnimationFinished();

public slots:
    void showBall();
    void hideBall();

signals:
    void ballVisibleChanged();
    void uiStateChanged();
    void audioLevelChanged();
    void speechPulseChanged();
    void recognizedTextChanged();
    void textSubmitted(const QString &text);

private slots:
    void refreshAudioLevel();

private:
    enum SubmitReason {
        NoSubmit,
        SubmitAfterHide
    };

    void setBallVisible(bool visible);
    void setUiState(const QString &state);
    void setAudioLevel(qreal level);
    void setSpeechPulse(qreal pulse);
    void setRecognizedText(const QString &text);
    void appendFinalText(const QString &text);
    void resetRecognitionText();
    void startRecognition();
    void stopRecognitionForSubmit();
    void finishRecognitionAndHide();
    void submitTextToAssistant(const QString &text);
    void startAudioLevelMeter();
    void stopAudioLevelMeter();
    void updateSpeechPulseFromText(const QString &text);

    SpeechRecognitionController *m_speechController = nullptr;
    QAudioInput *m_audioInput = nullptr;
    AudioLevelDevice *m_audioDevice = nullptr;
    QTimer m_audioLevelTimer;
    QTimer m_finishTimer;
    bool m_ballVisible = false;
    QString m_uiState = QStringLiteral("idle");
    qreal m_audioLevel = 0.0;
    qreal m_speechPulse = 0.0;
    QString m_recognizedText;
    QString m_finalText;
    QString m_lastPartialText;
    QString m_pendingSubmitText;
    SubmitReason m_submitReason = NoSubmit;
    QDateTime m_lastTextPulseAt;
};

#endif // AIBALLCONTROLLER_H
