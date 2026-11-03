#ifndef SDL2AUDIOPLAYERTHREAD_H
#define SDL2AUDIOPLAYERTHREAD_H

#include <QThread>
#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QEventLoop>

// 包含SDL头文件
#include <SDL2/SDL.h>

class SDL2AudioPlayerThread : public QThread
{
    Q_OBJECT

public:
    explicit SDL2AudioPlayerThread(QObject *parent = nullptr);
    ~SDL2AudioPlayerThread();

    // 公共接口（与 SDL2AudioPlayer 兼容）
    bool init(int sampleRate = 16000, int channels = 1, SDL_AudioFormat format = AUDIO_S16LSB);
    void enqueueData(const QByteArray &data);
    void clearQueue();
    void play();
    void pause();
    void stop();

    // 查询方法（线程安全）
    bool isPlaying() const;
    bool isInitialized() const;
    bool isQueueEmpty() const;
    qint64 getQueueSize() const;

signals:
    void playbackFinished();
    void underflow(); // 缓冲区即将耗尽
    void initialized(bool success); // 初始化完成信号（内部使用，但需要公开以便连接）
    void sdlThreadReady(); // SDL 线程就绪（SDL_Init 完成且事件循环已启动，可接收 requestInit）
    
    // 内部信号，用于在线程中触发处理函数
    void requestInit(int sampleRate, int channels, SDL_AudioFormat format);
    void requestEnqueueData(const QByteArray &data);
    void requestClearQueue();
    void requestPlay();
    void requestPause();
    void requestStop();

protected:
    void run() override;

private slots:
    void handleInit(int sampleRate, int channels, SDL_AudioFormat format);
    void handleEnqueueData(const QByteArray &data);
    void handleClearQueue();
    void handlePlay();
    void handlePause();
    void handleStop();

private:
    // SDL音频回调函数，必须是静态的
    static void audioCallback(void *userdata, Uint8 *stream, int len);

    // 在回调中被调用，用于填充数据
    void fillAudioBuffer(Uint8 *stream, int len);

    SDL_AudioSpec m_wantedSpec;
    SDL_AudioSpec m_obtainedSpec;
    bool m_isInitialized;
    bool m_isPlaying;

    QQueue<QByteArray> m_audioQueue;
    mutable QMutex m_queueMutex; // mutable 允许在const函数中锁定

    // 用于拼接不完整的音频帧
    QByteArray m_remainingData;

    // 线程同步
    mutable QMutex m_operationMutex; // mutable 允许在const函数中锁定
    QWaitCondition m_operationCondition;
    bool m_shouldQuit;
    QEventLoop *m_eventLoop;
};

#endif // SDL2AUDIOPLAYERTHREAD_H

