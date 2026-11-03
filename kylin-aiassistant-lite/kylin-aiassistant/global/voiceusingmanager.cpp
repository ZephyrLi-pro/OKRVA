#include "voiceusingmanager.h"
#include <ukui-log4qt.h>
#include <QDBusConnection>

VoiceUsingManager* VoiceUsingManager::m_instance = nullptr;

VoiceUsingManager::VoiceUsingManager(QObject *parent)
    : QObject{parent}
{

}

VoiceUsingManager *VoiceUsingManager::getInstance(QObject *parent)
{
    if (!m_instance) {
          m_instance = new VoiceUsingManager(parent);
      }
    return m_instance;
}

VoiceUsingManager::~VoiceUsingManager()
{

}

void VoiceUsingManager::initConnect()
{
    return;
}

void VoiceUsingManager::startInterruptVoiceUsing()
{
    if(m_currentUsingMoudule == VoiceUsingModule::IdleModule) {

        return;
    }
    // 广播发送关闭音频占用信号
    emit sigCloseCurModuleVoice(m_currentUsingMoudule);

    return;
}

VoiceUsingManager::VoiceUsingModule VoiceUsingManager::getCurVoiceUsingModule()
{

    return m_currentUsingMoudule;
}

void VoiceUsingManager::setCurVoiceUsingMoudule(VoiceUsingModule curModule)
{
    if(curModule != m_currentUsingMoudule) {
        m_currentUsingMoudule = curModule;
        KyInfo() << "当前设置正在使用语音模块为"<< m_currentUsingMoudule;
    }

    return;
}

VoiceUsingManager::VoiceUsingState VoiceUsingManager::getCurVoiceUsingState()
{
    return m_currentUsingState;
}

void VoiceUsingManager::setCurVoiceUsingState(const VoiceUsingState &curState)
{
    if(curState != m_currentUsingState) {
        m_currentUsingState = curState;
        KyInfo() << "当前音频的使用状态为" << m_currentUsingState;
    }

    return;
}

VoiceUsingManager::VoiceUsingModule VoiceUsingManager::getCurVoiceWaitingUsingModule()
{
    return m_waitingUsingMoudule;
}

void VoiceUsingManager::setCurVoiceWaitingUsingMoudule(const VoiceUsingModule &curState)
{
    if(curState != m_waitingUsingMoudule) {
        m_waitingUsingMoudule = curState;
        KyInfo() << "当前待使用音频的模块为" << m_waitingUsingMoudule;
    }
    return;
}

VoiceUsingManager::VoiceUsingModule VoiceUsingManager::getCurVoiceLastUsingModule()
{
    return m_lastUsingMoudule;

}

void VoiceUsingManager::setCurVoiceLastUsingMoudule(const VoiceUsingModule &curState)
{
    if(curState != m_lastUsingMoudule) {
        m_lastUsingMoudule = curState;
        KyInfo() << "当前待使用音频的模块为" << m_lastUsingMoudule;
    }

    return;
}
