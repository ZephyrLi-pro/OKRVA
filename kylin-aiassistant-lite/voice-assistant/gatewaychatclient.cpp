#include "gatewaychatclient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QTimer>
#include <QUuid>
#include <ukui-log4qt.h>

namespace {
const int kDefaultGatewayPort = 42617;
const QString kDefaultGatewayHost = QStringLiteral("127.0.0.1");
const QString kChatProtocol = QStringLiteral("kylinbot.v1");
QString assistantSettingsIniPath()
{
    return QDir::homePath() + QStringLiteral("/.config/kylin-aiassistant/settings.ini");
}

static QString loadSocketPathFromConfig()
{
    QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
    return ini.value(QStringLiteral("gateway-socket-path")).toString();
}
}

static QString errorTextFromGatewayObject(const QJsonObject &object)
{
    QString message = object.value(QStringLiteral("error_message")).toString();
    if (message.trimmed().isEmpty()) {
        message = object.value(QStringLiteral("message")).toString();
    }
    if (message.trimmed().isEmpty()) {
        message = object.value(QStringLiteral("detail")).toString();
    }
    if (message.trimmed().isEmpty()) {
        message = object.value(QStringLiteral("error")).toString();
    }
    return message.trimmed();
}

static QString errorTextFromGatewayPayload(const QString &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload.trimmed().toUtf8());
    if (!doc.isObject()) {
        return QString();
    }

    const QJsonObject object = doc.object();
    const int errorCode = object.value(QStringLiteral("error_code")).toInt(0);
    if (errorCode == 0) {
        return QString();
    }

    return errorTextFromGatewayObject(object);
}

GatewayChatClient::GatewayChatClient(QObject *parent)
    : QObject(parent)
    , m_webSocket(new UnixSocketWebSocket(this))
    , m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QString socketPath = loadSocketPathFromConfig();
    if (!socketPath.isEmpty()) {
        m_webSocket->setSocketPath(socketPath);
    }
    m_gatewayToken = loadGatewayToken();
    connect(m_webSocket, &UnixSocketWebSocket::connected, this, &GatewayChatClient::onConnected);
    connect(m_webSocket, &UnixSocketWebSocket::disconnected, this, &GatewayChatClient::onDisconnected);
    connect(m_webSocket, &UnixSocketWebSocket::textMessageReceived,
            this, &GatewayChatClient::onTextMessageReceived);
    connect(m_webSocket, &UnixSocketWebSocket::socketError,
            this, &GatewayChatClient::onSocketError);
}

GatewayChatClient::~GatewayChatClient()
{
    m_webSocket->close();
}

QString GatewayChatClient::encodeContentWithScreenshotImage(const QString &contentWithoutImageSuffix,
                                                            const QString &pngRawBase64)
{
    if (pngRawBase64.trimmed().isEmpty()) {
        return contentWithoutImageSuffix;
    }
    QString base64Clean = pngRawBase64;
    base64Clean.remove(QLatin1Char('\r'));
    base64Clean.remove(QLatin1Char('\n'));
    base64Clean.remove(QLatin1Char(' '));

    const QString imageDataUri = QLatin1String("data:image/png;base64,") + base64Clean;
    return contentWithoutImageSuffix + QLatin1String(" [IMAGE:") + imageDataUri + QLatin1Char(']');
}

void GatewayChatClient::sendMessage(const QString &content)
{
    if (m_shuttingDown) {
        return;
    }

    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty()) {
        Q_EMIT errorOccurred(QCoreApplication::translate("GatewayChatClient",
                                                       "Message is empty; nothing was sent."));
        return;
    }

    m_pendingMessage = trimmed;
    ensureConnected();
}

void GatewayChatClient::cancelResponse(bool reportError)
{
    m_pendingMessage.clear();

    if (!m_connectedToGateway) {
        return;
    }

    QJsonObject frame;
    frame[QStringLiteral("type")] = QStringLiteral("cancel");
    const QString payload = QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact));
    const qint64 sent = m_webSocket->sendTextMessage(payload);
    if (sent < 0) {
        qWarning() << "voice-assistant gateway cancel send failed:" << m_webSocket->errorString();
        if (reportError) {
            Q_EMIT errorOccurred(m_webSocket->errorString());
        }
    }
}

void GatewayChatClient::shutdown()
{
    m_shuttingDown = true;
    m_pendingMessage.clear();
    m_accumulatedChunk.clear();
    m_pairingRequestInFlight = false;
    m_authRetryCount = 0;

    if (m_connectedToGateway) {
        cancelResponse(false);
    }
    if (m_webSocket) {
        m_webSocket->close();
    }
}

bool GatewayChatClient::isConnected() const
{
    return m_connectedToGateway;
}

