#include "tasksessionmanager.h"
#include "gatewaywebsocketmanager.h"
#include "configmanager.h"
#include "unixsocketwebsocket.h"
#include <QDir>
#include <ukui-log4qt.h>

namespace {

QString expandHomePath(QString path)
{
    path = path.trimmed();
    if (path.startsWith(QStringLiteral("~/"))) {
        return QDir::homePath() + path.mid(1);
    }
    if (path == QStringLiteral("~")) {
        return QDir::homePath();
    }
    return path;
}

void configureGatewaySocket(UnixSocketWebSocket &client)
{
    const QString socketPath = expandHomePath(
        ConfigManager::getInstance()->getConfigValue("gateway-socket-path").toString());
    if (!socketPath.isEmpty()) {
        client.setSocketPath(socketPath);
    }
}

} // namespace

TaskSessionManager::TaskSessionManager(QObject* parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
}

TaskSessionManager::~TaskSessionManager()
{
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().wsManager) {
            it.value().wsManager->deleteLater();
        }
    }
    m_sessions.clear();
}

QString TaskSessionManager::createSession(const QString& uuid, int historyId)
{
    if (m_sessions.size() >= MAX_CONCURRENT_SESSIONS) {
        Q_EMIT sessionLimitReached();
        return QString();
    }

    QString sessionUuid = uuid.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : uuid;

    TaskSession session;
    session.sessionUuid = sessionUuid;
    session.historyId = historyId;
    session.wsManager = new GatewayWebSocketManager(sessionUuid, this);
    session.isReplying = false;

    connect(session.wsManager, &GatewayWebSocketManager::reply,
            this, [this, sessionUuid](const InnerMessage& msg) {
        Q_EMIT reply(sessionUuid, msg);
    });

    m_sessions[sessionUuid] = session;
    m_activeSessionUuid = sessionUuid;

    KyInfo() << "Task session created:" << sessionUuid;
    Q_EMIT sessionCreated(sessionUuid);
    return sessionUuid;
}

void TaskSessionManager::activateSession(const QString& sessionUuid)
{
    if (!m_sessions.contains(sessionUuid)) {
        KyInfo() << "Session not found for activation:" << sessionUuid;
        return;
    }

    m_activeSessionUuid = sessionUuid;
    KyInfo() << "Session activated:" << sessionUuid;
    Q_EMIT sessionActivated(sessionUuid);
}

void TaskSessionManager::deactivateSession()
{
    if (!m_activeSessionUuid.isEmpty()) {
        KyInfo() << "Deactivating session:" << m_activeSessionUuid;
        m_activeSessionUuid = "";
    }
}

QString TaskSessionManager::getActiveSessionUuid() const
{
    return m_activeSessionUuid;
}

TaskSession* TaskSessionManager::getSession(const QString& sessionUuid)
{
    if (!m_sessions.contains(sessionUuid)) {
        return nullptr;
    }
    return &m_sessions[sessionUuid];
}

TaskSession* TaskSessionManager::getSessionByHistoryId(int historyId)
{
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().historyId == historyId) {
            return &it.value();
        }
    }
    return nullptr;
}

void TaskSessionManager::sendMessage(const QString& sessionUuid, const QString& content)
{
    TaskSession* session = getSession(sessionUuid);
    if (!session) {
        KyInfo() << "Session not found for sending message:" << sessionUuid;
        return;
    }

    session->isReplying = true;
    session->isCancelled = false;
    session->wsManager->sendMessage(content);
}

void TaskSessionManager::cancelResponse(const QString& sessionUuid)
{
    TaskSession* session = getSession(sessionUuid);
    if (!session) {
        return;
    }

    session->wsManager->cancelResponse();
    session->isReplying = false;
    session->isCancelled = true;
}

void TaskSessionManager::closeSession(const QString& sessionUuid)
{
    if (!m_sessions.contains(sessionUuid)) {
        return;
    }

    if (m_activeSessionUuid == sessionUuid) {
        m_activeSessionUuid.clear();
    }

    destroySession(sessionUuid);
    KyInfo() << "Session closed:" << sessionUuid;
    Q_EMIT sessionClosed(sessionUuid);
}

void TaskSessionManager::deleteBackendSession(const QString& sessionUuid)
{
    if (sessionUuid.isEmpty()) {
        return;
    }

    QString gatewayToken = ConfigManager::getInstance()->getConfigValue("gateway-token").toString();
    QMap<QString, QString> headers;
    if (!gatewayToken.isEmpty()) {
        headers[QStringLiteral("Authorization")] = QStringLiteral("Bearer ") + gatewayToken;
    }

    const QString path = QStringLiteral("/api/sessions/") + sessionUuid;
    UnixSocketWebSocket socketClient;
    configureGatewaySocket(socketClient);
    UnixHttpResponse socketResp = socketClient.httpDelete(path, headers);
    if (socketResp.statusCode >= 200 && socketResp.statusCode < 300) {
        KyInfo() << "Backend session deleted via Unix socket:" << sessionUuid;
        return;
    }
    if (socketResp.statusCode != 0) {
        KyInfo() << "Failed to delete backend session via Unix socket:" << sessionUuid
                 << "status:" << socketResp.statusCode
                 << "body:" << QString::fromUtf8(socketResp.body).left(200);
        return;
    }

    KyInfo() << "Delete backend session via Unix socket failed, fallback to HTTP:"
             << sessionUuid << socketResp.errorString;

    QString gatewayHost = ConfigManager::getInstance()->getConfigValue("gateway-host").toString();
    if (gatewayHost.trimmed().isEmpty()) {
        gatewayHost = QStringLiteral("127.0.0.1:42617");
    }
    QUrl url("http://" + gatewayHost + path);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!gatewayToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + gatewayToken).toUtf8());
    }

    QNetworkReply* reply = m_networkManager->deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [reply, sessionUuid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            KyInfo() << "Failed to delete backend session:" << sessionUuid << "error:" << reply->errorString();
            return;
        }
        KyInfo() << "Backend session deleted:" << sessionUuid;
    });
}

QList<TaskSession> TaskSessionManager::getAllSessions() const
{
    return m_sessions.values();
}

int TaskSessionManager::sessionCount() const
{
    return m_sessions.size();
}

void TaskSessionManager::destroySession(const QString& sessionUuid)
{
    if (!m_sessions.contains(sessionUuid)) {
        return;
    }

    TaskSession& session = m_sessions[sessionUuid];

    if (session.wsManager) {
        disconnect(session.wsManager, nullptr, this, nullptr);
        session.wsManager->deleteLater();
        session.wsManager = nullptr;
    }

    m_sessions.remove(sessionUuid);
}
