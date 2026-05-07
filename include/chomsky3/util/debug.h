/**
 * Internal debug logging compatibility layer.
 */

#ifndef CHOMSKY3_UTIL_DEBUG_H
#define CHOMSKY3_UTIL_DEBUG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CHOMSKY3_DEBUG_TRACE = 0,
    CHOMSKY3_DEBUG_DEBUG = 1,
    CHOMSKY3_DEBUG_INFO = 2,
    CHOMSKY3_DEBUG_WARN = 3,
    CHOMSKY3_DEBUG_ERROR = 4
} chomsky3_debug_level_t;

typedef struct chomsky3_timer chomsky3_timer_t;
typedef struct chomsky3_ast_node chomsky3_ast_node_t;

void chomsky3_debug_enable(int enable);
int chomsky3_debug_is_enabled(void);
void chomsky3_debug_set_level(chomsky3_debug_level_t level);
chomsky3_debug_level_t chomsky3_debug_get_level(void);
int chomsky3_debug_set_file(const char *path);
void chomsky3_debug_close_file(void);
void chomsky3_debug_set_options(int show_timestamp, int show_location);
void chomsky3_debug_print_internal(
    chomsky3_debug_level_t level,
    const char *file,
    int line,
    const char *function,
    const char *format,
    ...
);
void chomsky3_debug_indent(void);
void chomsky3_debug_unindent(void);
void chomsky3_debug_reset_indent(void);
void chomsky3_debug_dump_memory(const void *ptr, size_t size, const char *label);
void chomsky3_debug_print_ast(const chomsky3_ast_node_t *root, const char *label);
chomsky3_timer_t *chomsky3_debug_timer_start(const char *label);
void chomsky3_debug_timer_stop(chomsky3_timer_t *timer);
void chomsky3_debug_enter_function(const char *function, const char *file, int line);
void chomsky3_debug_exit_function(const char *function);
void chomsky3_debug_print_call_stack(void);
void chomsky3_debug_breakpoint(const char *file, int line, const char *condition);
void chomsky3_debug_assert_internal(int condition,
                                    const char *condition_str,
                                    const char *file,
                                    int line,
                                    const char *function,
                                    const char *format,
                                    ...);
void chomsky3_debug_watch_var(const char *name, void *address, size_t size);
void chomsky3_debug_check_watches(void);
void chomsky3_debug_clear_watches(void);

#define CHOMSKY3_TRACE(...) \
    chomsky3_debug_print_internal(CHOMSKY3_DEBUG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CHOMSKY3_DEBUG(...) \
    chomsky3_debug_print_internal(CHOMSKY3_DEBUG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CHOMSKY3_INFO(...) \
    chomsky3_debug_print_internal(CHOMSKY3_DEBUG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CHOMSKY3_WARN(...) \
    chomsky3_debug_print_internal(CHOMSKY3_DEBUG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CHOMSKY3_ERROR(...) \
    chomsky3_debug_print_internal(CHOMSKY3_DEBUG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CHOMSKY3_ASSERT(cond, ...) \
    chomsky3_debug_assert_internal((cond), #cond, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_UTIL_DEBUG_H */
