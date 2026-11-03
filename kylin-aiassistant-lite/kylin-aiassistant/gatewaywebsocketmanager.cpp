#include "gatewaywebsocketmanager.h"
#include "configmanager.h"
#include <QDebug>
#include <QUuid>

GatewayWebSocketManager::GatewayWebSocketManager(const QString& sessionId, QObject* parent)
    : QObject(parent),
      m_webSocket(new UnixSocketWebSocket(this)),
      m_sessionId(sessionId),
      m_messageIndex(0),
      m_isReplying(false)
{
    QString socketPath = ConfigManager::getInstance()->getConfigValue("gateway-socket-path").toString();
    if (!socketPath.isEmpty()) {
        m_webSocket->setSocketPath(socketPath);
    }
    connect(m_webSocket, &UnixSocketWebSocket::connected, this, &GatewayWebSocketManager::onConnected);
    connect(m_webSocket, &UnixSocketWebSocket::disconnected, this, &GatewayWebSocketManager::onDisconnected);
    connect(m_webSocket, &UnixSocketWebSocket::textMessageReceived, this, &GatewayWebSocketManager::onTextMessageReceived);
    connect(m_webSocket, &UnixSocketWebSocket::socketError, this, &GatewayWebSocketManager::onError);
}

GatewayWebSocketManager::~GatewayWebSocketManager()
{
    if (m_webSocket) {
        m_webSocket->close();
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }
}

void GatewayWebSocketManager::ensureConnected()
{
    if (m_pairingInProgress) {
        qInfo() << "Pairing already in progress, skipping ensureConnected";
        return;
    }

    QString gatewayToken = ConfigManager::getInstance()->getConfigValue("gateway-token").toString();
    if (!gatewayToken.isEmpty()) {
        if (!validateGatewayToken()) {
            qInfo() << "Gateway token invalid, re-pairing synchronously";
            gatewayToken.clear();
        }
    }

    if (gatewayToken.isEmpty()) {
        qInfo() << "No gateway token, performing sync pairing";
        m_pairingInProgress = true;
        performSyncPairing();
        m_pairingInProgress = false;
        gatewayToken = ConfigManager::getInstance()->getConfigValue("gateway-token").toString();
        if (gatewayToken.isEmpty()) {
            qInfo() << "Sync pairing failed, cannot connect";
            return;
        }
    }

    if (m_sessionId.isEmpty()) {
        m_sessionId = QUuid::createUuid().toString();
    }

    QMap<QString, QString> queryParams;
    queryParams[QStringLiteral("session_id")] = m_sessionId;
    queryParams[QStringLiteral("token")] = gatewayToken;

    qInfo() << "Gateway WebSocket connecting via Unix socket:" << m_webSocket->socketPath();

    m_webSocket->open(m_webSocket->socketPath(), QStringLiteral("/ws/chat"), queryParams, QStringLiteral("kylinbot.v1"));
    qInfo() << "Gateway WebSocket opening to" << m_webSocket->socketPath() << "/ws/chat";
}

void GatewayWebSocketManager::sendMessage(const QString& content)
{
    m_expectingReply = true;
    if (m_webSocket->state() == UnixSocketWebSocket::ConnectedState) {
        QJsonObject msg;
        msg["type"] = "message";
        msg["content"] = content;
        qint64 sent = m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
        if (sent <= 0) {
            m_expectingReply = false;
            qInfo() << "Failed to send message, bytes sent:" << sent;
            InnerMessage errMsg("chat", "", "Failed to send message, please retry", false, true, m_messageIndex, 10000, "Gateway");
            Q_EMIT reply(errMsg);
        }
    } else {
        m_isReplying = true;
        m_pendingMessage = content;
        ensureConnected();
    }
}

void GatewayWebSocketManager::cancelResponse()
{
    if (m_webSocket->state() != UnixSocketWebSocket::ConnectedState) return;

    QJsonObject cancel;
    cancel["type"] = "cancel";
    qint64 sent = m_webSocket->sendTextMessage(QJsonDocument(cancel).toJson(QJsonDocument::Compact));
    if (sent <= 0) {
        qInfo() << "Failed to send cancel, bytes sent:" << sent;
    }
}

bool GatewayWebSocketManager::isConnected() const
{
    return m_webSocket->state() == UnixSocketWebSocket::ConnectedState;
}

