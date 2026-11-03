#pragma once

#include <string>
#include <kylin-ai/advai/knowledgebase/session.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base info management.
 * Stub for libkysdk-advanced-ai-dev
 */
typedef struct KnowledgeBaseInfo KnowledgeBaseInfo;

KnowledgeBaseInfo  *kb_knowledgebase_info_new(const char *name, const char *description);
void                kb_knowledgebase_info_free(KnowledgeBaseInfo *info);
KnowledgeBaseInfo  *kb_knowledgebase_info_get(KBSession *session, const char *kbId,
                                               int *error);
char               *kb_knowledgebase_create(KBSession *session, KnowledgeBaseInfo *info,
                                             int *error);
KnowledgeBaseInfo **kb_knowledgebase_info_list(KBSession *session, int *count, int *error);

const char         *kb_knowledgebase_info_get_id(KnowledgeBaseInfo *info);
void                kb_knowledgebase_info_set_id(KnowledgeBaseInfo *info, const char *id);

void                kb_free_string(char *str);

/* Document operations */
char      **kb_document_list(KBSession *session, const char *kbId, int *count, int *error);
int         kb_document_add(KBSession *session, const char *kbId, const char *docPath,
                             const char *docId, void *reserved);
int         kb_document_get_processing_progress(KBSession *session,
                                                 KnowledgeBaseInfo *info,
                                                 const char *docId,
                                                 int *status, int *error);
int         kb_document_delete(KBSession *session, const char *kbId, const char *docId);
int         kb_document_delete_multi(KBSession *session, const char *kbId,
                                      const char **docIds, int count, int **errorCodes);
int         kb_document_cancel_addition(KBSession *session, const char *kbId,
                                         const char **docIds, int count);

#ifdef __cplusplus
} // extern "C"

/* std::string return — must be outside extern "C" */
std::string kb_knowledgebase_info_get_name(KnowledgeBaseInfo *info);
#endif
