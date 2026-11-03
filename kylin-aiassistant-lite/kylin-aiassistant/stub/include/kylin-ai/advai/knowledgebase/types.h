#pragma once

#include <common/error.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Knowledge base document status and common error codes.
 * Stub for libkysdk-advanced-ai-dev
 */

/* Document processing status */
typedef enum {
    KB_DOC_STATUS_PENDING    = 0,
    KB_DOC_STATUS_PROCESSING = 1,
    KB_DOC_STATUS_COMPLETED  = 2,
    KB_DOC_STATUS_ERROR      = 3,
} KbDocStatus;

/* Knowledge base error codes */
typedef enum {
    KB_ERROR_NONE                    = 0,
    /* KB-level errors (300-399) */
    KB_ERROR_KB_ALREADY_EXISTS       = 301,
    KB_ERROR_KB_NOT_FOUND            = 302,
    KB_ERROR_KB_NAME_INVALID         = 303,
    KB_ERROR_APP_ID_INVALID          = 304,
    KB_ERROR_PASSWORD_INVALID        = 305,
    KB_ERROR_KB_ACCESS_DENIED        = 306,
    KB_ERROR_KB_CORRUPTED            = 307,
    KB_ERROR_KB_EMPTY                = 308,
    /* Document errors (400-499) */
    KB_ERROR_DOC_NOT_FOUND           = 401,
    KB_ERROR_DOC_TOO_LARGE           = 402,
    KB_ERROR_DOC_INVALID_FORMAT      = 403,
    KB_ERROR_DOC_ACCESS_DENIED       = 404,
    KB_ERROR_DOC_CORRUPTED           = 405,
    KB_ERROR_DOC_EMPTY               = 406,
    KB_ERROR_DOC_ID_INVALID          = 407,
    KB_ERROR_DOC_ID_NOT_FOUND        = 408,
    KB_ERROR_DOC_ID_ALREADY_EXISTS   = 409,
    KB_ERROR_DOC_BATCH_PARTIAL_FAILED = 410,
    KB_ERROR_DOC_BATCH_ALL_FAILED    = 411,
    KB_ERROR_DOC_PARSE_FAILED        = 412,
    /* Retrieval errors (500-599) */
    KB_ERROR_RETRIEVAL_FAILED        = 501,
    KB_ERROR_RETRIEVAL_NO_RESULTS    = 502,
    KB_ERROR_RETRIEVAL_TIMEOUT       = 503,
} KbErrorCode;

#ifdef __cplusplus
} // extern "C"
#endif
