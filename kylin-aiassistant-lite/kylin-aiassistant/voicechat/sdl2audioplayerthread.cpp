#include "sdl2audioplayerthread.h"
#include <ukui-log4qt.h>
#include <QQueue>
#include <QEventLoop>
#include <QTimer>

SDL2AudioPlayerThread::SDL2AudioPlayerThread(QObject *parent)
    : QThread(parent),
      m_isInitialized(false),
      m_isPlaying(false),
      m_shouldQuit(false),
      m_eventLoop(nullptr)
{
}

SDL2AudioPlayerThread::~SDL2AudioPlayerThread()
{
    // 停止播放
    if (isRunning()) {
        // 先停止播放（通过信号槽在线程中执行）
        requestStop();

        // 退出事件循环
        if (m_eventLoop) {
            QMetaObject::invokeMethod(m_eventLoop, "quit", Qt::QueuedConnection);
        }
    }

    // 等待线程结束
    if (isRunning()) {
        wait(3000); // 最多等待3秒
        if (isRunning()) {
            terminate(); // 如果还在运行，强制终止
            wait();
        }
    }
}

void SDL2AudioPlayerThread::run()
{
    // 禁止 SDL 接管 SIGTERM/SIGINT，否则 killall 等终端命令无法正常终止进程
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    // 在这个线程中初始化 SDL
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        KyInfo() << "SDL_Init failed: " << SDL_GetError();
        emit initialized(false);
        return;
    }

    KyInfo() << "SDL initialized in thread:" << QThread::currentThread();

    // 创建事件循环，以便处理信号槽
    QEventLoop eventLoop;
    m_eventLoop = &eventLoop;

    // 连接信号槽，使用 QueuedConnection 以便从主线程调用时能在线程中执行
    connect(this, &SDL2AudioPlayerThread::requestInit, this, &SDL2AudioPlayerThread::handleInit, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestEnqueueData, this, &SDL2AudioPlayerThread::handleEnqueueData, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestClearQueue, this, &SDL2AudioPlayerThread::handleClearQueue, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestPlay, this, &SDL2AudioPlayerThread::handlePlay, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestPause, this, &SDL2AudioPlayerThread::handlePause, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestStop, this, &SDL2AudioPlayerThread::handleStop, Qt::QueuedConnection);

    // 通知调用方：SDL_Init 已完成且事件循环已就绪，可安全发送 requestInit
    emit sdlThreadReady();

    // 运行事件循环
    eventLoop.exec();
    m_eventLoop = nullptr;

    // 清理
    if (m_isInitialized) {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        m_isInitialized = false;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    KyInfo() << "SDL thread exited";
}

bool SDL2AudioPlayerThread::init(int sampleRate, int channels, SDL_AudioFormat format)
{
    if (m_isInitialized) {
        stop();
    }

    // 确保线程已启动且事件循环就绪
    if (!isRunning()) {
        start();
        // 等待线程启动
        int waitCount = 0;
        const int maxWait = 500; // 最多等5秒（500*10ms）
        while (!isRunning() && waitCount < maxWait) {
            QThread::msleep(10);
            waitCount++;
        }
        if (!isRunning()) {
            qCritical() << "SDL2AudioPlayerThread: 线程启动超时";
            return false;
        }
        // 等待 sdlThreadReady：确保 SDL_Init 完成且事件循环已进入 exec()，再发送 requestInit
        QEventLoop readyLoop;
        auto conn = connect(this, &SDL2AudioPlayerThread::sdlThreadReady, &readyLoop, &QEventLoop::quit, Qt::QueuedConnection);
        QTimer::singleShot(3000, &readyLoop, &QEventLoop::quit); // 3 秒超时
        readyLoop.exec();
        disconnect(conn);
    }

    // 使用事件循环同步等待初始化完成
    QEventLoop loop;
    bool initSuccess = false;
    // 关键修正：在lambda捕获列表中加入&loop，或使用默认捕获&
    QMetaObject::Connection conn = connect(this, &SDL2AudioPlayerThread::initialized,
                                           &loop, [&loop, &initSuccess](bool success) {
        initSuccess = success;
        loop.quit(); // 现在能正确访问loop变量了
    }, Qt::QueuedConnection);

    // 发送初始化请求（信号会被投递到子线程的事件循环）
    emit requestInit(sampleRate, channels, format);

    // 设置超时，防止无限等待（5秒超时）
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);

    // 等待初始化完成或超时
    loop.exec();

    // 断开连接
    disconnect(conn);

    // 超时判断：如果仍未初始化成功，返回失败
    if (!initSuccess) {
        qWarning() << "SDL2AudioPlayerThread: 初始化超时或失败";
    }

    return initSuccess;
}

void SDL2AudioPlayerThread::enqueueData(const QByteArray &data)
{
    if (!m_isInitialized || data.isEmpty()) return;
    if (!isRunning()) return;
    emit requestEnqueueData(data);
}

void SDL2AudioPlayerThread::clearQueue()
{
    if (!isRunning()) return;
    emit requestClearQueue();
}

void SDL2AudioPlayerThread::play()
{
    if (m_isInitialized && !isRunning()) return;
    emit requestPlay();
}

void SDL2AudioPlayerThread::pause()
{
    if (!isRunning()) return;
    emit requestPause();
}

void SDL2AudioPlayerThread::stop()
{
    if (!isRunning()) {
        // 如果线程未运行，直接重置状态
        QMutexLocker locker(&m_operationMutex);
        m_isInitialized = false;
        m_isPlaying = false;
        return;
    }
    emit requestStop();
}

