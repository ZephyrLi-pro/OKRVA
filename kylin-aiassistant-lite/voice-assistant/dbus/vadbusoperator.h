#ifndef VADBUSOPERATOR_H
#define VADBUSOPERATOR_H

#include <QObject>
#include <QString>
#include <functional>

class VADBusOperator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ukui.voice_assistant")

public:
    explicit VADBusOperator(QObject *parent = nullptr);
    ~VADBusOperator();

    static QString pidFilePath();
    void setPrepareListening(std::function<QString()> callback) { m_prepareListening = callback; }
    void setInputStatus(std::function<QString()> callback) { m_inputStatus = callback; }
    void setPrepareFollowup(std::function<QString()> callback) { m_prepareFollowup = callback; }
    void setEndConversation(std::function<void()> callback) { m_endConversation = callback; }
    void setReadingContextHandler(std::function<bool(const QString &, const QString &, int)> callback) { m_readingContext = callback; }

    enum ExitReason {
        CloseButton = 0,
        Abnormal = 1
    };

signals:
    void ProcessExiting(int reason);
    void sigShowWindow();
    void sigShowExternalListening();
    void sigSubmitExternalText(const QString &text);
    void sigExternalRecognitionFailed(const QString &code);
    void sigQuit();

public slots:
    void ShowWindow();
    void ShowExternalListening();
    QString PrepareExternalListening();
    QString GetInputStatus();
    QString PrepareFollowupListening();
    void EndExternalConversation();
    bool SetReadingContext(const QString &title, const QString &text, int page);
    void SubmitExternalText(const QString &text);
    void ExternalRecognitionFailed(const QString &code);
    void Quit();
    bool Ping();
    qlonglong ProcessId();
private:
    std::function<QString()> m_prepareListening;
    std::function<QString()> m_inputStatus;
    std::function<QString()> m_prepareFollowup;
    std::function<void()> m_endConversation;
    std::function<bool(const QString &, const QString &, int)> m_readingContext;
};

#endif // VADBUSOPERATOR_H
