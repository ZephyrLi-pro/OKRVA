#ifndef VOICEUSINGMANAGER_H
#define VOICEUSINGMANAGER_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QString>
//管理音频输入和输出，避免助手内音频输入和输出同步进行显示异常
//在音频输出时避免音频输入，在音频输入时避免音频输出
//用户执行任何输入输出操作时，第一时间会打断原有音频操作，执行当前音频操作，用户响应优先原则
class VoiceUsingManager : public QObject
{
    Q_OBJECT
private:
    explicit VoiceUsingManager(QObject *parent = nullptr);
    static VoiceUsingManager* m_instance;        // 单例实例

public:
    static VoiceUsingManager* getInstance(QObject *parent = nullptr);

    ~VoiceUsingManager();

    //音频使用模块
    enum VoiceUsingModule {
        IdleModule = 0,             //空闲状态
        ChatVoiceInput,             //文本对话音频输入
        ChatVoiceOutput,            //文本对话音频输出
        WriteVoiceInput,            //写作对话音频输入
        WriteVoiceOutput,           //写作对话音频输出
        WriteSelectVoiceOutput,     //写作划词音频输出
        VoiceChatInput,             //语音通话音频输入
        VoiceChatOutPut,            //语音通话音频输出
        SelectOutput,               //全局划词音频输出
    };
    Q_ENUM(VoiceUsingModule)

    //音频状态
    enum VoiceUsingState {
        VoiceUsing = 0, //使用
        VoiceIdle,      //空闲
        VoiceError,     //错误
        VoiceInterrupt, //打断
    };
    Q_ENUM(VoiceUsingState)

signals:
    /**
     * @brief 当前待关闭音频模块
     * @param 模块模型
     */
    void sigCloseCurModuleVoice(const VoiceUsingModule curModule);

    /**
     * @brief 打断占用信号，所有音频占用模块都需要响应此信号
     * @param 当前打断发送人是否是语音通话模块
     */
    void sigInterruptUsing(bool isSendByVoiceChat);
public:
    /**
     * @brief 初始化信号槽链接
     */
    void initConnect();
public:
    /**
     * @brief 打断当前音频占用
     */
    void startInterruptVoiceUsing();

    /**
     * @brief 获取当前正在使用音频的模块
     * @return 当前在使用模块
     */
    VoiceUsingModule getCurVoiceUsingModule();

    /**
     * @brief 设置当前使用音频的模块
     * @param 当前待设置音频模块项
     */
    void setCurVoiceUsingMoudule(VoiceUsingManager::VoiceUsingModule curModule);

    /**
     * @brief  获取当前音频的使用状态
     * @return 当前的状态
     */
    VoiceUsingState getCurVoiceUsingState();

    /**
     * @brief setCurVoiceUsingState
     * @param curState
     */
    void setCurVoiceUsingState(const VoiceUsingState &curState);


    /**
     * @brief  获取等待使用音频的模块
     * @return 当前的状态
     */
    VoiceUsingModule getCurVoiceWaitingUsingModule();

    /**
     * @brief 设置当前等待使用的模块
     * @param curState
     */
    void setCurVoiceWaitingUsingMoudule(const VoiceUsingModule &curState);


    /**
     * @brief  获取上次使用音频的模块
     * @return 当前的状态
     */
    VoiceUsingModule getCurVoiceLastUsingModule();

    /**
     * @brief 设置上次使用的模块
     * @param curState
     */
    void setCurVoiceLastUsingMoudule(const VoiceUsingModule &curState);

private:
    VoiceUsingModule m_currentUsingMoudule = IdleModule; //当前使用模块
    VoiceUsingModule m_waitingUsingMoudule = VoiceChatInput; //等待使用模块
    VoiceUsingModule m_lastUsingMoudule = IdleModule; //上一次使用模块
    VoiceUsingState m_currentUsingState = VoiceIdle; // 当前音频使用状态
 };

#endif // VOICEUSINGMANAGER_H
