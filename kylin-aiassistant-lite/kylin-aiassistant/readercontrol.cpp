#include "readercontrol.h"
#include <QMetaObject>
#include <QDebug>

/**
 * @brief 在 minLen 附近查找合适的切分位置，优先按标点或空白分段
 * @param text 待查找文本
 * @param minLen 最少字符数，在此之后优先向前查找分割点
 * @param maxExtend 最多向前延伸查找的字符数
 * @param maxBacktrack 向前未找到时，允许向后回退查找的字符数
 * @return 分割位置。若命中标点/空白，返回其后一位；否则返回 minLen
 */
static int findSplitPositionAfterLength(const QString &text, int minLen, int maxExtend = 20, int maxBacktrack = 10)
{
    if (text.length() <= minLen) {
        return text.length();
    }

    // 常见中英文标点，用于在合适位置分段。
    const QString punctMarks = QStringLiteral("。？！，、；：.?!,;:()[]{}<>\"'“”‘’…—\n");
    int searchEnd = qMin(minLen + maxExtend, text.length());

    for (int i = minLen; i < searchEnd; ++i) {
        if (punctMarks.contains(text[i]) || text[i].isSpace()) {
            return i + 1;
        }
    }

    int searchStart = qMax(0, minLen - maxBacktrack);
    for (int i = minLen - 1; i >= searchStart; --i) {
        if (punctMarks.contains(text[i]) || text[i].isSpace()) {
            return i + 1;
        }
    }

    return minLen;
}

/**
 * @brief 文本合成语音回调函数
 * 采用按段缓冲策略：SYNTHESIZING 时只追加到缓冲，COMPLETED 时一次性入队播放，
 * 解决 SDK 回调顺序乱序导致的朗读内容颠倒和漏读问题。
 */
