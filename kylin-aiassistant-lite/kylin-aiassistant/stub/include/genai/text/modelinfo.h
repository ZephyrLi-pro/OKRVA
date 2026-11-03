#pragma once

/**
 * GenAI text model info (list and detail).
 * Stub for libkysdk-genai-nlp-dev
 */

/* ModelDeployType — C++ enum class used in switch statements */
enum class ModelDeployType {
    PublicCloud  = 0,
    PrivateCloud = 1,
    OnDevice     = 2,
};

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TextModelList TextModelList;
typedef struct TextModelInfo TextModelInfo;

/* List accessors */
const TextModelList *genai_text_get_model_list(void *session, int *errorCode);
int                  genai_text_model_list_get_count(const TextModelList *list);
const TextModelInfo *genai_text_model_list_get_model_info(const TextModelList *list,
                                                          int index, int *error);

/* Info accessors */
const char *genai_text_model_info_get_model_name(const TextModelInfo *info, int *error);
const char *genai_text_model_info_get_display_name(const TextModelInfo *info, int *error);

#ifdef __cplusplus
} // extern "C"

/* Deploy type accessor — returns C++ enum, must be outside extern "C" */
ModelDeployType genai_text_model_info_get_deploy_type(const TextModelInfo *info, int *error);
#endif
