#include "sdl2audioplayerthread.h"

#include <QDebug>
#include <QEventLoop>
#include <QMetaObject>

SDL2AudioPlayerThread::SDL2AudioPlayerThread(QObject *parent)
    : QThread(parent)
    , m_isInitialized(false)
    , m_isInitializing(false)
    , m_isPlaying(false)
    , m_shouldQuit(false)
    , m_expectingMoreData(false)
    , m_playbackFinishedPending(false)
    , m_threadReady(false)
{
}

SDL2AudioPlayerThread::~SDL2AudioPlayerThread()
{
    if (isRunning()) {
        requestStop();
        requestQuit();
        // Audio teardown on K1 can exceed 100 ms. Also request exit directly
        // so shutdown does not depend only on a queued custom quit signal.
        quit();
        if (!wait(3000)) {
            qWarning() << "SDL audio thread did not exit promptly; terminating during shutdown";
            terminate();
            wait(3000);
        }
    }
}

void SDL2AudioPlayerThread::run()
{
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        qWarning() << "SDL_Init failed:" << SDL_GetError();
        {
            QMutexLocker locker(&m_operationMutex);
            m_isInitializing = false;
            m_threadReady = false;
        }
        Q_EMIT initialized(false);
        return;
    }

    QEventLoop *eventLoop = new QEventLoop();
    QObject worker;

    connect(this, &SDL2AudioPlayerThread::requestInit,
            &worker, [this](int sampleRate, int channels, SDL_AudioFormat format) {
                handleInit(sampleRate, channels, format);
            }, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestEnqueueData,
            &worker, [this](const QByteArray &data) {
                handleEnqueueData(data);
            }, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestClearQueue,
            &worker, [this]() {
                handleClearQueue();
            }, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestPlay,
            &worker, [this]() {
                handlePlay();
            }, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestStop,
            &worker, [this]() {
                handleStop();
            }, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestExpectingMoreData,
            &worker, [this](bool expecting) {
                handleExpectingMoreData(expecting);
            }, Qt::QueuedConnection);
    connect(this, &SDL2AudioPlayerThread::requestQuit,
            eventLoop, &QEventLoop::quit, Qt::QueuedConnection);

    {
        QMutexLocker locker(&m_operationMutex);
        m_threadReady = true;
    }
    Q_EMIT sdlThreadReady();
    eventLoop->exec();

    {
        QMutexLocker locker(&m_operationMutex);
        m_threadReady = false;
    }
    delete eventLoop;
    if (isInitialized()) {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        QMutexLocker locker(&m_operationMutex);
        m_isInitialized = false;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDL2AudioPlayerThread::initAsync(int sampleRate, int channels, SDL_AudioFormat format)
{
    if (isInitialized()) {
        stop();
    }

    {
        QMutexLocker locker(&m_operationMutex);
        m_isInitializing = true;
    }

    if (!isRunning()) {
        QMetaObject::Connection *readyConn = new QMetaObject::Connection;
        *readyConn = connect(this, &SDL2AudioPlayerThread::sdlThreadReady, this,
                             [this, readyConn, sampleRate, channels, format]() {
            disconnect(*readyConn);
            delete readyConn;
            Q_EMIT requestInit(sampleRate, channels, format);
        }, Qt::QueuedConnection);
        start();
        return;
    }

    bool ready = false;
    {
        QMutexLocker locker(&m_operationMutex);
        ready = m_threadReady;
    }
    if (ready) {
        Q_EMIT requestInit(sampleRate, channels, format);
        return;
    }

    QMetaObject::Connection *readyConn = new QMetaObject::Connection;
    *readyConn = connect(this, &SDL2AudioPlayerThread::sdlThreadReady, this,
                         [this, readyConn, sampleRate, channels, format]() {
        disconnect(*readyConn);
        delete readyConn;
        Q_EMIT requestInit(sampleRate, channels, format);
    }, Qt::QueuedConnection);
}

void SDL2AudioPlayerThread::enqueueData(const QByteArray &data)
{
    if (!isInitialized() || data.isEmpty() || !isRunning()) {
        return;
    }
    Q_EMIT requestEnqueueData(data);
}

void SDL2AudioPlayerThread::clearQueue()
{
    if (isRunning()) {
        Q_EMIT requestClearQueue();
    }
}

void SDL2AudioPlayerThread::play()
{
    if (isInitialized()) {
        Q_EMIT requestPlay();
    }
}

void SDL2AudioPlayerThread::stop()
{
    if (!isRunning()) {
        QMutexLocker locker(&m_operationMutex);
        m_isInitialized = false;
        m_isPlaying = false;
        return;
    }
    Q_EMIT requestStop();
}

void SDL2AudioPlayerThread::setExpectingMoreData(bool expecting)
{
    if (!isRunning()) {
        QMutexLocker locker(&m_queueMutex);
        m_expectingMoreData = expecting;
        return;
    }
    Q_EMIT requestExpectingMoreData(expecting);
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

bool SDL2AudioPlayerThread::isInitializing() const
{
    QMutexLocker locker(&m_operationMutex);
    return m_isInitializing;
}

bool SDL2AudioPlayerThread::hasBufferedAudio() const
{
    return queueSize() > 0;
}

void SDL2AudioPlayerThread::handleInit(int sampleRate, int channels, SDL_AudioFormat format)
{
    if (isInitialized()) {
        handleStop();
    }

    m_wantedSpec.freq = sampleRate;
    m_wantedSpec.format = format;
    m_wantedSpec.channels = channels;
    m_wantedSpec.silence = 0;
    m_wantedSpec.samples = 4096;
    m_wantedSpec.callback = audioCallback;
    m_wantedSpec.userdata = this;

    if (SDL_OpenAudio(&m_wantedSpec, &m_obtainedSpec) < 0) {
        qWarning() << "SDL_OpenAudio failed:" << SDL_GetError();
        QMutexLocker locker(&m_operationMutex);
        m_isInitialized = false;
        m_isInitializing = false;
        Q_EMIT initialized(false);
        return;
    }

    {
        QMutexLocker locker(&m_operationMutex);
        m_isInitialized = true;
        m_isInitializing = false;
    }
    Q_EMIT initialized(true);
}

void SDL2AudioPlayerThread::handleEnqueueData(const QByteArray &data)
{
    if (!isInitialized() || data.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.enqueue(data);
    m_playbackFinishedPending = false;
}

void SDL2AudioPlayerThread::handleClearQueue()
{
    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.clear();
    m_remainingData.clear();
    m_playbackFinishedPending = false;
}

void SDL2AudioPlayerThread::handlePlay()
{
    {
        QMutexLocker locker(&m_operationMutex);
        if (!m_isInitialized || m_isPlaying) {
            return;
        }
    }

    SDL_PauseAudio(0);
    QMutexLocker locker(&m_operationMutex);
    m_isPlaying = true;
}

void SDL2AudioPlayerThread::handleStop()
{
    if (isInitialized()) {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        QMutexLocker locker(&m_operationMutex);
        m_isInitialized = false;
        m_isInitializing = false;
    }

    {
        QMutexLocker locker(&m_operationMutex);
        m_isPlaying = false;
        m_isInitializing = false;
    }
    handleClearQueue();
}

void SDL2AudioPlayerThread::handleExpectingMoreData(bool expecting)
{
    QMutexLocker locker(&m_queueMutex);
    m_expectingMoreData = expecting;
    if (expecting) {
        m_playbackFinishedPending = false;
    }
}

void SDL2AudioPlayerThread::audioCallback(void *userdata, Uint8 *stream, int len)
{
    SDL2AudioPlayerThread *player = static_cast<SDL2AudioPlayerThread *>(userdata);
    player->fillAudioBuffer(stream, len);
}

void SDL2AudioPlayerThread::fillAudioBuffer(Uint8 *stream, int len)
{
    SDL_memset(stream, 0, len);

    bool shouldEmitPlaybackFinished = false;
    bool shouldEmitUnderflow = false;

    {
        QMutexLocker locker(&m_queueMutex);
        int bytesWritten = 0;
        QByteArray currentData = m_remainingData;
        while (bytesWritten < len) {
            if (currentData.isEmpty()) {
                if (m_audioQueue.isEmpty()) {
                    break;
                }
                currentData = m_audioQueue.dequeue();
            }

            const int bytesToCopy = qMin(currentData.size(), len - bytesWritten);
            SDL_memcpy(stream + bytesWritten, currentData.constData(), bytesToCopy);
            bytesWritten += bytesToCopy;
            currentData = currentData.mid(bytesToCopy);
        }

        m_remainingData = currentData;
        const bool queueEmpty = m_remainingData.isEmpty() && m_audioQueue.isEmpty();
        if (bytesWritten < len && queueEmpty && !m_expectingMoreData && !m_playbackFinishedPending) {
            m_playbackFinishedPending = true;
            shouldEmitPlaybackFinished = true;
        } else if (!queueEmpty
                   && queueSizeLocked() < m_obtainedSpec.samples * m_obtainedSpec.channels * 2) {
            shouldEmitUnderflow = true;
        }
    }

    if (shouldEmitPlaybackFinished) {
        QMetaObject::invokeMethod(this, "playbackFinished", Qt::QueuedConnection);
    } else if (shouldEmitUnderflow) {
        QMetaObject::invokeMethod(this, "underflow", Qt::QueuedConnection);
    }
}

qint64 SDL2AudioPlayerThread::queueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return queueSizeLocked();
}

qint64 SDL2AudioPlayerThread::queueSizeLocked() const
{
    qint64 size = m_remainingData.size();
    for (const QByteArray &data : m_audioQueue) {
        size += data.size();
    }
    return size;
}
