/* Small ABI-safe wrapper, built against the pinned sherpa-onnx 1.13.2 c-api.h.
 * Links the already installed runtime; this does not rebuild ONNX Runtime.
 */
#include "c-api.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *rv_tts_create(const char *model, const char *tokens, const char *lexicon,
                    const char *rules, int32_t threads)
{
    if (strcmp(SherpaOnnxGetVersionStr(), "1.13.2") != 0) return NULL;
    SherpaOnnxOfflineTtsConfig config;
    memset(&config, 0, sizeof(config));
    config.model.vits.model = model;
    config.model.vits.tokens = tokens;
    config.model.vits.lexicon = lexicon;
    config.model.vits.noise_scale = 0.667f;
    config.model.vits.noise_scale_w = 0.8f;
    config.model.vits.length_scale = 1.0f;
    config.model.num_threads = threads;
    config.model.provider = "cpu";
    config.rule_fsts = rules;
    config.max_num_sentences = 1;
    config.silence_scale = 0.2f;
    return (void *)SherpaOnnxCreateOfflineTts(&config);
}

int32_t rv_tts_sample_rate(void *engine)
{
    return SherpaOnnxOfflineTtsSampleRate(engine);
}

int32_t rv_tts_num_speakers(void *engine)
{
    return SherpaOnnxOfflineTtsNumSpeakers(engine);
}

int32_t rv_tts_generate_wav(void *engine, const char *text, int32_t sid,
                           float speed, void **buffer, int64_t *size)
{
    *buffer = NULL;
    *size = 0;
    SherpaOnnxGenerationConfig config;
    memset(&config, 0, sizeof(config));
    config.sid = sid;
    config.speed = speed;
    config.silence_scale = 0.2f;
    const SherpaOnnxGeneratedAudio *audio = SherpaOnnxOfflineTtsGenerateWithConfig(
        engine, text, &config, NULL, NULL);
    if (!audio) return 0;
    int32_t ok = 0;
    if (audio->samples && audio->n > 0 && audio->n <= (32 * 1024 * 1024 - 44) / 2
        && audio->sample_rate >= 8000 && audio->sample_rate <= 48000) {
        int64_t length = SherpaOnnxWaveFileSize(audio->n);
        if (length > 0 && length <= 32 * 1024 * 1024) {
            *buffer = malloc((size_t)length);
            if (*buffer) {
                SherpaOnnxWriteWaveToBuffer(audio->samples, audio->n, audio->sample_rate, *buffer);
                *size = length;
                ok = 1;
            }
        }
    }
    SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
    return ok;
}

void rv_tts_free_audio(void *buffer)
{
    free(buffer);
}

void rv_tts_destroy(void *engine)
{
    if (engine) SherpaOnnxDestroyOfflineTts(engine);
}
