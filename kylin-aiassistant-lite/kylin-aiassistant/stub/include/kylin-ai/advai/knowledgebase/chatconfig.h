#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base chat configuration.
 * Stub for libkysdk-advanced-ai-dev
 */
typedef struct KBChatConfig KBChatConfig;

KBChatConfig *kb_chat_config_new(void);
void          kb_chat_config_free(KBChatConfig *config);
void          kb_chat_config_set_model(KBChatConfig *config, const char *model);
void          kb_chat_config_set_doc_ids(KBChatConfig *config, const char **docIds,
                                          int count);

#ifdef __cplusplus
} // extern "C"
#endif
