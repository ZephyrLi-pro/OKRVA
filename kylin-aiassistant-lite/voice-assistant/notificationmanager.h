#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QString>

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager();

    bool sendNotification(const QString &title, const QString &message, int urgency = 1, int timeout = 6000);

private:
    QDBusInterface *m_iface = nullptr;
};

#endif // NOTIFICATIONMANAGER_H
