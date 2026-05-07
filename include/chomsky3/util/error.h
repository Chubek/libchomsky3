/**
 * Internal error compatibility layer used by src/util.
 */

#ifndef CHOMSKY3_UTIL_ERROR_H
#define CHOMSKY3_UTIL_ERROR_H

#include <stdarg.h>
#include <stdio.h>
#include "chomsky3/error.h"
#include <string.h>

#ifndef CHOMSKY3_ERROR_MSG_SIZE
#define CHOMSKY3_ERROR_MSG_SIZE 512
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Legacy internal error type used by older utility sources. */
typedef chomsky3_error_t chomsky3_error_code_t;

/* Compatibility legacy error constants (pre-existing internal names). */
#ifndef CHOMSKY3_ERROR_NOMEM
#define CHOMSKY3_ERROR_NOMEM CHOMSKY3_ERROR_OUT_OF_MEMORY
#endif
#ifndef CHOMSKY3_ERROR_INVALID_ARG
#define CHOMSKY3_ERROR_INVALID_ARG CHOMSKY3_ERROR_INVALID_ARGUMENT
#endif
#ifndef CHOMSKY3_ERROR_PARSE
#define CHOMSKY3_ERROR_PARSE CHOMSKY3_ERROR_PARSE_SYNTAX
#endif
#ifndef CHOMSKY3_ERROR_SYNTAX
#define CHOMSKY3_ERROR_SYNTAX CHOMSKY3_ERROR_PARSE_SYNTAX
#endif
#ifndef CHOMSKY3_ERROR_SEMANTIC
#define CHOMSKY3_ERROR_SEMANTIC CHOMSKY3_ERROR_PARSE_INVALID_GROUP
#endif
#ifndef CHOMSKY3_ERROR_IO
#define CHOMSKY3_ERROR_IO CHOMSKY3_ERROR_IO_GENERIC
#endif
#ifndef CHOMSKY3_ERROR_NOT_FOUND
#define CHOMSKY3_ERROR_NOT_FOUND CHOMSKY3_ERROR_IO_READ
#endif
#ifndef CHOMSKY3_ERROR_EXISTS
#define CHOMSKY3_ERROR_EXISTS CHOMSKY3_ERROR_EXEC_INVALID_STATE
#endif
#ifndef CHOMSKY3_ERROR_OVERFLOW
#define CHOMSKY3_ERROR_OVERFLOW CHOMSKY3_ERROR_BUFFER_TOO_SMALL
#endif
#ifndef CHOMSKY3_ERROR_BUFFER_OVERFLOW
#define CHOMSKY3_ERROR_BUFFER_OVERFLOW CHOMSKY3_ERROR_BUFFER_TOO_SMALL
#endif
#ifndef CHOMSKY3_ERROR_UNDERFLOW
#define CHOMSKY3_ERROR_UNDERFLOW CHOMSKY3_ERROR_RANGE
#endif
#ifndef CHOMSKY3_ERROR_RANGE
#define CHOMSKY3_ERROR_RANGE CHOMSKY3_ERROR_PARSE_INVALID_RANGE
#endif
#ifndef CHOMSKY3_ERROR_STATE
#define CHOMSKY3_ERROR_STATE CHOMSKY3_ERROR_EXEC_INVALID_STATE
#endif
#ifndef CHOMSKY3_ERROR_TIMEOUT
#define CHOMSKY3_ERROR_TIMEOUT CHOMSKY3_ERROR_EXEC_TIMEOUT
#endif
#ifndef CHOMSKY3_ERROR_CANCELLED
#define CHOMSKY3_ERROR_CANCELLED CHOMSKY3_ERROR_INTERNAL
#endif
#ifndef CHOMSKY3_ERROR_UNKNOWN
#define CHOMSKY3_ERROR_UNKNOWN CHOMSKY3_ERROR_MAX
#endif

const char *chomsky3_error_string(chomsky3_error_code_t code);

void chomsky3_set_error_internal(chomsky3_error_code_t code,
                                 const char *file,
                                 int line,
                                 const char *function,
                                 const char *format,
                                 ...);

#define chomsky3_set_error(code, format, ...) \
    chomsky3_set_error_internal((code), __FILE__, __LINE__, __func__, (format), ##__VA_ARGS__)

void chomsky3_check_alloc_fail(const void *ptr, size_t size, const char *file, int line, const char *function);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_UTIL_ERROR_H */
