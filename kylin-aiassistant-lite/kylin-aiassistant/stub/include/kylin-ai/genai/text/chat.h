#pragma once

#include <kylin-ai/genai/text/config.h>
#include <kylin-ai/genai/text/result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GenAI text chat session and functions.
 * Stub for libkysdk-genai-nlp-dev
 */
typedef struct GenAiTextSession GenAiTextSession;

typedef void (*GenAiTextResultCallback)(ChatResult *result, void *userData);

GenAiTextSession *genai_text_create_session(void);
int  genai_text_init_session(GenAiTextSession *session);
void genai_text_result_set_callback(GenAiTextSession *session,
                                    GenAiTextResultCallback callback,
                                    void *userData);
void genai_text_chat_async(GenAiTextSession *session, const char *message);
void genai_text_stop_chat(GenAiTextSession *session);
void genai_text_destroy_session(GenAiTextSession **session);
void genai_text_set_model_config(GenAiTextSession *session, ChatModelConfig *config);
void genai_text_clear_chat_history_messages(GenAiTextSession *session);

#ifdef __cplusplus
} // extern "C"
#endif
