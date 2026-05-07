/**
 * libchomsky3 - Error Handling Interface
 * 
 * Header file providing error codes, error handling utilities, and diagnostic
 * information for the Chomsky3 regular expression library.
 */

#ifndef CHOMSKY3_ERROR_H
#define CHOMSKY3_ERROR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error code enumeration */
typedef enum {
    CHOMSKY3_OK = 0,                        /* Success */
    
    /* General errors (1-99) */
    CHOMSKY3_ERROR_GENERIC = 1,             /* Generic/unspecified error */
    CHOMSKY3_ERROR_INVALID_ARGUMENT = 2,    /* Invalid function argument */
    CHOMSKY3_ERROR_NULL_POINTER = 3,        /* Unexpected NULL pointer */
    CHOMSKY3_ERROR_OUT_OF_MEMORY = 4,       /* Memory allocation failed */
    CHOMSKY3_ERROR_BUFFER_TOO_SMALL = 5,    /* Provided buffer is too small */
    CHOMSKY3_ERROR_NOT_IMPLEMENTED = 6,     /* Feature not yet implemented */
    CHOMSKY3_ERROR_INTERNAL = 7,            /* Internal library error */
    CHOMSKY3_ERROR_UNSUPPORTED = 8,         /* Unsupported operation or feature */
    
    /* Parsing errors (100-199) */
    CHOMSKY3_ERROR_PARSE_GENERIC = 100,     /* Generic parsing error */
    CHOMSKY3_ERROR_PARSE_SYNTAX = 101,      /* Syntax error in pattern */
    CHOMSKY3_ERROR_PARSE_UNEXPECTED_CHAR = 102, /* Unexpected character */
    CHOMSKY3_ERROR_PARSE_UNEXPECTED_END = 103,  /* Unexpected end of pattern */
    CHOMSKY3_ERROR_PARSE_UNMATCHED_PAREN = 104, /* Unmatched parenthesis */
    CHOMSKY3_ERROR_PARSE_UNMATCHED_BRACKET = 105, /* Unmatched bracket */
    CHOMSKY3_ERROR_PARSE_INVALID_ESCAPE = 106,   /* Invalid escape sequence */
    CHOMSKY3_ERROR_PARSE_INVALID_RANGE = 107,    /* Invalid character range */
    CHOMSKY3_ERROR_PARSE_INVALID_QUANTIFIER = 108, /* Invalid quantifier */
    CHOMSKY3_ERROR_PARSE_INVALID_GROUP = 109,    /* Invalid group construct */
    CHOMSKY3_ERROR_PARSE_INVALID_BACKREF = 110,  /* Invalid backreference */
    CHOMSKY3_ERROR_PARSE_INVALID_CLASS = 111,    /* Invalid character class */
    CHOMSKY3_ERROR_PARSE_EMPTY_GROUP = 112,      /* Empty group not allowed */
    CHOMSKY3_ERROR_PARSE_NESTED_QUANTIFIER = 113, /* Nested quantifiers */
    CHOMSKY3_ERROR_PARSE_INVALID_UNICODE = 114,   /* Invalid Unicode escape */
    CHOMSKY3_ERROR_PARSE_INVALID_PROPERTY = 115,  /* Invalid Unicode property */
    CHOMSKY3_ERROR_PARSE_TOO_COMPLEX = 116,       /* Pattern too complex */
    CHOMSKY3_ERROR_PARSE_RECURSION_LIMIT = 117,   /* Recursion depth exceeded */
    
    /* Compilation errors (200-299) */
    CHOMSKY3_ERROR_COMPILE_GENERIC = 200,   /* Generic compilation error */
    CHOMSKY3_ERROR_COMPILE_TOO_LARGE = 201, /* Compiled pattern too large */
    CHOMSKY3_ERROR_COMPILE_TOO_MANY_CAPTURES = 202, /* Too many capture groups */
    CHOMSKY3_ERROR_COMPILE_INVALID_AST = 203,       /* Invalid AST structure */
    CHOMSKY3_ERROR_COMPILE_OPTIMIZATION_FAILED = 204, /* Optimization pass failed */
    CHOMSKY3_ERROR_COMPILE_CODEGEN_FAILED = 205,    /* Code generation failed */
    
    /* Execution errors (300-399) */
    CHOMSKY3_ERROR_EXEC_GENERIC = 300,      /* Generic execution error */
    CHOMSKY3_ERROR_EXEC_STACK_OVERFLOW = 301, /* Stack overflow during matching */
    CHOMSKY3_ERROR_EXEC_TIMEOUT = 302,      /* Execution timeout */
    CHOMSKY3_ERROR_EXEC_RECURSION_LIMIT = 303, /* Recursion limit exceeded */
    CHOMSKY3_ERROR_EXEC_INVALID_UTF8 = 304, /* Invalid UTF-8 in input */
    CHOMSKY3_ERROR_EXEC_INVALID_STATE = 305, /* Invalid execution state */
    CHOMSKY3_ERROR_EXEC_BACKTRACK_LIMIT = 306, /* Backtracking limit exceeded */
    
    /* JIT errors (400-499) */
    CHOMSKY3_ERROR_JIT_GENERIC = 400,       /* Generic JIT error */
    CHOMSKY3_ERROR_JIT_NOT_AVAILABLE = 401, /* JIT not available on platform */
    CHOMSKY3_ERROR_JIT_COMPILATION_FAILED = 402, /* JIT compilation failed */
    CHOMSKY3_ERROR_JIT_MEMORY_PROTECTION = 403,  /* Memory protection failed */
    CHOMSKY3_ERROR_JIT_UNSUPPORTED_FEATURE = 404, /* Feature not supported in JIT */
    
    /* I/O and serialization errors (500-599) */
    CHOMSKY3_ERROR_IO_GENERIC = 500,        /* Generic I/O error */
    CHOMSKY3_ERROR_IO_READ = 501,           /* Read error */
    CHOMSKY3_ERROR_IO_WRITE = 502,          /* Write error */
    CHOMSKY3_ERROR_IO_INVALID_FORMAT = 503, /* Invalid file format */
    CHOMSKY3_ERROR_IO_VERSION_MISMATCH = 504, /* Version mismatch */
    CHOMSKY3_ERROR_IO_CORRUPTED = 505,      /* Corrupted data */
    
    /* Unicode and encoding errors (600-699) */
    CHOMSKY3_ERROR_UNICODE_GENERIC = 600,   /* Generic Unicode error */
    CHOMSKY3_ERROR_UNICODE_INVALID_CODEPOINT = 601, /* Invalid codepoint */
    CHOMSKY3_ERROR_UNICODE_INVALID_UTF8 = 602,      /* Invalid UTF-8 sequence */
    CHOMSKY3_ERROR_UNICODE_INVALID_UTF16 = 603,     /* Invalid UTF-16 sequence */
    CHOMSKY3_ERROR_UNICODE_INVALID_UTF32 = 604,     /* Invalid UTF-32 sequence */
    CHOMSKY3_ERROR_UNICODE_ENCODING_FAILED = 605,   /* Encoding conversion failed */
    
    /* Resource limit errors (700-799) */
    CHOMSKY3_ERROR_LIMIT_PATTERN_SIZE = 700,    /* Pattern size limit exceeded */
    CHOMSKY3_ERROR_LIMIT_CAPTURE_GROUPS = 701,  /* Too many capture groups */
    CHOMSKY3_ERROR_LIMIT_BACKTRACK = 702,       /* Backtracking limit exceeded */
    CHOMSKY3_ERROR_LIMIT_RECURSION = 703,       /* Recursion limit exceeded */
    CHOMSKY3_ERROR_LIMIT_MEMORY = 704,          /* Memory limit exceeded */
    CHOMSKY3_ERROR_LIMIT_TIME = 705,            /* Time limit exceeded */
    CHOMSKY3_ERROR_LIMIT_COMPLEXITY = 706,      /* Complexity limit exceeded */
    
    CHOMSKY3_ERROR_MAX = 999                /* Maximum error code */
} chomsky3_error_t;

