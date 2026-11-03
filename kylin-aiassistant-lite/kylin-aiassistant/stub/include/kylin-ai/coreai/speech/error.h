#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speech recognition error codes.
 * Stub for libkysdk-coreai-speech-dev
 */
typedef enum {
    AISDK_NO_ERROR          = 0,
    AISDK_NET_ERROR         = 1,
    AISDK_AUTHENTICATION_FAILURE = 2,
    AISDK_RUNTIME_ERROR     = 3,
    AISDK_TOO_MANY_REQUESTS = 4,
    AISDK_SERVICE_ERROR     = 5,
    AISDK_SERVICE_TIMEOUT   = 6,
    AISDK_BAD_REQUEST       = 7,
    AISDK_MODEL_RUN_FAILED  = 8,
    AISDK_MODEL_RUN_TIME_OUT = 9,
    AISDK_MODEL_NOT_FOUND   = 10,
    AISDK_SSL_CERTIFICATE_ERROR = 12,
    /* Speech-specific errors (100-199) */
    SPEECH_RECOGNITION_AUDIO_DATA_SIZE_INVALID = 100,
    SPEECH_SYNTHESIS_TEXT_LENGTH_INVALID       = 101,
    SPEECH_PARAM_INVALID                       = 102,
    SPEECH_DEFAULT_MICROPHONE_INVALID          = 103,
    SPEECH_UNKNOWN_ERROR                       = 104,
    SPEECH_UNSUPPORTED_LANGUAGE                = 105,
    AI_COMMON_SPEECH_DEFAULT_MICROPHONE_MUTE   = 106,
} SpeechErrorCode;

#ifdef __cplusplus
} // extern "C"
#endif
