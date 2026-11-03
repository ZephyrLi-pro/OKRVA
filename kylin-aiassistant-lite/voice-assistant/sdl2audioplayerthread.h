#ifndef SDL2AUDIOPLAYERTHREAD_H
#define SDL2AUDIOPLAYERTHREAD_H

#include <QByteArray>
#include <QMutex>
#include <QQueue>
#include <QThread>

#include <SDL2/SDL.h>

class QEventLoop;

class SDL2AudioPlayerThread : public QThread
{
    Q_OBJECT

public:
    explicit SDL2AudioPlayerThread(QObject *parent = nullptr);
    ~SDL2AudioPlayerThread();

    void initAsync(int sampleRate = 16000, int channels = 1, SDL_AudioFormat format = AUDIO_S16LSB);
    void enqueueData(const QByteArray &data);
    void clearQueue();
    void play();
    void stop();
    void setExpectingMoreData(bool expecting);

    bool isPlaying() const;
    bool isInitialized() const;
    bool isInitializing() const;
    bool hasBufferedAudio() const;

signals:
    void playbackFinished();
    void underflow();
    void initialized(bool success);
    void sdlThreadReady();
    void requestQuit();
    void requestInit(int sampleRate, int channels, SDL_AudioFormat format);
    void requestEnqueueData(const QByteArray &data);
    void requestClearQueue();
    void requestPlay();
    void requestStop();
    void requestExpectingMoreData(bool expecting);

protected:
    void run() override;

private slots:
    void handleInit(int sampleRate, int channels, SDL_AudioFormat format);
    void handleEnqueueData(const QByteArray &data);
    void handleClearQueue();
    void handlePlay();
    void handleStop();
    void handleExpectingMoreData(bool expecting);

private:
    static void audioCallback(void *userdata, Uint8 *stream, int len);
    void fillAudioBuffer(Uint8 *stream, int len);
    qint64 queueSize() const;
    qint64 queueSizeLocked() const;

    SDL_AudioSpec m_wantedSpec;
    SDL_AudioSpec m_obtainedSpec;
    bool m_isInitialized;
    bool m_isInitializing;
    bool m_isPlaying;
    QQueue<QByteArray> m_audioQueue;
    mutable QMutex m_queueMutex;
    QByteArray m_remainingData;
    mutable QMutex m_operationMutex;
    bool m_shouldQuit;
    bool m_expectingMoreData;
    bool m_playbackFinishedPending;
    bool m_threadReady;
};

#endif // SDL2AUDIOPLAYERTHREAD_H
