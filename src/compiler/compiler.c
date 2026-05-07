/**
 * libchomsky3 - Compiler Implementation
 * 
 * Implements the compilation pipeline for transforming ERE AST into
 * intermediate representation and target-specific output.
 */

#include "chomsky3/compiler.h"
#include "chomsky3/error.h"
#include "chomsky3/parser.h"
#include "chomsky3/ir.h"
#include "chomsky3/pattern.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Internal compiler state */
typedef struct {
    chomsky3_ir_t *ir;              /* Current IR */
    chomsky3_ir_builder_t *builder; /* IR builder */
    bool *enabled_passes;           /* Enabled optimization passes */
    size_t num_passes;              /* Number of optimization passes */
} compiler_internal_t;

/* Optimization pass names */
static const char *OPT_PASS_NAMES[] = {
    "constant_folding",
    "dead_code_elimination",
    "common_subexpression_elimination",
    "loop_unrolling",
    "inlining",
    "peephole",
    NULL
};

/* Forward declarations */
static chomsky3_error_t compile_to_ir(
    chomsky3_compiler_t *compiler,
    const chomsky3_regex_t *regex
);
static chomsky3_error_t optimize_ir(
    chomsky3_compiler_t *compiler,
    chomsky3_ir_t *ir
);
static chomsky3_error_t generate_pattern(
    chomsky3_compiler_t *compiler,
    chomsky3_ir_t *ir,
    chomsky3_pattern_t **pattern
);
static double get_time_ms(void);
static size_t count_ast_nodes(const chomsky3_ast_node_t *node);
static void dump_ast_to_stderr(const chomsky3_regex_t *regex);
static void dump_ir_to_stderr(const chomsky3_ir_t *ir);

/**
 * Get default compilation options.
 */
void chomsky3_compile_options_default(
    chomsky3_compile_options_t *options,
    chomsky3_target_t target
) {
    if (!options) {
        return;
    }

    memset(options, 0, sizeof(*options));
    
    options->target = target;
    options->flags = 0;
    options->opt_level = CHOMSKY3_OPT_STANDARD;
    
    /* Bytecode defaults */
    options->bytecode.use_dfa = false;
    options->bytecode.minimize_dfa = true;
    options->bytecode.max_states = 0;
    
    /* JIT defaults */
    options->jit.inline_small_loops = true;
    options->jit.use_simd = true;
    options->jit.max_code_size = 0;
    
    /* C source defaults */
    options->c_source.emit_comments = true;
    options->c_source.emit_debug_info = false;
    options->c_source.function_name = "chomsky3_match";
    
    /* Memory limits */
    options->max_memory = 0;
    
    /* Debugging */
    options->dump_ast = false;
    options->dump_ir = false;
    options->dump_optimized_ir = false;
}

/**
 * Validate compilation options.
 */
bool chomsky3_compile_options_validate(const chomsky3_compile_options_t *options) {
    if (!options) {
        return false;
    }

    /* Validate target */
    if (options->target < CHOMSKY3_TARGET_BYTECODE ||
        options->target > CHOMSKY3_TARGET_C_SOURCE) {
        return false;
    }

    /* Validate optimization level */
    if (options->opt_level < CHOMSKY3_OPT_NONE ||
        options->opt_level > CHOMSKY3_OPT_AGGRESSIVE) {
        return false;
    }

    return true;
}

/**
 * Create a new compiler instance.
 */
