#ifndef READERCONTROL_H
#define READERCONTROL_H

#include <QObject>
#include <QObject>
#include <QFile>
#include <QMutex>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QAudioDecoder>
#include <QIODevice>
#include <QThread>
#include <QTemporaryFile>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QMediaPlayer>
#include <QQueue>
#include <QByteArray>
#include <ukui-log4qt.h>

#include <kylin-ai/coreai/speech/synthesizer.h>
#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/result.h>

#include "global/kaglobal.h"
#include "global/voiceusingmanager.h"
#include "voicechat/sdl2audioplayerthread.h"

#define READ_FILE_PATH "/.read"


class ReaderControl : public QObject
{
    Q_OBJECT
public:
    explicit ReaderControl(QObject *parent = nullptr);
    ~ReaderControl();

    enum ReaderState {
        Idle, //空闲状态
        Recognizing, //合成状态
        Reading,//合成状态
        Pause//暂停状态
    };
    Q_ENUM(ReaderState)


    /**
     * @brief 初始化信号槽连接
     */
    void initConnect();

    /**
     * @brief 将对象移动到工作线程
     * @param thread 目标线程
     */
    void moveToWorkerThread(QThread *thread);

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
    void reply(const QString &message,bool is_end,int index, int errorCode, QString model = "");
    ///
    /// \brief 重置索引信号，点击朗读失败时触发。
    ///
    void sigResetPreIndex();    //发送信号，重置前端preIndex值。  点击朗读失败时触发。
public:
    // 文本合成语音朗读部分
    /**
     * @brief 初始化朗读会话
     * @return 朗读会话是否创建成功
     */
    Q_INVOKABLE bool initReaderSession();
    /**
     * @brief 关闭朗读会话
     * @return 朗读会话是否关闭成功
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
     * @brief 停止朗读
     */
    Q_INVOKABLE void stopReading(bool emitEndSignal);



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
     * @return 返回当前是否正在朗读
     */
    Q_INVOKABLE bool getCurrentReaderStatus();
    /**
     * @brief 发送文本到合成器进行合成，合成完毕后自动触发朗读
     * @param 待合成文案
     * @param 当前索引
     * @return 是否合成成功
     */
    Q_INVOKABLE bool sendReadingText(const QString &msg, int index);

    /**
     * @brief 继续追加待朗读文本
     * @param 文本
     * @param 索引
     * @return 是否追加成功
     */
    Q_INVOKABLE bool continueAddReadingText(const QString &msg, int index);

    /**
     * @brief 获取当前朗读索引
     * @return 索引值
     */
    Q_INVOKABLE int getCurrentReadIndex();

    /**
     * @brief 插入待朗读文本
     * @param 文本信息
     * @return 是否插入成功
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
     * @brief 更新当前待朗读文本
     */
    Q_INVOKABLE void updateCurReadIndex(const int &index);

    /**
     * @brief 打断所有音频占用
     */
    Q_INVOKABLE void interruptAllVoiceUsing();

    //朗读控制器
    /**
     * @brief 开始阅读当前文本
     * @param 文本段
     * @return
     */
    Q_INVOKABLE bool startReadingCurrentText(const QString &msg);

private:
    SpeechSynthesizerSession *m_readerSession = nullptr;
    QThread *m_workerThread = nullptr;  // 工作线程指针
public:
    SDL2AudioPlayerThread *m_sdl2Player = nullptr;
    bool m_soundFlag = false;           //标志位，用于区分实时朗读和普通朗读
    bool isNeedRead;                      //合成音频过程中，记录用户暂停操作，如果用户暂停，合成结束后不朗读。
    int m_index = -1;
    bool m_isReaderSpeechSynthesizing;   //标志位，是否正在合成语音
    QStringList m_waitingReadingList; // 待朗读列表

    // 按段缓冲：解决 SDK 回调顺序乱序导致的朗读内容颠倒/漏读
    QByteArray m_currentSegmentBuffer;   // 当前段 PCM 缓冲，COMPLETED 时一次性入队
    QMutex m_callbackMutex;              // 回调与主逻辑的线程安全
};

#endif // READERCONTROL_H
