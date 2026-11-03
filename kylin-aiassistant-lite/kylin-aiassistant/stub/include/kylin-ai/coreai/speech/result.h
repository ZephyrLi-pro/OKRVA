#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speech result types and accessor functions.
 * Stub for libkysdk-coreai-speech-dev
 */
typedef enum {
    SPEECH_ERROR_OCCURRED           = 1,
    SPEECH_RECOGNITION_STARTED      = 2,
    SPEECH_RECOGNIZING              = 3,
    SPEECH_RECOGNIZED               = 4,
    SPEECH_RECOGNITION_COMPLETED    = 5,
    SPEECH_SYNTHESIS_STARTED        = 6,
    SPEECH_SYNTHESIZING             = 7,
    SPEECH_SYNTHESIS_COMPLETED      = 8,
} SpeechResultReason;

/* Opaque result types */
typedef struct SpeechRecognitionResult SpeechRecognitionResult;
typedef struct SpeechSynthesisResult   SpeechSynthesisResult;

/* Recognition result accessors */
int                speech_recognition_result_get_error_code(SpeechRecognitionResult *result);
const char        *speech_recognition_result_get_text(SpeechRecognitionResult *result);
int                speech_recognition_result_get_speaker_id(SpeechRecognitionResult *result);
SpeechResultReason speech_recognition_result_get_reason(SpeechRecognitionResult *result);

/* Synthesis result accessors */
SpeechResultReason speech_synthesis_result_get_reason(SpeechSynthesisResult *result);
int                speech_synthesis_result_get_error_code(SpeechSynthesisResult *result);
const uint8_t     *speech_synthesis_result_get_data(SpeechSynthesisResult *result,
                                                     uint32_t *dataLength);
int                speech_synthesis_result_get_audio_rate(SpeechSynthesisResult *result);
int                speech_synthesis_result_get_audio_channel(SpeechSynthesisResult *result);

#ifdef __cplusplus
} // extern "C"
#endif
