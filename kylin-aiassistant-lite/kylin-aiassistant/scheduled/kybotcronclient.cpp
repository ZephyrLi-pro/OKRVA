#include "kybotcronclient.h"

#include "../configmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <unistd.h>
#include <QDebug>

#include "global/kaglobal.h"

KybotCronClient::KybotCronClient(QObject* parent)
    : QObject(parent)
{
}

void KybotCronClient::prepareForShutdown()
{
    qWarning() << "[SHUTDOWN] KybotCronClient::prepareForShutdown";
    m_shuttingDown = true;
}

bool KybotCronClient::hasToken() const
{
    const QString token = ConfigManager::getInstance()
        ->getConfigValue("gateway-token").toString();
    return !token.isEmpty();
}

bool KybotCronClient::readToken(QString& tokenOut) const
{
    tokenOut = ConfigManager::getInstance()
        ->getConfigValue("gateway-token").toString();
    return !tokenOut.isEmpty();
}

QString KybotCronClient::socketPath() const
{
    QString path = ConfigManager::getInstance()
        ->getConfigValue("gateway-socket-path").toString().trimmed();
    if (!path.isEmpty()) {
        if (path.startsWith(QStringLiteral("~/"))) {
            path = QDir::homePath() + path.mid(1);
        }
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

KybotCronClient::SocketResponse KybotCronClient::doSocketGet(
    const QString& path, const QString& token) const
{
    SocketResponse resp;
    if (m_shuttingDown) {
        qWarning() << "[SHUTDOWN] KybotCronClient::doSocketGet skipped" << path;
        resp.error = QStringLiteral("shutting down");
        return resp;
    }

    const QString sockPath = socketPath();

    QLocalSocket sock;
    sock.connectToServer(sockPath);
    if (!sock.waitForConnected(5000)) {
        resp.error = QStringLiteral("Unix socket connect failed (%1): %2")
            .arg(sockPath, sock.errorString());
        return resp;
    }

    const QString httpReq =
        QStringLiteral("GET %1 HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "Authorization: Bearer %2\r\n"
                       "Accept: application/json\r\n"
                       "Connection: close\r\n"
                       "\r\n")
        .arg(path, token);

    sock.write(httpReq.toUtf8());
    sock.flush();

    QByteArray allData;
    while (!m_shuttingDown) {
        if (!sock.waitForReadyRead(500)) {
            break;
        }
        allData.append(sock.readAll());
        if (sock.bytesAvailable() == 0 && sock.state() == QLocalSocket::ConnectedState) {
            continue;
        }
        if (allData.contains("\r\n\r\n")) {
            break;
        }
    }
    sock.abort();
    sock.disconnectFromServer();

    const int headerEnd = allData.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        resp.error = QStringLiteral("Invalid HTTP response from Unix socket");
        return resp;
    }

    const QString statusLineStr = QString::fromLatin1(allData.left(headerEnd));
    if (statusLineStr.contains(QStringLiteral("HTTP/1."))) {
        resp.statusCode = statusLineStr.section(QLatin1Char(' '), 1, 1).toInt();
    }
    resp.body = allData.mid(headerEnd + 4);
    return resp;
}

bool KybotCronClient::handleErrorStatus(int statusCode, const QString& error,
                                        const QString& context, const QString& jobId)
{
    if (statusCode == 401) {
        KyInfo() << "KybotCronClient: 401 Unauthorized, clearing gateway token";
        ConfigManager::getInstance()->updateConfig("gateway-token", "");
        Q_EMIT tokenInvalidated();
        return true;
    }
    if (statusCode == 0 || statusCode >= 400) {
        const QString reason = error.isEmpty()
            ? QStringLiteral("HTTP %1").arg(statusCode)
            : error;
        KyInfo() << "KybotCronClient:" << context << "failed:" << reason;
        if (jobId.isEmpty()) {
            Q_EMIT jobsFetchFailed(reason);
        } else {
            Q_EMIT runsFetchFailed(jobId, reason);
        }
        return true;
    }
    return false;
}

void KybotCronClient::fetchAllJobs()
{
    QString token;
    if (!readToken(token)) {
        KyInfo() << "KybotCronClient::fetchAllJobs missing token";
        Q_EMIT tokenMissing();
        return;
    }

    KyInfo() << "KybotCronClient::fetchAllJobs -> socket:" << socketPath();
    const SocketResponse resp = doSocketGet(QStringLiteral("/api/cron"), token);

    if (handleErrorStatus(resp.statusCode, resp.error, QStringLiteral("fetchAllJobs"))) {
        return;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(resp.body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString reason = QStringLiteral("invalid json: %1").arg(err.errorString());
        KyInfo() << "KybotCronClient::fetchAllJobs" << reason;
        Q_EMIT jobsFetchFailed(reason);
        return;
    }

    const QJsonArray jobs = doc.object().value("jobs").toArray();
    KyInfo() << "KybotCronClient::fetchAllJobs ok, jobs count=" << jobs.size();
    Q_EMIT jobsFetched(jobs);
}

void KybotCronClient::fetchRuns(const QString& jobId, int limit)
{
    QString token;
    if (!readToken(token)) {
        Q_EMIT tokenMissing();
        return;
    }
    if (jobId.isEmpty()) {
        Q_EMIT runsFetchFailed(jobId, QStringLiteral("empty jobId"));
        return;
    }

    QString path = QStringLiteral("/api/cron/%1/runs").arg(jobId);
    if (limit > 0) {
        path += QStringLiteral("?limit=%1").arg(limit);
    }

    KyInfo() << "KybotCronClient::fetchRuns ->" << path;
    const SocketResponse resp = doSocketGet(path, token);

    if (handleErrorStatus(resp.statusCode, resp.error,
                          QStringLiteral("fetchRuns"), jobId)) {
        return;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(resp.body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString reason = QStringLiteral("invalid json: %1").arg(err.errorString());
        KyInfo() << "KybotCronClient::fetchRuns" << reason << "jobId=" << jobId;
        Q_EMIT runsFetchFailed(jobId, reason);
        return;
    }

    const QJsonArray runs = doc.object().value("runs").toArray();
    KyInfo() << "KybotCronClient::fetchRuns ok, jobId=" << jobId
             << "count=" << runs.size();
    Q_EMIT runsFetched(jobId, runs);
}

void KybotCronClient::fetchRecentRuns(int limit)
{
    QString token;
    if (!readToken(token)) {
        Q_EMIT tokenMissing();
        return;
    }

    QString path = QStringLiteral("/api/cron/runs/recent?job_type=agent");
    if (limit > 0) {
        path += QStringLiteral("&limit=%1").arg(limit);
    }

    KyInfo() << "KybotCronClient::fetchRecentRuns ->" << path;
    const SocketResponse resp = doSocketGet(path, token);

    if (handleErrorStatus(resp.statusCode, resp.error,
                          QStringLiteral("fetchRecentRuns"))) {
        return;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(resp.body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString reason = QStringLiteral("invalid json: %1").arg(err.errorString());
        KyInfo() << "KybotCronClient::fetchRecentRuns" << reason;
        Q_EMIT recentRunsFetchFailed(reason);
        return;
    }

    const QJsonArray runs = doc.object().value("runs").toArray();
    KyInfo() << "KybotCronClient::fetchRecentRuns ok, count=" << runs.size();
    Q_EMIT recentRunsFetched(runs);
}
