#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GenAI text chat result.
 * Stub for libkysdk-genai-nlp-dev
 */
typedef struct ChatResult           ChatResult;
typedef struct ChatResultSearchInfo ChatResultSearchInfo;
typedef struct ChatResultSearchItem ChatResultSearchItem;

int         chat_result_get_error_code(ChatResult *result);
const char *chat_result_get_error_message(ChatResult *result);
const char *chat_result_get_assistant_message(ChatResult *result);
int         chat_result_get_is_end(ChatResult *result);
const char *chat_result_get_reasoning_message(ChatResult *result);
const char *chat_result_get_model_name(ChatResult *result);

const ChatResultSearchInfo *chat_result_get_search_info(ChatResult *result, int *error);
size_t      chat_result_search_info_get_count(const ChatResultSearchInfo *info, int *error);
const ChatResultSearchItem *chat_result_search_info_get_item(const ChatResultSearchInfo *info, size_t index, int *error);
size_t      chat_result_search_item_get_index(const ChatResultSearchItem *item, int *error);
const char *chat_result_search_item_get_title(const ChatResultSearchItem *item, int *error);
const char *chat_result_search_item_get_url(const ChatResultSearchItem *item, int *error);

#ifdef __cplusplus
} // extern "C"
#endif
