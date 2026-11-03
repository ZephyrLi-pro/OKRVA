#ifndef KYBOTCRONSYNCMANAGER_H
#define KYBOTCRONSYNCMANAGER_H

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>

class QTimer;
class KybotCronClient;
class CMsgPane;

/**
 * KybotCronSyncManager
 *
 * 周期性从 KylinBot Gateway 拉取 cron 历史，把 agent 类型 job 的每一次
 * 运行结果写入助手本地 RECORD 表（mode_type=1，与现有 task mode 历史一致）。
 *
 * 工作流：
 *   1. start() 后立即触发首次 sync（启动补齐离线期间产生的 run）；
 *   2. QTimer 每 5s 触发一次 sync；
 *   3. 单次 sync：
 *        a. GET /api/cron/runs/recent?job_type=agent&limit=100；
 *        b. 对每条 run（含已删除的单次任务），调用 CMsgPane::importKybotCronRun 去重入库；
 *        c. 新插入则发 runImported(sessionUuid) 信号。
 *
 * 容错：
 *   - kybot 未运行 / 网络错 / 401 → 静默失败，下次轮询再试；
 *   - 未配对（token 为空）→ 跳过本轮 sync，不报错；
 *   - 同一进程内用 m_seenRunIds 做内存级二级去重，避免一轮内重复 emit；
 *     但落库依然以 ChatDatabase::sessionExists 为最终唯一依据。
 *
 * 不依赖任何已有任务模式 session 状态，因此即使用户没进入 task mode
 * 也会照常同步历史。
 */
class KybotCronSyncManager : public QObject
{
    Q_OBJECT
public:
    // intervalMs <= 0 时不启 timer（用于手工/测试场景）
    KybotCronSyncManager(KybotCronClient* client,
                         CMsgPane* msgPane,
                         int intervalMs = 5 * 1000,
                         QObject* parent = nullptr);
    ~KybotCronSyncManager() override = default;

    // 启动定时器 + 立即执行一次 sync
    void start();
    void stop();

    // 主动触发一次 sync（不会重置定时器）
    Q_INVOKABLE void syncOnce();

Q_SIGNALS:
    // 单条 run 新导入到数据库时发出；sessionUuid 形如 "kybot-cron:{run_id}"
    void runImported(const QString& sessionUuid);

private Q_SLOTS:
    void onRecentRunsFetched(const QJsonArray& runs);
    void onRecentRunsFetchFailed(const QString& reason);
    void onTokenMissing();
    // 收到 tokenInvalidated 后触发重新配对
    void onTokenInvalidated();

private:
    // 解析 RFC3339 时间为秒级 epoch；失败返回 0。
    static qint64 parseRfc3339ToEpochSecs(const QString& s);

    // 把一条 run JSON 转成 RECORD 行；返回是否新增。
    bool importOneRun(const QJsonObject& run);

private:
    QPointer<KybotCronClient> m_client;
    QPointer<CMsgPane>        m_msgPane;
    QTimer*                   m_timer = nullptr;
    int                       m_intervalMs;
    bool                      m_stopped = false;

    // 内存级二级去重，进程内已 emit 过的 run_id（仅作为优化，
    // 真正的持久化去重仍由 sessionExists 保证）
    QSet<QString> m_seenRunIds;
};

#endif // KYBOTCRONSYNCMANAGER_H
