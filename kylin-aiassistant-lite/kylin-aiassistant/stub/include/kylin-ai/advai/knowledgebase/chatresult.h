#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base chat result types.
 * Stub for libkysdk-advanced-ai-dev
 */
typedef struct KBChatResult    KBChatResult;
typedef struct KBChatReference KBChatReference;
typedef struct KBChatSource    KBChatSource;

/* KBChatResult accessors */
int                    kb_chat_result_is_finished(const KBChatResult *result);
const char            *kb_chat_result_get_model(const KBChatResult *result);
const char            *kb_chat_result_get_response(const KBChatResult *result);
const char            *kb_chat_result_get_reasoning_content(const KBChatResult *result);
const KBChatReference **kb_chat_result_get_reference(const KBChatResult *result,
                                                      int *count);
const char           **kb_chat_result_get_reference_file_ids(const KBChatResult *result,
                                                              int *count);
const KBChatSource   **kb_chat_result_get_source(const KBChatResult *result, int *count);

/* KBChatReference accessors */
int kb_chat_reference_get_file_number(const KBChatReference *ref);
int kb_chat_reference_get_chunk_index(const KBChatReference *ref);

/* KBChatSource accessors */
const char *kb_chat_source_get_file_id(const KBChatSource *source);
const char *kb_chat_source_get_content(const KBChatSource *source);

#ifdef __cplusplus
} // extern "C"
#endif