void GatewayWebSocketManager::onConnected()
{
    qInfo() << "Gateway WebSocket connected, session:" << m_sessionId;
    Q_EMIT connected(m_sessionId);

    if (!m_pendingMessage.isEmpty()) {
        QJsonObject msg;
        msg["type"] = "message";
        msg["content"] = m_pendingMessage;
        qint64 sent = m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
        if (sent <= 0) {
            m_expectingReply = false;
            qInfo() << "Failed to send pending message, bytes sent:" << sent;
            InnerMessage errMsg("chat", "", "Failed to send message, please retry", false, true, m_messageIndex, 10000, "Gateway");
            Q_EMIT reply(errMsg);
        }

        m_pendingMessage.clear();
    }
}

void GatewayWebSocketManager::onDisconnected()
{
    qInfo() << "Gateway WebSocket disconnected, session:" << m_sessionId;

    Q_EMIT disconnected(m_sessionId);

    if (m_expectingReply || m_isReplying) {
        QString content = m_accumulatedChunk;
        if (content.isEmpty() && m_accumulatedThinking.isEmpty()) {
            content = "Connection to gateway was interrupted";
        }
        InnerMessage msg("chat", m_accumulatedThinking, content, true, true, m_messageIndex, -1, "Gateway");
        Q_EMIT reply(msg);
        m_messageIndex++;
    }

    m_expectingReply = false;
    m_isReplying = false;
    m_accumulatedChunk.clear();
    m_accumulatedThinking.clear();
}

void GatewayWebSocketManager::onTextMessageReceived(const QString& message)
{
    parseAndEmitMessage(message);
}

void GatewayWebSocketManager::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qInfo() << "Gateway WebSocket erro enum:" << error;
    qInfo() << "Gateway WebSocket error:" << m_webSocket->errorString();

    if (m_expectingReply || m_isReplying) {
        InnerMessage msg("chat", "", "Gateway connection error: " + m_webSocket->errorString(), false, true, m_messageIndex, -1, "Gateway");
        Q_EMIT reply(msg);
    }
    m_expectingReply = false;
    m_isReplying = false;
}

void GatewayWebSocketManager::requestNewPairingCode()
{
    UnixHttpResponse r = m_webSocket->httpPost(QStringLiteral("/admin/paircode/new"));
    if (r.statusCode == 0) {
        qInfo() << "Failed to generate new pairing code:" << r.errorString;
        m_expectingReply = false;
        m_pendingMessage.clear();
        InnerMessage errMsg("chat", "", "Failed to connect to gateway: " + r.errorString, false, true, m_messageIndex, 10000, "Gateway");
        Q_EMIT reply(errMsg);
        return;
    }

    if (r.statusCode == 400) {
        QJsonDocument doc400 = QJsonDocument::fromJson(r.body);
        if (doc400.isObject()) {
            QJsonObject obj = doc400.object();
            bool pairingRequired = obj.value("pairing_required").toBool();
            if (!pairingRequired) {
                qInfo() << "Pairing not required, proceeding directly";
                return;
            }
        }
    }

    if (r.statusCode != 200) {
        QString bodyPreview = QString::fromUtf8(r.body).left(500);
        qInfo() << "requestNewPairingCode unexpected status:" << r.statusCode
                << "body:" << bodyPreview;
    }

    QJsonDocument doc = QJsonDocument::fromJson(r.body);
    if (!doc.isObject()) {
        qInfo() << "Invalid new pairing code response, body:" << QString::fromUtf8(r.body).left(200);
        m_expectingReply = false;
        m_pendingMessage.clear();
        InnerMessage errMsg("chat", "", "Invalid gateway response, please restart service", false, true, m_messageIndex, 10000, "Gateway");
        Q_EMIT reply(errMsg);
        return;
    }

    QJsonObject obj = doc.object();
    bool pairingRequired = obj.value("pairing_required").toBool();
    QString code = obj.value("pairing_code").toString();

    if (!pairingRequired) {
        qInfo() << "Pairing not required, proceeding directly";
    } else if (code.isEmpty()) {
        qInfo() << "Pairing required but no code available, please check gateway service terminal";
        m_expectingReply = false;
        m_pendingMessage.clear();
        InnerMessage errMsg("chat", "", "Pairing required but no code available, please check gateway service terminal", false, true, m_messageIndex, 10000, "Gateway");
        Q_EMIT reply(errMsg);
    } else {
        qInfo() << "New pairing code generated:" << code;
        doPairWithCode(code);
    }
}