/* Error severity levels */
typedef enum {
    CHOMSKY3_SEVERITY_INFO = 0,     /* Informational message */
    CHOMSKY3_SEVERITY_WARNING = 1,  /* Warning (non-fatal) */
    CHOMSKY3_SEVERITY_ERROR = 2,    /* Error (operation failed) */
    CHOMSKY3_SEVERITY_FATAL = 3     /* Fatal error (unrecoverable) */
} chomsky3_error_severity_t;

/* Error location information */
typedef struct {
    const char *filename;       /* Source filename (can be NULL) */
    size_t line;                /* Line number (0 if unknown) */
    size_t column;              /* Column number (0 if unknown) */
    size_t offset;              /* Byte offset in pattern/input */
    size_t length;              /* Length of problematic region */
} chomsky3_error_location_t;

/* Detailed error information structure */
typedef struct {
    chomsky3_error_t code;              /* Error code */
    chomsky3_error_severity_t severity; /* Error severity */
    const char *message;                /* Human-readable error message */
    chomsky3_error_location_t location; /* Location information */
    const char *context;                /* Context string (e.g., pattern snippet) */
    const char *suggestion;             /* Suggested fix (can be NULL) */
    void *user_data;                    /* User-defined data */
} chomsky3_error_info_t;

/* Error callback function type */
typedef void (*chomsky3_error_callback_t)(
    const chomsky3_error_info_t *error_info,
    void *user_data
);

