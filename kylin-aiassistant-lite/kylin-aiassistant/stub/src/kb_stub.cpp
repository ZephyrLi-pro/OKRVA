/**
 * Knowledge base SDK stub implementations.
 * Stub for libkysdk-advanced-ai-dev
 *
 * All functions return safe no-op defaults so the application can compile
 * and run (with disabled knowledge base features) without the real SDK.
 */

#include <kylin-ai/advai/knowledgebase/chatconfig.h>
#include <kylin-ai/advai/knowledgebase/chathistory.h>
#include <kylin-ai/advai/knowledgebase/chatresponse.h>
#include <kylin-ai/advai/knowledgebase/chatresult.h>
#include <kylin-ai/advai/knowledgebase/error.h>
#include <kylin-ai/advai/knowledgebase/knowledgebaseinfo.h>
#include <kylin-ai/advai/knowledgebase/session.h>
#include <kylin-ai/advai/knowledgebase/types.h>

#include <cstdlib>
#include <cstring>
#include <string>

/* ──────────────────────────────────────────────────────────────
 * Internal stub structs
 * ────────────────────────────────────────────────────────────── */
struct KBSession {
    bool initialized = false;
};

struct KnowledgeBaseInfo {
    std::string name;
    std::string id;
    std::string description;
};

struct KBChatHistory {
    int messageCount = 0;
};

struct KBChatConfig {
    std::string model;
};

struct KBChatResult {
    bool        finished         = true;
    const char *model            = "";
    const char *response         = "";
    const char *reasoningContent = "";
};

struct KBChatResponse {
    int         errorCode = 0;
    KBChatResult result;
};

struct KBChatReference {};
struct KBChatSource {};

/* ──────────────────────────────────────────────────────────────
 * Error helpers
 * ────────────────────────────────────────────────────────────── */
const char *kb_get_error_message(int /*errorCode*/)
{
    return "stub: no SDK installed";
}

/* ──────────────────────────────────────────────────────────────
 * KBSession
 * ────────────────────────────────────────────────────────────── */
KBSession *kb_session_create(const char * /*appId*/, const char * /*password*/,
                             const char * /*dbPath*/, int *error)
{
    if (error) *error = 0; /* AI_COMMON_SUCCESS */
    return new KBSession();
}

void kb_session_destroy(KBSession *session)
{
    delete session;
}

int kb_session_init(KBSession *session)
{
    if (session) session->initialized = true;
    return 0; /* AI_COMMON_SUCCESS */
}

/* ──────────────────────────────────────────────────────────────
 * KnowledgeBaseInfo
 * ────────────────────────────────────────────────────────────── */
KnowledgeBaseInfo *kb_knowledgebase_info_new(const char *name, const char *description)
{
    KnowledgeBaseInfo *info = new KnowledgeBaseInfo();
    if (name)        info->name        = name;
    if (description) info->description = description;
    return info;
}

void kb_knowledgebase_info_free(KnowledgeBaseInfo *info)
{
    delete info;
}

KnowledgeBaseInfo *kb_knowledgebase_info_get(KBSession * /*session*/,
                                              const char * /*kbId*/,
                                              int *error)
{
    if (error) *error = 0;
    return new KnowledgeBaseInfo();
}

char *kb_knowledgebase_create(KBSession * /*session*/, KnowledgeBaseInfo * /*info*/,
                               int *error)
{
    if (error) *error = 0;
    return strdup("stub-kb-id");
}

KnowledgeBaseInfo **kb_knowledgebase_info_list(KBSession * /*session*/, int *count,
                                               int *error)
{
    if (count) *count = 0;
    if (error) *error = 0;
    return nullptr;
}

/* Outside extern "C" — returns std::string */
std::string kb_knowledgebase_info_get_name(KnowledgeBaseInfo *info)
{
    return info ? info->name : std::string();
}

const char *kb_knowledgebase_info_get_id(KnowledgeBaseInfo *info)
{
    return info ? info->id.c_str() : "";
}

void kb_knowledgebase_info_set_id(KnowledgeBaseInfo *info, const char *id)
{
    if (info && id) info->id = id;
}

void kb_free_string(char *str)
{
    free(str);
}

/* ──────────────────────────────────────────────────────────────
 * Document operations
 * ────────────────────────────────────────────────────────────── */
char **kb_document_list(KBSession * /*session*/, const char * /*kbId*/,
                        int *count, int *error)
{
    if (count) *count = 0;
    if (error) *error = 0;
    /* Return a null-terminated empty array */
    char **list = new char *[1];
    list[0] = nullptr;
    return list;
}

