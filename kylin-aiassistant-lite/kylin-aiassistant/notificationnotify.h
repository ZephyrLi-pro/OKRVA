#ifndef NOTIFICATIONNOTIFY_H
#define NOTIFICATIONNOTIFY_H

#include <QObject>
#include <QString>
#include <QDBusMessage>
#include <QList>
#include <QVariant>
#include <QMap>
#include <QStringList>
#include <ukui-log4qt.h>

class NotificationNotify : public QObject
{
    Q_OBJECT

public:
    static NotificationNotify* getInstance();
    void sendNotification(const QString &title, const QString &body, const QString &sessionId = "", bool includeActions = true);
    void closeNotification();

signals:
    void viewClicked(const QString &sessionId);

private slots:
    void onActionInvoked(quint32 id, const QString &actionKey);
    void onNotificationClosed(quint32 id, quint32 reason);

private:
    explicit NotificationNotify(QObject *parent = nullptr);
    ~NotificationNotify();
    NotificationNotify(const NotificationNotify&) = delete;
    NotificationNotify& operator=(const NotificationNotify&) = delete;

    QMap<quint32, QString> m_notificationSessionMap;
};

#endif