void GatewayWebSocketManager::doPairWithCode(const QString& pairingCode)
{
    QMap<QString, QString> headers;
    headers[QStringLiteral("X-Pairing-Code")] = pairingCode;
    UnixHttpResponse r = m_webSocket->httpPost(QStringLiteral("/pair"), QByteArray(), headers);
    if (r.statusCode == 0) {
        qInfo() << "Pairing failed:" << r.errorString;
        m_expectingReply = false;
        m_pendingMessage.clear();
        InnerMessage errMsg("chat", "", "Pairing failed: " + r.errorString, false, true, m_messageIndex, 10000, "Gateway");
        Q_EMIT reply(errMsg);
        return;
    }

    if (r.statusCode != 200) {
        QString bodyPreview = QString::fromUtf8(r.body).left(500);
        qInfo() << "doPairWithCode unexpected status:" << r.statusCode
                << "body:" << bodyPreview;
    }

    QJsonDocument doc = QJsonDocument::fromJson(r.body);
    if (!doc.isObject()) {
        qInfo() << "Invalid pairing response, body:" << QString::fromUtf8(r.body).left(200);
        m_expectingReply = false;
        m_pendingMessage.clear();
        InnerMessage errMsg("chat", "", "Invalid pairing response", false, true, m_messageIndex, 10000, "Gateway");
        Q_EMIT reply(errMsg);
        return;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("token")) {
        QString token = obj["token"].toString();
        saveToken(token);
        qInfo() << "Pairing succeeded, token saved";
        ensureConnected();
    }
}

void GatewayWebSocketManager::performSyncPairing()
{
    UnixHttpResponse r1 = m_webSocket->httpPost(QStringLiteral("/admin/paircode/new"));
    if (r1.statusCode == 0) {
        qInfo() << "Sync pair code failed:" << r1.errorString;
        return;
    }

    if (r1.statusCode != 200 && r1.statusCode != 400) {
        QString bodyPreview = QString::fromUtf8(r1.body).left(500);
        qInfo() << "Sync pair code failed, status:" << r1.statusCode
                << "body:" << bodyPreview;
        return;
    }

    QJsonDocument doc1 = QJsonDocument::fromJson(r1.body);
    if (!doc1.isObject()) {
        qInfo() << "Invalid paircode response, body:" << QString::fromUtf8(r1.body).left(200);
        return;
    }

    QJsonObject obj1 = doc1.object();
    bool pairingRequired = obj1.value("pairing_required").toBool();
    QString code = obj1.value("pairing_code").toString();

    if (!pairingRequired) {
        qInfo() << "Sync pairing not required, proceeding directly";
        return;
    }

    if (code.isEmpty()) {
        qInfo() << "No pairing code available";
        return;
    }

    qInfo() << "Sync pairing code obtained:" << code;

    QMap<QString, QString> headers;
    headers[QStringLiteral("X-Pairing-Code")] = code;
    UnixHttpResponse r2 = m_webSocket->httpPost(QStringLiteral("/pair"), QByteArray(), headers);
    if (r2.statusCode == 0) {
        qInfo() << "Sync pairing failed:" << r2.errorString;
        return;
    }

    if (r2.statusCode != 200) {
        QString bodyPreview = QString::fromUtf8(r2.body).left(500);
        qInfo() << "Sync pairing failed, status:" << r2.statusCode
                << "body:" << bodyPreview;
        return;
    }

    QJsonDocument doc2 = QJsonDocument::fromJson(r2.body);
    if (!doc2.isObject()) {
        qInfo() << "Invalid pairing response, body:" << QString::fromUtf8(r2.body).left(200);
        return;
    }

    QJsonObject obj2 = doc2.object();
    if (obj2.contains("token")) {
        QString token = obj2["token"].toString();
        saveToken(token);
        qInfo() << "Sync pairing succeeded, token saved";
    }
}

void GatewayWebSocketManager::saveToken(const QString& token)
{
    ConfigManager::getInstance()->updateConfig("gateway-token", token);
}