static void synthesisResultCallback(SpeechSynthesisResult *result, void *user_data)
{
    ReaderControl *chatReader = static_cast<ReaderControl*>(user_data);
    if (chatReader == nullptr) {
        KyInfo() << "Invalid callback user data pointer.";
        return;
    }
    if (chatReader->m_soundFlag) {
        return;
    }

    SpeechResultReason reason = speech_synthesis_result_get_reason(result);
    int errorCode = speech_synthesis_result_get_error_code(result);

    if (errorCode != 0 && errorCode != -1) {
        KyInfo() << "当前错误码为" << errorCode;
        // 错误处理：断网等情况下需要立刻停止播放并复位前端按钮状态。
        // 注意：回调线程不一定是对象所属线程，使用异步投递避免线程问题。
        const QString errorStr = Globals::getErrorMsg(static_cast<SpeechErrorCode>(errorCode));
        QMetaObject::invokeMethod(chatReader, [chatReader, errorStr, errorCode]() {
            // 先停止实际播放（清队列 + stop）再通知前端结束
            if (chatReader->m_sdl2Player) {
                chatReader->m_sdl2Player->clearQueue();
                chatReader->m_sdl2Player->stop();
            }
            chatReader->m_waitingReadingList.clear();
            chatReader->m_isReaderSpeechSynthesizing = false;
            chatReader->isNeedRead = false;

            Q_EMIT chatReader->reply(errorStr, true, 0, errorCode);
            Q_EMIT chatReader->sigReadEnd(0);
            Q_EMIT chatReader->sigResetPreIndex();
        }, Qt::QueuedConnection);
        return;
    }

    // 在能获取有效参数的阶段（SYNTHESIZING/STARTED/COMPLETE）缓存音频参数
    static int cachedSampleRate = 0;   // 缓存采样率
    static int cachedChannels = 0;     // 缓存声道数
    if ((reason == SPEECH_SYNTHESIZING || reason == SPEECH_SYNTHESIS_STARTED ||reason == SPEECH_SYNTHESIS_COMPLETED) &&
        (cachedSampleRate == 0 || cachedChannels == 0)) {
        cachedSampleRate = speech_synthesis_result_get_audio_rate(result);
        cachedChannels = speech_synthesis_result_get_audio_channel(result);
        KyInfo() << "缓存音频参数 - 采样率:" << cachedSampleRate << "声道数:" << cachedChannels;
    }

    uint32_t dataLength = 0;
    const uint8_t *pcmData = speech_synthesis_result_get_data(result, &dataLength);

    // SYNTHESIZING：追加到当前段缓冲，不立即入队（避免回调乱序导致播放错乱）
    if (pcmData && dataLength > 0) {
        QMutexLocker locker(&chatReader->m_callbackMutex);
        chatReader->m_currentSegmentBuffer.append(reinterpret_cast<const char*>(pcmData), dataLength);
    }

    // COMPLETED：将整段缓冲一次性入队，保证顺序正确
    if (reason == SPEECH_SYNTHESIS_COMPLETED) {
        KyInfo() << "本次分段语音合成完成，将缓冲数据入队播放。";
        QByteArray segmentData;
        {
            QMutexLocker locker(&chatReader->m_callbackMutex);
            segmentData = chatReader->m_currentSegmentBuffer;
            chatReader->m_currentSegmentBuffer.clear();
        }

        if (!segmentData.isEmpty()) {
            if (!chatReader->m_sdl2Player->isInitialized()) {
                //使用缓存的音频参数初始化，而非在COMPLETED阶段直接获取
                bool initOk = chatReader->m_sdl2Player->init(cachedSampleRate, cachedChannels, AUDIO_S16LSB);
                if (!initOk) {
                    QThread::msleep(300);
                    initOk = chatReader->m_sdl2Player->init(cachedSampleRate, cachedChannels, AUDIO_S16LSB);
                }
                if (!initOk) {
                    KyInfo() << "SDL 音频播放器初始化失败！缓存参数：采样率=" << cachedSampleRate << "声道数=" << cachedChannels;
                    chatReader->m_soundFlag = true;
                    // 重置缓存，避免下次使用错误参数
                    cachedSampleRate = 0;
                    cachedChannels = 0;
                    return;
                }
                KyInfo() << "SDL 音频播放器初始化成功。采样率:" << cachedSampleRate << "声道数:" << cachedChannels;
            }
            chatReader->m_sdl2Player->enqueueData(segmentData);
            if (!chatReader->m_sdl2Player->isPlaying() && chatReader->isNeedRead) {
                chatReader->startReading();
                KyInfo() << "SDL 开始播放音频。";
            }
        }

        // 检查是否还有待合成段落
        if (!chatReader->m_waitingReadingList.isEmpty()) {
            bool addFlag = chatReader->continueAddReadingText(
                chatReader->m_waitingReadingList.first(),
                chatReader->getCurrentReadIndex());
            if (addFlag) {
                chatReader->m_waitingReadingList.removeFirst();
                KyInfo() << "本次朗读结束后追加内容成功，当前待朗读内容:" << chatReader->m_waitingReadingList;
            } else {
                chatReader->m_isReaderSpeechSynthesizing = false;
                // 重置缓存，准备下次合成
                cachedSampleRate = 0;
                cachedChannels = 0;
            }
        } else {
            chatReader->m_isReaderSpeechSynthesizing = false;
            // 重置缓存，准备下次合成
            cachedSampleRate = 0;
            cachedChannels = 0;
        }
    } else if (reason == SPEECH_SYNTHESIS_STARTED) {
        KyInfo() << "语音合成任务已启动。";
    }

    return;
}
ReaderControl::ReaderControl(QObject *parent)
    : QObject{parent}
    ,m_isReaderSpeechSynthesizing(false)
    ,m_workerThread(nullptr)
{
    m_sdl2Player = new SDL2AudioPlayerThread(this); // 初始化SDL播放器

    initConnect();
}

ReaderControl::~ReaderControl()
{
    closeReaderSession();

    m_sdl2Player->clearQueue();
    m_sdl2Player->deleteLater();
    m_sdl2Player = nullptr;

    // 清理工作线程
    // 注意：如果当前对象在工作线程中，析构函数会在工作线程中执行
    // 此时不能调用 wait()，否则会导致死锁
    if (m_workerThread) {
        if (QThread::currentThread() == m_workerThread) {
            // 如果当前在工作线程中，只退出线程，不等待
            m_workerThread->quit();
            m_workerThread = nullptr; // 线程会在主线程中清理
        } else {
            // 如果在主线程中，可以安全地等待线程退出
            qWarning() << "[SHUTDOWN] ReaderControl::~ReaderControl wait workerThread begin";
            m_workerThread->quit();
            m_workerThread->wait();
            qWarning() << "[SHUTDOWN] ReaderControl::~ReaderControl wait workerThread end";
            m_workerThread->deleteLater();
            m_workerThread = nullptr;
        }
    }
}

