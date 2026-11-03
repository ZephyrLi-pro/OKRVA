#include "speecherrormessage.h"

#include <QCoreApplication>

#include <kylin-ai/common/error.h>

QString speechMicrophoneUnavailableMessage()
{
    return QCoreApplication::translate(
            "SpeechErrors",
            "The microphone is occupied or unavailable, and voice chat cannot be used. Please close other apps and try again.");
}

QString speechErrorMessageForCode(int errorCode)
{
    switch (errorCode) {
    case AISDK_NO_ERROR:
        return QString();
    case AISDK_NET_ERROR:
        return QCoreApplication::translate(
                "SpeechErrors",
                "The network connection is not available, please check the network settings and try again.");
    case AISDK_AUTHENTICATION_FAILURE:
        return QCoreApplication::translate(
                "SpeechErrors",
                "Authentication error. Check whether the model account configuration information is incorrect.");
    case AISDK_RUNTIME_ERROR:
        return QCoreApplication::translate(
                "SpeechErrors", "Runtime error, please restart the service or system and try again.");
    case AISDK_TOO_MANY_REQUESTS:
        return QCoreApplication::translate(
                "SpeechErrors",
                "Requests are too frequent, try again later, or reduce the request frequency to avoid this issue.");
    case AISDK_SERVICE_ERROR:
        return QCoreApplication::translate(
                "SpeechErrors",
                "Cloud service error. The current request cannot be completed, check whether the service status is "
                "normal and try again.");
    case AISDK_SERVICE_TIMEOUT:
        return QCoreApplication::translate(
                "SpeechErrors",
                "The service response timed out. The request was not completed in time, please try again.");
    case AISDK_BAD_REQUEST:
        return QCoreApplication::translate(
                "SpeechErrors",
                "The request parameter or format is incorrect. Check that the parameters are formatted correctly and "
                "resubmit the request.");
    case AISDK_MODEL_RUN_FAILED:
        return QCoreApplication::translate(
                "SpeechErrors",
                "If the local model fails to run and the request cannot be completed, check the model configuration "
                "and try again.");
    case AISDK_MODEL_RUN_TIME_OUT:
        return QCoreApplication::translate(
                "SpeechErrors", "The local model ran timed out, please try again later.");
    case AISDK_MODEL_NOT_FOUND:
        return QCoreApplication::translate(
                "SpeechErrors",
                "There is currently no available voice model. Please add or enable a model under System Settings > AI "
                "Module Management > Model Configuration.");
    case AISDK_SSL_CERTIFICATE_ERROR:
        return QCoreApplication::translate(
                "SpeechErrors",
                "Network abnormalities may be caused by the following reasons:\n\n"
                "1.Request time validation failed, please check the system time settings or try changing the model "
                "and retry.\n\n"
                "2.The network is not authenticated, please try network authentication or contact IT.");
    case AI_COMMON_SPEECH_RECOGNITION_AUDIO_DATA_SIZE_INVALID:
        return QCoreApplication::translate(
                "SpeechErrors",
                "If the length of the speech recognition audio is too short or too long, please extend or shorten the "
                "audio length and submit it again.");
    case AI_COMMON_SPEECH_SYNTHESIS_TEXT_LENGTH_INVALID:
        return QCoreApplication::translate(
                "SpeechErrors",
                "If the text length of the speech synthesis text is too long, please send or shorten the text in "
                "parts and try again.");
    case AI_COMMON_SPEECH_PARAM_INVALID:
        return QCoreApplication::translate(
                "SpeechErrors",
                "The voice model configuration is invalid, check whether the model configuration information is "
                "incorrect.");
    case AI_COMMON_SPEECH_DEFAULT_MICROPHONE_INVALID:
        return QCoreApplication::translate(
                "SpeechErrors", "The microphone was not detected, please connect the device and try again.");
    case AI_COMMON_SPEECH_UNKNOWN_ERROR:
        return QCoreApplication::translate(
                "SpeechErrors", "An unknown error has occurred, please try again later.");
    case AI_COMMON_SPEECH_UNSUPPORTED_LANGUAGE:
        return QCoreApplication::translate(
                "SpeechErrors",
                "This language is not supported at this time. Please try a different language.");
    case AI_COMMON_SPEECH_DEFAULT_MICROPHONE_MUTE:
        return QCoreApplication::translate(
                "SpeechErrors",
                "No sound input detected. The device microphone may be muted. Please check and try again.");
    default:
        return QCoreApplication::translate("SpeechErrors", "Undefined error, returned (%1).").arg(errorCode);
    }
}
