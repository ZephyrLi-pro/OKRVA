#include "unixsocketwebsocket.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <unistd.h>

namespace {

constexpr int kWebSocketWriteTimeoutMs = 30000;

bool writeAllToLocalSocket(QLocalSocket *socket, const QByteArray &data, QString *errorOut)
{
    if (!socket) {
        if (errorOut) {
            *errorOut = QStringLiteral("socket is null");
        }
        return false;
    }
    if (data.isEmpty()) {
        return true;
    }

    qint64 written = 0;
    while (written < data.size()) {
        const qint64 n = socket->write(data.constData() + written, data.size() - written);
        if (n < 0) {
            if (errorOut) {
                *errorOut = socket->errorString();
            }
            return false;
        }
        if (n == 0) {
            if (!socket->waitForBytesWritten(kWebSocketWriteTimeoutMs)) {
                if (errorOut) {
                    *errorOut = socket->errorString().isEmpty()
                                      ? QStringLiteral("waitForBytesWritten timed out")
                                      : socket->errorString();
                }
                return false;
            }
            continue;
        }
        written += n;
    }

    if (!socket->waitForBytesWritten(kWebSocketWriteTimeoutMs)) {
        if (errorOut) {
            *errorOut = socket->errorString().isEmpty()
                              ? QStringLiteral("waitForBytesWritten timed out")
                              : socket->errorString();
        }
        return false;
    }
    return true;
}

} // namespace

UnixSocketWebSocket::UnixSocketWebSocket(QObject* parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);

    connect(m_socket, &QLocalSocket::connected, this, &UnixSocketWebSocket::onSocketConnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &UnixSocketWebSocket::onSocketReadyRead);
//    connect(m_socket, &QLocalSocket::errorOccurred,
//            this, &UnixSocketWebSocket::onSocketErrorOccurred);
    connect(m_socket, &QLocalSocket::disconnected, this, &UnixSocketWebSocket::onSocketDisconnected);
    connect(m_timer, &QTimer::timeout, this, &UnixSocketWebSocket::onTimeout);
}

UnixSocketWebSocket::~UnixSocketWebSocket()
{
    close();
}

void UnixSocketWebSocket::open(const QString& socketPath, const QString& requestPath,
                               const QMap<QString, QString>& queryParams,
                               const QString& protocol)
{
    if (m_state == ConnectingState || m_state == ConnectedState) {
        close();
    }

    m_socketPath = socketPath;
    m_wsPath = requestPath;
    m_queryParams = queryParams;
    m_protocol = protocol;
    m_readBuffer.clear();
    m_handshakeDone = false;
    m_errorString.clear();
    m_state = ConnectingState;

    m_socket->connectToServer(socketPath);
    if (!m_socket->waitForConnected(5000)) {
        m_errorString = m_socket->errorString();
        m_state = ClosedState;
        emit socketError(QAbstractSocket::HostNotFoundError);
        return;
    }
}

void UnixSocketWebSocket::close()
{
    if (m_state == ClosedState) {
        return;
    }
    m_state = ClosedState;
    m_handshakeDone = false;
    m_timer->stop();
    m_socket->disconnectFromServer();
}

qint64 UnixSocketWebSocket::sendTextMessage(const QString& message)
{
    if (m_state != ConnectedState || !m_handshakeDone) {
        return -1;
    }
    if (!sendWebSocketFrame(message)) {
        return -1;
    }
    return message.size();
}

void UnixSocketWebSocket::onSocketConnected()
{
    sendHandshake();
}

void UnixSocketWebSocket::onSocketReadyRead()
{
    m_readBuffer.append(m_socket->readAll());

    if (!m_handshakeDone) {
        parseHandshakeResponse();
    } else {
        readWebSocketFrame();
    }
}

void UnixSocketWebSocket::onSocketErrorOccurred(QLocalSocket::LocalSocketError)
{
    m_errorString = m_socket->errorString();
    m_state = ClosedState;
    emit socketError(QAbstractSocket::RemoteHostClosedError);
}