int kb_document_add(KBSession * /*session*/, const char * /*kbId*/,
                    const char * /*docPath*/, const char * /*docId*/,
                    void * /*reserved*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

int kb_document_get_processing_progress(KBSession * /*session*/,
                                         KnowledgeBaseInfo * /*info*/,
                                         const char * /*docId*/,
                                         int *status, int *error)
{
    if (status) *status = KB_DOC_STATUS_COMPLETED;
    if (error)  *error  = 0;
    return 100; /* 100% */
}

int kb_document_delete(KBSession * /*session*/, const char * /*kbId*/,
                       const char * /*docId*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

int kb_document_delete_multi(KBSession * /*session*/, const char * /*kbId*/,
                              const char ** /*docIds*/, int /*count*/,
                              int ** /*errorCodes*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

int kb_document_cancel_addition(KBSession * /*session*/, const char * /*kbId*/,
                                 const char ** /*docIds*/, int /*count*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

/* ──────────────────────────────────────────────────────────────
 * KBChatHistory
 * ────────────────────────────────────────────────────────────── */
KBChatHistory *kb_chat_history_new(void)
{
    return new KBChatHistory();
}

void kb_chat_history_free(KBChatHistory *history)
{
    delete history;
}

int kb_chat_history_add_question(KBChatHistory *history, const char * /*question*/)
{
    if (history) ++history->messageCount;
    return 0;
}

int kb_chat_history_add_response(KBChatHistory *history, const char * /*response*/)
{
    if (history) ++history->messageCount;
    return 0;
}

int kb_chat_history_get_message_count(KBChatHistory *history)
{
    return history ? history->messageCount : 0;
}

void kb_chat_history_delete_question(KBChatHistory * /*history*/, int /*index*/) {}
void kb_chat_history_delete_response(KBChatHistory * /*history*/, int /*index*/) {}

/* ──────────────────────────────────────────────────────────────
 * KBChatConfig
 * ────────────────────────────────────────────────────────────── */
KBChatConfig *kb_chat_config_new(void)
{
    return new KBChatConfig();
}

void kb_chat_config_free(KBChatConfig *config)
{
    delete config;
}

void kb_chat_config_set_model(KBChatConfig *config, const char *model)
{
    if (config && model) config->model = model;
}

void kb_chat_config_set_doc_ids(KBChatConfig * /*config*/, const char ** /*docIds*/,
                                 int /*count*/)
{
    /* stub: no-op */
}

/* ──────────────────────────────────────────────────────────────
 * Chat streaming
 * ────────────────────────────────────────────────────────────── */
int kb_chat_stream(KBSession * /*session*/, const char * /*model*/, const char * /*kbId*/,
                   const char * /*question*/, KBChatHistory * /*history*/,
                   KBChatConfig * /*config*/, KBChatStreamCallback /*callback*/,
                   void * /*userData*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

int kb_chat_stream_stop(KBSession * /*session*/, const char * /*model*/)
{
    return 0; /* AI_COMMON_SUCCESS */
}

/* ──────────────────────────────────────────────────────────────
 * KBChatResponse / KBChatResult accessors
 * ────────────────────────────────────────────────────────────── */
int kb_chat_response_get_error_code(const KBChatResponse *response)
{
    return response ? response->errorCode : -1;
}

const KBChatResult *kb_chat_response_get_result(const KBChatResponse *response)
{
    return response ? &response->result : nullptr;
}

int kb_chat_result_is_finished(const KBChatResult *result)
{
    return result ? result->finished : 1;
}

const char *kb_chat_result_get_model(const KBChatResult *result)
{
    return result ? result->model : "";
}

const char *kb_chat_result_get_response(const KBChatResult *result)
{
    return result ? result->response : "";
}

const char *kb_chat_result_get_reasoning_content(const KBChatResult *result)
{
    return result ? result->reasoningContent : "";
}

const KBChatReference **kb_chat_result_get_reference(const KBChatResult * /*result*/,
                                                       int *count)
{
    if (count) *count = 0;
    return nullptr;
}

const char **kb_chat_result_get_reference_file_ids(const KBChatResult * /*result*/,
                                                    int *count)
{
    if (count) *count = 0;
    return nullptr;
}

const KBChatSource **kb_chat_result_get_source(const KBChatResult * /*result*/, int *count)
{
    if (count) *count = 0;
    return nullptr;
}

/* ──────────────────────────────────────────────────────────────
 * KBChatReference / KBChatSource accessors
 * ────────────────────────────────────────────────────────────── */
int kb_chat_reference_get_file_number(const KBChatReference * /*ref*/)
{
    return 0;
}

int kb_chat_reference_get_chunk_index(const KBChatReference * /*ref*/)
{
    return 0;
}

const char *kb_chat_source_get_file_id(const KBChatSource * /*source*/)
{
    return "";
}

const char *kb_chat_source_get_content(const KBChatSource * /*source*/)
{
    return "";
}
