#pragma once

#include <kylin-ai/advai/knowledgebase/chatresult.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base chat response (wrapper around result).
 * Stub for libkysdk-advanced-ai-dev
 */
typedef struct KBChatResponse KBChatResponse;

int                  kb_chat_response_get_error_code(const KBChatResponse *response);
const KBChatResult  *kb_chat_response_get_result(const KBChatResponse *response);

#ifdef __cplusplus
} // extern "C"
#endif