void UnixSocketWebSocket::onSocketDisconnected()
{
    m_state = ClosedState;
    m_handshakeDone = false;
    emit disconnected();
}

void UnixSocketWebSocket::onTimeout()
{
    m_errorString = QStringLiteral("WebSocket handshake timeout");
    m_state = ClosedState;
    emit socketError(QAbstractSocket::SocketTimeoutError);
    close();
}

void UnixSocketWebSocket::sendHandshake()
{
    QByteArray keyBytes(16, 0);
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(keyBytes.data()), 4);
    const QByteArray keyB64 = keyBytes.toBase64();
    m_key = QString::fromLatin1(keyB64);

    QByteArray requestLine = "GET " + m_wsPath.toLatin1();
    if (!m_queryParams.isEmpty()) {
        QByteArray query;
        for (auto it = m_queryParams.constBegin(); it != m_queryParams.constEnd(); ++it) {
            if (!query.isEmpty()) query += "&";
            query += it.key().toLatin1() + "=" + it.value().toLatin1();
        }
        requestLine += "?" + query;
    }
    requestLine += " HTTP/1.1\r\n";

    QByteArray handshake = requestLine;
    handshake += "Host: localhost\r\n";
    handshake += "Upgrade: websocket\r\n";
    handshake += "Connection: Upgrade\r\n";
    handshake += "Sec-WebSocket-Key: " + keyB64 + "\r\n";
    if (!m_protocol.isEmpty()) {
        handshake += "Sec-WebSocket-Protocol: " + m_protocol.toLatin1() + "\r\n";
    }
    handshake += "Sec-WebSocket-Version: 13\r\n";
    handshake += "User-Agent: QtWebSocket/5.0\r\n";
    handshake += "\r\n";

    qDebug() << "Sending handshake request:\n" << handshake;
    m_socket->write(handshake);
    m_socket->flush();

    m_timer->start(10000);
}

void UnixSocketWebSocket::parseHandshakeResponse()
{
    int headerEnd = m_readBuffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return;
    }

    QByteArray header = m_readBuffer.left(headerEnd);
    m_readBuffer = m_readBuffer.mid(headerEnd + 4);

    qDebug() << "Full server response:\n" << header;

    if (header.contains("101 Switching Protocols")) {
        const QByteArray keyB64 = m_key.toLatin1();
        const QByteArray acceptKey = keyB64 + QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        const QByteArray acceptKeyHash = QCryptographicHash::hash(acceptKey, QCryptographicHash::Sha1);
        const QByteArray expectedAcceptValue = acceptKeyHash.toBase64();
        qDebug() << "Accept verification:"
                 << "\n  keyB64=" << keyB64
                 << "\n  expectedAcceptValue=" << expectedAcceptValue;

        // Extract server's accept value (case-insensitive header match)
        QByteArray serverAcceptValue;
        const QList<QByteArray> lines = header.split('\n');
        for (const QByteArray& line : lines) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.toLower().startsWith("sec-websocket-accept:")) {
                serverAcceptValue = trimmed.mid(trimmed.indexOf(':') + 1).trimmed();
                break;
            }
        }
        if (serverAcceptValue == expectedAcceptValue) {
            m_handshakeDone = true;
            m_timer->stop();
            m_state = ConnectedState;
            emit connected();
        } else {
            qDebug() << "Accept mismatch!";
            QList<QByteArray> lines = header.split('\n');
            for (const QByteArray& line : lines) {
                if (line.toLower().startsWith("sec-websocket-accept:")) {
                    qDebug() << "  Server accept:" << line;
                }
            }
            m_errorString = QStringLiteral("Invalid Sec-WebSocket-Accept from server");
            emit socketError(QAbstractSocket::UnknownSocketError);
            close();
        }
    } else {
        m_errorString = QStringLiteral("Handshake failed: %1").arg(QString::fromUtf8(header));
        emit socketError(QAbstractSocket::UnknownSocketError);
        close();
    }
}

