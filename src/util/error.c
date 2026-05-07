#include "chomsky3/util/error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const k_default_error_message = "Success";

static char g_last_error_message[CHOMSKY3_ERROR_MSG_SIZE] = "Success";
static char g_last_error_context[CHOMSKY3_ERROR_MSG_SIZE] = "";
static char g_last_error_suggestion[CHOMSKY3_ERROR_MSG_SIZE] = "";

static chomsky3_error_info_t g_last_error = {
    .code = CHOMSKY3_OK,
    .severity = CHOMSKY3_SEVERITY_INFO,
    .message = g_last_error_message,
    .location = {NULL, 0, 0, 0, 0},
    .context = NULL,
    .suggestion = NULL,
    .user_data = NULL
};

static chomsky3_error_severity_t g_log_level = CHOMSKY3_SEVERITY_WARNING;
static chomsky3_error_callback_t g_error_callback = NULL;
static void *g_error_callback_data = NULL;
static FILE *g_log_file = NULL;

static const char *const severity_strings[] = {
    [CHOMSKY3_SEVERITY_INFO] = "INFO",
    [CHOMSKY3_SEVERITY_WARNING] = "WARN",
    [CHOMSKY3_SEVERITY_ERROR] = "ERROR",
    [CHOMSKY3_SEVERITY_FATAL] = "FATAL"
};

void chomsky3_log(chomsky3_error_severity_t level, const char *format, ...);

static void copy_error_text(char *dest, size_t size, const char *src)
{
    if (!dest || size == 0) {
        return;
    }

    if (src && src[0] != '\0') {
        snprintf(dest, size, "%s", src);
    } else {
        dest[0] = '\0';
    }
}

static void copy_source_location(chomsky3_error_location_t *dst,
                                const char *file,
                                int line,
                                const char *function)
{
    if (!dst) {
        return;
    }

    dst->filename = file;
    dst->line = (line > 0) ? (size_t)line : 0;
    dst->column = 0;
    dst->offset = 0;
    dst->length = 0;
    (void)function;
}

/* Keep this for compatibility with legacy callers. */
const char *chomsky3_error_string(chomsky3_error_code_t code)
{
    switch (code) {
        case CHOMSKY3_OK:
            return k_default_error_message;
        case CHOMSKY3_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case CHOMSKY3_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case CHOMSKY3_ERROR_NULL_POINTER:
            return "Unexpected NULL pointer";
        case CHOMSKY3_ERROR_BUFFER_TOO_SMALL:
            return "Provided buffer is too small";
        case CHOMSKY3_ERROR_NOT_IMPLEMENTED:
            return "Feature not yet implemented";
        case CHOMSKY3_ERROR_UNSUPPORTED:
            return "Unsupported operation or feature";
        case CHOMSKY3_ERROR_INTERNAL:
            return "Internal library error";
        case CHOMSKY3_ERROR_PARSE_SYNTAX:
            return "Syntax error";
        case CHOMSKY3_ERROR_PARSE_INVALID_RANGE:
            return "Out of range";
        case CHOMSKY3_ERROR_EXEC_INVALID_STATE:
            return "Invalid state";
        case CHOMSKY3_ERROR_EXEC_TIMEOUT:
            return "Operation timed out";
        case CHOMSKY3_ERROR_GENERIC:
            return "Generic/unspecified error";
        default:
            if (code > CHOMSKY3_ERROR_MAX) {
                return "Unknown error";
            }
            return "Unknown error";
    }
}

static void set_last_error_from_internal(chomsky3_error_code_t code,
                                        const char *file,
                                        int line,
                                        const char *function,
                                        const char *message)
{
    g_last_error.code = code;
    g_last_error.severity = CHOMSKY3_SEVERITY_ERROR;
    copy_error_text(g_last_error_message, sizeof(g_last_error_message), message);
    g_last_error.message = g_last_error_message;
    copy_source_location(&g_last_error.location, file, line, function);

    if (function && function[0] != '\0') {
        copy_error_text(g_last_error_context, sizeof(g_last_error_context), function);
        g_last_error.context = g_last_error_context;
    } else {
        g_last_error.context = NULL;
    }

    g_last_error.suggestion = NULL;
}

