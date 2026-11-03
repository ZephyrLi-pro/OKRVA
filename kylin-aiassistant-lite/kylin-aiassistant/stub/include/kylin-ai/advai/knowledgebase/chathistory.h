#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base chat history.
 * Stub for libkysdk-advanced-ai-dev
 */
typedef struct KBChatHistory KBChatHistory;

KBChatHistory *kb_chat_history_new(void);
void           kb_chat_history_free(KBChatHistory *history);
int            kb_chat_history_add_question(KBChatHistory *history, const char *question);
int            kb_chat_history_add_response(KBChatHistory *history, const char *response);
int            kb_chat_history_get_message_count(KBChatHistory *history);
void           kb_chat_history_delete_question(KBChatHistory *history, int index);
void           kb_chat_history_delete_response(KBChatHistory *history, int index);

#ifdef __cplusplus
} // extern "C"
#endif