/**
 * Get a human-readable string for an error code.
 * 
 * @param error The error code.
 * @return A constant string describing the error.
 */
const char *chomsky3_error_string(chomsky3_error_t error);

/**
 * Get a short error name for an error code.
 * 
 * @param error The error code.
 * @return A constant string with the error name (e.g., "PARSE_SYNTAX").
 */
const char *chomsky3_error_name(chomsky3_error_t error);

/**
 * Check if an error code represents success.
 * 
 * @param error The error code to check.
 * @return true if the error code is CHOMSKY3_OK, false otherwise.
 */
static inline bool chomsky3_error_is_ok(chomsky3_error_t error) {
    return error == CHOMSKY3_OK;
}

/**
 * Check if an error code represents a parsing error.
 * 
 * @param error The error code to check.
 * @return true if the error is a parsing error, false otherwise.
 */
static inline bool chomsky3_error_is_parse_error(chomsky3_error_t error) {
    return error >= CHOMSKY3_ERROR_PARSE_GENERIC && error < CHOMSKY3_ERROR_COMPILE_GENERIC;
}

/**
 * Check if an error code represents a compilation error.
 * 
 * @param error The error code to check.
 * @return true if the error is a compilation error, false otherwise.
 */
static inline bool chomsky3_error_is_compile_error(chomsky3_error_t error) {
    return error >= CHOMSKY3_ERROR_COMPILE_GENERIC && error < CHOMSKY3_ERROR_EXEC_GENERIC;
}

/**
 * Check if an error code represents an execution error.
 * 
 * @param error The error code to check.
 * @return true if the error is an execution error, false otherwise.
 */
static inline bool chomsky3_error_is_exec_error(chomsky3_error_t error) {
    return error >= CHOMSKY3_ERROR_EXEC_GENERIC && error < CHOMSKY3_ERROR_JIT_GENERIC;
}

/**
 * Initialize an error info structure with default values.
 * 
 * @param info The error info structure to initialize.
 */
void chomsky3_error_info_init(chomsky3_error_info_t *info);

/**
 * Create a detailed error info structure.
 * 
 * @param code The error code.
 * @param message Custom error message (can be NULL to use default).
 * @param location Location information (can be NULL).
 * @return Allocated error info structure, or NULL on allocation failure.
 */
chomsky3_error_info_t *chomsky3_error_info_create(
    chomsky3_error_t code,
    const char *message,
    const chomsky3_error_location_t *location
);

/**
 * Free an error info structure.
 * 
 * @param info The error info structure to free.
 */
void chomsky3_error_info_free(chomsky3_error_info_t *info);

/**
 * Set a custom error message for an error info structure.
 * 
 * @param info The error info structure.
 * @param message The custom message (will be copied).
 * @return CHOMSKY3_OK on success, error code on failure.
 */
chomsky3_error_t chomsky3_error_info_set_message(
    chomsky3_error_info_t *info,
    const char *message
);

/**
 * Set context information for an error.
 * 
 * @param info The error info structure.
 * @param context Context string (e.g., pattern snippet, will be copied).
 * @return CHOMSKY3_OK on success, error code on failure.
 */