void chomsky3_set_error_internal(chomsky3_error_code_t code,
                                 const char *file,
                                 int line,
                                 const char *function,
                                 const char *format,
                                 ...)
{
    char message[CHOMSKY3_ERROR_MSG_SIZE];
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
    } else {
        copy_error_text(message, sizeof(message), chomsky3_error_string(code));
    }

    set_last_error_from_internal(code, file, line, function, message);

    if (g_error_callback) {
        g_error_callback(&g_last_error, g_error_callback_data);
    }

    chomsky3_log(CHOMSKY3_SEVERITY_ERROR,
                 "%s:%zu: %s",
                 file ? file : "unknown",
                 g_last_error.location.line,
                 message);
}

const chomsky3_error_info_t *chomsky3_get_last_error(void)
{
    return &g_last_error;
}

void chomsky3_clear_last_error(void)
{
    g_last_error.code = CHOMSKY3_OK;
    g_last_error.severity = CHOMSKY3_SEVERITY_INFO;
    copy_error_text(g_last_error_message, sizeof(g_last_error_message), "Success");
    g_last_error.message = g_last_error_message;
    g_last_error.location = (chomsky3_error_location_t){NULL, 0, 0, 0, 0};
    g_last_error.context = NULL;
    g_last_error.suggestion = NULL;
    g_last_error.user_data = NULL;
}

void chomsky3_set_last_error(const chomsky3_error_info_t *info)
{
    if (!info) {
        chomsky3_clear_last_error();
        return;
    }

    g_last_error.code = info->code;
    g_last_error.severity = info->severity;
    copy_error_text(g_last_error_message,
                    sizeof(g_last_error_message),
                    info->message ? info->message : k_default_error_message);
    g_last_error.message = g_last_error_message;
    g_last_error.location = info->location;
    copy_error_text(g_last_error_context,
                    sizeof(g_last_error_context),
                    info->context);
    g_last_error.context = g_last_error_context[0] != '\0' ? g_last_error_context : NULL;
    copy_error_text(g_last_error_suggestion,
                    sizeof(g_last_error_suggestion),
                    info->suggestion);
    g_last_error.suggestion = g_last_error_suggestion[0] != '\0'
                                ? g_last_error_suggestion
                                : NULL;
    g_last_error.user_data = info->user_data;
}

void chomsky3_set_error_callback(chomsky3_error_callback_t callback,
                                void *user_data)
{
    g_error_callback = callback;
    g_error_callback_data = user_data;
}

void chomsky3_set_error_handler(chomsky3_error_callback_t handler, void *user_data)
{
    chomsky3_set_error_callback(handler, user_data);
}

void chomsky3_set_log_level(chomsky3_error_severity_t level)
{
    if (level < CHOMSKY3_SEVERITY_INFO || level > CHOMSKY3_SEVERITY_FATAL) {
        return;
    }
    g_log_level = level;
}

chomsky3_error_severity_t chomsky3_get_log_level(void)
{
    return g_log_level;
}

int chomsky3_set_log_file(const char *path)
{
    if (g_log_file && g_log_file != stderr) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    if (!path || path[0] == '\0') {
        g_log_file = stderr;
        return 0;
    }

    g_log_file = fopen(path, "a");
    if (!g_log_file) {
        g_log_file = stderr;
        return -1;
    }

    return 0;
}

void chomsky3_close_log_file(void)
{
    if (g_log_file && g_log_file != stderr) {
        fclose(g_log_file);
    }
    g_log_file = stderr;
}

void chomsky3_log(chomsky3_error_severity_t level, const char *format, ...)
{
    if (level < g_log_level) {
        return;
    }
    if (!format) {
        return;
    }

    FILE *out = g_log_file ? g_log_file : stderr;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32] = "unknown-time";
    if (tm_info) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    const char *level_str =
        (level >= CHOMSKY3_SEVERITY_INFO && level <= CHOMSKY3_SEVERITY_FATAL)
            ? severity_strings[level]
            : "UNKNOWN";

    fprintf(out, "[%s] [%s] ", timestamp, level_str);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
    fprintf(out, "\n");
    fflush(out);
}

