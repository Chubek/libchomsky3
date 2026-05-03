/**
 * libchomsky3 - Optimization Interface
 * 
 * Header file providing interfaces for various optimization passes that can be
 * applied to the Intermediate Representation (IR) of compiled regular expressions.
 * These optimizations aim to improve performance (speed and size) of the generated
 * code (bytecode, JIT, or C).
 */

#ifndef CHOMSKY3_OPTIMIZE_H
#define CHOMSKY3_OPTIMIZE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "ir.h" // Depends on the Intermediate Representation

#ifdef __cplusplus
extern "C" {
#endif

/* Optimization level enumeration (typically used by compiler/codegen) */
typedef enum {
    CHOMSKY3_OPT_LEVEL_NONE = 0,
    CHOMSKY3_OPT_LEVEL_BASIC = 1,
    CHOMSKY3_OPT_LEVEL_STANDARD = 2,
    CHOMSKY3_OPT_LEVEL_AGGRESSIVE = 3
} chomsky3_opt_level_t;

/* Optimization pass identifiers (for enabled_passes bitset) */
typedef enum {
    CHOMSKY3_OPT_PASS_DCE = 0,                  /* Dead Code Elimination */
    CHOMSKY3_OPT_PASS_CSE = 1,                  /* Common Subexpression Elimination */
    CHOMSKY3_OPT_PASS_CONSTANT_FOLDING = 2,     /* Constant Folding and Propagation */
    CHOMSKY3_OPT_PASS_LOOP_OPT = 3,             /* Loop Optimizations */
    CHOMSKY3_OPT_PASS_STRENGTH_REDUCTION = 4,   /* Strength Reduction */
    CHOMSKY3_OPT_PASS_CFG_SIMPLIFY = 5,         /* Control Flow Graph Simplification */
    CHOMSKY3_OPT_PASS_PEEPHOLE = 6,             /* Peephole Optimization */
    CHOMSKY3_OPT_PASS_REGISTER_ALLOC = 7,       /* Register Allocation */
    CHOMSKY3_OPT_PASS_INLINE = 8,               /* Function/Block Inlining */
    CHOMSKY3_OPT_PASS_TAIL_CALL = 9,            /* Tail Call Optimization */
    CHOMSKY3_OPT_PASS_MAX = 32                  /* Maximum number of passes */
} chomsky3_opt_pass_id_t;

/* Generic optimization context or configuration */
typedef struct {
    chomsky3_opt_level_t level;     /* Overall optimization level */
    uint32_t enabled_passes;        /* Bitset to enable/disable specific passes */
    uint32_t max_iterations;        /* Maximum iterations for iterative passes */
    size_t max_inline_size;         /* Maximum size for inlining decisions */
    bool verify_after_pass;         /* Run verification after each pass */
    bool print_stats;               /* Print statistics after optimization */
} chomsky3_optimization_config_t;

/* Per-pass statistics */
typedef struct {
    const char *pass_name;
    double time_ms;
    size_t changes_made;
    bool converged;
} chomsky3_pass_stats_t;

/* Structure to hold optimization statistics */
typedef struct {
    double total_time_ms;           /* Total time spent optimizing */
    size_t num_passes_run;          /* Number of passes executed */
    chomsky3_pass_stats_t *pass_stats; /* Per-pass statistics (dynamically allocated) */
    
    /* Aggregate statistics */
    size_t instructions_removed;
    size_t blocks_merged;
    size_t constants_folded;
    size_t dead_stores_eliminated;
    size_t common_subexpressions_eliminated;
    size_t loops_optimized;
    size_t branches_folded;
} chomsky3_optimization_stats_t;

/**
 * Initialize default optimization configuration.
 * 
 * @param config Optimization configuration structure to fill.
 * @param level The desired overall optimization level.
 */
void chomsky3_optimization_config_default(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_level_t level
);

/**
 * Enable a specific optimization pass.
 * 
 * @param config The optimization configuration to modify.
 * @param pass_id The pass identifier to enable.
 */
void chomsky3_optimization_enable_pass(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
);

/**
 * Disable a specific optimization pass.
 * 
 * @param config The optimization configuration to modify.
 * @param pass_id The pass identifier to disable.
 */
void chomsky3_optimization_disable_pass(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
);

/**
 * Check if a specific optimization pass is enabled.
 * 
 * @param config The optimization configuration to check.
 * @param pass_id The pass identifier to check.
 * @return true if the pass is enabled, false otherwise.
 */
bool chomsky3_optimization_is_pass_enabled(
    const chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
);

/**
 * Allocate and initialize optimization statistics structure.
 * 
 * @param max_passes Maximum number of passes to track.
 * @return Pointer to allocated statistics structure, or NULL on failure.
 */
chomsky3_optimization_stats_t *chomsky3_optimization_stats_create(size_t max_passes);

/**
 * Free optimization statistics structure.
 * 
 * @param stats The statistics structure to free.
 */
void chomsky3_optimization_stats_free(chomsky3_optimization_stats_t *stats);

/**
 * Print optimization statistics to a file stream.
 * 
 * @param stats The statistics to print.
 * @param stream The output stream (e.g., stdout, stderr).
 */
void chomsky3_optimization_stats_print(
    const chomsky3_optimization_stats_t *stats,
    FILE *stream
);

/**
 * Apply a set of optimization passes to the IR.
 * 
 * This function orchestrates the application of various optimization passes
 * based on the provided configuration.
 * 
 * @param ir The Intermediate Representation to optimize.
 * @param config The optimization configuration specifying level and passes.
 * @param stats Structure to store optimization statistics (can be NULL).
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_ir(
    chomsky3_ir_t *ir,
    const chomsky3_optimization_config_t *config,
    chomsky3_optimization_stats_t *stats
);

/**
 * Run a specific optimization pass on the IR.
 * 
 * This function is typically called internally by chomsky3_optimize_ir,
 * but can be called directly for custom optimization pipelines.
 * 
 * @param ir The Intermediate Representation to optimize.
 * @param pass The optimization pass function and its configuration.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_run_optimization_pass(
    chomsky3_ir_t *ir,
    const chomsky3_ir_pass_t *pass
);

/**
 * Optimize the IR for size.
 * 
 * Applies a set of optimizations focused on reducing the final code size.
 * 
 * @param ir The Intermediate Representation to optimize.
 * @param stats Structure to store optimization statistics (can be NULL).
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_for_size(
    chomsky3_ir_t *ir,
    chomsky3_optimization_stats_t *stats
);

/**
 * Optimize the IR for speed.
 * 
 * Applies a set of optimizations focused on maximizing execution speed.
 * 
 * @param ir The Intermediate Representation to optimize.
 * @param stats Structure to store optimization statistics (can be NULL).
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_for_speed(
    chomsky3_ir_t *ir,
    chomsky3_optimization_stats_t *stats
);

/**
 * Perform a peephole optimization on a basic block.
 * 
 * Peephole optimizations examine small sequences of instructions and replace
 * them with equivalent, more efficient sequences.
 * 
 * @param block The basic block to optimize.
 * @return The number of instructions changed or removed.
 */
size_t chomsky3_optimize_peephole_block(chomsky3_ir_block_t *block);

/**
 * Perform dead code elimination.
 * 
 * Removes instructions that do not affect the program's outcome.
 * 
 * @param ir The Intermediate Representation to process.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_dce(chomsky3_ir_t *ir);

/**
 * Perform common subexpression elimination.
 * 
 * Identifies and eliminates redundant computations.
 * 
 * @param ir The Intermediate Representation to process.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_cse(chomsky3_ir_t *ir);

/**
 * Perform constant folding and propagation.
 * 
 * Evaluates constant expressions at compile time and propagates their values.
 * 
 * @param ir The Intermediate Representation to process.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_constant_folding(chomsky3_ir_t *ir);

/**
 * Perform loop optimizations.
 * 
 * Includes optimizations like loop unrolling, invariant code motion, etc.
 * Requires loop analysis to be performed first.
 * 
 * @param ir The Intermediate Representation to process.
 * @param analysis Precomputed loop analysis data (can be NULL if to be computed internally).
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_loops(
    chomsky3_ir_t *ir,
    chomsky3_ir_analysis_t *analysis
);

/**
 * Perform strength reduction.
 * 
 * Replaces expensive operations (like multiplication) with cheaper ones (like addition)
 * where possible, e.g., within loops.
 * 
 * @param ir The Intermediate Representation to process.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_strength_reduction(chomsky3_ir_t *ir);

/**
 * Simplify the Control Flow Graph (CFG).
 * 
 * Merges blocks, removes unreachable code, folds conditional branches, etc.
 * 
 * @param ir The Intermediate Representation to process.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_cfg(chomsky3_ir_t *ir);

/**
 * Perform inline expansion of small blocks or functions.
 * 
 * Replaces calls to small blocks with their body to reduce overhead.
 * 
 * @param ir The Intermediate Representation to process.
 * @param max_size Maximum size threshold for inlining.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_inline(
    chomsky3_ir_t *ir,
    size_t max_size
);

/**
 * Perform tail call optimization.
 * 
 * Converts tail-recursive calls into loops to save stack space.
 * 
 * @param ir The Intermediate Representation to process.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_tail_calls(chomsky3_ir_t *ir);

/**
 * Convert IR to Static Single Assignment (SSA) form.
 * 
 * SSA form simplifies many data-flow analyses and optimizations.
 * 
 * @param ir The Intermediate Representation to convert.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_ir_to_ssa(chomsky3_ir_t *ir);

/**
 * Convert IR from SSA form back to a non-SSA representation.
 * 
 * Necessary before code generation if the target does not support SSA directly.
 * Typically involves inserting copy operations.
 * 
 * @param ir The Intermediate Representation to convert.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_ir_from_ssa(chomsky3_ir_t *ir);

/**
 * Perform register allocation for the IR.
 * 
 * Assigns virtual registers to physical registers or stack locations.
 * This is often one of the final optimization steps.
 * 
 * @param ir The Intermediate Representation to process.
 * @param analysis Data flow analysis results (e.g., liveness) might be needed.
 * @return CHOMSKY3_OK on success, or an error code on failure.
 */
chomsky3_error_t chomsky3_optimize_register_allocation(
    chomsky3_ir_t *ir,
    const chomsky3_ir_analysis_t *analysis
);

/**
 * Verify the integrity of the IR after optimization.
 * 
 * Checks for structural consistency, type correctness, and other invariants.
 * Useful for debugging optimization passes.
 * 
 * @param ir The Intermediate Representation to verify.
 * @return CHOMSKY3_OK if verification passes, error code otherwise.
 */
chomsky3_error_t chomsky3_ir_verify(const chomsky3_ir_t *ir);

/* --- Specific Optimization Pass Definitions --- */
/* These functions would typically be defined in .c files and exposed via IR passes */

/**
 * Get a reference to the DCE optimization pass.
 * @return A pointer to the DCE optimization pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_dce(void);

/**
 * Get a reference to the CSE optimization pass.
 * @return A pointer to the CSE optimization pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_cse(void);

/**
 * Get a reference to the Constant Folding optimization pass.
 * @return A pointer to the Constant Folding optimization pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_constant_folding(void);

/**
 * Get a reference to the Loop Optimization pass.
 * @return A pointer to the Loop Optimization pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_loop_optimizations(void);

/**
 * Get a reference to the Strength Reduction pass.
 * @return A pointer to the Strength Reduction pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_strength_reduction(void);

/**
 * Get a reference to the CFG Simplification pass.
 * @return A pointer to the CFG Simplification pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_cfg_simplify(void);

/**
 * Get a reference to the Peephole Optimization pass.
 * @return A pointer to the Peephole Optimization pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_peephole(void);

/**
 * Get a reference to the Register Allocation pass.
 * @return A pointer to the Register Allocation pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_register_allocation(void);

/**
 * Get a reference to the Inline Expansion pass.
 * @return A pointer to the Inline Expansion pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_inline(void);

/**
 * Get a reference to the Tail Call Optimization pass.
 * @return A pointer to the Tail Call Optimization pass structure.
 */
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_tail_calls(void);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_OPTIMIZE_H */