bool GatewayWebSocketManager::validateGatewayToken()
{
    QString token = ConfigManager::getInstance()->getConfigValue("gateway-token").toString().trimmed();
    if (token.isEmpty()) {
        return false;
    }

    QMap<QString, QString> headers;
    headers[QStringLiteral("Authorization")] = QStringLiteral("Bearer ") + token;
    UnixHttpResponse resp = m_webSocket->httpGet(QStringLiteral("/api/health"), headers);

    if (resp.statusCode == 200) {
        return true;
    }

    if (resp.statusCode == 401) {
        qInfo() << "Gateway token invalidated (401), clearing saved token";
        ConfigManager::getInstance()->updateConfig("gateway-token", "");
        return false;
    }

    if (resp.errorString.contains(QStringLiteral("refused"), Qt::CaseInsensitive) ||
        resp.errorString.contains(QStringLiteral("No such file"), Qt::CaseInsensitive)) {
        qInfo() << "Gateway socket not found, assuming token still valid";
        return true;
    }

    QString bodyPreview = QString::fromUtf8(resp.body).left(200);
    qInfo() << "Token validation unexpected status:" << resp.statusCode
            << "body:" << bodyPreview << "- assuming token still valid";
    return true;
}

void GatewayWebSocketManager::parseAndEmitMessage(const QString& jsonMessage)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonMessage.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qInfo() << "Invalid JSON from Gateway:" << jsonMessage;
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "session_start") {
        m_isReplying = false;
        return;
    }

    const bool isReplyEvent = (type == QLatin1String("chunk")
                               || type == QLatin1String("thinking")
                               || type == QLatin1String("chunk_reset")
                               || type == QLatin1String("tool_call")
                               || type == QLatin1String("tool_result")
                               || type == QLatin1String("done")
                               || type == QLatin1String("error")
                               || type == QLatin1String("agent_start"));
    if (isReplyEvent && !m_expectingReply) {
        return;
    }

    if (type == "chunk") {
        QString content = obj["content"].toString();
        m_accumulatedChunk.append(content);

        InnerMessage msg("chat", "", content, false, false, m_messageIndex, 0, "Gateway");
        Q_EMIT reply(msg);
        return;
    }

    if (type == "thinking") {
        QString content = obj["content"].toString();
        m_accumulatedThinking.append(content);

        InnerMessage msg("chat", content, "", false, false, m_messageIndex, 0, "Gateway");
        Q_EMIT reply(msg);
        return;
    }

    if (type == "chunk_reset") {
        m_accumulatedChunk.clear();
        return;
    }

    if (type == "tool_call" || type == "tool_result") {
        // 工具调用事件复用 reply 信号透传，仅 type 与 extraJson 有意义；
        // 其它字段留空，避免下游把它当作普通消息读取 model/index。
        InnerMessage msg(type, "", "", false, false, 0, 0, "", "",
                         QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        Q_EMIT reply(msg);
        return;
    }

    if (type == "done") {
        m_expectingReply = false;
        m_isReplying = false;
        QString fullResponse = obj["full_response"].toString();

        InnerMessage msg("chat", m_accumulatedThinking, fullResponse, true, true, m_messageIndex, 0, "Gateway");
        Q_EMIT reply(msg);

        m_messageIndex++;
        m_accumulatedChunk.clear();
        m_accumulatedThinking.clear();
        return;
    }

    if (type == "error") {
        QString errorMsg = obj["message"].toString();
        const QString wsCode = obj["code"].toString();
        int errorCode = -2;
        // 无可用模型：首次连接走 AGENT_INIT_FAILED，删模型后继续对话走 AGENT_RELOAD_FAILED。
        const bool noModelConfigured =
            wsCode == QLatin1String("NO_MODEL_CONFIGURED")
            || (errorMsg.contains(QLatin1String("no model configured"))
                && (wsCode == QLatin1String("AGENT_INIT_FAILED")
                    || wsCode == QLatin1String("AGENT_RELOAD_FAILED")));
        if (noModelConfigured) {
            errorCode = kGatewayInnerErrorNoModelConfigured;
        }

        InnerMessage msg("chat", "", errorMsg, false, true, m_messageIndex, errorCode, "Gateway");
        Q_EMIT reply(msg);

        m_expectingReply = false;
        m_isReplying = false;
        return;
    }

    if (type == "agent_start") {
        QString model = obj["model"].toString();
        m_isReplying = true;
        qInfo() << "Gateway agent started, model:" << model;
        return;
    }
}
