#ifndef RECORDERCONTROLPROXY_H
#define RECORDERCONTROLPROXY_H

#include <QObject>
#include "recordercontrol.h"

/**
 * @brief RecorderControl 的代理类，用于在主线程中提供异步调用接口
 * 
 * 该类在主线程中创建，所有方法调用都通过 QMetaObject::invokeMethod 
 * 异步转发到工作线程中的 RecorderControl 对象，确保 QML 调用不会阻塞。
 */
class RecorderControlProxy : public QObject
{
    Q_OBJECT
public:
    explicit RecorderControlProxy(RecorderControl *recorderControl, QObject *parent = nullptr);
    ~RecorderControlProxy();

    /**
     * @brief 设置实际的 RecorderControl 对象
     */
    void setRecorderControl(RecorderControl *recorderControl);

signals:
    /**
     * @brief 语音识别信息信号，发送到控制模块并转发到前端生成气泡
     * @param 合成结果内容
     */
    void sigReceiveSpeechRecognitionMessage(const QString message);
    ///
    /// \brief 录音回复
    /// \param message
    /// \param is_end
    /// \param index
    /// \param errorCode
    /// \param model
    ///
    void reply(const QString &message, bool is_end, int index, int errorCode, QString model = "");
    /**
     * @brief 取消语音输入
     */
    void sigCancelSendVoice();

public:
    /**
     * @brief 初始化语音识别会话
     * @return 语音识别会话是否创建成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool initSpeechSession();
    /**
     * @brief 关闭语音识别会话
     * @return 无返回值（异步调用）
     */
    Q_INVOKABLE void closeSpeechSession();
    /**
     * @brief 启动语音识别
     * @return 是否启动成功（异步调用，返回值可能不准确）
     */
    Q_INVOKABLE bool startSpeechRecognition();
    /**
     * @brief 停止语音识别
     */
    Q_INVOKABLE void stopSpeechRecognition();
    /**
     * @brief 打断所有音频占用
     */
    Q_INVOKABLE void interruptAllVoiceUsing();

private:
    RecorderControl *m_recorderControl;  // 实际的工作线程中的 RecorderControl 对象

    /**
     * @brief 初始化信号转发
     */
    void initSignalForwarding();
};

#endif // RECORDERCONTROLPROXY_H

