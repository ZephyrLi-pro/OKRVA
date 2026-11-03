#include "kadbusclient.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDebug>

const QString KADBusClient::s_serviceName = QStringLiteral("org.ukui.kylin_aiassistant");
const QString KADBusClient::s_objectPath = QStringLiteral("/org/ukui/kylin_aiassistant");
const QString KADBusClient::s_interfaceName = QStringLiteral("org.ukui.kylin_aiassistant");

namespace {
bool isKylinAiAssistantRegistered()
{
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    return interface && interface->isServiceRegistered(KADBusClient::serviceName());
}
}

KADBusClient::KADBusClient(QObject *parent)
    : QObject{parent}
{
}

KADBusClient::~KADBusClient()
{
}

void KADBusClient::allocateVoiceSessionIdAsync()
{
    if (!isKylinAiAssistantRegistered()) {
        qWarning() << "KADBusClient::allocateVoiceSessionIdAsync: parent service is not registered";
        Q_EMIT voiceSessionIdAllocated(QString());
        return;
    }

    QDBusInterface *iface = new QDBusInterface(s_serviceName, s_objectPath, s_interfaceName,
                                               QDBusConnection::sessionBus(), this);
    if (!iface->isValid()) {
        qWarning() << "KADBusClient::allocateVoiceSessionIdAsync: interface not valid:" << iface->lastError().message();
        iface->deleteLater();
        Q_EMIT voiceSessionIdAllocated(QString());
        return;
    }

    QDBusPendingCall call = iface->asyncCall(QStringLiteral("AllocateVoiceSessionId"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, iface]() {
        watcher->deleteLater();
        iface->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "KADBusClient::allocateVoiceSessionIdAsync: call failed:" << reply.errorMessage();
            Q_EMIT voiceSessionIdAllocated(QString());
            return;
        }
        Q_EMIT voiceSessionIdAllocated(reply.arguments().value(0).toString());
    });
}

void KADBusClient::saveVoiceChatMessageAsync(const QString &sessionID, int msgIndex,
                                             const QString &message, int chatType,
                                             qlonglong operateTime,
                                             const QString &sessionUuid)
{
    if (!isKylinAiAssistantRegistered()) {
        qWarning() << "KADBusClient::saveVoiceChatMessageAsync: parent service is not registered";
        Q_EMIT voiceChatMessageSaved(msgIndex, false);
        return;
    }

    QDBusInterface *iface = new QDBusInterface(s_serviceName, s_objectPath, s_interfaceName,
                                               QDBusConnection::sessionBus(), this);
    if (!iface->isValid()) {
        qWarning() << "KADBusClient::saveVoiceChatMessageAsync: interface not valid:" << iface->lastError().message();
        iface->deleteLater();
        Q_EMIT voiceChatMessageSaved(msgIndex, false);
        return;
    }

    QDBusPendingCall call = iface->asyncCall(QStringLiteral("SaveVoiceChatMessage"),
                                             sessionID, msgIndex, message, chatType, operateTime, sessionUuid);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, iface, msgIndex]() {
        watcher->deleteLater();
        iface->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "KADBusClient::saveVoiceChatMessageAsync: call failed:" << reply.errorMessage();
            Q_EMIT voiceChatMessageSaved(msgIndex, false);
            return;
        }
        Q_EMIT voiceChatMessageSaved(msgIndex, reply.arguments().value(0).toBool());
    });
}

void KADBusClient::notifyHistoryChangedAsync()
{
    if (!isKylinAiAssistantRegistered()) {
        qWarning() << "KADBusClient::notifyHistoryChangedAsync: parent service is not registered";
        return;
    }

    QDBusInterface *iface = new QDBusInterface(s_serviceName, s_objectPath, s_interfaceName,
                                               QDBusConnection::sessionBus(), this);
    if (!iface->isValid()) {
        qWarning() << "KADBusClient::notifyHistoryChangedAsync: interface not valid:" << iface->lastError().message();
        iface->deleteLater();
        return;
    }

    QDBusPendingCall call = iface->asyncCall(QStringLiteral("NotifyHistoryChanged"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, iface]() {
        watcher->deleteLater();
        iface->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "KADBusClient::notifyHistoryChangedAsync: call failed:" << reply.errorMessage();
        }
    });
}

void KADBusClient::notifyVoiceTaskCompletedAsync(const QString &sessionID, const QString &message)
{
    if (!isKylinAiAssistantRegistered()) {
        qWarning() << "KADBusClient::notifyVoiceTaskCompletedAsync: parent service is not registered";
        Q_EMIT voiceTaskCompletedNotified(false);
        return;
    }

    QDBusInterface *iface = new QDBusInterface(s_serviceName, s_objectPath, s_interfaceName,
                                               QDBusConnection::sessionBus(), this);
    if (!iface->isValid()) {
        qWarning() << "KADBusClient::notifyVoiceTaskCompletedAsync: interface not valid:" << iface->lastError().message();
        iface->deleteLater();
        Q_EMIT voiceTaskCompletedNotified(false);
        return;
    }

    QDBusPendingCall call = iface->asyncCall(QStringLiteral("NotifyVoiceTaskCompleted"), sessionID, message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, iface]() {
        watcher->deleteLater();
        iface->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "KADBusClient::notifyVoiceTaskCompletedAsync: call failed:" << reply.errorMessage();
            Q_EMIT voiceTaskCompletedNotified(false);
            return;
        }
        Q_EMIT voiceTaskCompletedNotified(reply.arguments().isEmpty()
                                          || reply.arguments().value(0).toBool());
    });
}
