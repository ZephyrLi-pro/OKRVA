#pragma once

#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speech recognition session and functions.
 * Stub for libkysdk-coreai-speech-dev
 */
typedef struct SpeechRecognitionSession SpeechRecognitionSession;

typedef void (*SpeechRecognitionResultCallback)(SpeechRecognitionResult *result,
                                                void *user_data);

SpeechRecognitionSession *speech_recognizer_create_session(void);
int  speech_recognizer_init_session(SpeechRecognitionSession *session);
void speech_recognizer_result_set_callback(SpeechRecognitionSession *session,
                                           SpeechRecognitionResultCallback callback,
                                           void *user_data);
void speech_recognizer_destroy_session(SpeechRecognitionSession **session);
void speech_recognizer_set_audio_config(SpeechRecognitionSession *session,
                                        AudioConfig *config);
void speech_recognizer_start_continuous_recognition_async(SpeechRecognitionSession *session);
void speech_recognizer_stop_continuous_recognition_async(SpeechRecognitionSession *session);

#ifdef __cplusplus
} // extern "C"
#endif