void GatewayChatClient::onConnected()
{
    if (m_shuttingDown) {
        m_webSocket->close();
        return;
    }

    qInfo() << "voice-assistant gateway websocket connected";
    m_connectedToGateway = true;
    flushPendingOutbound();
}

void GatewayChatClient::onDisconnected()
{
    qInfo() << "voice-assistant gateway websocket disconnected";
    m_connectedToGateway = false;
}

void GatewayChatClient::onTextMessageReceived(const QString &message)
{
    if (m_shuttingDown) {
        return;
    }

    parseMessage(message);
}

void GatewayChatClient::onSocketError()
{
    if (m_shuttingDown) {
        return;
    }

    const QString socketError = m_webSocket->errorString();
    qWarning() << "voice-assistant gateway websocket error:" << socketError;

    if (socketError.contains(QStringLiteral("401"))) {
        qInfo() << "voice-assistant got 401, clearing stale token and will re-pair";
        clearGatewayToken();
        if (!m_pendingMessage.isEmpty() && m_authRetryCount < 1) {
            ++m_authRetryCount;
            QTimer::singleShot(0, this, &GatewayChatClient::ensureConnected);
            return;
        }
    }

    m_authRetryCount = 0;
    emitReplyRoundError(mappedSocketErrorMessage());
}

void GatewayChatClient::ensureConnected()
{
    if (m_shuttingDown) {
        return;
    }

    if (m_connectedToGateway) {
        flushPendingOutbound();
        return;
    }

    const UnixSocketWebSocket::State state = m_webSocket->state();
    if (state == UnixSocketWebSocket::ConnectingState) {
        return;
    }

    const QString savedToken = loadGatewayToken();
    if (!savedToken.isEmpty() && savedToken != m_gatewayToken) {
        m_gatewayToken = savedToken;
        m_pairingNotRequired = false;
    }

    // 如果没有 token，且 Gateway 要求配对，先走 pairing 流程获取 token。
    if (m_gatewayToken.isEmpty() && !m_pairingNotRequired) {
        qInfo() << "voice-assistant no gateway token, requesting new pairing code";
        requestNewPairingCode();
        return;
    }

    openGatewayWebSocket();
}

void GatewayChatClient::openGatewayWebSocket()
{
    if (m_shuttingDown) {
        return;
    }

    QMap<QString, QString> queryParams;
    queryParams[QStringLiteral("session_id")] = m_sessionId;
    queryParams[QStringLiteral("name")] = QStringLiteral("voice-assistant");
    if (!m_gatewayToken.isEmpty()) {
        queryParams[QStringLiteral("token")] = m_gatewayToken;
    }
    qInfo() << "voice-assistant gateway connecting via Unix socket:" << m_webSocket->socketPath();
    m_webSocket->open(m_webSocket->socketPath(), QStringLiteral("/ws/chat"), queryParams, QStringLiteral("kylinbot.v1"));
}

void GatewayChatClient::flushPendingOutbound()
{
    if (m_shuttingDown) {
        m_pendingMessage.clear();
        return;
    }

    if (!m_connectedToGateway || m_pendingMessage.isEmpty()) {
        return;
    }

    const QString toSend = m_pendingMessage;
    m_pendingMessage.clear();
    if (!sendPayload(toSend)) {
        m_pendingMessage = toSend;
    }
}

bool GatewayChatClient::sendPayload(const QString &content)
{
    if (m_shuttingDown) {
        return false;
    }

    m_accumulatedChunk.clear();
    m_replyRoundErrorEmitted = false;

    QJsonObject frame;
    frame[QStringLiteral("type")] = QStringLiteral("message");
    frame[QStringLiteral("content")] = content;
    
    const QString wire = QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact));
    
    const qint64 sent = m_webSocket->sendTextMessage(wire);
    if (sent < 0) {
        qWarning() << "voice-assistant gateway send message failed:" << m_webSocket->errorString();
        Q_EMIT errorOccurred(m_webSocket->errorString());
        return false;
    }

    Q_EMIT replyStarted();

    // 打印日志时隐藏 base64 图片数据，避免缓冲区占用过多
    QString logContent = frame["content"].toString();
    // 兼容 JPEG/PNG：只要出现 [IMAGE: 标记就截断，避免日志打印完整 base64
    int imageTagStart = logContent.indexOf("[IMAGE:");
    if (imageTagStart != -1) {
        logContent = logContent.left(imageTagStart) + "[IMAGE:...truncated...]";
    }
    if (logContent.contains(QStringLiteral("\"reference_text\""))) {
        KyInfo() << "voice-assistant gateway sendPayload: type=message reference context redacted, chars="
                 << content.size();
    } else {
        KyInfo() << "voice-assistant gateway sendPayload: type=message content=" << logContent;
    }

    return true;
}

