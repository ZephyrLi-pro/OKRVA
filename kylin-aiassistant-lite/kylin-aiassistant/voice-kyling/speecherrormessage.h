#ifndef SPEECHERRORMESSAGE_H
#define SPEECHERRORMESSAGE_H

#include <QString>

/** 与主工程 Globals::getErrorMsg(SpeechErrorCode) 文案对齐，供语音识别错误码展示。 */
QString speechErrorMessageForCode(int errorCode);
QString speechMicrophoneUnavailableMessage();

#endif