void ReaderControl::initConnect()
{
    // 连接SDL播放器的信号
    connect(m_sdl2Player, &SDL2AudioPlayerThread::playbackFinished, this, [=]() {
        KyInfo() << "SDL playback finished signal received.";
        // 在这里处理播放结束后的逻辑
        isNeedRead = false;
        emit sigReadEnd(0);
        m_sdl2Player->stop();
    });

    connect(m_sdl2Player, &SDL2AudioPlayerThread::underflow, this, []() {
        KyInfo() << "Audio underflow detected. Consider enqueuing more data.";
    });

    connect(VoiceUsingManager::getInstance(), &VoiceUsingManager::sigInterruptUsing, this,[&](){
        KyInfo() << "当前正在执行打断原有朗读的流程" << this;
        closeReaderSession();  // 合并 interruptReading 逻辑，避免重复 stop
    });

    return;
}

void ReaderControl::moveToWorkerThread(QThread *thread)
{
    if (thread == nullptr) {
        KyInfo() << "Invalid thread pointer, cannot move ReaderControl to thread.";
        return;
    }

    m_workerThread = thread;
    
    // 启动线程
    if (!m_workerThread->isRunning()) {
        m_workerThread->start();
    }

    // 将对象移动到工作线程
    this->moveToThread(m_workerThread);
    
    // 注意：SDL2AudioPlayerThread 本身就是一个线程，不需要移动到其他线程

    KyInfo() << "ReaderControl moved to worker thread:" << m_workerThread;
}

bool ReaderControl::initReaderSession()
{
    if(this->m_readerSession != nullptr)
        return true;
    QString error;

    KyInfo() << "*** start initSession speech_synthesizer_create_session()";

    SpeechSynthesizerSession *m_session = speech_synthesizer_create_session();
    if(m_session == nullptr) {
        KyInfo() << "error : speech_synthesizer_create_session failed!";
        //TODO： 错误处理
        return false;
    }
    KyInfo() << "*** end initSession speech_synthesizer_create_session()";

    KyInfo() << "*** start initSession speech_synthesizer_init_session()";

    int ret =  speech_synthesizer_init_session(m_session);
    if(ret){
        KyInfo() << "reader init session error:" << ret;
        error = Globals::getErrorMsg(static_cast<SpeechErrorCode>(ret));
        Q_EMIT reply(error, true, 0, ret, "");
        return false;
    }
    KyInfo() << "*** end initSession speech_synthesizer_init_session()";

    if(m_session) {
        this->m_readerSession = m_session;
    }else {
         return false;
    }

    speech_synthesizer_result_set_callback(m_readerSession, synthesisResultCallback, this);

    return true;
}

bool ReaderControl::closeReaderSession()
{
    bool hasActiveReading = m_sdl2Player->isPlaying() ||
                           m_isReaderSpeechSynthesizing ||
                           isNeedRead ||
                           !m_waitingReadingList.isEmpty();

    m_soundFlag = true;
    // 优先停止播放，保证响应及时
    m_sdl2Player->clearQueue();
    m_sdl2Player->stop();
    m_waitingReadingList.clear();
    isNeedRead = false;

    {
        QMutexLocker locker(&m_callbackMutex);
        m_currentSegmentBuffer.clear();
    }
    if(this->m_readerSession != nullptr){
        SpeechSynthesizerSession ** sSession = &m_readerSession;
        speech_synthesizer_destroy_session(sSession);
    }
    this->m_readerSession = nullptr;
    m_isReaderSpeechSynthesizing = false;

    if (hasActiveReading) {
        qDebug() << "只有在真的有朗读活动被打断时，才发送 sigReadEnd 信号";
        Q_EMIT sigReadEnd(0);
    }

    return true;
}

void ReaderControl::startReading()
{
    KyInfo() << "reader startReading";

    if(m_sdl2Player) {
        m_sdl2Player->play();
    }
    // 发送信号通知前端更新进度
    emit sigPlayingStart();

    isNeedRead = true;

    return;
}

void ReaderControl::pauseReading()
{
    if(isNeedRead) {
        isNeedRead = false;
    }

    if(m_sdl2Player) {
        m_sdl2Player->pause();
    }

    return;
}

