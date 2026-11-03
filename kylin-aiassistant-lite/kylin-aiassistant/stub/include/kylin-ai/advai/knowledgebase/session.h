#pragma once

#include <kylin-ai/advai/knowledgebase/chathistory.h>
#include <kylin-ai/advai/knowledgebase/chatconfig.h>
#include <kylin-ai/advai/knowledgebase/chatresponse.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base session management and chat streaming.
 * Stub for libkysdk-advanced-ai-dev
 */
typedef struct KBSession KBSession;

typedef int (*KBChatStreamCallback)(const KBChatResponse *response, void *userData);

KBSession *kb_session_create(const char *appId, const char *password,
                             const char *dbPath, int *error);
void       kb_session_destroy(KBSession *session);
int        kb_session_init(KBSession *session);

int        kb_chat_stream(KBSession *session, const char *model, const char *kbId,
                          const char *question, KBChatHistory *history,
                          KBChatConfig *config, KBChatStreamCallback callback,
                          void *userData);
int        kb_chat_stream_stop(KBSession *session, const char *model);

#ifdef __cplusplus
} // extern "C"
#endif
