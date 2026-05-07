#include "chomsky3/util/debug.h"
#include "chomsky3/util/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* Debug configuration */
static int g_debug_enabled = 0;
static chomsky3_debug_level_t g_debug_level = CHOMSKY3_DEBUG_INFO;
static FILE *g_debug_file = NULL;
static int g_debug_indent = 0;
static int g_debug_show_timestamp = 1;
static int g_debug_show_location = 1;

/* Debug level strings */
static const char *debug_level_strings[] = {
    [CHOMSKY3_DEBUG_TRACE] = "TRACE",
    [CHOMSKY3_DEBUG_DEBUG] = "DEBUG",
    [CHOMSKY3_DEBUG_INFO] = "INFO",
    [CHOMSKY3_DEBUG_WARN] = "WARN",
    [CHOMSKY3_DEBUG_ERROR] = "ERROR"
};

/* Enable/disable debugging */
void chomsky3_debug_enable(int enable) {
    g_debug_enabled = enable;
}

/* Check if debugging is enabled */
int chomsky3_debug_is_enabled(void) {
    return g_debug_enabled;
}

/* Set debug level */
void chomsky3_debug_set_level(chomsky3_debug_level_t level) {
    g_debug_level = level;
}

/* Get debug level */
chomsky3_debug_level_t chomsky3_debug_get_level(void) {
    return g_debug_level;
}

/* Set debug output file */
int chomsky3_debug_set_file(const char *path) {
    if (g_debug_file && g_debug_file != stderr) {
        fclose(g_debug_file);
    }

    if (!path) {
        g_debug_file = NULL;
        return 0;
    }

    g_debug_file = fopen(path, "a");
    if (!g_debug_file) {
        g_debug_file = stderr;
        return -1;
    }

    return 0;
}

/* Close debug file */
void chomsky3_debug_close_file(void) {
    if (g_debug_file && g_debug_file != stderr) {
        fclose(g_debug_file);
        g_debug_file = NULL;
    }
}

/* Set debug options */
void chomsky3_debug_set_options(int show_timestamp, int show_location) {
    g_debug_show_timestamp = show_timestamp;
    g_debug_show_location = show_location;
}

/* Print debug message (internal) */
void chomsky3_debug_print_internal(chomsky3_debug_level_t level,
                                    const char *file,
                                    int line,
                                    const char *function,
                                    const char *format,
                                    ...) {
    if (!g_debug_enabled || level < g_debug_level) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;

    /* Timestamp */
    if (g_debug_show_timestamp) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
        fprintf(out, "[%s] ", timestamp);
    }

    /* Debug level */
    const char *level_str = (level >= 0 && level <= CHOMSKY3_DEBUG_ERROR)
                            ? debug_level_strings[level]
                            : "UNKNOWN";
    fprintf(out, "[%s] ", level_str);

    /* Indentation */
    for (int i = 0; i < g_debug_indent; i++) {
        fprintf(out, "  ");
    }

    /* Message */
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    /* Location */
    if (g_debug_show_location && file) {
        fprintf(out, " (%s:%d", file, line);
        if (function) {
            fprintf(out, " in %s", function);
        }
        fprintf(out, ")");
    }

    fprintf(out, "\n");
    fflush(out);
}

/* Increase debug indent */
void chomsky3_debug_indent(void) {
    g_debug_indent++;
}

/* Decrease debug indent */
void chomsky3_debug_unindent(void) {
    if (g_debug_indent > 0) {
        g_debug_indent--;
    }
}

/* Reset debug indent */
void chomsky3_debug_reset_indent(void) {
    g_debug_indent = 0;
}

/* Print memory dump */
void chomsky3_debug_dump_memory(const void *ptr, size_t size, const char *label) {
    if (!g_debug_enabled) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;
    const unsigned char *bytes = (const unsigned char *)ptr;

    fprintf(out, "Memory dump: %s (%zu bytes at %p)\n", 
            label ? label : "unknown", size, ptr);

    for (size_t i = 0; i < size; i += 16) {
        fprintf(out, "%08zx: ", i);

        /* Hex bytes */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                fprintf(out, "%02x ", bytes[i + j]);
            } else {
                fprintf(out, "   ");
            }
            if (j == 7) {
                fprintf(out, " ");
            }
        }

        fprintf(out, " |");

        /* ASCII representation */
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = bytes[i + j];
            fprintf(out, "%c", (c >= 32 && c < 127) ? c : '.');
        }

        fprintf(out, "|\n");
    }

    fflush(out);
}

/* Print AST node (recursive) */
static void print_ast_node_recursive(const chomsky3_ast_node_t *node, int depth) {
    if (!node) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;

    /* Indentation */
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }

    /* Node type */
    fprintf(out, "<%s>", chomsky3_ast_node_type_string(node->type));

    /* Node value if present */
    if (node->value) {
        fprintf(out, " \"%s\"", node->value);
    }

    /* Position info */
    fprintf(out, " [%d:%d-%d:%d]",
            node->start_line, node->start_col,
            node->end_line, node->end_col);

    fprintf(out, "\n");

    /* Children */
    for (size_t i = 0; i < node->num_children; i++) {
        print_ast_node_recursive(node->children[i], depth + 1);
    }
}

/* Print AST tree */
void chomsky3_debug_print_ast(const chomsky3_ast_node_t *root, const char *label) {
    if (!g_debug_enabled) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;

    fprintf(out, "AST dump: %s\n", label ? label : "unknown");
    print_ast_node_recursive(root, 0);
    fflush(out);
}

/* Performance timer */
typedef struct {
    clock_t start;
    const char *label;
} chomsky3_timer_t;

