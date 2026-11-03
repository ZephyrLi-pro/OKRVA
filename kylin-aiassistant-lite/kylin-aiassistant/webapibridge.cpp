#include "webapibridge.h"
#include "configmanager.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <unistd.h>
#include <QDebug>

namespace {

QMap<QString, QString> parseHeadersJson(const QString &headersJson)
{
    QMap<QString, QString> headers;
    if (headersJson.isEmpty()) {
        return headers;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(headersJson.toUtf8());
    if (!doc.isObject()) {
        return headers;
    }
    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        headers.insert(it.key(), it.value().toString());
    }
    return headers;
}

void injectGatewayToken(QMap<QString, QString> &headers)
{
    if (headers.contains(QStringLiteral("Authorization"))) {
        return;
    }
    const QString token =
        ConfigManager::getInstance()->getConfigValue("gateway-token").toString().trimmed();
    if (!token.isEmpty()) {
        headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer ") + token);
    }
}

QString expandSocketPath(QString path)
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

bool isLikelySocketPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    // host:port 不是合法的 QLocalSocket 名称
    if (path.contains(QStringLiteral("://")) || path.contains(QLatin1Char(':'))) {
        return false;
    }
    return true;
}

QString readDmiValue(const QString &fileName)
{
    QFile file(QStringLiteral("/sys/class/dmi/id/") + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool dmiEquals(const QString &actual, const QString &expected)
{
    return actual.compare(expected, Qt::CaseInsensitive) == 0;
}

bool isLongRunningGatewayRequest(const QString &method, const QString &path)
{
    if (method != QStringLiteral("POST")) {
        return false;
    }
    const QString normalized = path.split(QLatin1Char('?')).constFirst();
    return normalized.startsWith(QStringLiteral("/api/cron/"))
        && normalized.endsWith(QStringLiteral("/run"));
}

int requestTimeoutMs(const QString &method, const QString &path)
{
    // Match gateway LONG_RUNNING_REQUEST_TIMEOUT_SECS (10 minutes).
    if (isLongRunningGatewayRequest(method, path)) {
        return 600000;
    }
    return 30000;
}

bool readUnixSocketHttpResponse(QLocalSocket &sock,
                                int totalTimeoutMs,
                                QByteArray *allData,
                                QString *errorOut)
{
    QElapsedTimer timer;
    timer.start();
    allData->clear();

    while (sock.state() != QLocalSocket::UnconnectedState) {
        const int elapsed = static_cast<int>(timer.elapsed());
        if (elapsed >= totalTimeoutMs) {
            if (errorOut) {
                *errorOut = QStringLiteral("Unix socket read timeout");
            }
            return false;
        }

        const int slice = std::min(10000, totalTimeoutMs - elapsed);
        if (sock.waitForReadyRead(slice)) {
            allData->append(sock.readAll());
            continue;
        }

        if (sock.bytesAvailable() > 0) {
            allData->append(sock.readAll());
            continue;
        }
    }

    if (sock.bytesAvailable() > 0) {
        allData->append(sock.readAll());
    }
    return true;
}

bool requestViaUnixSocket(const QString &sockPath,
                          const QString &method,
                          const QString &path,
                          const QMap<QString, QString> &headers,
                          const QByteArray &rawBody,
                          int *statusCode,
                          QByteArray *respBody,
                          QString *errorOut)
{
    QLocalSocket sock;
    sock.connectToServer(sockPath);
    if (!sock.waitForConnected(5000)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Unix socket connection failed: %1").arg(sock.errorString());
        }
        return false;
    }

    QString httpReq = QStringLiteral("%1 %2 HTTP/1.1\r\n").arg(method, path);
    httpReq += QStringLiteral("Host: localhost\r\n");

    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        httpReq += it.key() + QStringLiteral(": ") + it.value() + QStringLiteral("\r\n");
    }

    const bool hasBody =
        !rawBody.isEmpty() && method != QStringLiteral("GET") && method != QStringLiteral("HEAD");
    if (hasBody) {
        if (!headers.contains(QStringLiteral("Content-Type"))) {
            httpReq += QStringLiteral("Content-Type: application/json\r\n");
        }
        httpReq += QStringLiteral("Content-Length: %1\r\n").arg(rawBody.size());
    }

    httpReq += QStringLiteral("Connection: close\r\n\r\n");

    QByteArray reqData = httpReq.toUtf8();
    if (hasBody) {
        reqData.append(rawBody);
    }

    sock.write(reqData);
    sock.flush();

    QByteArray allData;
    const int timeoutMs = requestTimeoutMs(method, path);
    if (!readUnixSocketHttpResponse(sock, timeoutMs, &allData, errorOut)) {
        sock.disconnectFromServer();
        return false;
    }
    sock.disconnectFromServer();

    const int headerEnd = allData.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid HTTP response from Unix socket");
        }
        return false;
    }

    const QByteArray statusLine = allData.left(headerEnd);
    *respBody = allData.mid(headerEnd + 4);

    const QString statusLineStr = QString::fromLatin1(statusLine);
    if (statusLineStr.contains(QStringLiteral("HTTP/1."))) {
        *statusCode = statusLineStr.section(QLatin1Char(' '), 1, 1).toInt();
    } else {
        *statusCode = 0;
    }
    return true;
}

