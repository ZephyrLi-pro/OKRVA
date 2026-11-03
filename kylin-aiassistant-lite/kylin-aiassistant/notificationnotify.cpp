#include "notificationnotify.h"
#include <QDBusConnection>

NotificationNotify* NotificationNotify::getInstance()
{
    static NotificationNotify instance;
    return &instance;
}

NotificationNotify::NotificationNotify(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect("org.freedesktop.Notifications",
                                          "/org/freedesktop/Notifications",
                                          "org.freedesktop.Notifications",
                                          "ActionInvoked",
                                          this, SLOT(onActionInvoked(uint,QString)));
    QDBusConnection::sessionBus().connect("org.freedesktop.Notifications",
                                          "/org/freedesktop/Notifications",
                                          "org.freedesktop.Notifications",
                                          "NotificationClosed",
                                          this, SLOT(onNotificationClosed(uint,uint)));
}

NotificationNotify::~NotificationNotify()
{
}

void NotificationNotify::sendNotification(const QString &title, const QString &body, const QString &sessionId, bool includeActions)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        KyInfo() << "发送失败：D-Bus session bus 未连接";
        return;
    }

    QList<QVariant> args;
    QStringList actions;
    if (includeActions) {
        actions << "noticeView" << tr("View");
    }
    QString info = body;
    QMap<QString, QVariant> hints;

    hints.insert("urgency", 1);
    hints.insert(QStringLiteral("x-ukui-popup-timeout"), 6000);
    if (includeActions) {
        hints.insert("x-ukui-actions", true);
    }

    args << QString(tr("kylin-aiassistant"))
         << (unsigned int)0
         << QString("kylin-ai-assistant")
         << QString(title)
         << QString(info)
         << actions
         << hints
         << 6000;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "Notify");
    msg.setArguments(args);

    QDBusMessage response = QDBusConnection::sessionBus().call(msg, QDBus::BlockWithGui);
    if (response.type() == QDBusMessage::ReplyMessage && !response.arguments().isEmpty()) {
        quint32 noticeId = response.arguments().first().toUInt();
        if (includeActions && !sessionId.isEmpty()) {
            m_notificationSessionMap[noticeId] = sessionId;
        }
        KyInfo() << "发送成功！通知ID：" << noticeId;
    } else {
        KyInfo() << "发送失败：" << response.type() << response.errorMessage();
    }
}

void NotificationNotify::onActionInvoked(quint32 id, const QString &actionKey)
{
    KyInfo() << "通知查看按钮点击 id:" << id;
    if (actionKey == "noticeView" && m_notificationSessionMap.contains(id)) {
        QString sessionId = m_notificationSessionMap.value(id);
        emit viewClicked(sessionId);
        m_notificationSessionMap.remove(id);
    }
}

void NotificationNotify::onNotificationClosed(quint32 id, quint32 reason)
{
    if (m_notificationSessionMap.contains(id)) {
        KyInfo() << "通知关闭 id:" << id << "reason:" << reason;
        m_notificationSessionMap.remove(id);
    }
}

void NotificationNotify::closeNotification()
{
    for (auto it = m_notificationSessionMap.begin(); it != m_notificationSessionMap.end(); ++it) {
        QList<QVariant> args;
        args << it.key();

        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "CloseNotification");
        msg.setArguments(args);

        QDBusConnection::sessionBus().call(msg, QDBus::BlockWithGui);
        KyInfo() << "关闭通知 ID:" << it.key();
    }
    m_notificationSessionMap.clear();
}