chomsky3_error_t chomsky3_error_info_set_context(
    chomsky3_error_info_t *info,
    const char *context
);

/**
 * Set a suggestion for fixing the error.
 * 
 * @param info The error info structure.
 * @param suggestion Suggestion string (will be copied).
 * @return CHOMSKY3_OK on success, error code on failure.
 */
chomsky3_error_t chomsky3_error_info_set_suggestion(
    chomsky3_error_info_t *info,
    const char *suggestion
);

/**
 * Format an error message with location information.
 * 
 * @param info The error info structure.
 * @param buffer Buffer to write the formatted message.
 * @param buffer_size Size of the buffer.
 * @return Number of characters written (excluding null terminator).
 */
size_t chomsky3_error_format(
    const chomsky3_error_info_t *info,
    char *buffer,
    size_t buffer_size
);

/**
 * Print an error message to a file stream.
 * 
 * @param info The error info structure.
 * @param stream The output stream (e.g., stderr).
 */
void chomsky3_error_print(
    const chomsky3_error_info_t *info,
    FILE *stream
);

/**
 * Print an error message with context highlighting to a file stream.
 * 
 * This function prints the error with visual indicators showing where
 * in the pattern or input the error occurred.
 * 
 * @param info The error info structure.
 * @param pattern The full pattern or input string.
 * @param stream The output stream (e.g., stderr).
 */
void chomsky3_error_print_with_context(
    const chomsky3_error_info_t *info,
    const char *pattern,
    FILE *stream
);

/**
 * Set a global error callback function.
 * 
 * The callback will be invoked whenever an error occurs in the library.
 * 
 * @param callback The callback function (NULL to disable).
 * @param user_data User data to pass to the callback.
 */
void chomsky3_set_error_callback(
    chomsky3_error_callback_t callback,
    void *user_data
);

/**
 * Get the last error that occurred in the current thread.
 * 
 * @return Pointer to the last error info, or NULL if no error occurred.
 */
const chomsky3_error_info_t *chomsky3_get_last_error(void);

/**
 * Clear the last error for the current thread.
 */
void chomsky3_clear_last_error(void);

/**
 * Set the last error for the current thread.
 * 
 * This is typically used internally by the library.
 * 
 * @param info The error info to set (will be copied).
 */
void chomsky3_set_last_error(const chomsky3_error_info_t *info);

/**
 * Create an error location structure.
 * 
 * @param offset Byte offset in the pattern/input.
 * @param length Length of the problematic region.
 * @param line Line number (0 if unknown).
 * @param column Column number (0 if unknown).
 * @return Initialized error location structure.
 */
chomsky3_error_location_t chomsky3_error_location_create(
    size_t offset,
    size_t length,
    size_t line,
    size_t column
);

/**
 * Calculate line and column from byte offset in a string.
 * 
 * @param str The input string.
 * @param offset Byte offset in the string.
 * @param line Output parameter for line number (1-based).
 * @param column Output parameter for column number (1-based).
 */
void chomsky3_error_calculate_position(
    const char *str,
    size_t offset,
    size_t *line,
    size_t *column
);

/* Convenience macros for error handling */

/**
 * Return if the error code is not OK.
 */
#define CHOMSKY3_RETURN_IF_ERROR(expr) \
    do { \
        chomsky3_error_t _err = (expr); \
        if (_err != CHOMSKY3_OK) { \
            return _err; \
        } \
    } while (0)

/**
 * Go to a label if the error code is not OK.
 */
#define CHOMSKY3_GOTO_IF_ERROR(expr, label) \
    do { \
        chomsky3_error_t _err = (expr); \
        if (_err != CHOMSKY3_OK) { \
            goto label; \
        } \
    } while (0)

/**
 * Set error and return if condition is true.
 */
#define CHOMSKY3_CHECK(cond, error_code) \
    do { \
        if (!(cond)) { \
            return (error_code); \
        } \
    } while (0)

/**
 * Check for NULL pointer and return error if NULL.
 */
#define CHOMSKY3_CHECK_NULL(ptr) \
    CHOMSKY3_CHECK((ptr) != NULL, CHOMSKY3_ERROR_NULL_POINTER)