void UnixSocketWebSocket::readWebSocketFrame()
{
    while (m_readBuffer.size() >= 2) {
        char firstByte = m_readBuffer[0];
        char secondByte = m_readBuffer[1];

        int opcode = firstByte & 0x0F;
        bool MASK = (secondByte & 0x80) != 0;
        quint64 payloadLen = secondByte & 0x7F;

        int headerLen = 2;
        if (payloadLen == 126) {
            if (m_readBuffer.size() < 4) return;
            payloadLen = ((quint8)m_readBuffer[2] << 8) | (quint8)m_readBuffer[3];
            headerLen = 4;
        } else if (payloadLen == 127) {
            if (m_readBuffer.size() < 10) return;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | (quint8)m_readBuffer[2 + i];
            }
            headerLen = 10;
        }

        int maskOffset = headerLen;
        int payloadOffset = headerLen;
        int totalLen = maskOffset + (MASK ? 4 : 0) + (int)payloadLen;
        if (m_readBuffer.size() < totalLen) {
            return;
        }

        QByteArray payload;
        payload.reserve((int)payloadLen);
        if (MASK) {
            char maskKey[4];
            maskKey[0] = m_readBuffer[maskOffset];
            maskKey[1] = m_readBuffer[maskOffset + 1];
            maskKey[2] = m_readBuffer[maskOffset + 2];
            maskKey[3] = m_readBuffer[maskOffset + 3];
            for (quint64 i = 0; i < payloadLen; ++i) {
                payload.append(m_readBuffer[payloadOffset + (int)i] ^ maskKey[i % 4]);
            }
        } else {
            payload = m_readBuffer.mid(payloadOffset, (int)payloadLen);
        }
        m_readBuffer = m_readBuffer.mid(totalLen);

        if (opcode == 0x01) {
            emit textMessageReceived(QString::fromUtf8(payload));
        } else if (opcode == 0x09) {
            // Ping — reply with pong (opcode 0x0A) carrying same payload
            sendControlFrame(0x0A, payload);
        } else if (opcode == 0x08) {
            close();
            return;
        }
    }
}

bool UnixSocketWebSocket::sendWebSocketFrame(const QString& text)
{
    QByteArray payload = text.toUtf8();

    QByteArray frame;
    // FIN + text opcode
    frame.append(static_cast<char>(0x81));

    // RFC 6455 §5.1: client MUST mask all frames (set bit 7 of length byte)
    if (payload.size() < 126) {
        frame.append(static_cast<char>(0x80 | payload.size()));
    } else if (payload.size() < 65536) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((payload.size() >> 8) & 0xFF));
        frame.append(static_cast<char>(payload.size() & 0xFF));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        const quint64 len = static_cast<quint64>(payload.size());
        for (int i = 7; i >= 0; --i) {
            frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }

    // Generate 4-byte masking key
    char maskKey[4];
    quint32 maskVal = QRandomGenerator::global()->generate();
    memcpy(maskKey, &maskVal, 4);
    frame.append(maskKey, 4);

    // XOR payload with masking key
    for (int i = 0; i < payload.size(); ++i) {
        frame.append(payload[i] ^ maskKey[i % 4]);
    }

    QString writeError;
    if (!writeAllToLocalSocket(m_socket, frame, &writeError)) {
        m_errorString = writeError;
        emit socketError(QAbstractSocket::UnknownSocketError);
        return false;
    }
    return true;
}

void UnixSocketWebSocket::sendControlFrame(int opcode, const QByteArray& payload)
{
    QByteArray frame;
    frame.append(static_cast<char>(0x80 | opcode)); // FIN + opcode
    // Masked, payload length (control frames max 125 bytes)
    frame.append(static_cast<char>(0x80 | qMin(payload.size(), 125)));

    char maskKey[4];
    quint32 maskVal = QRandomGenerator::global()->generate();
    memcpy(maskKey, &maskVal, 4);
    frame.append(maskKey, 4);

    int len = qMin(payload.size(), 125);
    for (int i = 0; i < len; ++i) {
        frame.append(payload[i] ^ maskKey[i % 4]);
    }

    QString writeError;
    if (!writeAllToLocalSocket(m_socket, frame, &writeError)) {
        m_errorString = writeError;
    }
}

