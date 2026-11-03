/**
 * GenAI text SDK stub implementations.
 * Stub for libkysdk-genai-nlp-dev
 *
 * All functions return safe no-op defaults so the application can compile
 * and run (with disabled AI features) without the real SDK installed.
 */

#include <kylin-ai/genai/text/chat.h>
#include <kylin-ai/genai/text/config.h>
#include <kylin-ai/genai/text/result.h>
#include <genai/text/modelinfo.h>

#include <cstdlib>
#include <cstring>

/* ──────────────────────────────────────────────────────────────
 * Internal stub structs
 * ────────────────────────────────────────────────────────────── */
struct ChatModelConfig {
    char name[256] = {};
    int  stream    = 1;
};

struct ChatResult {
    int         errorCode     = 0;
    const char *message       = "";
    const char *reasoning     = "";
    const char *modelName     = "";
    int         isEnd         = 1;
};

struct ChatResultSearchInfo {
    int count = 0;
};

struct ChatResultSearchItem {
    size_t      index = 0;
    const char *title = "";
    const char *url   = "";
};

struct GenAiTextSession {
    GenAiTextResultCallback callback  = nullptr;
    void                   *userData  = nullptr;
    ChatModelConfig        *config    = nullptr;
};

struct TextModelList {
    int count = 0;
};

struct TextModelInfo {
    const char    *modelName   = "";
    const char    *displayName = "";
    ModelDeployType deployType = ModelDeployType::PublicCloud;
};

/* ──────────────────────────────────────────────────────────────
 * ChatModelConfig
 * ────────────────────────────────────────────────────────────── */
ChatModelConfig *chat_model_config_create(void)
{
    return new ChatModelConfig();
}

void chat_model_config_destroy(ChatModelConfig **config)
{
    if (config && *config) {
        delete *config;
        *config = nullptr;
    }
}

void chat_model_config_set_name(ChatModelConfig *config, const char *name)
{
    if (config && name) {
        strncpy(config->name, name, sizeof(config->name) - 1);
    }
}

void chat_model_config_set_stream(ChatModelConfig *config, int stream)
{
    if (config) config->stream = stream;
}

void chat_model_config_set_thinking_enabled(ChatModelConfig * /*config*/,
                                            int /*enabled*/,
                                            void * /*reserved*/)
{
    /* stub: no-op */
}

void chat_model_config_set_web_search_enabled(ChatModelConfig * /*config*/,
                                              int /*enabled*/,
                                              void * /*reserved*/)
{
    /* stub: no-op */
}

/* ──────────────────────────────────────────────────────────────
 * ChatResult accessors
 * ────────────────────────────────────────────────────────────── */
int chat_result_get_error_code(ChatResult *result)
{
    return result ? result->errorCode : -1;
}

const char *chat_result_get_assistant_message(ChatResult *result)
{
    return result ? result->message : nullptr;
}

int chat_result_get_is_end(ChatResult *result)
{
    return result ? result->isEnd : 1;
}

const char *chat_result_get_reasoning_message(ChatResult *result)
{
    return result ? result->reasoning : nullptr;
}

const char *chat_result_get_error_message(ChatResult *result)
{
    return result ? result->message : nullptr;
}

const char *chat_result_get_model_name(ChatResult *result)
{
    return result ? result->modelName : "";
}

const ChatResultSearchInfo *chat_result_get_search_info(ChatResult * /*result*/, int *error)
{
    if (error) *error = 0;
    return nullptr; /* stub: no search info */
}

size_t chat_result_search_info_get_count(const ChatResultSearchInfo *info, int *error)
{
    if (error) *error = 0;
    return info ? static_cast<size_t>(info->count) : 0;
}

const ChatResultSearchItem *chat_result_search_info_get_item(const ChatResultSearchInfo * /*info*/,
                                                             size_t /*index*/, int *error)
{
    if (error) *error = -1;
    return nullptr;
}

size_t chat_result_search_item_get_index(const ChatResultSearchItem *item, int *error)
{
    if (error) *error = 0;
    return item ? item->index : 0;
}

const char *chat_result_search_item_get_title(const ChatResultSearchItem *item, int *error)
{
    if (error) *error = 0;
    return item ? item->title : "";
}

const char *chat_result_search_item_get_url(const ChatResultSearchItem *item, int *error)
{
    if (error) *error = 0;
    return item ? item->url : "";
}

/* ──────────────────────────────────────────────────────────────
 * GenAiTextSession
 * ────────────────────────────────────────────────────────────── */
GenAiTextSession *genai_text_create_session(void)
{
    return new GenAiTextSession();
}

int genai_text_init_session(GenAiTextSession * /*session*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

void genai_text_result_set_callback(GenAiTextSession *session,
                                    GenAiTextResultCallback callback,
                                    void *userData)
{
    if (session) {
        session->callback = callback;
        session->userData = userData;
    }
}

void genai_text_chat_async(GenAiTextSession * /*session*/, const char * /*message*/)
{
    /* stub: no-op */
}

void genai_text_stop_chat(GenAiTextSession * /*session*/)
{
    /* stub: no-op */
}

void genai_text_destroy_session(GenAiTextSession **session)
{
    if (session && *session) {
        delete *session;
        *session = nullptr;
    }
}

void genai_text_set_model_config(GenAiTextSession *session, ChatModelConfig *config)
{
    if (session) session->config = config;
}

void genai_text_clear_chat_history_messages(GenAiTextSession * /*session*/)
{
    /* stub: no-op */
}

/* ──────────────────────────────────────────────────────────────
 * TextModelList / TextModelInfo
 * ────────────────────────────────────────────────────────────── */
const TextModelList *genai_text_get_model_list(void * /*session*/, int *errorCode)
{
    if (errorCode) *errorCode = 0;
    /* Return an empty static list */
    static TextModelList emptyList;
    emptyList.count = 0;
    return &emptyList;
}

int genai_text_model_list_get_count(const TextModelList *list)
{
    return list ? list->count : 0;
}

const TextModelInfo *genai_text_model_list_get_model_info(const TextModelList * /*list*/,
                                                          int /*index*/,
                                                          int *error)
{
    if (error) *error = -1;
    return nullptr;
}

const char *genai_text_model_info_get_model_name(const TextModelInfo *info, int *error)
{
    if (error) *error = 0;
    return info ? info->modelName : "";
}

const char *genai_text_model_info_get_display_name(const TextModelInfo *info, int *error)
{
    if (error) *error = 0;
    return info ? info->displayName : "";
}

ModelDeployType genai_text_model_info_get_deploy_type(const TextModelInfo *info, int *error)
{
    if (error) *error = 0;
    return info ? info->deployType : ModelDeployType::PublicCloud;
}