/* Start timer */
chomsky3_timer_t *chomsky3_debug_timer_start(const char *label) {
    if (!g_debug_enabled) {
        return NULL;
    }

    chomsky3_timer_t *timer = malloc(sizeof(chomsky3_timer_t));
    if (!timer) {
        return NULL;
    }

    timer->start = clock();
    timer->label = label;

    CHOMSKY3_DEBUG("Timer started: %s", label ? label : "unknown");

    return timer;
}

/* Stop timer and print elapsed time */
void chomsky3_debug_timer_stop(chomsky3_timer_t *timer) {
    if (!timer) {
        return;
    }

    clock_t end = clock();
    double elapsed = ((double)(end - timer->start)) / CLOCKS_PER_SEC;

    CHOMSKY3_DEBUG("Timer stopped: %s (%.6f seconds)", 
                   timer->label ? timer->label : "unknown", elapsed);

    free(timer);
}

/* Call stack tracking */
#define MAX_CALL_STACK 128

typedef struct {
    const char *function;
    const char *file;
    int line;
} call_frame_t;

static call_frame_t g_call_stack[MAX_CALL_STACK];
static int g_call_stack_depth = 0;

/* Enter function */
void chomsky3_debug_enter_function(const char *function, const char *file, int line) {
    if (!g_debug_enabled) {
        return;
    }

    if (g_call_stack_depth < MAX_CALL_STACK) {
        g_call_stack[g_call_stack_depth].function = function;
        g_call_stack[g_call_stack_depth].file = file;
        g_call_stack[g_call_stack_depth].line = line;
        g_call_stack_depth++;
    }

    CHOMSKY3_TRACE("-> %s", function);
    chomsky3_debug_indent();
}

/* Exit function */
void chomsky3_debug_exit_function(const char *function) {
    if (!g_debug_enabled) {
        return;
    }

    chomsky3_debug_unindent();
    CHOMSKY3_TRACE("<- %s", function);

    if (g_call_stack_depth > 0) {
        g_call_stack_depth--;
    }
}

/* Print call stack */
void chomsky3_debug_print_call_stack(void) {
    if (!g_debug_enabled || g_call_stack_depth == 0) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;

    fprintf(out, "Call stack:\n");
    for (int i = g_call_stack_depth - 1; i >= 0; i--) {
        fprintf(out, "  #%d: %s at %s:%d\n",
                g_call_stack_depth - i - 1,
                g_call_stack[i].function,
                g_call_stack[i].file ? g_call_stack[i].file : "unknown",
                g_call_stack[i].line);
    }
    fflush(out);
}

/* Breakpoint helper */
void chomsky3_debug_breakpoint(const char *file, int line, const char *condition) {
    if (!g_debug_enabled) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;

    fprintf(out, "*** BREAKPOINT HIT ***\n");
    fprintf(out, "Location: %s:%d\n", file ? file : "unknown", line);
    if (condition) {
        fprintf(out, "Condition: %s\n", condition);
    }
    chomsky3_debug_print_call_stack();
    fflush(out);

    /* Optionally pause execution */
    #ifdef CHOMSKY3_DEBUG_INTERACTIVE
    fprintf(out, "Press Enter to continue...\n");
    getchar();
    #endif
}

/* Assert with debug info */
void chomsky3_debug_assert_internal(int condition,
                                     const char *condition_str,
                                     const char *file,
                                     int line,
                                     const char *function,
                                     const char *format,
                                     ...) {
    if (condition) {
        return;
    }

    FILE *out = g_debug_file ? g_debug_file : stderr;

    fprintf(out, "*** DEBUG ASSERTION FAILED ***\n");
    fprintf(out, "Condition: %s\n", condition_str);
    fprintf(out, "Location: %s:%d in %s\n",
            file ? file : "unknown",
            line,
            function ? function : "unknown");

    if (format) {
        fprintf(out, "Message: ");
        va_list args;
        va_start(args, format);
        vfprintf(out, format, args);
        va_end(args);
        fprintf(out, "\n");
    }

    chomsky3_debug_print_call_stack();
    fflush(out);

    #ifdef CHOMSKY3_DEBUG_ABORT_ON_ASSERT
    abort();
    #endif
}

/* Variable watch */
typedef struct var_watch {
    const char *name;
    void *address;
    size_t size;
    void *last_value;
    struct var_watch *next;
} var_watch_t;

static var_watch_t *g_watch_list = NULL;

/* Add variable to watch list */
void chomsky3_debug_watch_var(const char *name, void *address, size_t size) {
    if (!g_debug_enabled) {
        return;
    }

    var_watch_t *watch = malloc(sizeof(var_watch_t));
    if (!watch) {
        return;
    }

    watch->name = name;
    watch->address = address;
    watch->size = size;
    watch->last_value = malloc(size);
    if (watch->last_value) {
        memcpy(watch->last_value, address, size);
    }

    watch->next = g_watch_list;
    g_watch_list = watch;

    CHOMSKY3_DEBUG("Watching variable: %s at %p (%zu bytes)", name, address, size);
}

/* Check watched variables for changes */
void chomsky3_debug_check_watches(void) {
    if (!g_debug_enabled) {
        return;
    }

    for (var_watch_t *watch = g_watch_list; watch; watch = watch->next) {
        if (watch->last_value && memcmp(watch->address, watch->last_value, watch->size) != 0) {
            CHOMSKY3_DEBUG("Variable changed: %s at %p", watch->name, watch->address);
            chomsky3_debug_dump_memory(watch->address, watch->size, watch->name);
            memcpy(watch->last_value, watch->address, watch->size);
        }
    }
}

/* Clear watch list */
void chomsky3_debug_clear_watches(void) {
    var_watch_t *current = g_watch_list;
    while (current) {
        var_watch_t *next = current->next;
        free(current->last_value);
        free(current);
        current = next;
    }
    g_watch_list = NULL;
}