/* Legacy compact aliases used across the source tree */
#ifndef CHOMSKY3_ERR_OK
#define CHOMSKY3_ERR_OK CHOMSKY3_OK
#endif
#ifndef CHOMSKY3_ERR_INVALID_ARGUMENT
#define CHOMSKY3_ERR_INVALID_ARGUMENT CHOMSKY3_ERROR_INVALID_ARGUMENT
#endif
#ifndef CHOMSKY3_ERR_OUT_OF_MEMORY
#define CHOMSKY3_ERR_OUT_OF_MEMORY CHOMSKY3_ERROR_OUT_OF_MEMORY
#endif
#ifndef CHOMSKY3_ERR_BUFFER_TOO_SMALL
#define CHOMSKY3_ERR_BUFFER_TOO_SMALL CHOMSKY3_ERROR_BUFFER_TOO_SMALL
#endif
#ifndef CHOMSKY3_ERR_NOT_IMPLEMENTED
#define CHOMSKY3_ERR_NOT_IMPLEMENTED CHOMSKY3_ERROR_NOT_IMPLEMENTED
#endif
#ifndef CHOMSKY3_ERR_INTERNAL
#define CHOMSKY3_ERR_INTERNAL CHOMSKY3_ERROR_INTERNAL
#endif
#ifndef CHOMSKY3_ERR_UNSUPPORTED
#define CHOMSKY3_ERR_UNSUPPORTED CHOMSKY3_ERROR_UNSUPPORTED
#endif
#ifndef CHOMSKY3_ERR_INVALID_STATE
#define CHOMSKY3_ERR_INVALID_STATE CHOMSKY3_ERROR_EXEC_INVALID_STATE
#endif
#ifndef CHOMSKY3_ERR_INVALID_FORMAT
#define CHOMSKY3_ERR_INVALID_FORMAT CHOMSKY3_ERROR_IO_INVALID_FORMAT
#endif
#ifndef CHOMSKY3_ERR_UNSUPPORTED_VERSION
#define CHOMSKY3_ERR_UNSUPPORTED_VERSION CHOMSKY3_ERROR_IO_VERSION_MISMATCH
#endif
#ifndef CHOMSKY3_ERR_CHECKSUM_MISMATCH
#define CHOMSKY3_ERR_CHECKSUM_MISMATCH CHOMSKY3_ERROR_IO_CORRUPTED
#endif
#ifndef CHOMSKY3_ERR_IO_ERROR
#define CHOMSKY3_ERR_IO_ERROR CHOMSKY3_ERROR_IO_GENERIC
#endif
#ifndef CHOMSKY3_ERR_RESOURCE_LIMIT
#define CHOMSKY3_ERR_RESOURCE_LIMIT CHOMSKY3_ERROR_LIMIT_COMPLEXITY
#endif
#ifndef CHOMSKY3_ERR_COMPILATION_FAILED
#define CHOMSKY3_ERR_COMPILATION_FAILED CHOMSKY3_ERROR_COMPILE_CODEGEN_FAILED
#endif
#ifndef CHOMSKY3_ERR_INVALID_BYTECODE
#define CHOMSKY3_ERR_INVALID_BYTECODE CHOMSKY3_ERROR_EXEC_INVALID_UTF8
#endif
#ifndef CHOMSKY3_ERR_NO_MATCH
#define CHOMSKY3_ERR_NO_MATCH CHOMSKY3_ERROR_EXEC_GENERIC
#endif
#ifndef CHOMSKY3_ERR_UNSUPPORTED_COMPRESSION
#define CHOMSKY3_ERR_UNSUPPORTED_COMPRESSION CHOMSKY3_ERROR_INVALID_ARGUMENT
#endif

/* Additional legacy top-level error names */
#ifndef CHOMSKY3_ERROR_COMPILATION_FAILED
#define CHOMSKY3_ERROR_COMPILATION_FAILED CHOMSKY3_ERROR_COMPILE_CODEGEN_FAILED
#endif
#ifndef CHOMSKY3_ERROR_RESOURCE_LIMIT
#define CHOMSKY3_ERROR_RESOURCE_LIMIT CHOMSKY3_ERROR_LIMIT_COMPLEXITY
#endif

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_ERROR_H */
