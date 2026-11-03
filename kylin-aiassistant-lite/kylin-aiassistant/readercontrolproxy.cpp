#include "readercontrolproxy.h"
#include <QMetaObject>
#include <QThread>
#include <ukui-log4qt.h>

ReaderControlProxy::ReaderControlProxy(ReaderControl *readerControl, QObject *parent)
    : QObject(parent)
    , m_readerControl(readerControl)
{
    initSignalForwarding();
}

ReaderControlProxy::~ReaderControlProxy()
{
    // 代理对象不需要清理 ReaderControl，它由外部管理
}

void ReaderControlProxy::setReaderControl(ReaderControl *readerControl)
{
    if (m_readerControl == readerControl) {
        return;
    }

    // 断开旧对象的信号连接
    if (m_readerControl) {
        disconnect(m_readerControl, nullptr, this, nullptr);
    }

    m_readerControl = readerControl;
    
    // 重新连接信号
    initSignalForwarding();
}

void ReaderControlProxy::initSignalForwarding()
{
    if (!m_readerControl) {
        return;
    }

    // 转发所有信号
    connect(m_readerControl, &ReaderControl::sigPlayingStart, 
            this, &ReaderControlProxy::sigPlayingStart);
    connect(m_readerControl, &ReaderControl::sigReadEnd, 
            this, &ReaderControlProxy::sigReadEnd);
    connect(m_readerControl, &ReaderControl::reply, 
            this, &ReaderControlProxy::reply);
    connect(m_readerControl, &ReaderControl::sigResetPreIndex, 
            this, &ReaderControlProxy::sigResetPreIndex);
}

bool ReaderControlProxy::initReaderSession()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    // 使用 QMetaObject::invokeMethod 异步调用
    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "initReaderSession",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke initReaderSession";
    }
    return result;
}

bool ReaderControlProxy::closeReaderSession()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "closeReaderSession",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke closeReaderSession";
    }
    return result;
}

void ReaderControlProxy::startReading()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "startReading",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke startReading";
    }
}

void ReaderControlProxy::pauseReading()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "pauseReading",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke pauseReading";
    }
}

void ReaderControlProxy::stopReading()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "stopReading",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke stopReading";
    }
}

void ReaderControlProxy::continueReading()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "continueReading",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke continueReading";
    }
}

void ReaderControlProxy::interruptReading()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "interruptReading",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke interruptReading";
    }
}

bool ReaderControlProxy::getCurrentReaderStatus()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    // 注意：对于需要返回值的调用，如果使用 QueuedConnection，返回值无法获取
    // 这里使用 BlockingQueuedConnection 会阻塞，但可以获取返回值
    // 或者可以考虑通过信号返回结果，但为了保持接口兼容性，这里使用阻塞调用
    bool result = false;
    QMetaObject::invokeMethod(m_readerControl, 
                              "getCurrentReaderStatus",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result));
    return result;
}

bool ReaderControlProxy::sendReadingText(const QString &msg, int index)
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "sendReadingText",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, msg),
                                            Q_ARG(int, index));
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke sendReadingText";
    }
    return result;
}

bool ReaderControlProxy::continueAddReadingText(const QString &msg, int index)
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "continueAddReadingText",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, msg),
                                            Q_ARG(int, index));
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke continueAddReadingText";
    }
    return result;
}

int ReaderControlProxy::getCurrentReadIndex()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return -1;
    }

    // 需要返回值，使用 BlockingQueuedConnection
    int result = -1;
    QMetaObject::invokeMethod(m_readerControl, 
                              "getCurrentReadIndex",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, result));
    return result;
}

bool ReaderControlProxy::insertWaitingReadingListItem(const QString &msg)
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "insertWaitingReadingListItem",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, msg));
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke insertWaitingReadingListItem";
    }
    return result;
}

void ReaderControlProxy::clearWaitingReadingList()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "clearWaitingReadingList",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke clearWaitingReadingList";
    }
}

void ReaderControlProxy::clearReadingPlayerData()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "clearReadingPlayerData",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke clearReadingPlayerData";
    }
}

void ReaderControlProxy::interruptAllVoiceUsing()
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return;
    }

    // QueuedConnection：打断在 ReaderControl 所在媒体线程执行，主线程/QML 不等待，避免卡 UI。
    // 与 startReadingCurrentText 等同为入队时，Qt 按调用顺序处理，仍为「先打断、再开始」。
    bool result = QMetaObject::invokeMethod(m_readerControl,
                                            "interruptAllVoiceUsing",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke interruptAllVoiceUsing";
    }
}

bool ReaderControlProxy::startReadingCurrentText(const QString &msg)
{
    if (!m_readerControl) {
        KyInfo() << "ReaderControlProxy: ReaderControl is null";
        return false;
    }

    bool result = QMetaObject::invokeMethod(m_readerControl, 
                                            "startReadingCurrentText",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, msg));
    if (!result) {
        KyInfo() << "ReaderControlProxy: Failed to invoke startReadingCurrentText";
    }
    return result;
}
