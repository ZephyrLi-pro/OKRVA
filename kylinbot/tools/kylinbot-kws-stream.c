// SPDX-License-Identifier: MIT
//
// Minimal persistent sherpa-onnx keyword-spotting worker for KylinBot.
// It reads mono little-endian f32 PCM from stdin and emits one line per hit:
//
//   KEYWORD<TAB><display text>
//
// The model is initialized once, avoiding the multi-second startup cost of
// invoking sherpa-onnx-keyword-spotter for every VAD window.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c-api.h"

#define PCM_SAMPLES_PER_READ 4096

static int parse_positive_int(const char *value, const char *name) {
  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (value[0] == '\0' || end == NULL || *end != '\0' || parsed <= 0 ||
      parsed > INT32_MAX) {
    fprintf(stderr, "Invalid %s: %s\n", name, value);
    exit(2);
  }
  return (int)parsed;
}

static float parse_positive_float(const char *value, const char *name) {
  char *end = NULL;
  float parsed = strtof(value, &end);
  if (value[0] == '\0' || end == NULL || *end != '\0' || parsed <= 0.0f) {
    fprintf(stderr, "Invalid %s: %s\n", name, value);
    exit(2);
  }
  return parsed;
}

int main(int argc, char **argv) {
  if (argc != 11) {
    fprintf(stderr,
            "Usage: %s ENCODER DECODER JOINER TOKENS KEYWORDS PROVIDER "
            "SAMPLE_RATE THREADS SCORE THRESHOLD\n",
            argv[0]);
    return 2;
  }

  const int sample_rate = parse_positive_int(argv[7], "sample rate");
  const int threads = parse_positive_int(argv[8], "thread count");
  const float score = parse_positive_float(argv[9], "keyword score");
  const float threshold = parse_positive_float(argv[10], "keyword threshold");

  SherpaOnnxOnlineTransducerModelConfig transducer;
  memset(&transducer, 0, sizeof(transducer));
  transducer.encoder = argv[1];
  transducer.decoder = argv[2];
  transducer.joiner = argv[3];

  SherpaOnnxOnlineModelConfig model;
  memset(&model, 0, sizeof(model));
  model.transducer = transducer;
  model.tokens = argv[4];
  model.num_threads = threads;
  model.provider = argv[6];
  model.model_type = "zipformer2";
  model.modeling_unit = "cjkchar";

  SherpaOnnxKeywordSpotterConfig config;
  memset(&config, 0, sizeof(config));
  config.feat_config.sample_rate = 16000;
  config.feat_config.feature_dim = 80;
  config.model_config = model;
  config.max_active_paths = 4;
  config.num_trailing_blanks = 1;
  config.keywords_score = score;
  config.keywords_threshold = threshold;
  config.keywords_file = argv[5];

  const SherpaOnnxKeywordSpotter *spotter =
      SherpaOnnxCreateKeywordSpotter(&config);
  if (spotter == NULL) {
    fprintf(stderr, "Failed to create sherpa-onnx keyword spotter\n");
    return 1;
  }

  const SherpaOnnxOnlineStream *stream =
      SherpaOnnxCreateKeywordStream(spotter);
  if (stream == NULL) {
    fprintf(stderr, "Failed to create sherpa-onnx keyword stream\n");
    SherpaOnnxDestroyKeywordSpotter(spotter);
    return 1;
  }

  // Tell the parent that initialization (the expensive part) is complete.
  puts("READY");
  fflush(stdout);

  float samples[PCM_SAMPLES_PER_READ];
  for (;;) {
    const size_t count =
        fread(samples, sizeof(samples[0]), PCM_SAMPLES_PER_READ, stdin);
    if (count == 0) {
      if (feof(stdin)) {
        break;
      }
      if (ferror(stdin)) {
        perror("Failed to read PCM from stdin");
        break;
      }
      continue;
    }

    SherpaOnnxOnlineStreamAcceptWaveform(stream, sample_rate, samples,
                                         (int32_t)count);
    while (SherpaOnnxIsKeywordStreamReady(spotter, stream)) {
      SherpaOnnxDecodeKeywordStream(spotter, stream);
    }

    const SherpaOnnxKeywordResult *result =
        SherpaOnnxGetKeywordResult(spotter, stream);
    if (result != NULL && result->keyword != NULL && result->keyword[0] != '\0') {
      printf("KEYWORD\t%s\n", result->keyword);
      fflush(stdout);
      SherpaOnnxResetKeywordStream(spotter, stream);
    }
    if (result != NULL) {
      SherpaOnnxDestroyKeywordResult(result);
    }
  }

  SherpaOnnxOnlineStreamInputFinished(stream);
  SherpaOnnxDestroyOnlineStream(stream);
  SherpaOnnxDestroyKeywordSpotter(spotter);
  return ferror(stdin) ? 1 : 0;
}
