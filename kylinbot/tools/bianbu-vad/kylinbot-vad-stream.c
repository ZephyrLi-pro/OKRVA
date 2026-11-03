// Local, persistent Silero VAD. No microphone access or audio files are created.
// stdin: little-endian uint32 sample count followed by float32 mono samples.
// count=0 resets the utterance. stdout: READY, then one 0/1 line per request.
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c-api.h"

static int read_exact(void *buffer, size_t size) {
    unsigned char *p = buffer;
    size_t done = 0;
    while (done < size) {
        size_t n = fread(p + done, 1, size - done, stdin);
        if (n == 0) return done == 0 && feof(stdin) ? 0 : -1;
        done += n;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: kylinbot-vad-stream MODEL INPUT_SAMPLE_RATE\n");
        return 2;
    }
    char *end = NULL;
    long rate = strtol(argv[2], &end, 10);
    if (!end || *end || rate < 8000 || rate > 192000) return 2;
    SherpaOnnxVadModelConfig config;
    memset(&config, 0, sizeof(config));
    config.silero_vad.model = argv[1];
    config.silero_vad.threshold = 0.5f;
    config.silero_vad.min_silence_duration = 0.12f;
    config.silero_vad.min_speech_duration = 0.08f;
    config.silero_vad.window_size = 512;
    config.silero_vad.max_speech_duration = 60.0f;
    config.sample_rate = 16000;
    config.num_threads = 1;
    config.provider = "cpu";
    const SherpaOnnxVoiceActivityDetector *vad =
        SherpaOnnxCreateVoiceActivityDetector(&config, 30.0f);
    if (!vad) return 1;
    const SherpaOnnxLinearResampler *resampler = rate == 16000 ? NULL :
        SherpaOnnxCreateLinearResampler((int32_t)rate, 16000,
                                      (float)(rate < 16000 ? rate : 16000) * 0.45f, 6);
    if (rate != 16000 && !resampler) {
        SherpaOnnxDestroyVoiceActivityDetector(vad);
        return 1;
    }
    float *samples = malloc(65536 * sizeof(float));
    if (!samples) return 1;
    setvbuf(stdout, NULL, _IOLBF, 0);
    puts("READY");
    int status = 0;
    for (;;) {
        unsigned char header[4];
        int result = read_exact(header, sizeof(header));
        if (result == 0) break;
        if (result < 0) { status = 3; break; }
        uint32_t n = (uint32_t)header[0] | ((uint32_t)header[1] << 8) |
                     ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);
        if (n > 65536) { status = 3; break; }
        if (n == 0) {
            SherpaOnnxVoiceActivityDetectorReset(vad);
            if (resampler) SherpaOnnxLinearResamplerReset(resampler);
            puts("0");
            continue;
        }
        if (read_exact(samples, n * sizeof(float)) != 1) { status = 3; break; }
        if (resampler) {
            const SherpaOnnxResampleOut *out =
                SherpaOnnxLinearResamplerResample(resampler, samples, (int32_t)n, 0);
            if (!out) { status = 1; break; }
            SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad, out->samples, out->n);
            SherpaOnnxLinearResamplerResampleFree(out);
        } else {
            SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad, samples, (int32_t)n);
        }
        printf("%d\n", !!SherpaOnnxVoiceActivityDetectorDetected(vad));
        while (!SherpaOnnxVoiceActivityDetectorEmpty(vad)) {
            SherpaOnnxVoiceActivityDetectorPop(vad);
        }
    }
    free(samples);
    if (resampler) SherpaOnnxDestroyLinearResampler(resampler);
    SherpaOnnxDestroyVoiceActivityDetector(vad);
    return status;
}
