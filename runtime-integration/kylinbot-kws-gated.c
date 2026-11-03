// SPDX-License-Identifier: MIT
//
// Minimal persistent sherpa-onnx keyword-spotting worker for KylinBot.
// It reads mono little-endian f32 PCM from stdin and emits one line per hit:
//
//   KEYWORD<TAB><display text>
//
// The model is initialized once, avoiding the multi-second startup cost of
// invoking sherpa-onnx-keyword-spotter for every VAD window.

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "c-api.h"

#define PCM_SAMPLES_PER_READ 4096

// Features are opt-in: existing callers keep the original CLI and protocol.
// Pin AFTER ORT initialization too; its worker pools may choose their own mask.
static int pin_threads(const char *value) {
  if (!value || !*value) return 0;
  int lo, hi;
  char extra;
  if (sscanf(value, "%d-%d%c", &lo, &hi, &extra) != 2 || lo < 0 ||
      hi < lo || hi >= CPU_SETSIZE) return -1;
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (int cpu = lo; cpu <= hi; ++cpu) CPU_SET(cpu, &mask);
  DIR *tasks = opendir("/proc/self/task");
  if (!tasks) return -1;
  struct dirent *entry;
  int failed = 0;
  while ((entry = readdir(tasks))) {
    char *end;
    long tid = strtol(entry->d_name, &end, 10);
    if (*end || tid <= 0) continue;
    if (sched_setaffinity((pid_t)tid, sizeof(mask), &mask) && errno != ESRCH)
      failed = 1;
  }
  closedir(tasks);
  return failed ? -1 : 0;
}

// Fail OPEN if the optional policy watcher fails or its heartbeat goes stale.
// This is a tiny control file, never audio or conversation content.
static int policy_busy(const char *path) {
  if (!path || !*path) return 0;
  int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) return 0;
  struct stat st;
  char text[80] = {0}, extra;
  int busy = 0;
  long long stamp;
  if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == getuid() &&
      !(st.st_mode & 0077) && st.st_size > 0 && st.st_size < 80) {
    ssize_t count = read(fd, text, sizeof(text) - 1);
    if (count > 0 && sscanf(text, "%lld %d %c", &stamp, &busy, &extra) == 2) {
      time_t now = time(NULL);
      if (stamp <= (long long)now && (long long)now - stamp <= 3 && busy == 1) {
        close(fd);
        return 1;
      }
    }
  }
  close(fd);
  return 0;
}

static void decode(const SherpaOnnxKeywordSpotter *spotter,
                   const SherpaOnnxOnlineStream *stream, int rate,
                   const float *samples, int32_t count) {
  SherpaOnnxOnlineStreamAcceptWaveform(stream, rate, samples, count);
  while (SherpaOnnxIsKeywordStreamReady(spotter, stream))
    SherpaOnnxDecodeKeywordStream(spotter, stream);
  const SherpaOnnxKeywordResult *result =
      SherpaOnnxGetKeywordResult(spotter, stream);
  if (result && result->keyword && result->keyword[0]) {
    printf("KEYWORD\t%s\n", result->keyword);
    fflush(stdout);
    SherpaOnnxResetKeywordStream(spotter, stream);
  }
  if (result) SherpaOnnxDestroyKeywordResult(result);
}

typedef struct {
  const SherpaOnnxVoiceActivityDetector *vad;
  const SherpaOnnxLinearResampler *resampler;
  float *ring;
  int capacity, size, head, active, tail;
} Gate;

static void gate_reset(Gate *gate) {
  gate->size = gate->head = gate->active = gate->tail = 0;
  if (gate->vad) SherpaOnnxVoiceActivityDetectorReset(gate->vad);
  if (gate->resampler) SherpaOnnxLinearResamplerReset(gate->resampler);
}

static void gate_destroy(Gate *gate) {
  if (gate->vad) SherpaOnnxDestroyVoiceActivityDetector(gate->vad);
  if (gate->resampler) SherpaOnnxDestroyLinearResampler(gate->resampler);
  free(gate->ring);
  memset(gate, 0, sizeof(*gate));
}

static void gate_init(Gate *gate, int rate) {
  const char *enabled = getenv("KYLINBOT_KWS_VAD_GATE");
  if (!enabled || strcmp(enabled, "1")) return;
  const char *model = getenv("KYLINBOT_VAD_MODEL");
  if (!model || access(model, R_OK)) goto fallback;
  SherpaOnnxVadModelConfig config;
  memset(&config, 0, sizeof(config));
  config.silero_vad.model = model;
  config.silero_vad.threshold = 0.5f;
  config.silero_vad.min_silence_duration = 0.12f;
  config.silero_vad.min_speech_duration = 0.08f;
  config.silero_vad.max_speech_duration = 60.0f;
  config.silero_vad.window_size = 512;
  config.sample_rate = 16000;
  config.num_threads = 1;
  config.provider = "cpu";
  gate->vad = SherpaOnnxCreateVoiceActivityDetector(&config, 30.0f);
  if (!gate->vad) goto fallback;
  if (rate != 16000) {
    float cutoff = 0.495f * (rate < 16000 ? rate : 16000);
    gate->resampler = SherpaOnnxCreateLinearResampler(rate, 16000, cutoff, 6);
    if (!gate->resampler) goto fallback;
  }
  // Retain the beginning of speech while VAD is accumulating its decision.
  gate->capacity = (int)(rate * 0.6f);
  gate->ring = calloc((size_t)gate->capacity, sizeof(float));
  if (!gate->ring) goto fallback;
  fprintf(stderr, "KWS VAD gate enabled (0.6s pre-roll, 1.0s tail)\n");
  return;
fallback:
  gate_destroy(gate);
  fprintf(stderr, "KWS VAD gate unavailable; continuing original wake detection\n");
}