chomsky3_compiler_t *chomsky3_compiler_new(
    chomsky3_context_t *ctx,
    const chomsky3_compile_options_t *options
) {
    if (!ctx) {
        return NULL;
    }

    chomsky3_compiler_t *compiler = calloc(1, sizeof(chomsky3_compiler_t));
    if (!compiler) {
        return NULL;
    }

    compiler->ctx = ctx;

    /* Set options */
    if (options) {
        if (!chomsky3_compile_options_validate(options)) {
            free(compiler);
            return NULL;
        }
        compiler->options = *options;
    } else {
        chomsky3_compile_options_default(&compiler->options, CHOMSKY3_TARGET_BYTECODE);
    }

    /* Initialize internal state */
    compiler_internal_t *internal = calloc(1, sizeof(compiler_internal_t));
    if (!internal) {
        free(compiler);
        return NULL;
    }

    /* Count optimization passes */
    size_t num_passes = 0;
    while (OPT_PASS_NAMES[num_passes]) {
        num_passes++;
    }

    internal->num_passes = num_passes;
    internal->enabled_passes = calloc(num_passes, sizeof(bool));
    if (!internal->enabled_passes) {
        free(internal);
        free(compiler);
        return NULL;
    }

    /* Enable passes based on optimization level */
    switch (compiler->options.opt_level) {
        case CHOMSKY3_OPT_AGGRESSIVE:
            internal->enabled_passes[3] = true; /* loop_unrolling */
            internal->enabled_passes[4] = true; /* inlining */
            /* fallthrough */
        case CHOMSKY3_OPT_STANDARD:
            internal->enabled_passes[2] = true; /* common_subexpression_elimination */
            /* fallthrough */
        case CHOMSKY3_OPT_BASIC:
            internal->enabled_passes[0] = true; /* constant_folding */
            internal->enabled_passes[1] = true; /* dead_code_elimination */
            internal->enabled_passes[5] = true; /* peephole */
            /* fallthrough */
        case CHOMSKY3_OPT_NONE:
        default:
            break;
    }

    /* Create IR builder */
    internal->builder = chomsky3_ir_builder_create(ctx);
    if (!internal->builder) {
        free(internal->enabled_passes);
        free(internal);
        free(compiler);
        return NULL;
    }

    compiler->internal = internal;

    /* Initialize stats */
    memset(&compiler->stats, 0, sizeof(compiler->stats));

    return compiler;
}

/**
 * Free a compiler instance.
 */
void chomsky3_compiler_free(chomsky3_compiler_t *compiler) {
    if (!compiler) {
        return;
    }

    if (compiler->internal) {
        compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;
        
        if (internal->ir) {
            chomsky3_ir_destroy(internal->ir);
        }
        
        if (internal->builder) {
            chomsky3_ir_builder_destroy(internal->builder);
        }
        
        free(internal->enabled_passes);
        free(internal);
    }

    free(compiler);
}

/**
 * Reset compiler state for reuse.
 */
void chomsky3_compiler_reset(chomsky3_compiler_t *compiler) {
    if (!compiler || !compiler->internal) {
        return;
    }

    compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;

    /* Destroy old IR */
    if (internal->ir) {
        chomsky3_ir_destroy(internal->ir);
        internal->ir = NULL;
    }

    /* Reset stats */
    memset(&compiler->stats, 0, sizeof(compiler->stats));
}

/**
 * Compile a regex AST to a pattern.
 */
chomsky3_error_t chomsky3_compiler_compile(
    chomsky3_compiler_t *compiler,
    const chomsky3_regex_t *regex,
    chomsky3_pattern_t **pattern
) {
    if (!compiler || !regex || !pattern) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;
    if (!internal) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    chomsky3_error_t err;
    double start_time = get_time_ms();

    /* Reset state */
    chomsky3_compiler_reset(compiler);

    /* Count AST nodes */
    compiler->stats.ast_nodes = count_ast_nodes(regex->root);

    /* Dump AST if requested */
    if (compiler->options.dump_ast) {
        dump_ast_to_stderr(regex);
    }

    /* Compile to IR */
    err = compile_to_ir(compiler, regex);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    compiler->stats.ir_instructions = internal->ir->total_instructions;

    /* Dump IR if requested */
    if (compiler->options.dump_ir) {
        dump_ir_to_stderr(internal->ir);
    }

    /* Optimize IR */
    double opt_start = get_time_ms();
    err = optimize_ir(compiler, internal->ir);
    if (err != CHOMSKY3_OK) {
        return err;
    }
    compiler->stats.optimization_time_ms = get_time_ms() - opt_start;
    compiler->stats.optimized_instructions = internal->ir->total_instructions;

    /* Dump optimized IR if requested */
    if (compiler->options.dump_optimized_ir) {
        fprintf(stderr, "\n=== Optimized IR ===\n");
        dump_ir_to_stderr(internal->ir);
    }

    /* Generate target pattern */
    err = generate_pattern(compiler, internal->ir, pattern);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    compiler->stats.compile_time_ms = get_time_ms() - start_time;

    return CHOMSKY3_OK;
}

/**
 * Compile a pattern string directly.
 */
chomsky3_error_t chomsky3_compiler_compile_string(
    chomsky3_compiler_t *compiler,
    const char *pattern_str,
    size_t length,
    chomsky3_pattern_t **pattern
) {
    if (!compiler || !pattern_str || !pattern) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Parse pattern string to AST */
    chomsky3_regex_t *regex = NULL;
    chomsky3_error_t err = chomsky3_parse(
        compiler->ctx,
        pattern_str,
        length,
        compiler->options.flags,
        &regex
    );

    if (err != CHOMSKY3_OK) {
        return err;
    }

    /* Compile AST */
    err = chomsky3_compiler_compile(compiler, regex, pattern);

    /* Free AST */
    chomsky3_regex_free(regex);

    return err;
}

/**
 * Get compilation statistics.
 */
void chomsky3_compiler_get_stats(
    const chomsky3_compiler_t *compiler,
    chomsky3_compile_stats_t *stats
) {
    if (!compiler || !stats) {
        return;
    }

    *stats = compiler->stats;
}

/**
 * Set optimization level.
 */
void chomsky3_compiler_set_opt_level(
    chomsky3_compiler_t *compiler,
    chomsky3_opt_level_t level
) {
    if (!compiler || !compiler->internal) {
        return;
    }

    compiler->options.opt_level = level;

    compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;

    /* Reset all passes */
    memset(internal->enabled_passes, 0, internal->num_passes * sizeof(bool));

    /* Enable passes based on level */
    switch (level) {
        case CHOMSKY3_OPT_AGGRESSIVE:
            internal->enabled_passes[3] = true; /* loop_unrolling */
            internal->enabled_passes[4] = true; /* inlining */
            /* fallthrough */
        case CHOMSKY3_OPT_STANDARD:
            internal->enabled_passes[2] = true; /* common_subexpression_elimination */
            /* fallthrough */
        case CHOMSKY3_OPT_BASIC:
            internal->enabled_passes[0] = true; /* constant_folding */
            internal->enabled_passes[1] = true; /* dead_code_elimination */
            internal->enabled_passes[5] = true; /* peephole */
            /* fallthrough */
        case CHOMSKY3_OPT_NONE:
        default:
            break;
    }
}

/**
 * Enable or disable specific optimization pass.
 */
bool chomsky3_compiler_set_opt_pass(
    chomsky3_compiler_t *compiler,
    const char *pass_name,
    bool enabled
) {
    if (!compiler || !pass_name || !compiler->internal) {
        return false;
    }

    compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;

    /* Find pass by name */
    for (size_t i = 0; i < internal->num_passes; i++) {
        if (strcmp(OPT_PASS_NAMES[i], pass_name) == 0) {
            internal->enabled_passes[i] = enabled;
            return true;
        }
    }

    return false;
}

/**
 * Get list of available optimization passes.
 */
const char **chomsky3_compiler_list_opt_passes(size_t *count) {
    if (count) {
        *count = 0;
        while (OPT_PASS_NAMES[*count]) {
            (*count)++;
        }
    }
    return OPT_PASS_NAMES;
}

/**
 * Estimate memory usage for a given regex.
 */
size_t chomsky3_compiler_estimate_memory(
    chomsky3_compiler_t *compiler,
    const chomsky3_regex_t *regex
) {
    if (!compiler || !regex) {
        return 0;
    }

    /* Rough estimation based on AST size */
    size_t ast_nodes = count_ast_nodes(regex->root);
    
    /* Estimate:
     * - Each AST node -> ~2-3 IR instructions
     * - Each instruction ~128 bytes
     * - Pattern overhead ~1KB
     */
    size_t estimated = ast_nodes * 3 * 128 + 1024;

    /* Add DFA state estimation if applicable */
    if (compiler->options.bytecode.use_dfa) {
        /* Rough DFA estimation: 2^(capture_groups) * avg_transitions */
        size_t groups = regex->num_groups;
        size_t dfa_estimate = (1 << (groups < 10 ? groups : 10)) * 256;
        estimated += dfa_estimate;
    }

    return estimated;
}

/* ========================================================================
 * Internal Implementation
 * ======================================================================== */

/**
 * Compile regex AST to IR.
 */
static chomsky3_error_t compile_to_ir(
    chomsky3_compiler_t *compiler,
    const chomsky3_regex_t *regex
) {
    compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;

    /* Build IR from AST */
    chomsky3_error_t err = chomsky3_ir_build_from_ast(
        internal->builder,
        regex,
        &internal->ir
    );

    if (err != CHOMSKY3_OK) {
        return err;
    }

    return CHOMSKY3_OK;
}

/**
 * Optimize IR based on enabled passes.
 */
static chomsky3_error_t optimize_ir(
    chomsky3_compiler_t *compiler,
    chomsky3_ir_t *ir
) {
    if (!compiler || !ir) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    compiler_internal_t *internal = (compiler_internal_t *)compiler->internal;

    /* Skip if no optimization */
    if (compiler->options.opt_level == CHOMSKY3_OPT_NONE) {
        return CHOMSKY3_OK;
    }

    /* Run enabled optimization passes */
    for (size_t i = 0; i < internal->num_passes; i++) {
        if (!internal->enabled_passes[i]) {
            continue;
        }

        /* TODO: Implement actual optimization passes */
        /* For now, this is a placeholder */
        (void)OPT_PASS_NAMES[i];
    }

    return CHOMSKY3_OK;
}

/**
 * Generate target pattern from IR.
 */
static chomsky3_error_t generate_pattern(
    chomsky3_compiler_t *compiler,
    chomsky3_ir_t *ir,
    chomsky3_pattern_t **pattern
) {
    if (!compiler || !ir || !pattern) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Create pattern structure */
    chomsky3_pattern_t *pat = calloc(1, sizeof(chomsky3_pattern_t));
    if (!pat) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    pat->ctx = compiler->ctx;
    pat->target = compiler->options.target;
    pat->flags = compiler->options.flags;

    /* Generate target-specific code */
    switch (compiler->options.target) {
        case CHOMSKY3_TARGET_BYTECODE: {
            /* TODO: Generate bytecode from IR */
            /* For now, store IR reference */
            pat->bytecode.code = NULL;
            pat->bytecode.size = 0;
            pat->bytecode.num_registers = ir->num_registers;
            compiler->stats.bytecode_size = 0;
            break;
        }

        case CHOMSKY3_TARGET_JIT: {
            /* TODO: Generate JIT code from IR */
            pat->jit.code = NULL;
            pat->jit.size = 0;
            pat->jit.entry_point = NULL;
            compiler->stats.bytecode_size = 0;
            break;
        }

        case CHOMSKY3_TARGET_C_SOURCE: {
            /* TODO: Generate C source from IR */
            pat->c_source.source = NULL;
            pat->c_source.length = 0;
            compiler->stats.bytecode_size = 0;
            break;
        }

        default:
            free(pat);
            return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Copy metadata */
    pat->num_groups = ir->source_ast ? ir->source_ast->num_groups : 0;
    pat->max_backtrack_depth = 0; /* TODO: Calculate from IR */

    *pattern = pat;
    return CHOMSKY3_OK;
}

/**
 * Get current time in milliseconds.
 */
static double get_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/**
 * Count AST nodes recursively.
 */
static size_t count_ast_nodes(const chomsky3_ast_node_t *node) {
    if (!node) {
        return 0;
    }

    size_t count = 1;

    switch (node->type) {
        case CHOMSKY3_AST_CONCATENATION:
            for (size_t i = 0; i < node->data.concatenation.num_children; i++) {
                count += count_ast_nodes(node->data.concatenation.children[i]);
            }
            break;

        case CHOMSKY3_AST_ALTERNATION:
            for (size_t i = 0; i < node->data.alternation.num_alternatives; i++) {
                count += count_ast_nodes(node->data.alternation.alternatives[i]);
            }
            break;

        case CHOMSKY3_AST_QUANTIFIER:
            count += count_ast_nodes(node->data.quantifier.child);
            break;

        case CHOMSKY3_AST_GROUP:
            count += count_ast_nodes(node->data.group.child);
            break;

        case CHOMSKY3_AST_LOOKAROUND:
            count += count_ast_nodes(node->data.lookaround.child);
            break;

        default:
            /* Leaf nodes */
            break;
    }

    return count;
}

/**
 * Dump AST to stderr for debugging.
 */
static void dump_ast_to_stderr(const chomsky3_regex_t *regex) {
    if (!regex) {
        return;
    }

    fprintf(stderr, "\n=== AST Dump ===\n");
    fprintf(stderr, "Pattern: %s\n", regex->pattern ? regex->pattern : "(null)");
    fprintf(stderr, "Flags: 0x%x\n", regex->flags);
    fprintf(stderr, "Groups: %zu\n", regex->num_groups);
    fprintf(stderr, "Nodes: %zu\n", count_ast_nodes(regex->root));
    fprintf(stderr, "\n");

    /* TODO: Implement detailed AST tree printing */
}

/**
 * Dump IR to stderr for debugging.
 */
static void dump_ir_to_stderr(const chomsky3_ir_t *ir) {
    if (!ir) {
        return;
    }

    fprintf(stderr, "\n=== IR Dump ===\n");
    fprintf(stderr, "Name: %s\n", ir->name ? ir->name : "(null)");
    fprintf(stderr, "Blocks: %zu\n", ir->num_blocks);
    fprintf(stderr, "Instructions: %zu\n", ir->total_instructions);
    fprintf(stderr, "Registers: %zu\n", ir->num_registers);
    fprintf(stderr, "\n");

    /* TODO: Implement detailed IR block/instruction printing */
}
