#ifndef RECORDERCONTROL_H
#define RECORDERCONTROL_H

#include <QObject>
#include <QAudioBuffer> //音频缓存
#include <QAudioInput>
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <ukui-log4qt.h>

#include <kylin-ai/coreai/speech/recognizer.h>
#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/error.h>
#include <kylin-ai/coreai/speech/result.h>
#include <kylin-ai/coreai/speech/synthesizer.h>
#include <aiconfig/modelconfig.h>
#include <kylin-ai/common/macros.h>
#include <kylin-ai/private/osassistant/osassistant.h>
#include <common/error.h>

#include "global/kaglobal.h"
class RecorderControl : public QObject
{
    Q_OBJECT
public:
    explicit RecorderControl(QObject *parent = nullptr);
    ~RecorderControl();

    /**
     * @brief 将对象移动到工作线程
     * @param thread 目标线程
     */
    void moveToWorkerThread(QThread *thread);

    /**
     * @brief 初始化信号槽
     */
    void initConnects();

    /**
     * @brief 处理输入设备切换
     */
    void handleInputDeviceChanged();
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
    void reply(const QString &message,bool is_end,int index, int errorCode, QString model = "");
    /**
     * @brief 取消语音输入
     */
    void sigCancelSendVoice();
public:
    /**
     * @brief 初始化语音识别会话
     * @return 语音识别会话是否创建成功
     */
    Q_INVOKABLE bool initSpeechSession();
    /**
     * @brief 关闭语音识别会话
     * @return 无返回值
     */
    Q_INVOKABLE void closeSpeechSession();
    /**
     * @brief 启动语音识别
     */
    Q_INVOKABLE bool startSpeechRecognition();
    /**
     * @brief 停止语音识别
     */
    Q_INVOKABLE void stopSpeechRecognition();
    /**
     * @brief 解析和发送错误信息
     * @param 错误码
     */
    void parseAndSendErrorMsg(const int errorCode);
    /**
     * @brief convertMarkDownToHtml
     * @param markDown
     * @return
     */
    QString convertMarkDownToHtml(const QString &markDown);
    /**
     * @brief markdownToHtml
     * @param markdown
     * @return
     */
    QString markdownToHtml(const std::string& markdown);

    /**
     * @brief 打断所有音频占用
     */
    Q_INVOKABLE void interruptAllVoiceUsing();

    /**
     * @brief 识别停止收尾：销毁 session 并通知 QML 复位 UI
     */
    void finishRecognitionStop();

    /**
     * @brief 用户主动停止后收到最终识别结果时收尾
     */
    void finishRecognitionStopIfRequested(SpeechResultReason reason);

private:
    SpeechRecognitionSession *m_speechSession = nullptr;
    AudioConfig *m_speechconfig = nullptr;         //语音配置
    QThread *m_workerThread = nullptr;  // 工作线程指针
    bool m_stopping = false;
public:
    bool isRecording = false;
};

#endif // RECORDERCONTROL_H