static void gate_feed(Gate *gate, const SherpaOnnxKeywordSpotter *spotter,
                      const SherpaOnnxOnlineStream *stream, int rate,
                      const float *samples, int count) {
  if (!gate->vad) {
    decode(spotter, stream, rate, samples, count);
    return;
  }
  for (int i = 0; i < count; ++i) {
    gate->ring[gate->head] = samples[i];
    gate->head = (gate->head + 1) % gate->capacity;
    if (gate->size < gate->capacity) ++gate->size;
  }
  const SherpaOnnxResampleOut *out = NULL;
  if (gate->resampler) {
    out = SherpaOnnxLinearResamplerResample(gate->resampler, samples, count, 0);
    if (!out) {
      gate_destroy(gate);
      fprintf(stderr, "KWS resampling failed; falling back to continuous wake detection\n");
      decode(spotter, stream, rate, samples, count);
      return;
    }
    SherpaOnnxVoiceActivityDetectorAcceptWaveform(gate->vad, out->samples, out->n);
  } else {
    SherpaOnnxVoiceActivityDetectorAcceptWaveform(gate->vad, samples, count);
  }
  int speech = SherpaOnnxVoiceActivityDetectorDetected(gate->vad);
  if (out) SherpaOnnxLinearResamplerResampleFree(out);
  while (!SherpaOnnxVoiceActivityDetectorEmpty(gate->vad))
    SherpaOnnxVoiceActivityDetectorPop(gate->vad);
  if (speech && !gate->active) {
    gate->active = 1;
    int start = (gate->head + gate->capacity - gate->size) % gate->capacity;
    int first = gate->capacity - start;
    if (first > gate->size) first = gate->size;
    decode(spotter, stream, rate, gate->ring + start, first);
    if (first < gate->size)
      decode(spotter, stream, rate, gate->ring, gate->size - first);
  } else if (gate->active) {
    decode(spotter, stream, rate, samples, count);
  }
  if (speech) gate->tail = rate; // Zipformer needs trailing frames to finish a hit.
  else if (gate->active) {
    gate->tail -= count;
    if (gate->tail <= 0) {
      SherpaOnnxResetKeywordStream(spotter, stream);
      gate->active = 0;
    }
  }
}

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
  if (value[0] == '\0' || end == NULL || *end != '\0' || !isfinite(parsed) || parsed <= 0.0f) {
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
  if (sample_rate < 8000 || sample_rate > 192000 || threads > 64) return 2;
  const char *cpuset = getenv("KYLINBOT_KWS_CPUSET");
  if (pin_threads(cpuset)) {
    fprintf(stderr, "Invalid or unavailable KWS CPU placement\n");
    return 2;
  }

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

  Gate gate = {0};
  gate_init(&gate, sample_rate);
  if (pin_threads(cpuset)) {
    fprintf(stderr, "Cannot enforce KWS CPU placement after initialization\n");
    gate_destroy(&gate);
    SherpaOnnxDestroyOnlineStream(stream);
    SherpaOnnxDestroyKeywordSpotter(spotter);
    return 1;
  }
  const char *busy_file = getenv("KYLINBOT_KWS_BUSY_FILE");
  int was_busy = 0;
  double last_frame = 0;
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

    struct timespec now = {0};
    clock_gettime(CLOCK_MONOTONIC, &now);
    double frame_time = now.tv_sec + now.tv_nsec / 1e9;
    int busy = policy_busy(busy_file);
    // The parent pauses feeding during post-wake recording; do not splice
    // old pre-roll into a later utterance when feeding resumes.
    if (busy != was_busy || (gate.vad && last_frame && frame_time - last_frame > 1.0)) {
      SherpaOnnxResetKeywordStream(spotter, stream);
      gate_reset(&gate);
      was_busy = busy;
    }
    last_frame = frame_time;
    // Consume/discard rather than stopping the process: no stale audio backlog.
    if (!busy) gate_feed(&gate, spotter, stream, sample_rate, samples, (int)count);
  }

  SherpaOnnxOnlineStreamInputFinished(stream);
  gate_destroy(&gate);
  SherpaOnnxDestroyOnlineStream(stream);
  SherpaOnnxDestroyKeywordSpotter(spotter);
  return ferror(stdin) ? 1 : 0;
}