void ReaderControl::stopReading()
{
    if(m_sdl2Player) {
        m_sdl2Player->stop();
    }
    // 清空待朗读队列
    m_waitingReadingList.clear();
    // 设置需要朗读为false
    isNeedRead = false;
    KyInfo() << "结束当前朗读stopReading()";

    emit sigReadEnd(0);

    return;
}

void ReaderControl::stopReading(bool emitEndSignal)
{
    if(m_sdl2Player) {
        m_sdl2Player->stop();
    }
    // 清空待朗读队列
    m_waitingReadingList.clear();
    // 设置需要朗读为false
    isNeedRead = false;
    KyInfo() << "结束当前朗读stopReading(bool emitEndSignal)";

    if(emitEndSignal == true) {
        emit sigReadEnd(0);
    }
    return;
}

void ReaderControl::continueReading()
{
    isNeedRead = true;

    //检查播放器状态，如果播放器队列为空则返回，否则则进行播放
    if(m_sdl2Player->isQueueEmpty()) {

        return;
    }else {
        m_sdl2Player->play();
        emit sigPlayingStart();
    }

    return;
}

void ReaderControl::interruptReading()
{
    {
        QMutexLocker locker(&m_callbackMutex);
        m_currentSegmentBuffer.clear();
    }
    // 检查是否真的有朗读活动需要打断
    bool hasActiveReading = m_sdl2Player->isPlaying() ||
                            m_isReaderSpeechSynthesizing ||
                            isNeedRead ||
                            !m_waitingReadingList.isEmpty();
    // 打断播放
    if(m_sdl2Player->isPlaying()) {
        m_sdl2Player->clearQueue(); //清空队列
        m_sdl2Player->stop();//停止播放
    }
    // 清空待朗读队列
    m_waitingReadingList.clear();
    // 设置需要朗读为false
    isNeedRead = false;

    // 只有在真的有朗读活动被打断时，才发送 sigReadEnd 信号
    if (hasActiveReading) {
        qDebug() << "只有在真的有朗读活动被打断时，才发送 sigReadEnd 信号";

        Q_EMIT sigReadEnd(0);
    }

    return;
}

bool ReaderControl::getCurrentReaderStatus()
{
    bool readerStatus = m_sdl2Player->isPlaying() || m_isReaderSpeechSynthesizing;
    KyInfo() << "当前朗读模块状态为" << readerStatus << "播放器是否正在播放：" <<m_sdl2Player->isPlaying()
             << "当前是否正在进行语音合成" << m_isReaderSpeechSynthesizing;
    return readerStatus;
}

bool ReaderControl::sendReadingText(const QString &msg, int index)
{
    KyInfo() << "发送待朗读文本" << msg;
    {
        QMutexLocker locker(&m_callbackMutex);
        m_currentSegmentBuffer.clear();
    }
    if (msg.length() == 0) {
        KyInfo() << "文本为空";
        Q_EMIT sigReadEnd(index);
        Q_EMIT sigResetPreIndex();

        return false;
    }

    if(!initReaderSession()) {
        KyInfo() << "初始化session失败，文字发送失败，已经返回";
        Q_EMIT sigReadEnd(index);
        Q_EMIT sigResetPreIndex();

        return false;
    }
    m_soundFlag = false;

    //记录播放语音的index值，音频播放结束时使用
    m_index = index;

    // 暂停原有播放并清除原有播放队列内容
//    if(m_sdl2Player) {
//        m_sdl2Player->stop();
//    }

    // 使用pcm原始音频文件作为输出源
    auto *synthesizerConfig = audio_config_create_audio_output_from_pcm_data();
    audio_config_set_input_audio_rate(synthesizerConfig, 16000);
    speech_synthesizer_set_audio_config(m_readerSession, synthesizerConfig);

    QByteArray array = msg.toUtf8();
    const char* text = array.data();

    speech_synthesizer_synthesize_text_async(m_readerSession, text, (uint32_t)(array.length()));

    isNeedRead = true;
    m_isReaderSpeechSynthesizing = true;

    return true;
}

