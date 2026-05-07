#include "chomsky3/util/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* Global error state */
static chomsky3_error_t g_last_error = {
    .code = CHOMSKY3_OK,
    .message = {0},
    .file = NULL,
    .line = 0,
    .function = NULL
};

static chomsky3_log_level_t g_log_level = CHOMSKY3_LOG_WARN;
static chomsky3_error_handler_t g_error_handler = NULL;
static void *g_error_handler_data = NULL;
static FILE *g_log_file = NULL;

/* Error code to string mapping */
static const char *error_strings[] = {
    [CHOMSKY3_OK] = "Success",
    [CHOMSKY3_ERROR_NOMEM] = "Out of memory",
    [CHOMSKY3_ERROR_INVALID_ARG] = "Invalid argument",
    [CHOMSKY3_ERROR_PARSE] = "Parse error",
    [CHOMSKY3_ERROR_SYNTAX] = "Syntax error",
    [CHOMSKY3_ERROR_SEMANTIC] = "Semantic error",
    [CHOMSKY3_ERROR_IO] = "I/O error",
    [CHOMSKY3_ERROR_NOT_FOUND] = "Not found",
    [CHOMSKY3_ERROR_EXISTS] = "Already exists",
    [CHOMSKY3_ERROR_OVERFLOW] = "Buffer overflow",
    [CHOMSKY3_ERROR_UNDERFLOW] = "Buffer underflow",
    [CHOMSKY3_ERROR_RANGE] = "Out of range",
    [CHOMSKY3_ERROR_STATE] = "Invalid state",
    [CHOMSKY3_ERROR_UNSUPPORTED] = "Unsupported operation",
    [CHOMSKY3_ERROR_INTERNAL] = "Internal error",
    [CHOMSKY3_ERROR_TIMEOUT] = "Operation timed out",
    [CHOMSKY3_ERROR_CANCELLED] = "Operation cancelled",
    [CHOMSKY3_ERROR_UNKNOWN] = "Unknown error"
};

/* Log level to string mapping */
static const char *log_level_strings[] = {
    [CHOMSKY3_LOG_TRACE] = "TRACE",
    [CHOMSKY3_LOG_DEBUG] = "DEBUG",
    [CHOMSKY3_LOG_INFO] = "INFO",
    [CHOMSKY3_LOG_WARN] = "WARN",
    [CHOMSKY3_LOG_ERROR] = "ERROR",
    [CHOMSKY3_LOG_FATAL] = "FATAL"
};

/* Get error code string */
const char *chomsky3_error_string(chomsky3_error_code_t code) {
    if (code < 0 || code >= CHOMSKY3_ERROR_UNKNOWN) {
        return error_strings[CHOMSKY3_ERROR_UNKNOWN];
    }
    return error_strings[code];
}

/* Set error with context */
void chomsky3_set_error_internal(chomsky3_error_code_t code,
                                  const char *file,
                                  int line,
                                  const char *function,
                                  const char *format, ...) {
    g_last_error.code = code;
    g_last_error.file = file;
    g_last_error.line = line;
    g_last_error.function = function;

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_last_error.message, CHOMSKY3_ERROR_MSG_SIZE, format, args);
        va_end(args);
    } else {
        snprintf(g_last_error.message, CHOMSKY3_ERROR_MSG_SIZE, "%s",
                 chomsky3_error_string(code));
    }

    /* Call custom error handler if set */
    if (g_error_handler) {
        g_error_handler(&g_last_error, g_error_handler_data);
    }

    /* Log error */
    chomsky3_log(CHOMSKY3_LOG_ERROR, "%s:%d: %s: %s",
                 file ? file : "unknown",
                 line,
                 function ? function : "unknown",
                 g_last_error.message);
}

/* Get last error */
const chomsky3_error_t *chomsky3_get_last_error(void) {
    return &g_last_error;
}

/* Clear last error */
void chomsky3_clear_error(void) {
    g_last_error.code = CHOMSKY3_OK;
    g_last_error.message[0] = '\0';
    g_last_error.file = NULL;
    g_last_error.line = 0;
    g_last_error.function = NULL;
}

/* Set custom error handler */
void chomsky3_set_error_handler(chomsky3_error_handler_t handler, void *user_data) {
    g_error_handler = handler;
    g_error_handler_data = user_data;
}

/* Set log level */
void chomsky3_set_log_level(chomsky3_log_level_t level) {
    g_log_level = level;
}

/* Get log level */
chomsky3_log_level_t chomsky3_get_log_level(void) {
    return g_log_level;
}

/* Set log file */
int chomsky3_set_log_file(const char *path) {
    if (g_log_file && g_log_file != stderr) {
        fclose(g_log_file);
    }

    if (!path) {
        g_log_file = NULL;
        return 0;
    }

    g_log_file = fopen(path, "a");
    if (!g_log_file) {
        g_log_file = stderr;
        return -1;
    }

    return 0;
}