void GatewayChatClient::parseMessage(const QString &jsonMessage)
{
    const QJsonDocument doc = QJsonDocument::fromJson(jsonMessage.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "voice-assistant invalid gateway message:" << jsonMessage;
        emitReplyRoundError(serviceErrorMessage());
        return;
    }

    const QJsonObject object = doc.object();
    const QString type = object.value(QStringLiteral("type")).toString();
    const int sdkErrorCode = object.value(QStringLiteral("error_code")).toInt(0);
    if (sdkErrorCode != 0) {
        const QString msg = errorTextFromGatewayObject(object);
        qWarning() << "voice-assistant gateway sdk-style error:" << msg << "code:" << sdkErrorCode;
        emitReplyRoundError(msg.trimmed().isEmpty() ? serviceErrorMessage() : msg.trimmed());
        return;
    }

    if (type == QStringLiteral("session_start") || type == QStringLiteral("connected")) {
        return;
    }

    if (type == QStringLiteral("chunk")) {
        const QString chunk = object.value(QStringLiteral("content")).toString();
        if (!chunk.isEmpty()) {
            Q_EMIT replyProgress();
            m_accumulatedChunk.append(chunk);
            Q_EMIT chunkReceived(chunk);
        }
        return;
    }

    if (type == QStringLiteral("chunk_reset")) {
        // chunk_reset：服务端开启新一轮流式输出或丢弃当前片段缓存时下发，用于清空本地已累积的答复片段。
        m_accumulatedChunk.clear();
        Q_EMIT replyProgress();
        return;
    }

    if (type == QStringLiteral("done")) {
        // full_response 为网关在流式结束后的权威全文，可能与客户端分片拼接一致，也可能经过规范化；
        // UI 通常已随 chunk 增量展示，此处仍发射完整字符串供 TTS 等消费端使用。
        const QString fullResponse = object.value(QStringLiteral("full_response")).toString();
        qInfo() << "voice-assistant gateway reply done, streamed chars:" << m_accumulatedChunk.size();
        m_authRetryCount = 0;
        const QString response = fullResponse.isEmpty() ? m_accumulatedChunk : fullResponse;
        const QString embeddedError = errorTextFromGatewayPayload(response);
        if (!embeddedError.isEmpty()) {
            emitReplyRoundError(embeddedError);
            m_accumulatedChunk.clear();
            return;
        }
        Q_EMIT replyFinished(response);
        m_accumulatedChunk.clear();
        return;
    }

    if (type == QStringLiteral("error")) {
        QString msg = object.value(QStringLiteral("message")).toString();
        if (msg.trimmed().isEmpty()) {
            msg = object.value(QStringLiteral("error_message")).toString();
        }
        qWarning() << "voice-assistant gateway service error:" << msg;
        emitReplyRoundError(msg.trimmed().isEmpty() ? serviceErrorMessage() : msg.trimmed());
        return;
    }

    if (type == QStringLiteral("draft_start")) {
        Q_EMIT replyProgress();
        const QString content = object.value(QStringLiteral("content")).toString();
        if (!content.isEmpty()) {
            Q_EMIT thinkingReceived(content);
        }
        return;
    }

    if (type == QStringLiteral("tool_call") || type == QStringLiteral("tool_result")) {
        Q_EMIT replyProgress();
        qInfo() << "voice-assistant gateway tool frame (not shown as thinking):" << jsonMessage;
        return;
    }

    qDebug() << "voice-assistant ignored gateway message:" << jsonMessage;
}

QString GatewayChatClient::readKylinBotToml() const
{
    return QString();
}

QString GatewayChatClient::loadHostFromConfigs(const QString &) const
{
    return QString();
}

int GatewayChatClient::loadPortFromConfigs(const QString &) const
{
    return 0;
}

QString GatewayChatClient::loadOriginFromConfigs(const QString &, const QString &, int) const
{
    return QString();
}

QString GatewayChatClient::loadGatewayToken() const
{
    QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
    return ini.value(QStringLiteral("gateway-token")).toString();
}

void GatewayChatClient::saveToken(const QString &token)
{
    m_gatewayToken = token;
    m_pairingNotRequired = false;
    QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
    ini.setValue(QStringLiteral("gateway-token"), token);
}

void GatewayChatClient::clearGatewayToken()
{
    m_gatewayToken.clear();
    m_pairingNotRequired = false;
    QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
    ini.remove(QStringLiteral("gateway-token"));
}

