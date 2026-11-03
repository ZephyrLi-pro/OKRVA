#ifndef FOLLOWUPMANAGER_H
#define FOLLOWUPMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>
#include "docparse/docparsethread.h"

/**
 * @brief 追问管理器类
 * 负责管理追问功能的生命周期，包括：
 * - 初始化追问线程
 * - 构建和发送追问请求
 * - 处理追问结果
 * - 超时处理
 * - 显式销毁资源
 */
class FollowUpManager : public QObject
{
    Q_OBJECT

public:
    explicit FollowUpManager(QObject *parent = nullptr);
    ~FollowUpManager();

    /**
     * @brief 初始化追问管理器
     * 创建追问线程和超时定时器
     */
    void initialize();

    /**
     * @brief 构建并发送追问请求
     * @param answerContent AI回答的内容
     * @param isUserStopped 用户是否主动停止回答
     */
    void buildFollowUpRequest(const QString &answerContent, bool isUserStopped);

    /**
     * @brief 停止追问请求
     * 停止当前正在进行的追问请求
     */
    void stopFollowUpRequest();

    /**
     * @brief 显式销毁资源
     * 停止线程并清理所有资源
     */
    void destroy();

    /**
     * @brief 检查是否正在处理追问请求
     * @return true 如果正在处理，false 否则
     */
    bool isProcessing() const;

signals:
    /**
     * @brief 追问问题生成完成信号
     * @param questions 生成的问题列表
     */
    void questionsGenerated(const QStringList &questions);

    /**
     * @brief 追问请求失败信号
     * @param error 错误信息
     */
    void requestFailed(const QString &error);

private slots:
    /**
     * @brief 处理追问线程返回的消息
     * @param message JSON格式的消息
     */
    void onRecFollowUpMsg(const QString &message);

    /**
     * @brief 处理追问线程错误
     * @param error 错误信息
     */
    void onFollowUpError(const QString &error);

    /**
     * @brief 超时处理
     */
    void onTimeout();

private:
    /**
     * @brief 解析追问结果
     * @param result 原始结果字符串
     * @return 解析后的问题列表
     */
    QStringList parseQuestions(const QString &result);

    DocParseThread *m_followUpThread = nullptr;      // 追问处理线程
    QTimer *m_timeoutTimer = nullptr;               // 超时定时器
    QString m_followUpRecvMsg;                       // 累计追问结果
    bool m_isFollowUpEnd = false;                    // 追问是否结束
    bool m_followUpTimeout = false;                  // 是否超时
    bool m_isProcessing = false;                     // 是否正在处理
    static const int TIMEOUT_INTERVAL = 10000;      // 超时时间（10秒）
};

#endif // FOLLOWUPMANAGER_H


