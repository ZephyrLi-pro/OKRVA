#include "recordercontrolproxy.h"
#include <QMetaObject>
#include <ukui-log4qt.h>

RecorderControlProxy::RecorderControlProxy(RecorderControl *recorderControl, QObject *parent)
    : QObject(parent)
    , m_recorderControl(recorderControl)
{
    initSignalForwarding();
}

RecorderControlProxy::~RecorderControlProxy()
{
    // 代理对象不需要清理 RecorderControl，它由外部管理
}

void RecorderControlProxy::setRecorderControl(RecorderControl *recorderControl)
{
    if (m_recorderControl == recorderControl) {
        return;
    }

    // 断开旧对象的信号连接
    if (m_recorderControl) {
        disconnect(m_recorderControl, nullptr, this, nullptr);
    }

    m_recorderControl = recorderControl;
    
    // 重新连接信号
    initSignalForwarding();
}

void RecorderControlProxy::initSignalForwarding()
{
    if (!m_recorderControl) {
        return;
    }

    // 转发所有信号
    connect(m_recorderControl, &RecorderControl::sigReceiveSpeechRecognitionMessage, 
            this, &RecorderControlProxy::sigReceiveSpeechRecognitionMessage);
    connect(m_recorderControl, &RecorderControl::reply, 
            this, &RecorderControlProxy::reply);
    connect(m_recorderControl, &RecorderControl::sigCancelSendVoice, 
            this, &RecorderControlProxy::sigCancelSendVoice);
}

bool RecorderControlProxy::initSpeechSession()
{
    if (!m_recorderControl) {
        KyInfo() << "RecorderControlProxy: RecorderControl is null";
        return false;
    }

    // 使用 QMetaObject::invokeMethod 异步调用
    bool result = QMetaObject::invokeMethod(m_recorderControl, 
                                            "initSpeechSession",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "RecorderControlProxy: Failed to invoke initSpeechSession";
    }
    return result;
}

void RecorderControlProxy::closeSpeechSession()
{
    if (!m_recorderControl) {
        KyInfo() << "RecorderControlProxy: RecorderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_recorderControl, 
                                            "closeSpeechSession",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "RecorderControlProxy: Failed to invoke closeSpeechSession";
    }
}

bool RecorderControlProxy::startSpeechRecognition()
{
    if (!m_recorderControl) {
        KyInfo() << "RecorderControlProxy: RecorderControl is null";
        return false;
    }

    bool result = QMetaObject::invokeMethod(m_recorderControl, 
                                            "startSpeechRecognition",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "RecorderControlProxy: Failed to invoke startSpeechRecognition";
    }
    return result;
}

void RecorderControlProxy::stopSpeechRecognition()
{
    if (!m_recorderControl) {
        KyInfo() << "RecorderControlProxy: RecorderControl is null";
        return;
    }

    bool result = QMetaObject::invokeMethod(m_recorderControl, 
                                            "stopSpeechRecognition",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "RecorderControlProxy: Failed to invoke stopSpeechRecognition";
    }
}

void RecorderControlProxy::interruptAllVoiceUsing()
{
    if (!m_recorderControl) {
        KyInfo() << "RecorderControlProxy: RecorderControl is null";
        return;
    }
    // QueuedConnection：在录音控制所在工作线程执行，避免主线程阻塞卡死界面。
    bool result = QMetaObject::invokeMethod(m_recorderControl,
                                            "interruptAllVoiceUsing",
                                            Qt::QueuedConnection);
    if (!result) {
        KyInfo() << "RecorderControlProxy: Failed to invoke interruptAllVoiceUsing";
    }
}

