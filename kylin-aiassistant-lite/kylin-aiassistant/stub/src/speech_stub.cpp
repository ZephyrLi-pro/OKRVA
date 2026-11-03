/**
 * Speech SDK stub implementations.
 * Stub for libkysdk-coreai-speech-dev
 *
 * All functions return safe no-op defaults so the application can compile
 * and run (with disabled speech features) without the real SDK installed.
 */

#include <kylin-ai/coreai/speech/audioconfig.h>
#include <kylin-ai/coreai/speech/recognizer.h>
#include <kylin-ai/coreai/speech/result.h>
#include <kylin-ai/coreai/speech/synthesizer.h>

#include <cstdlib>
#include <cstring>

/* ──────────────────────────────────────────────────────────────
 * Internal stub structs
 * ────────────────────────────────────────────────────────────── */
struct AudioConfig {
    int sampleRate = 16000;
};

struct SpeechRecognitionResult {
    int errorCode                    = 0;
    const char *text                 = "";
    int speakerId                    = 0;
    SpeechResultReason reason        = SPEECH_RECOGNIZED;
};

struct SpeechSynthesisResult {
    int errorCode                    = 0;
    SpeechResultReason reason        = SPEECH_SYNTHESIS_COMPLETED;
    int audioRate                    = 16000;
    int audioChannel                 = 1;
};

struct SpeechRecognitionSession {
    SpeechRecognitionResultCallback callback = nullptr;
    void *userData                           = nullptr;
    AudioConfig *audioConfig                 = nullptr;
};

struct SpeechSynthesizerSession {
    SpeechSynthesisResultCallback callback = nullptr;
    void *userData                         = nullptr;
    AudioConfig *audioConfig               = nullptr;
};

/* ──────────────────────────────────────────────────────────────
 * AudioConfig
 * ────────────────────────────────────────────────────────────── */
AudioConfig *audio_config_create_continuous_audio_input_from_default_microphone(void)
{
    return new AudioConfig();
}

AudioConfig *audio_config_create_audio_output_from_pcm_data(void)
{
    return new AudioConfig();
}

void audio_config_set_input_audio_rate(AudioConfig *config, int rate)
{
    if (config) config->sampleRate = rate;
}

void audio_config_destroy(AudioConfig **config)
{
    if (config && *config) {
        delete *config;
        *config = nullptr;
    }
}

/* ──────────────────────────────────────────────────────────────
 * SpeechRecognitionResult accessors
 * ────────────────────────────────────────────────────────────── */
int speech_recognition_result_get_error_code(SpeechRecognitionResult *result)
{
    return result ? result->errorCode : -1;
}

const char *speech_recognition_result_get_text(SpeechRecognitionResult *result)
{
    return result ? result->text : "";
}

int speech_recognition_result_get_speaker_id(SpeechRecognitionResult *result)
{
    return result ? result->speakerId : 0;
}

SpeechResultReason speech_recognition_result_get_reason(SpeechRecognitionResult *result)
{
    return result ? result->reason : SPEECH_RECOGNIZED;
}

/* ──────────────────────────────────────────────────────────────
 * SpeechSynthesisResult accessors
 * ────────────────────────────────────────────────────────────── */
SpeechResultReason speech_synthesis_result_get_reason(SpeechSynthesisResult *result)
{
    return result ? result->reason : SPEECH_SYNTHESIS_COMPLETED;
}

int speech_synthesis_result_get_error_code(SpeechSynthesisResult *result)
{
    return result ? result->errorCode : -1;
}

const uint8_t *speech_synthesis_result_get_data(SpeechSynthesisResult * /*result*/,
                                                  uint32_t *dataLength)
{
    if (dataLength) *dataLength = 0;
    return nullptr;
}

int speech_synthesis_result_get_audio_rate(SpeechSynthesisResult *result)
{
    return result ? result->audioRate : 16000;
}

int speech_synthesis_result_get_audio_channel(SpeechSynthesisResult *result)
{
    return result ? result->audioChannel : 1;
}

/* ──────────────────────────────────────────────────────────────
 * SpeechRecognitionSession
 * ────────────────────────────────────────────────────────────── */
SpeechRecognitionSession *speech_recognizer_create_session(void)
{
    return new SpeechRecognitionSession();
}

int speech_recognizer_init_session(SpeechRecognitionSession * /*session*/)
{
    return 0; /* AISDK_NO_ERROR */
}

void speech_recognizer_result_set_callback(SpeechRecognitionSession *session,
                                           SpeechRecognitionResultCallback callback,
                                           void *user_data)
{
    if (session) {
        session->callback = callback;
        session->userData = user_data;
    }
}

void speech_recognizer_destroy_session(SpeechRecognitionSession **session)
{
    if (session && *session) {
        delete *session;
        *session = nullptr;
    }
}

void speech_recognizer_set_audio_config(SpeechRecognitionSession *session,
                                        AudioConfig *config)
{
    if (session) session->audioConfig = config;
}

void speech_recognizer_start_continuous_recognition_async(SpeechRecognitionSession * /*session*/)
{
    /* stub: no-op */
}

void speech_recognizer_stop_continuous_recognition_async(SpeechRecognitionSession * /*session*/)
{
    /* stub: no-op */
}

/* ──────────────────────────────────────────────────────────────
 * SpeechSynthesizerSession
 * ────────────────────────────────────────────────────────────── */
SpeechSynthesizerSession *speech_synthesizer_create_session(void)
{
    return new SpeechSynthesizerSession();
}

int speech_synthesizer_init_session(SpeechSynthesizerSession * /*session*/)
{
    return 0; /* AISDK_NO_ERROR */
}

void speech_synthesizer_result_set_callback(SpeechSynthesizerSession *session,
                                            SpeechSynthesisResultCallback callback,
                                            void *user_data)
{
    if (session) {
        session->callback = callback;
        session->userData = user_data;
    }
}

void speech_synthesizer_destroy_session(SpeechSynthesizerSession **session)
{
    if (session && *session) {
        delete *session;
        *session = nullptr;
    }
}

void speech_synthesizer_set_audio_config(SpeechSynthesizerSession *session,
                                         AudioConfig *config)
{
    if (session) session->audioConfig = config;
}

void speech_synthesizer_synthesize_text_async(SpeechSynthesizerSession * /*session*/,
                                              const char * /*text*/,
                                              uint32_t /*length*/)
{
    /* stub: no-op */
}