bool ReaderControl::continueAddReadingText(const QString &msg, int index)
{
    KyInfo() << "发送待追加朗读文本" << msg;
    {
        QMutexLocker locker(&m_callbackMutex);
        m_currentSegmentBuffer.clear();
    }
    if (msg.length() == 0) {
        KyInfo() << "文本为空";
        Q_EMIT sigReadEnd(index);
        Q_EMIT sigResetPreIndex();

        return false;
    }
    if(!initReaderSession()) {
        KyInfo() << "初始化session失败，文字发送失败，已经返回";
        Q_EMIT sigReadEnd(index);
        Q_EMIT sigResetPreIndex();

        return false;
    }

    // 使用pcm原始音频文件作为输出源
    auto *synthesizerConfig = audio_config_create_audio_output_from_pcm_data();
    audio_config_set_input_audio_rate(synthesizerConfig, 16000);
    speech_synthesizer_set_audio_config(m_readerSession, synthesizerConfig);

    QByteArray array = msg.toUtf8();
    const char* text = array.data();

    speech_synthesizer_synthesize_text_async(m_readerSession, text, (uint32_t)(array.length()));

//    isNeedRead = true; //追加合成时不需要主动设置需要朗读，否则会自动启动朗读
    m_isReaderSpeechSynthesizing = true;

    return true;
}

int ReaderControl::getCurrentReadIndex()
{
    return m_index;
}

bool ReaderControl::insertWaitingReadingListItem(const QString &msg)
{
    qDebug() << "当前待插入朗读文本" << msg;
    if(msg.isEmpty()) {

        return true;
    }

    m_waitingReadingList.append(msg);
    if(!m_waitingReadingList.isEmpty())   {

        // 如果正在合成，则追加到列表尾部就返回
        if(m_isReaderSpeechSynthesizing) {
            qDebug() << "当前正在合成，则等待合成完成后再执行当前内容";

            return true;
        } else {
            // 如果不是正在合成，则追加到列表并发送文本
            qDebug() << "当前不是正在合成，开始发送待朗读文本，发送完成后并删除待朗读此内容";

            // 发送本次朗读文本
            sendReadingText(msg,m_index);
            m_waitingReadingList.removeFirst();

            return true;
        }

    }

    return true;
}

void ReaderControl::clearWaitingReadingList()
{
    m_waitingReadingList.clear();

    return;
}

void ReaderControl::clearReadingPlayerData()
{
    m_sdl2Player->clearQueue();

    return;
}

void ReaderControl::updateCurReadIndex(const int &index)
{
    qDebug() << "当前待更新朗读索引" << index;

    m_index = index;

    return;
}

void ReaderControl::interruptAllVoiceUsing()
{
    emit  VoiceUsingManager::getInstance()->sigInterruptUsing(false);// 打断所有的音频占用
}
bool ReaderControl::startReadingCurrentText(const QString &msg)
{
    // 判空
    if (msg.isEmpty()) {
        qDebug() << "待朗读文本为空，无需处理";
        return false;
    }

    int textLength = msg.count();

    if (textLength <= 30) {
        bool sendResult = sendReadingText(msg, 0);
        return sendResult;
    }
    else {
        // 先清空原有待朗读列表（避免旧数据干扰）
        clearWaitingReadingList();

        const int kMinSegmentLen = 30;
        int startIndex = 0; // 分段起始索引
        while (startIndex < textLength) {
            while (startIndex < textLength && msg[startIndex].isSpace()) {
                ++startIndex;
            }
            if (startIndex >= textLength) {
                break;
            }

            int remainingLen = textLength - startIndex;
            int segLen;
            if (remainingLen <= kMinSegmentLen) {
                segLen = remainingLen;
            } else {
                // 超过 30 字时，寻找 30 字后最近的标点进行分段，使朗读更自然
                segLen = findSplitPositionAfterLength(msg.mid(startIndex), kMinSegmentLen);
            }
            QString subText = msg.mid(startIndex, segLen);
            if (!subText.trimmed().isEmpty()) {
                m_waitingReadingList.append(subText);
            }

            startIndex += segLen;
        }
        // 分段完成后，触发第一个分段的朗读
        if (!m_waitingReadingList.isEmpty()) {
            // 直接取出第一个分段发送朗读，避免重复插入
            QString firstText = m_waitingReadingList.takeFirst(); // 取出并删除第一个元素（安全，已判空）
            bool sendResult = sendReadingText(firstText, 0);
            if (!sendResult) {
                // 发送失败时清空列表，避免残留
                clearWaitingReadingList();
                KyInfo() << "发送朗读问题失败:"<< firstText;
                return false;
            }
            KyInfo() << "发送第一个分段文本：" << firstText << "，结果：" << sendResult;
        }

        return true;
    }
}