void GatewayChatClient::requestNewPairingCode()
{
    if (m_shuttingDown) {
        return;
    }

    if (m_pairingRequestInFlight) {
        return;
    }

    m_pairingRequestInFlight = true;
    UnixHttpResponse r = m_webSocket->httpPost(QStringLiteral("/admin/paircode/new"));

    if (r.statusCode == 400 && !r.body.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(r.body);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            const QJsonValue pairingRequiredValue = obj.value(QStringLiteral("pairing_required"));
            if (pairingRequiredValue.isBool() && !pairingRequiredValue.toBool()) {
                qInfo() << "voice-assistant pairing not required (from 400 response), proceeding directly";
                m_pairingRequestInFlight = false;
                m_pairingNotRequired = true;
                openGatewayWebSocket();
                return;
            }
        }
    }

    if (r.statusCode == 0) {
        qWarning() << "voice-assistant failed to generate new pairing code:" << r.errorString;
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Failed to connect to gateway: ") + r.errorString);
        return;
    }

    if (r.statusCode != 200) {
        QString bodyPreview = QString::fromUtf8(r.body).left(500);
        qWarning() << "voice-assistant requestNewPairingCode unexpected status:" << r.statusCode
                   << "body:" << bodyPreview;
    }

    QJsonDocument doc = QJsonDocument::fromJson(r.body);
    if (!doc.isObject()) {
        qWarning() << "voice-assistant invalid new pairing code response, body:"
                   << QString::fromUtf8(r.body).left(200);
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Invalid gateway response, please restart service"));
        return;
    }

    QJsonObject obj = doc.object();
    const QJsonValue pairingRequiredValue = obj.value(QStringLiteral("pairing_required"));
    if (!pairingRequiredValue.isBool()) {
        qWarning() << "voice-assistant new pairing code response missing pairing_required";
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Invalid gateway response, please restart service"));
        return;
    }

    const bool pairingRequired = pairingRequiredValue.toBool();
    QString code = obj.value(QStringLiteral("pairing_code")).toString();

    if (!pairingRequired) {
        qInfo() << "voice-assistant pairing not required, proceeding directly";
        m_pairingRequestInFlight = false;
        m_pairingNotRequired = true;
        openGatewayWebSocket();
    } else if (code.isEmpty()) {
        qWarning() << "voice-assistant pairing required but no code available";
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Pairing required but no code available, please check gateway service terminal"));
    } else {
        qInfo() << "voice-assistant new pairing code generated:" << code;
        doPairWithCode(code);
    }
}

void GatewayChatClient::doPairWithCode(const QString &pairingCode)
{
    if (m_shuttingDown) {
        return;
    }

    QMap<QString, QString> headers;
    headers[QStringLiteral("X-Pairing-Code")] = pairingCode;
    UnixHttpResponse r = m_webSocket->httpPost(QStringLiteral("/pair"), QByteArray(), headers);

    if (r.statusCode == 0) {
        qWarning() << "voice-assistant pairing failed:" << r.errorString;
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Pairing failed: ") + r.errorString);
        return;
    }

    if (r.statusCode != 200) {
        QString bodyPreview = QString::fromUtf8(r.body).left(500);
        qWarning() << "voice-assistant doPairWithCode unexpected status:" << r.statusCode
                   << "body:" << bodyPreview;
    }

    QJsonDocument doc = QJsonDocument::fromJson(r.body);
    if (!doc.isObject()) {
        qWarning() << "voice-assistant invalid pairing response, body:"
                   << QString::fromUtf8(r.body).left(200);
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Invalid pairing response"));
        return;
    }

    QJsonObject obj = doc.object();
    const QString token = obj.value(QStringLiteral("token")).toString();
    if (token.trimmed().isEmpty()) {
        qWarning() << "voice-assistant pairing response missing token";
        m_pairingRequestInFlight = false;
        m_pendingMessage.clear();
        m_authRetryCount = 0;
        Q_EMIT errorOccurred(tr("Invalid pairing response"));
        return;
    }

    saveToken(token);
    m_pairingRequestInFlight = false;
    qInfo() << "voice-assistant pairing succeeded, token saved";
    ensureConnected();
}

void GatewayChatClient::emitReplyRoundError(const QString &message)
{
    const QString trimmed = message.trimmed();
    const QString text = trimmed.isEmpty() ? serviceErrorMessage() : trimmed;
    if (m_replyRoundErrorEmitted) {
        qInfo() << "voice-assistant gateway duplicate reply-round error suppressed:" << text;
        return;
    }
    m_replyRoundErrorEmitted = true;
    Q_EMIT errorOccurred(text);
}

QString GatewayChatClient::networkErrorMessage() const
{
    return tr("Network exception. Please check the network connection and try again.");
}

QString GatewayChatClient::serviceErrorMessage() const
{
    return tr("Service exception. Please try again later.");
}

QString GatewayChatClient::mappedSocketErrorMessage() const
{
    const QString err = m_webSocket->errorString();
    if (err.contains(QStringLiteral("refused"), Qt::CaseInsensitive) ||
        err.contains(QStringLiteral("not found"), Qt::CaseInsensitive) ||
        err.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
        return networkErrorMessage();
    }
    return serviceErrorMessage();
}