UnixHttpResponse UnixSocketWebSocket::httpGet(const QString& path, const QMap<QString, QString>& headers)
{
    return sendHttpRequest(QStringLiteral("GET"), path, QByteArray(), headers);
}

UnixHttpResponse UnixSocketWebSocket::httpPost(const QString& path, const QByteArray& body, const QMap<QString, QString>& headers)
{
    return sendHttpRequest(QStringLiteral("POST"), path, body, headers);
}

UnixHttpResponse UnixSocketWebSocket::httpDelete(const QString& path, const QMap<QString, QString>& headers)
{
    return sendHttpRequest(QStringLiteral("DELETE"), path, QByteArray(), headers);
}

UnixHttpResponse UnixSocketWebSocket::sendHttpRequest(const QString& method, const QString& path,
                                                     const QByteArray& body, const QMap<QString, QString>& headers)
{
    UnixHttpResponse resp;

    QLocalSocket sock;
    sock.connectToServer(socketPath());
    if (!sock.waitForConnected(5000)) {
        resp.errorString = sock.errorString();
        return resp;
    }

    QString requestLine = QStringLiteral("%1 %2 HTTP/1.1\r\n").arg(method).arg(path);
    QString httpReq = requestLine;
    httpReq += QStringLiteral("Host: localhost\r\n");
    if (!headers.isEmpty()) {
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
            httpReq += it.key() + QStringLiteral(": ") + it.value() + QStringLiteral("\r\n");
        }
    }
    if (method == QStringLiteral("POST") && !body.isEmpty()) {
        httpReq += QStringLiteral("Content-Type: application/json\r\n");
        httpReq += QStringLiteral("Content-Length: %1\r\n").arg(body.size());
    }
    httpReq += QStringLiteral("Connection: close\r\n");
    httpReq += QStringLiteral("\r\n");

    QByteArray reqData = httpReq.toUtf8();
    if (!body.isEmpty()) {
        reqData.append(body);
    }
    sock.write(reqData);
    sock.flush();

    QByteArray allData;
    while (sock.waitForReadyRead(5000)) {
        allData.append(sock.readAll());
        if (sock.bytesAvailable() == 0) break;
    }
    sock.disconnectFromServer();

    int headerEnd = allData.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        resp.errorString = QStringLiteral("Invalid HTTP response: no header separator");
        return resp;
    }

    QByteArray statusLine = allData.left(headerEnd);
    QByteArray bodyData = allData.mid(headerEnd + 4);

    if (statusLine.contains("HTTP/1.1 ")) {
        QString statusStr = QString::fromLatin1(statusLine).section(' ', 1, 1);
        resp.statusCode = statusStr.toInt();
    } else if (statusLine.contains("HTTP/1.0 ")) {
        QString statusStr = QString::fromLatin1(statusLine).section(' ', 1, 1);
        resp.statusCode = statusStr.toInt();
    }

    if (resp.statusCode == 0) {
        resp.errorString = QStringLiteral("Failed to parse HTTP status code from: %1").arg(QString::fromLatin1(statusLine));
        return resp;
    }

    resp.body = bodyData;
    return resp;
}

QString UnixSocketWebSocket::socketPath() const
{
    if (!m_configuredSocketPath.isEmpty()) {
        return m_configuredSocketPath;
    }

    const QString legacyPath = QDir::homePath() + QStringLiteral("/.kylinbot/gateway.sock");

    QString runtimeDir = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("XDG_RUNTIME_DIR"));
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStringLiteral("/run/user/%1").arg(getuid());
    }

    const QString runtimePath = runtimeDir + QStringLiteral("/kylin-bot/gateway.sock");
    if (QFileInfo::exists(runtimePath)) {
        return runtimePath;
    }
    if (QFileInfo::exists(legacyPath)) {
        return legacyPath;
    }
    return runtimePath;
}
