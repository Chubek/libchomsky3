/** Minimal optimization pass scaffolding. */

#include "chomsky3/optimize.h"

chomsky3_error_t chomsky3_optimize_ir(
    chomsky3_ir_t *ir,
    const chomsky3_optimization_config_t *config,
    chomsky3_optimization_stats_t *stats
) { (void)config; (void)stats; (void)stats; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_run_optimization_pass(chomsky3_ir_t *ir, const chomsky3_ir_pass_t *pass) { (void)pass; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_for_size(chomsky3_ir_t *ir, chomsky3_optimization_stats_t *stats) { (void)stats; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_for_speed(chomsky3_ir_t *ir, chomsky3_optimization_stats_t *stats) { (void)stats; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
size_t chomsky3_optimize_peephole_block(chomsky3_ir_block_t *block) { (void)block; return 0; }
chomsky3_error_t chomsky3_optimize_dce(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_cse(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_constant_folding(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_loops(chomsky3_ir_t *ir, chomsky3_ir_analysis_t *analysis) { (void)analysis; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_strength_reduction(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_cfg(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_inline(chomsky3_ir_t *ir, size_t max_size) { (void)max_size; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_tail_calls(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_ir_to_ssa(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_ir_from_ssa(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_optimize_register_allocation(chomsky3_ir_t *ir, const chomsky3_ir_analysis_t *analysis) { (void)analysis; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }

const chomsky3_ir_pass_t *chomsky3_get_opt_pass_dce(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_cse(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_constant_folding(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_loop_optimizations(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_strength_reduction(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_cfg_simplify(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_peephole(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_register_allocation(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_inline(void) { return NULL; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_tail_calls(void) { return NULL; }