char *chomsky3_format_error(const chomsky3_error_info_t *error)
{
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
             "Severity: %s\n"
             "Message: %s\n"
             "Location: %s:%zu",
             chomsky3_error_string(error->code),
             (int)error->code,
             severity_strings[error->severity],
             error->message ? error->message : k_default_error_message,
             error->location.filename ? error->location.filename : "unknown",
             error->location.line);

    if (error->context && error->context[0] != '\0') {
        size_t used = strlen(buffer);
        snprintf(buffer + used, size - used, "\nContext: %s", error->context);
    }

    if (error->suggestion && error->suggestion[0] != '\0') {
        size_t used = strlen(buffer);
        snprintf(buffer + used, size - used, "\nSuggestion: %s", error->suggestion);
    }

    return buffer;
}

void chomsky3_print_error(FILE *stream, const chomsky3_error_info_t *error)
{
    if (!error || error->code == CHOMSKY3_OK) {
        return;
    }

    if (!stream) {
        stream = stderr;
    }

    char *formatted = chomsky3_format_error(error);
    if (formatted) {
        fprintf(stream, "%s\n", formatted);
        free(formatted);
    }
}

void chomsky3_assert_internal(int condition,
                              const char *condition_str,
                              const char *file,
                              int line,
                              const char *function,
                              const char *format, ...)
{
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

void *chomsky3_check_alloc_internal(void *ptr,
                                    const char *file,
                                    int line,
                                    const char *function)
{
    if (!ptr) {
        chomsky3_set_error_internal(CHOMSKY3_ERROR_NOMEM,
                                    file, line, function,
                                    "Memory allocation failed");
        return NULL;
    }

    return ptr;
}

void chomsky3_check_alloc_fail(const void *ptr,
                               size_t size,
                               const char *file,
                               int line,
                               const char *function)
{
    (void)size;
    if (!ptr) {
        chomsky3_set_error_internal(CHOMSKY3_ERROR_NOMEM,
                                    file, line, function,
                                    "Memory allocation failed");
    }
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

void chomsky3_push_diag_context(const char *context, const char *file, int line)
{
    if (g_diag_stack_depth < MAX_DIAG_STACK) {
        g_diag_stack[g_diag_stack_depth].context = context;
        g_diag_stack[g_diag_stack_depth].file = file;
        g_diag_stack[g_diag_stack_depth].line = line;
        g_diag_stack_depth++;
    }
}

void chomsky3_pop_diag_context(void)
{
    if (g_diag_stack_depth > 0) {
        g_diag_stack_depth--;
    }
}

void chomsky3_print_diag_trace(FILE *stream)
{
    if (!stream || g_diag_stack_depth == 0) {
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

typedef struct error_checkpoint {
    chomsky3_error_code_t saved_code;
    char saved_message[CHOMSKY3_ERROR_MSG_SIZE];
    int diag_depth;
} error_checkpoint_t;

void *chomsky3_create_error_checkpoint(void)
{
    error_checkpoint_t *cp = malloc(sizeof(error_checkpoint_t));
    if (!cp) {
        return NULL;
    }

    cp->saved_code = g_last_error.code;
    copy_error_text(cp->saved_message,
                    sizeof(cp->saved_message),
                    g_last_error.message ? g_last_error.message : "");
    cp->diag_depth = g_diag_stack_depth;

    return cp;
}

void chomsky3_restore_error_checkpoint(void *checkpoint)
{
    if (!checkpoint) {
        return;
    }

    error_checkpoint_t *cp = (error_checkpoint_t *)checkpoint;
    set_last_error_from_internal(cp->saved_code, "unknown", 0, "checkpoint", cp->saved_message);
    g_diag_stack_depth = cp->diag_depth;
}

void chomsky3_free_error_checkpoint(void *checkpoint)
{
    free(checkpoint);
}

/* Compatibility wrappers retained for older call sites. */
void chomsky3_clear_error(void)
{
    chomsky3_clear_last_error();
}
