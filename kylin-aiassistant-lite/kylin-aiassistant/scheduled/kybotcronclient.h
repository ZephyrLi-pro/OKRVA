#ifndef KYBOTCRONCLIENT_H
#define KYBOTCRONCLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QString>

/**
 * KybotCronClient
 *
 * 对 KylinBot Gateway 的 /api/cron 系列接口的轻量 REST 封装，仅暴露 Qt 助手
 * 同步 cron 历史所需的只读接口：
 *   GET /api/cron/runs/recent   → 列出最近 N 次运行（含已删除的单次任务）
 *   GET /api/cron               → 列出所有 cron job（保留兼容）
 *   GET /api/cron/{id}/runs     → 列出某 job 的最近 N 次运行（保留兼容）
 *
 * 鉴权方式：`Authorization: Bearer <gateway-token>`，token 复用
 * `ConfigManager` 中现有的 `gateway-token`，与 `GatewayWebSocketManager` 一致。
 * 通信方式：通过 Unix socket（路径取自 `gateway-socket-path` 配置项，
 * 默认为 ~/.kylinbot/gateway.sock）发送 HTTP/1.1 请求，不再走 TCP。
 *
 * 设计要点：
 *   - 不持有 token 副本，每次发请求重新从 ConfigManager 取，
 *     这样用户重新配对后无需重启即生效；
 *   - 没有 token 时直接发 tokenMissing 信号，调用方据此跳过同步；
 *   - 网络错误一律静默并发 fetch*Failed 信号，调用方决定是否重试。
 *
 * 线程：仅在主线程使用。
 */
class KybotCronClient : public QObject
{
    Q_OBJECT
public:
    explicit KybotCronClient(QObject* parent = nullptr);
    ~KybotCronClient() override = default;

    // 是否已有配对 token。无 token 时所有 fetch* 都会安静失败并发 tokenMissing。
    bool hasToken() const;

    // GET /api/cron — 拉取全部 cron job 列表。
    // 成功：发 jobsFetched(QJsonArray jobs)；
    // 失败：发 jobsFetchFailed(QString reason)。
    void fetchAllJobs();

    // GET /api/cron/{id}/runs?limit=N — 拉取指定 job 的最近运行。
    // 成功：发 runsFetched(QString jobId, QJsonArray runs)；
    // 失败：发 runsFetchFailed(QString jobId, QString reason)。
    void fetchRuns(const QString& jobId, int limit = 20);

    // GET /api/cron/runs/recent?limit=N&job_type=agent — 拉取全局最近运行。
    // 成功：发 recentRunsFetched(QJsonArray runs)；
    // 失败：发 recentRunsFetchFailed(QString reason)。
    void fetchRecentRuns(int limit = 100);

    /** Abort in-flight socket reads during app shutdown. */
    void prepareForShutdown();

Q_SIGNALS:
    void jobsFetched(const QJsonArray& jobs);
    void jobsFetchFailed(const QString& reason);

    void runsFetched(const QString& jobId, const QJsonArray& runs);
    void runsFetchFailed(const QString& jobId, const QString& reason);

    void recentRunsFetched(const QJsonArray& runs);
    void recentRunsFetchFailed(const QString& reason);

    // 配对 token 缺失（用户尚未在助手内完成 kybot 配对）。
    void tokenMissing();

    // gateway-token 被服务端拒绝（401），已清空本地 token
    void tokenInvalidated();

private:
    // 取当前 gateway-socket-path（优先）或默认 ~/.kylinbot/gateway.sock。
    QString socketPath() const;

    // 取当前 token，为空返回 false。
    bool readToken(QString& tokenOut) const;

    struct SocketResponse {
        int statusCode = 0;
        QByteArray body;
        QString error;
    };

    // 通过 Unix socket 发送 HTTP/1.1 GET 请求，返回响应。
    SocketResponse doSocketGet(const QString& path, const QString& token) const;

    // 处理 401 / 错误状态码的公共逻辑，返回 true 表示已处理（调用方应直接返回）。
    bool handleErrorStatus(int statusCode, const QString& error,
                           const QString& context, const QString& jobId = QString());

    mutable bool m_shuttingDown = false;
};

#endif // KYBOTCRONCLIENT_H