bool requestViaHttp(const QString &gatewayHost,
                    const QString &method,
                    const QString &path,
                    const QMap<QString, QString> &headers,
                    const QByteArray &rawBody,
                    int timeoutMs,
                    int *statusCode,
                    QByteArray *respBody,
                    QString *errorOut)
{
    const QUrl url(QStringLiteral("http://%1%2").arg(gatewayHost, path));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    QNetworkAccessManager nam;
    QNetworkReply *reply = nullptr;
    const bool hasBody =
        !rawBody.isEmpty() && method != QStringLiteral("GET") && method != QStringLiteral("HEAD");

    if (method == QStringLiteral("GET") || method == QStringLiteral("HEAD")) {
        reply = nam.get(req);
    } else if (hasBody) {
        if (!headers.contains(QStringLiteral("Content-Type"))) {
            req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        }
        reply = nam.sendCustomRequest(req, method.toUtf8(), rawBody);
    } else {
        reply = nam.sendCustomRequest(req, method.toUtf8());
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (errorOut) {
            *errorOut = QStringLiteral("HTTP request timeout");
        }
        reply->deleteLater();
        return false;
    }

    *statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (*statusCode == 0 && reply->error() != QNetworkReply::NoError) {
        if (errorOut) {
            *errorOut = QStringLiteral("HTTP request failed: %1").arg(reply->errorString());
        }
        reply->deleteLater();
        return false;
    }

    *respBody = reply->readAll();
    reply->deleteLater();
    return true;
}

} // namespace

WebApiBridge::WebApiBridge(QObject *parent)
    : QObject(parent)
{
}