/* Close log file */
void chomsky3_close_log_file(void) {
    if (g_log_file && g_log_file != stderr) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

/* Log message */
void chomsky3_log(chomsky3_log_level_t level, const char *format, ...) {
    if (level < g_log_level) {
        return;
    }

    FILE *out = g_log_file ? g_log_file : stderr;
    
    /* Timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Log level */
    const char *level_str = (level >= 0 && level <= CHOMSKY3_LOG_FATAL)
                            ? log_level_strings[level]
                            : "UNKNOWN";

    /* Print header */
    fprintf(out, "[%s] [%s] ", timestamp, level_str);

    /* Print message */
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);
}

/* Format error message */
char *chomsky3_format_error(const chomsky3_error_t *error) {
    if (!error || error->code == CHOMSKY3_OK) {
        return NULL;
    }

    size_t size = 512;
    char *buffer = malloc(size);
    if (!buffer) {
        return NULL;
    }

    snprintf(buffer, size,
             "Error: %s (%d)\n"
             "Message: %s\n"
             "Location: %s:%d in %s",
             chomsky3_error_string(error->code),
             error->code,
             error->message,
             error->file ? error->file : "unknown",
             error->line,
             error->function ? error->function : "unknown");

    return buffer;
}

/* Print error to stream */
void chomsky3_print_error(FILE *stream, const chomsky3_error_t *error) {
    if (!error || error->code == CHOMSKY3_OK) {
        return;
    }

    char *formatted = chomsky3_format_error(error);
    if (formatted) {
        fprintf(stream, "%s\n", formatted);
        free(formatted);
    }
}

/* Assert with message */
void chomsky3_assert_internal(int condition,
                              const char *condition_str,
                              const char *file,
                              int line,
                              const char *function,
                              const char *format, ...) {
    if (condition) {
        return;
    }

    fprintf(stderr, "Assertion failed: %s\n", condition_str);
    fprintf(stderr, "Location: %s:%d in %s\n",
            file ? file : "unknown",
            line,
            function ? function : "unknown");

    if (format) {
        fprintf(stderr, "Message: ");
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, "\n");
    }

    abort();
}

/* Check allocation */
void *chomsky3_check_alloc_internal(void *ptr,
                                     const char *file,
                                     int line,
                                     const char *function) {
    if (!ptr) {
        chomsky3_set_error_internal(CHOMSKY3_ERROR_NOMEM,
                                     file, line, function,
                                     "Memory allocation failed");
        return NULL;
    }
    return ptr;
}

/* Diagnostic context stack */
#define MAX_DIAG_STACK 32

typedef struct {
    const char *context;
    const char *file;
    int line;
} diag_frame_t;

static diag_frame_t g_diag_stack[MAX_DIAG_STACK];
static int g_diag_stack_depth = 0;

/* Push diagnostic context */
void chomsky3_push_diag_context(const char *context, const char *file, int line) {
    if (g_diag_stack_depth < MAX_DIAG_STACK) {
        g_diag_stack[g_diag_stack_depth].context = context;
        g_diag_stack[g_diag_stack_depth].file = file;
        g_diag_stack[g_diag_stack_depth].line = line;
        g_diag_stack_depth++;
    }
}

/* Pop diagnostic context */
void chomsky3_pop_diag_context(void) {
    if (g_diag_stack_depth > 0) {
        g_diag_stack_depth--;
    }
}

/* Print diagnostic stack trace */
void chomsky3_print_diag_trace(FILE *stream) {
    if (g_diag_stack_depth == 0) {
        return;
    }

    fprintf(stream, "Diagnostic trace:\n");
    for (int i = g_diag_stack_depth - 1; i >= 0; i--) {
        fprintf(stream, "  [%d] %s at %s:%d\n",
                g_diag_stack_depth - i - 1,
                g_diag_stack[i].context,
                g_diag_stack[i].file ? g_diag_stack[i].file : "unknown",
                g_diag_stack[i].line);
    }
}

/* Error recovery checkpoint */
typedef struct error_checkpoint {
    chomsky3_error_code_t saved_code;
    char saved_message[CHOMSKY3_ERROR_MSG_SIZE];
    int diag_depth;
} error_checkpoint_t;

/* Create error checkpoint */
void *chomsky3_create_error_checkpoint(void) {
    error_checkpoint_t *cp = malloc(sizeof(error_checkpoint_t));
    if (!cp) {
        return NULL;
    }

    cp->saved_code = g_last_error.code;
    strncpy(cp->saved_message, g_last_error.message, CHOMSKY3_ERROR_MSG_SIZE);
    cp->diag_depth = g_diag_stack_depth;

    return cp;
}

/* Restore error checkpoint */
void chomsky3_restore_error_checkpoint(void *checkpoint) {
    if (!checkpoint) {
        return;
    }

    error_checkpoint_t *cp = (error_checkpoint_t *)checkpoint;
    g_last_error.code = cp->saved_code;
    strncpy(g_last_error.message, cp->saved_message, CHOMSKY3_ERROR_MSG_SIZE);
    g_diag_stack_depth = cp->diag_depth;
}

/* Free error checkpoint */
void chomsky3_free_error_checkpoint(void *checkpoint) {
    free(checkpoint);
}
