#include "notificationmanager.h"

#include <QDebug>
#include <QList>
#include <QVariant>
#include <QStringList>
#include <QMap>
#include <QDBusConnection>
#include <QDBusMessage>

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
    , m_iface(new QDBusInterface(QStringLiteral("org.freedesktop.Notifications"),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QStringLiteral("org.freedesktop.Notifications"),
                                 QDBusConnection::sessionBus(),
                                 this))
{
}

NotificationManager::~NotificationManager()
{
}

bool NotificationManager::sendNotification(const QString &title, const QString &message, int urgency, int timeout)
{
    if (!m_iface->isValid()) {
        qWarning() << "NotificationManager: D-Bus interface is not valid:" << m_iface->lastError().message();
        return false;
    }

    QList<QVariant> args;
    QStringList actions;
    QMap<QString, QVariant> hints;
    hints.insert(QStringLiteral("urgency"), urgency);
    hints.insert(QStringLiteral("x-ukui-popup-timeout"), timeout);

    args << tr("Kylin Aiassistant")
         << static_cast<unsigned int>(0)
         << QStringLiteral("kylin-ai-assistant")
         << title
         << message
         << actions
         << hints
         << timeout;

    QDBusMessage response = m_iface->callWithArgumentList(QDBus::AutoDetect, QStringLiteral("Notify"), args);
    if (response.type() == QDBusMessage::ReplyMessage && !response.arguments().isEmpty()) {
        qDebug() << "Notification sent successfully, ID:" << response.arguments().first().toUInt();
        return true;
    } else {
        qWarning() << "Notification failed, response type:" << response.type();
        return false;
    }
}
