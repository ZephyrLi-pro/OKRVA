#ifndef READERCONTROLPROXY_H
#define READERCONTROLPROXY_H

#include <QObject>
#include "readercontrol.h"

/**
 * @brief ReaderControl 的代理类，用于在主线程中提供异步调用接口
 * 
 * 该类在主线程中创建，所有方法调用都通过 QMetaObject::invokeMethod 
 * 异步转发到工作线程中的 ReaderControl 对象，确保 QML 调用不会阻塞。
 */
class ReaderControlProxy : public QObject
{
    Q_OBJECT
public:
    explicit ReaderControlProxy(ReaderControl *readerControl, QObject *parent = nullptr);
    ~ReaderControlProxy();

    /**
     * @brief 设置实际的 ReaderControl 对象
     */
    void setReaderControl(ReaderControl *readerControl);

signals:
    /**
     * @brief 启动播放信号
     */
    void sigPlayingStart();
    /**
     * @brief 朗读结束信号
     * @param 索引
     */
    void sigReadEnd(int index);
    ///
    /// \brief 朗读回复
    /// \param message
    /// \param is_end
    /// \param index
    /// \param errorCode
    /// \param model
    ///
    void reply(const QString &message, bool is_end, int index, int errorCode, QString model = "");
    ///
    /// \brief 重置索引信号，点击朗读失败时触发。
    ///
    void sigResetPreIndex();

public:
    // 文本合成语音朗读部分
    /**
     * @brief 初始化朗读会话
     * @return 朗读会话是否创建成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool initReaderSession();
    /**
     * @brief 关闭朗读会话
     * @return 朗读会话是否关闭成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool closeReaderSession();
    /**
     * @brief 启动朗读
     */
    Q_INVOKABLE void startReading();
    /**
     * @brief 暂停朗读
     */
    Q_INVOKABLE void pauseReading();
    /**
     * @brief 停止朗读
     */
    Q_INVOKABLE void stopReading();

    /**
     * @brief 继续朗读
     */
    Q_INVOKABLE void continueReading();

    /**
     * @brief 打断朗读
     */
    Q_INVOKABLE void interruptReading();

    /**
     * @brief 获取当前阅读状态
     * @return 返回当前是否正在朗读（注意：这是同步调用，会阻塞）
     */
    Q_INVOKABLE bool getCurrentReaderStatus();
    /**
     * @brief 发送文本到合成器进行合成，合成完毕后自动触发朗读
     * @param 待合成文案
     * @param 当前索引
     * @return 是否合成成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool sendReadingText(const QString &msg, int index);

    /**
     * @brief 继续追加待朗读文本
     * @param 文本
     * @param 索引
     * @return 是否追加成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool continueAddReadingText(const QString &msg, int index);

    /**
     * @brief 获取当前朗读索引
     * @return 索引值（注意：这是同步调用，会阻塞）
     */
    Q_INVOKABLE int getCurrentReadIndex();

    /**
     * @brief 插入待朗读文本
     * @param 文本信息
     * @return 是否插入成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool insertWaitingReadingListItem(const QString &msg);

    /**
     * @brief 清理待朗读列表
     */
    Q_INVOKABLE void clearWaitingReadingList();

    /**
     * @brief 清理阅读播放器内容
     */
    Q_INVOKABLE void clearReadingPlayerData();

    /**
     * @brief 打断所有音频占用
     */
    Q_INVOKABLE void interruptAllVoiceUsing();

    //朗读控制器
    /**
     * @brief 开始阅读当前文本
     * @param 文本段
     * @return 是否成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool startReadingCurrentText(const QString &msg);

private:
    ReaderControl *m_readerControl;  // 实际的工作线程中的 ReaderControl 对象

    /**
     * @brief 初始化信号转发
     */
    void initSignalForwarding();
};

#endif // READERCONTROLPROXY_H
