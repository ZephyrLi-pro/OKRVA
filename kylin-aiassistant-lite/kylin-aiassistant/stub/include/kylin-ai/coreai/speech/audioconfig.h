#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audio configuration types and functions.
 * Stub for libkysdk-coreai-speech-dev
 */
typedef struct AudioConfig AudioConfig;

AudioConfig *audio_config_create_continuous_audio_input_from_default_microphone(void);
AudioConfig *audio_config_create_audio_output_from_pcm_data(void);
void         audio_config_set_input_audio_rate(AudioConfig *config, int rate);
void         audio_config_destroy(AudioConfig **config);

#ifdef __cplusplus
} // extern "C"
#endif
