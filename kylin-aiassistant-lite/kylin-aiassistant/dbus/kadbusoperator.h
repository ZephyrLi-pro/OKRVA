#ifndef KADBUSOPERATOR_H
#define KADBUSOPERATOR_H

#include <QObject>

class KADBusOperator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ukui.kylin_aiassistant")

public:
    explicit KADBusOperator(QObject *parent = nullptr);
    ~KADBusOperator();

signals:
    void sigShowOrHideView();
    void sigShowAIBall(bool visible);
    void sigNotifyHistoryChanged();
    void sigShowChatAndSendText(QString text);

public slots:
    void showOrHideView();
    void showAIBall(bool visible);
    QString AllocateVoiceSessionId();
    bool SaveVoiceChatMessage(QString sessionID, int msgIndex, QString message, int chatType, qlonglong operateTime, QString sessionUuid = QString());
    void NotifyHistoryChanged();
    bool NotifyVoiceTaskCompleted(QString sessionID, QString message);
    bool ShowChatAndSendText(QString text);
    QString GetVoiceContext();
};

#endif // KADBUSOPERATOR_H
