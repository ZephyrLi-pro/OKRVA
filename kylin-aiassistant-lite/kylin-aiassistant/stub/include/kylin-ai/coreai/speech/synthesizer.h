#pragma once

#include <stdint.h>
#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speech synthesizer session and functions.
 * Stub for libkysdk-coreai-speech-dev
 */
typedef struct SpeechSynthesizerSession SpeechSynthesizerSession;

typedef void (*SpeechSynthesisResultCallback)(SpeechSynthesisResult *result,
                                              void *user_data);

SpeechSynthesizerSession *speech_synthesizer_create_session(void);
int  speech_synthesizer_init_session(SpeechSynthesizerSession *session);
void speech_synthesizer_result_set_callback(SpeechSynthesizerSession *session,
                                            SpeechSynthesisResultCallback callback,
                                            void *user_data);
void speech_synthesizer_destroy_session(SpeechSynthesizerSession **session);
void speech_synthesizer_set_audio_config(SpeechSynthesizerSession *session,
                                         AudioConfig *config);
void speech_synthesizer_synthesize_text_async(SpeechSynthesizerSession *session,
                                              const char *text,
                                              uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif
