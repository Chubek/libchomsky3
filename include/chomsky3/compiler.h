/**
 * libchomsky3 - Compiler Interface
 * 
 * Header file providing the compilation pipeline interface for transforming
 * ERE AST into intermediate representation and optimized forms.
 */

#ifndef CHOMSKY3_COMPILER_H
#define CHOMSKY3_COMPILER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "regex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_compiler chomsky3_compiler_t;
typedef struct chomsky3_compile_options chomsky3_compile_options_t;

/* Optimization levels */
typedef enum {
    CHOMSKY3_OPT_NONE = 0,      /* No optimization */
    CHOMSKY3_OPT_BASIC = 1,     /* Basic optimizations (constant folding, dead code) */
    CHOMSKY3_OPT_STANDARD = 2,  /* Standard optimizations (+ common subexpression elimination) */
    CHOMSKY3_OPT_AGGRESSIVE = 3 /* Aggressive optimizations (+ loop unrolling, inlining) */
} chomsky3_opt_level_t;

/* Compiler statistics */
typedef struct {
    size_t ast_nodes;           /* Number of AST nodes */
    size_t ir_instructions;     /* Number of IR instructions */
    size_t optimized_instructions; /* IR instructions after optimization */
    size_t bytecode_size;       /* Final bytecode size in bytes */
    size_t num_states;          /* Number of states (for NFA/DFA) */
    size_t num_transitions;     /* Number of transitions */
    double compile_time_ms;     /* Compilation time in milliseconds */
    double optimization_time_ms; /* Optimization time in milliseconds */
} chomsky3_compile_stats_t;

/* Compilation options */
struct chomsky3_compile_options {
    chomsky3_target_t target;   /* Compilation target */
    chomsky3_flags_t flags;     /* Compilation flags */
    chomsky3_opt_level_t opt_level; /* Optimization level */
    
    /* Target-specific options */
    struct {
        bool use_dfa;           /* Use DFA instead of NFA for bytecode */
        bool minimize_dfa;      /* Minimize DFA states */
        size_t max_states;      /* Maximum DFA states (0 = unlimited) */
    } bytecode;
    
    struct {
        bool inline_small_loops; /* Inline small loops */
        bool use_simd;          /* Use SIMD instructions if available */
        size_t max_code_size;   /* Maximum JIT code size (0 = unlimited) */
    } jit;
    
    struct {
        bool emit_comments;     /* Emit comments in generated C code */
        bool emit_debug_info;   /* Emit debug information */
        const char *function_name; /* Generated function name */
    } c_source;
    
    /* Memory limits */
    size_t max_memory;          /* Maximum memory usage (0 = unlimited) */
    
    /* Debugging */
    bool dump_ast;              /* Dump AST to stderr */
    bool dump_ir;               /* Dump IR to stderr */
    bool dump_optimized_ir;     /* Dump optimized IR to stderr */
};

/* Compiler context */
struct chomsky3_compiler {
    chomsky3_context_t *ctx;    /* Parent context */
    chomsky3_compile_options_t options; /* Compilation options */
    chomsky3_compile_stats_t stats; /* Compilation statistics */
    
    /* Internal state */
    void *internal;             /* Opaque internal state */
};

/**
 * Create a new compiler instance.
 * 
 * @param ctx Parent context
 * @param options Compilation options (NULL for defaults)
 * @return New compiler or NULL on failure
 */
chomsky3_compiler_t *chomsky3_compiler_new(
    chomsky3_context_t *ctx,
    const chomsky3_compile_options_t *options
);

/**
 * Free a compiler instance.
 * 
 * @param compiler Compiler to free
 */
void chomsky3_compiler_free(chomsky3_compiler_t *compiler);

/**
 * Get default compilation options.
 * 
 * @param options Options structure to fill
 * @param target Target type
 */
void chomsky3_compile_options_default(
    chomsky3_compile_options_t *options,
    chomsky3_target_t target
);

/**
 * Compile a regex AST to a pattern.
 * 
 * @param compiler Compiler instance
 * @param regex Regex AST to compile
 * @param pattern Output pattern (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_compiler_compile(
    chomsky3_compiler_t *compiler,
    const chomsky3_regex_t *regex,
    chomsky3_pattern_t **pattern
);

/**
 * Compile a pattern string directly.
 * 
 * @param compiler Compiler instance
 * @param pattern_str Pattern string
 * @param length Pattern length
 * @param pattern Output pattern (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_compiler_compile_string(
    chomsky3_compiler_t *compiler,
    const char *pattern_str,
    size_t length,
    chomsky3_pattern_t **pattern
);

/**
 * Get compilation statistics.
 * 
 * @param compiler Compiler instance
 * @param stats Output statistics structure
 */
void chomsky3_compiler_get_stats(
    const chomsky3_compiler_t *compiler,
    chomsky3_compile_stats_t *stats
);

/**
 * Reset compiler state for reuse.
 * 
 * @param compiler Compiler instance
 */
void chomsky3_compiler_reset(chomsky3_compiler_t *compiler);

/**
 * Validate compilation options.
 * 
 * @param options Options to validate
 * @return true if valid, false otherwise
 */
bool chomsky3_compile_options_validate(const chomsky3_compile_options_t *options);

/**
 * Estimate memory usage for a given regex.
 * 
 * @param compiler Compiler instance
 * @param regex Regex to estimate
 * @return Estimated memory usage in bytes (0 on error)
 */
size_t chomsky3_compiler_estimate_memory(
    chomsky3_compiler_t *compiler,
    const chomsky3_regex_t *regex
);

/**
 * Set optimization level.
 * 
 * @param compiler Compiler instance
 * @param level Optimization level
 */
void chomsky3_compiler_set_opt_level(
    chomsky3_compiler_t *compiler,
    chomsky3_opt_level_t level
);

/**
 * Enable or disable specific optimization pass.
 * 
 * @param compiler Compiler instance
 * @param pass_name Name of optimization pass
 * @param enabled Enable or disable
 * @return true on success, false if pass not found
 */
bool chomsky3_compiler_set_opt_pass(
    chomsky3_compiler_t *compiler,
    const char *pass_name,
    bool enabled
);

/**
 * Get list of available optimization passes.
 * 
 * @param count Output number of passes
 * @return Array of pass names (NULL-terminated)
 */
const char **chomsky3_compiler_list_opt_passes(size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_COMPILER_H */
