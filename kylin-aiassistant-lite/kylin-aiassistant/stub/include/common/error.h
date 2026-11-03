#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Common AI SDK error codes shared across all modules.
 * Stub for libkysdk-ai-common-dev
 */
typedef enum {
    AI_COMMON_SUCCESS                    = 0,
    AI_COMMON_INVALID_PARAMETER          = 1,
    AI_COMMON_PERMISSION_DENIED          = 2,
    AI_COMMON_SERVICE_ERROR              = 3,
    AI_COMMON_SERVICE_TIMEOUT            = 4,
    AI_COMMON_SERVER_INITIALIZATION_FAILED = 5,
    AI_COMMON_SERVER_CONNECTION_FAILED   = 6,
    AI_COMMON_SSL_CERTIFICATE_ERROR      = 7,
    /* Network errors (100-199) */
    AI_COMMON_NET_ERROR                  = 100,
    AI_COMMON_NETWORK_TIMEOUT            = 101,
    AI_COMMON_NETWORK_CONNECTION_FAILED  = 102,
    AI_COMMON_AUTHENTICATION_FAILURE     = 103,
    /* Session errors (200-299) */
    AISDK_INVALID_SESSION                = 200,
    AI_COMMON_INVALID_SESSION            = 201,
    /* NLP / model errors (600-699) */
    AI_COMMON_MODEL_NOT_FOUND            = 600,
    AI_COMMON_NLP_INPUT_INVALID          = 601,
    AI_COMMON_TOO_MANY_REQUESTS          = 602,
    AI_COMMON_NLP_PARAM_ERROR            = 603,
    AI_COMMON_RUNTIME_OUTDATED           = 604,
    AI_COMMON_MODEL_RUN_TIME_OUT         = 605,
    AI_COMMON_RUNTIME_ERROR              = 606,
    AI_COMMON_BAD_REQUEST                = 607,
    AI_COMMON_NLP_TOKEN_LIMITED          = 608,
    /* Unknown */
    AI_COMMON_UNKNOWN_ERROR              = -1,
} AiCommonError;

#ifdef __cplusplus
} // extern "C"
#endif