QString WebApiBridge::socketPath() const
{
    QString path = ConfigManager::getInstance()->getConfigValue("gateway-socket-path").toString();
    path = expandSocketPath(path);
    if (!path.isEmpty() && isLikelySocketPath(path)) {
        return path;
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

bool WebApiBridge::providerDropdownSafeMode() const
{
    static const bool safeMode = []() {
        const QString sysVendor = readDmiValue(QStringLiteral("sys_vendor"));
        const QString productName = readDmiValue(QStringLiteral("product_name"));
        const QString productVersion = readDmiValue(QStringLiteral("product_version"));
        const QString boardName = readDmiValue(QStringLiteral("board_name"));

        const bool matched =
            dmiEquals(sysVendor, QStringLiteral("KaiTian")) &&
            dmiEquals(productName, QStringLiteral("X1f C1d")) &&
            dmiEquals(productVersion, QStringLiteral("KaiTian")) &&
            dmiEquals(boardName, QStringLiteral("LXKT-FTD3-X1"));

        qWarning() << "[WebApiBridge] providerDropdownSafeMode"
                   << "matched=" << matched
                   << "sys_vendor=" << sysVendor
                   << "product_name=" << productName
                   << "product_version=" << productVersion
                   << "board_name=" << boardName;
        return matched;
    }();
    return safeMode;
}

bool WebApiBridge::isWaylandPlatform() const
{
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"),
                                                       Qt::CaseInsensitive);
}

void WebApiBridge::noteHttpWorkerDone(const QString &requestId)
{
    qWarning() << "[SHUTDOWN] WebApiBridge worker done" << requestId
               << "inFlight=" << (m_inFlightHttp.fetch_sub(1) - 1);
}

void WebApiBridge::prepareForShutdown()
{
    qWarning() << "[SHUTDOWN] WebApiBridge::prepareForShutdown"
               << "inFlightHttp=" << m_inFlightHttp.load();
    m_shuttingDown = true;
}

WebApiBridge::~WebApiBridge()
{
    qWarning() << "[SHUTDOWN] WebApiBridge::~WebApiBridge"
               << "inFlightHttp=" << m_inFlightHttp.load();
}

void WebApiBridge::httpRequest(const QString &requestId,
                               const QString &method,
                               const QString &path,
                               const QString &body,
                               const QString &headersJson)
{
    if (m_shuttingDown) {
        qWarning() << "[SHUTDOWN] WebApiBridge::httpRequest rejected" << method << path << requestId;
        QMetaObject::invokeMethod(this, "httpResponse", Qt::QueuedConnection,
            Q_ARG(QString, requestId),
            Q_ARG(int, 0),
            Q_ARG(QString, QStringLiteral("{\"error\":\"shutting down\"}")));
        return;
    }

    m_inFlightHttp.fetch_add(1);

    const QString sockPath = socketPath();
    QString gatewayHost =
        ConfigManager::getInstance()->getConfigValue("gateway-host").toString().trimmed();
    if (gatewayHost.isEmpty()) {
        gatewayHost = QStringLiteral("127.0.0.1:42617");
    }

    QtConcurrent::run([this, requestId, method, path, body, headersJson, sockPath, gatewayHost]() {
        struct DoneGuard {
            WebApiBridge *self;
            QString id;
            ~DoneGuard() { self->noteHttpWorkerDone(id); }
        } done{this, requestId};

        if (m_shuttingDown) {
            qWarning() << "[SHUTDOWN] WebApiBridge worker skipped" << requestId;
            QMetaObject::invokeMethod(this, "httpResponse", Qt::QueuedConnection,
                Q_ARG(QString, requestId),
                Q_ARG(int, 0),
                Q_ARG(QString, QStringLiteral("{\"error\":\"shutting down\"}")));
            return;
        }

        int statusCode = 0;
        QByteArray respBody;
        QString error;

        QMap<QString, QString> headers = parseHeadersJson(headersJson);
        injectGatewayToken(headers);

        QByteArray rawBody;
        const bool isBase64Encoded = headers.contains(QStringLiteral("X-Body-Encoding")) &&
            headers.value(QStringLiteral("X-Body-Encoding")) == QStringLiteral("base64");
        if (isBase64Encoded) {
            headers.remove(QStringLiteral("X-Body-Encoding"));
            rawBody = QByteArray::fromBase64(body.toUtf8());
        } else {
            rawBody = body.toUtf8();
        }

        const int timeoutMs = requestTimeoutMs(method, path);

        const bool viaSocket = requestViaUnixSocket(
            sockPath, method, path, headers, rawBody, &statusCode, &respBody, &error);

        if (!viaSocket) {
            const QString socketErr = error;
            if (!requestViaHttp(gatewayHost,
                                method,
                                path,
                                headers,
                                rawBody,
                                timeoutMs,
                                &statusCode,
                                &respBody,
                                &error)) {
                const QString combined = socketErr.isEmpty()
                    ? error
                    : QStringLiteral("%1; HTTP fallback: %2").arg(socketErr, error);
                QMetaObject::invokeMethod(this, "httpResponse", Qt::QueuedConnection,
                    Q_ARG(QString, requestId),
                    Q_ARG(int, 0),
                    Q_ARG(QString, QStringLiteral("{\"error\":\"%1\"}").arg(combined)));
                return;
            }
        }

        QMetaObject::invokeMethod(this, "httpResponse", Qt::QueuedConnection,
            Q_ARG(QString, requestId),
            Q_ARG(int, statusCode),
            Q_ARG(QString, QString::fromUtf8(respBody)));
    });
}

bool WebApiBridge::isGatewayReachable()
{
    if (m_shuttingDown) {
        qWarning() << "[SHUTDOWN] WebApiBridge::isGatewayReachable skipped (shutting down)";
        return false;
    }

    int statusCode = 0;
    QByteArray respBody;
    QString error;

    // Public /health — do not attach gateway token (stale token must not block liveness).
    const QMap<QString, QString> headers;

    const QString sockPath = socketPath();
    const int timeoutMs = 5000;

    if (requestViaUnixSocket(sockPath,
                             QStringLiteral("GET"),
                             QStringLiteral("/health"),
                             headers,
                             QByteArray(),
                             &statusCode,
                             &respBody,
                             &error)
        && statusCode >= 200 && statusCode < 300) {
        return true;
    }

    QString gatewayHost =
        ConfigManager::getInstance()->getConfigValue("gateway-host").toString().trimmed();
    if (gatewayHost.isEmpty()) {
        gatewayHost = QStringLiteral("127.0.0.1:42617");
    }

    statusCode = 0;
    respBody.clear();
    error.clear();
    if (requestViaHttp(gatewayHost,
                       QStringLiteral("GET"),
                       QStringLiteral("/health"),
                       headers,
                       QByteArray(),
                       timeoutMs,
                       &statusCode,
                       &respBody,
                       &error)) {
        return statusCode >= 200 && statusCode < 300;
    }

    return false;
}

bool WebApiBridge::startGatewayDaemon()
{
    if (m_shuttingDown) {
        return false;
    }

    if (isGatewayReachable()) {
        return true;
    }

    // Match manual "kylin-bot daemon" (PATH-resolved). Do not use autostart.sh —
    // it spawns extra socat/bash helpers and can leave the app stuck on exit.
    return QProcess::startDetached(QStringLiteral("kylin-bot"),
                                   {QStringLiteral("daemon")});
}
