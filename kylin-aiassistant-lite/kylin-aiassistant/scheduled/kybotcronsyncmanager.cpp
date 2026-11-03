#include "kybotcronsyncmanager.h"

#include "kybotcronclient.h"
#include "../msgpane.h"

#include <QDateTime>
#include <QJsonValue>
#include <QTimer>
#include <QDebug>

#include "global/kaglobal.h"

KybotCronSyncManager::KybotCronSyncManager(KybotCronClient* client,
                                           CMsgPane* msgPane,
                                           int intervalMs,
                                           QObject* parent)
    : QObject(parent),
      m_client(client),
      m_msgPane(msgPane),
      m_intervalMs(intervalMs)
{
    if (m_client) {
        connect(m_client.data(), &KybotCronClient::recentRunsFetched,
                this, &KybotCronSyncManager::onRecentRunsFetched);
        connect(m_client.data(), &KybotCronClient::recentRunsFetchFailed,
                this, &KybotCronSyncManager::onRecentRunsFetchFailed);
        connect(m_client.data(), &KybotCronClient::tokenMissing,
                this, &KybotCronSyncManager::onTokenMissing);
        connect(m_client.data(), &KybotCronClient::tokenInvalidated,
                this, &KybotCronSyncManager::onTokenInvalidated);
    }
}

void KybotCronSyncManager::start()
{
    if (m_intervalMs > 0 && !m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(m_intervalMs);
        connect(m_timer, &QTimer::timeout, this, &KybotCronSyncManager::syncOnce);
        m_timer->start();
    }
    // 启动后立即跑一次，补齐离线期间的 run
    QTimer::singleShot(0, this, &KybotCronSyncManager::syncOnce);
}

void KybotCronSyncManager::stop()
{
    qWarning() << "[SHUTDOWN] KybotCronSyncManager::stop";
    m_stopped = true;
    if (m_timer) {
        m_timer->stop();
    }
}

void KybotCronSyncManager::syncOnce()
{
    if (m_stopped) {
        qWarning() << "[SHUTDOWN] KybotCronSyncManager::syncOnce skipped (stopped)";
        return;
    }

    if (!m_client) {
        KyInfo() << "KybotCronSync: syncOnce skipped (no client)";
        return;
    }
    if (!m_client->hasToken()) {
        // 未配对：不打扰用户，等用户先在 task mode 配对
        KyInfo() << "KybotCronSync: syncOnce skipped (no gateway-token; "
                    "task mode pairing not done yet)";
        return;
    }
    KyInfo() << "KybotCronSync: syncOnce triggered";
    m_client->fetchRecentRuns(100);
}

void KybotCronSyncManager::onRecentRunsFetched(const QJsonArray& runs)
{
    if (!m_msgPane) return;

    int imported = 0;
    int skipped = 0;
    // kybot 返回 runs 按 started_at DESC 排列，按时间顺序导入更符合直觉
    for (int i = runs.size() - 1; i >= 0; --i) {
        if (importOneRun(runs.at(i).toObject())) {
            ++imported;
        } else {
            ++skipped;
        }
    }

    KyInfo() << "KybotCronSync: recent runs processed, total=" << runs.size()
             << "imported=" << imported << "skipped=" << skipped;
}

void KybotCronSyncManager::onRecentRunsFetchFailed(const QString& reason)
{
    KyInfo() << "KybotCronSync: fetch recent runs failed:" << reason;
}

void KybotCronSyncManager::onTokenMissing()
{
    KyInfo() << "KybotCronSync: gateway-token missing, sync skipped";
}

void KybotCronSyncManager::onTokenInvalidated()
{
    KyInfo() << "KybotCronSync: gateway-token invalidated (401), triggering re-pairing";
    if (m_msgPane) {
        m_msgPane->syncGatewayPairing();
    }
}

bool KybotCronSyncManager::importOneRun(const QJsonObject& run)
{
    const QString jobType = run.value(QStringLiteral("job_type")).toString();
    if (jobType == QStringLiteral("shell")) {
        return false;
    }

    const QString jobId = run.value(QStringLiteral("job_id")).toString();
    if (jobId.isEmpty()) {
        return false;
    }

    // CronRun.id 在 kybot 端是 i64（数字字面量），所以 toString() 直接调用返回空。
    // 必须经 QVariant 转换才能正确把数字渲染为字符串。
    const QJsonValue idVal = run.value(QStringLiteral("id"));
    QString runId;
    if (idVal.isDouble()) {
        runId = QString::number(static_cast<qint64>(idVal.toVariant().toLongLong()));
    } else if (idVal.isString()) {
        runId = idVal.toString();
    }
    if (runId.isEmpty()) {
        KyInfo() << "KybotCronSync: importOneRun skipped (empty runId), jobId=" << jobId;
        return false;
    }

    // 进程内已处理过的 run 直接跳过
    if (m_seenRunIds.contains(runId)) {
        return false;
    }

    // output 可能为 null（run 仍在进行中）— 跳过，等下次同步
    const QJsonValue outputVal = run.value(QStringLiteral("output"));
    if (outputVal.isNull() || outputVal.isUndefined()) {
        return false;
    }

    const QString runOutput = outputVal.toString();
    const QString runStatus = run.value(QStringLiteral("status")).toString();
    const qint64  startedAtSecs = parseRfc3339ToEpochSecs(
        run.value(QStringLiteral("started_at")).toString());

    QString jobName = run.value(QStringLiteral("job_name")).toString();
    if (jobName.isEmpty()) {
        jobName = tr("Scheduled Task");
    }
    const QString jobPrompt = run.value(QStringLiteral("job_prompt")).toString();

    const bool inserted = m_msgPane->importKybotCronRun(
        jobId, jobName, jobPrompt,
        runId, runOutput, runStatus, startedAtSecs);

    // 不论是否新插入都记入内存集合，避免本进程后续重复尝试
    m_seenRunIds.insert(runId);

    if (inserted) {
        const QString sessionUuid = QStringLiteral("kybot-cron:%1").arg(runId);
        KyInfo() << "KybotCronSyncManager: imported run" << sessionUuid
                 << "job=" << jobName << "status=" << runStatus;
        Q_EMIT runImported(sessionUuid);
    }
    return inserted;
}

qint64 KybotCronSyncManager::parseRfc3339ToEpochSecs(const QString& s)
{
    if (s.isEmpty()) return 0;
    // RFC3339 与 ISO8601 在常见形态下兼容，Qt 的 ISODateWithMs 接受时区偏移
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(s, Qt::ISODate);
    }
    if (!dt.isValid()) {
        return 0;
    }
    return dt.toSecsSinceEpoch();
}
