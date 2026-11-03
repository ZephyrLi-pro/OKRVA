#ifndef WEBAPIBRIDGE_H
#define WEBAPIBRIDGE_H

#include <QObject>
#include <QLocalSocket>
#include <atomic>

/**
 * QWebChannel 桥接类：将 WebEngineView 中 JS 的 HTTP 请求
 * 转发到 gateway 后端（优先 Unix Socket，失败时回退到 gateway-host HTTP）。
 *
 * JS 调用 httpRequest() 发起请求，C++ 在工作线程中完成 HTTP 请求，
 * 完成后通过 httpResponse 信号返回结果。
 */
class WebApiBridge : public QObject
{
    Q_OBJECT
public:
    explicit WebApiBridge(QObject *parent = nullptr);
    ~WebApiBridge() override;

    /**
     * JS 端调用入口。
     * @param requestId  请求唯一标识，用于 JS 端匹配 Promise
     * @param method     HTTP 方法：GET / POST / PUT / PATCH / DELETE / OPTIONS
     * @param path       请求路径，如 /api/status
     * @param body       请求体（JSON 字符串），GET 时传空串
     * @param headersJson 请求头 JSON 对象字符串，如 {"Authorization":"Bearer xxx"}
     */
    Q_INVOKABLE void httpRequest(const QString &requestId,
                                 const QString &method,
                                 const QString &path,
                                 const QString &body,
                                 const QString &headersJson);

    /** True when gateway /health responds 200 (Unix socket or HTTP fallback). */
    Q_INVOKABLE bool isGatewayReachable();

    /** Start kylin-bot daemon (same as running `kylin-bot daemon`). Returns whether launch was attempted. */
    Q_INVOKABLE bool startGatewayDaemon();

    /** True on machines that must avoid the provider dropdown's native scroll layer. */
    Q_INVOKABLE bool providerDropdownSafeMode() const;

    /** True when Qt is using a native Wayland platform plugin. */
    Q_INVOKABLE bool isWaylandPlatform() const;

    /** Stop accepting bridge calls during app shutdown (avoids blocking the main thread). */
    Q_INVOKABLE void prepareForShutdown();

signals:
    /**
     * 请求完成后发射，JS 端通过监听此信号获取响应。
     * @param requestId  与请求对应的唯一标识
     * @param statusCode HTTP 状态码，0 表示连接失败
     * @param body       响应体字符串
     */
    void httpResponse(const QString &requestId,
                      int statusCode,
                      const QString &body);

private:
    QString socketPath() const;
    void noteHttpWorkerDone(const QString &requestId);

    bool m_shuttingDown = false;
    std::atomic<int> m_inFlightHttp{0};
};

#endif // WEBAPIBRIDGE_H
