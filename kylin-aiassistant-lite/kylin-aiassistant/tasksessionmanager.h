#ifndef TASKSESSIONMANAGER_H
#define TASKSESSIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "tasksession.h"
#include "global/kaglobal.h"

class TaskSessionManager : public QObject {
    Q_OBJECT

public:
    static const int MAX_CONCURRENT_SESSIONS = 5;

    explicit TaskSessionManager(QObject* parent = nullptr);
    ~TaskSessionManager();

    QString createSession(const QString& uuid = QString(), int historyId = 0);
    void activateSession(const QString& sessionUuid);
    QString getActiveSessionUuid() const;
    TaskSession* getSession(const QString& sessionUuid);
    TaskSession* getSessionByHistoryId(int historyId);
    void sendMessage(const QString& sessionUuid, const QString& content);
    void cancelResponse(const QString& sessionUuid);
    void closeSession(const QString& sessionUuid);
    void deleteBackendSession(const QString& sessionUuid);
    void deactivateSession();
    QList<TaskSession> getAllSessions() const;
    int sessionCount() const;

signals:
    void sessionCreated(const QString& sessionUuid);
    void sessionActivated(const QString& sessionUuid);
    void sessionClosed(const QString& sessionUuid);
    void reply(const QString& sessionUuid, const InnerMessage& message);
    void sessionLimitReached();

private:
    void destroySession(const QString& sessionUuid);

    QMap<QString, TaskSession> m_sessions = {};
    QString m_activeSessionUuid = "";
    QNetworkAccessManager* m_networkManager = nullptr;
};

#endif
