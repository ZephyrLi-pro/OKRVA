#ifndef KADBUSCLIENT_H
#define KADBUSCLIENT_H

#include <QObject>
#include <QDBusInterface>

class KADBusClient : public QObject
{
    Q_OBJECT

public:
    explicit KADBusClient(QObject *parent = nullptr);
    ~KADBusClient();

    void allocateVoiceSessionIdAsync();
    void saveVoiceChatMessageAsync(const QString &sessionID, int msgIndex,
                                   const QString &message, int chatType,
                                   qlonglong operateTime,
                                   const QString &sessionUuid = QString());
    void notifyHistoryChangedAsync();
    void notifyVoiceTaskCompletedAsync(const QString &sessionID, const QString &message);
    static QString serviceName() { return s_serviceName; }

signals:
    void voiceSessionIdAllocated(const QString &sessionID);
    void voiceChatMessageSaved(int msgIndex, bool success);
    void voiceTaskCompletedNotified(bool success);

private:
    static const QString s_serviceName;
    static const QString s_objectPath;
    static const QString s_interfaceName;
};

#endif // KADBUSCLIENT_H