bool SDL2AudioPlayerThread::isPlaying() const
{
    QMutexLocker locker(&m_operationMutex);
    return m_isPlaying;
}

bool SDL2AudioPlayerThread::isInitialized() const
{
    QMutexLocker locker(&m_operationMutex);
    return m_isInitialized;
}

bool SDL2AudioPlayerThread::isQueueEmpty() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_audioQueue.isEmpty();
}

qint64 SDL2AudioPlayerThread::getQueueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    qint64 size = 0;
    for (const auto& arr : m_audioQueue) {
        size += arr.size();
    }
    size += m_remainingData.size();
    return size;
}

void SDL2AudioPlayerThread::handleInit(int sampleRate, int channels, SDL_AudioFormat format)
{
    if (m_isInitialized) {
        handleStop();
    }
    // 修复未清空结构体导致的异常
    SDL_memset(&m_wantedSpec, 0, sizeof(m_wantedSpec));

    m_wantedSpec.freq = sampleRate;
    m_wantedSpec.format = format;
    m_wantedSpec.channels = channels;
    m_wantedSpec.silence = 0;
    m_wantedSpec.samples = 4096; // 缓冲区大小，可调整
    m_wantedSpec.callback = audioCallback;
    m_wantedSpec.userdata = this;

    if (SDL_OpenAudio(&m_wantedSpec, &m_obtainedSpec) < 0) {
        KyInfo() << "SDL_OpenAudio failed: " << SDL_GetError();
        m_isInitialized = false;
        emit initialized(false);
        return;
    }

    KyInfo() << "SDL Audio initialized. Obtained spec:";
    KyInfo() << "  Freq: " << m_obtainedSpec.freq;
    KyInfo() << "  Format: " << m_obtainedSpec.format;
    KyInfo() << "  Channels: " << m_obtainedSpec.channels;
    KyInfo() << "  Samples: " << m_obtainedSpec.samples;

    m_isInitialized = true;
    emit initialized(true);
}

void SDL2AudioPlayerThread::handleEnqueueData(const QByteArray &data)
{
    if (!m_isInitialized || data.isEmpty()) return;

    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.enqueue(data);
}

void SDL2AudioPlayerThread::handleClearQueue()
{
    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.clear();
    m_remainingData.clear();
}

void SDL2AudioPlayerThread::handlePlay()
{
    if (m_isInitialized && !m_isPlaying) {
        // 1. 计算200毫秒的静音数据（根据音频参数计算大小）
        int sampleRate = m_obtainedSpec.freq; // 采样率（比如16000）
        int channels = m_obtainedSpec.channels; // 声道数（比如1）
        int bytesPerSample = SDL_AUDIO_BITSIZE(m_obtainedSpec.format) / 8; // 每个样本的字节数（比如2）
        int silenceFrames = sampleRate * 0.8; // 200毫秒的帧数
        int silenceSize = silenceFrames * channels * bytesPerSample; // 静音数据总大小

        // 2. 生成静音数据（16位PCM的静音是0x0000，8位是0x80）
        QByteArray silenceData(silenceSize, 0); // 填充0，对应16位PCM静音

        // 3. 先将静音数据入队
        {
            QMutexLocker locker(&m_queueMutex);
            m_audioQueue.enqueue(silenceData);
        }

        SDL_PauseAudio(0);
        m_isPlaying = true;
        KyInfo() << "Playback started.";
    }
}

void SDL2AudioPlayerThread::handlePause()
{
    if (m_isInitialized && m_isPlaying) {
        SDL_PauseAudio(1);
        m_isPlaying = false;
        KyInfo() << "Playback paused.";
    }
}

void SDL2AudioPlayerThread::handleStop()
{
    if (m_isInitialized) {
        SDL_PauseAudio(1); // 暂停播放
        SDL_CloseAudio();  // 关闭音频设备
        m_isInitialized = false; // 重置初始化状态
    }
    m_isPlaying = false;

    handleClearQueue();
    KyInfo() << "Playback stopped.";
}

void SDL2AudioPlayerThread::audioCallback(void *userdata, Uint8 *stream, int len)
{
    SDL2AudioPlayerThread *player = static_cast<SDL2AudioPlayerThread*>(userdata);
    player->fillAudioBuffer(stream, len);
}

void SDL2AudioPlayerThread::fillAudioBuffer(Uint8 *stream, int len)
{
    // 首先，用静音填充整个缓冲区，以防没有足够的数据
    SDL_memset(stream, 0, len);

    int bytesWritten = 0;
    QByteArray currentData = m_remainingData;

    while (bytesWritten < len) {
        if (currentData.isEmpty()) {
            QMutexLocker locker(&m_queueMutex);
            if (m_audioQueue.isEmpty()) {
                // 队列空了，播放静音
                break;
            }
            currentData = m_audioQueue.dequeue();
        }

        int bytesToCopy = qMin(currentData.size(), len - bytesWritten);
        SDL_memcpy(stream + bytesWritten, currentData.data(), bytesToCopy);

        bytesWritten += bytesToCopy;
        currentData = currentData.mid(bytesToCopy);
    }

    m_remainingData = currentData;

    // 如果队列空了并且没有剩余数据，说明播放结束
    if (bytesWritten < len && getQueueSize() == 0) {
        QMetaObject::invokeMethod(this, "playbackFinished", Qt::QueuedConnection);
        qDebug() << "播放结束";
    } else if (getQueueSize() < m_obtainedSpec.samples * m_obtainedSpec.channels * 2) { // 简单的下溢检测
        QMetaObject::invokeMethod(this, "underflow", Qt::QueuedConnection);
    }
}

