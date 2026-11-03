#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GenAI text model configuration.
 * Stub for libkysdk-genai-nlp-dev
 */
typedef struct ChatModelConfig ChatModelConfig;

ChatModelConfig *chat_model_config_create(void);
void chat_model_config_destroy(ChatModelConfig **config);
void chat_model_config_set_name(ChatModelConfig *config, const char *name);
void chat_model_config_set_stream(ChatModelConfig *config, int stream);
void chat_model_config_set_thinking_enabled(ChatModelConfig *config, int enabled,
                                            void *reserved);
void chat_model_config_set_web_search_enabled(ChatModelConfig *config, int enabled,
                                              void *reserved);

#ifdef __cplusplus
} // extern "C"
#endif
