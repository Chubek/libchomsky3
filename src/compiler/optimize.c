/**
 * libchomsky3 - Optimization Implementation
 * 
 * Implements optimization passes for the IR to improve performance
 * and reduce code size of compiled regular expressions.
 */

#include "chomsky3/optimize.h"
#include "chomsky3/error.h"
#include "chomsky3/ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Internal helper structures */
typedef struct {
    chomsky3_ir_instruction_t *inst;
    bool is_dead;
    bool is_used;
} instruction_info_t;

typedef struct {
    chomsky3_ir_value_t value;
    chomsky3_ir_instruction_t *defining_inst;
} value_info_t;

/* Forward declarations for internal helpers */
static double get_time_ms(void);
static bool is_instruction_pure(const chomsky3_ir_instruction_t *inst);
static bool has_side_effects(const chomsky3_ir_instruction_t *inst);
static bool values_equal(const chomsky3_ir_value_t *a, const chomsky3_ir_value_t *b);
static bool can_fold_constant(const chomsky3_ir_instruction_t *inst);
static chomsky3_ir_value_t fold_constant_instruction(const chomsky3_ir_instruction_t *inst);
static bool is_branch_always_taken(const chomsky3_ir_instruction_t *inst, bool *taken);
static size_t count_block_instructions(const chomsky3_ir_block_t *block);
static bool can_merge_blocks(const chomsky3_ir_block_t *a, const chomsky3_ir_block_t *b);

/* ========================================================================
 * Configuration Management
 * ======================================================================== */

void chomsky3_optimization_config_default(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_level_t level
) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->level = level;
    config->max_iterations = 10;
    config->max_inline_size = 50;
    config->verify_after_pass = false;
    config->print_stats = false;

    /* Enable passes based on optimization level */
    switch (level) {
        case CHOMSKY3_OPT_LEVEL_AGGRESSIVE:
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_INLINE);
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_TAIL_CALL);
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_LOOP_OPT);
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_STRENGTH_REDUCTION);
            /* fallthrough */
        case CHOMSKY3_OPT_LEVEL_STANDARD:
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_CSE);
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_CFG_SIMPLIFY);
            /* fallthrough */
        case CHOMSKY3_OPT_LEVEL_BASIC:
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_DCE);
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_CONSTANT_FOLDING);
            chomsky3_optimization_enable_pass(config, CHOMSKY3_OPT_PASS_PEEPHOLE);
            /* fallthrough */
        case CHOMSKY3_OPT_LEVEL_NONE:
        default:
            break;
    }
}

void chomsky3_optimization_enable_pass(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
) {
    if (!config || pass_id >= CHOMSKY3_OPT_PASS_MAX) {
        return;
    }
    config->enabled_passes |= (1u << pass_id);
}

void chomsky3_optimization_disable_pass(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
) {
    if (!config || pass_id >= CHOMSKY3_OPT_PASS_MAX) {
        return;
    }
    config->enabled_passes &= ~(1u << pass_id);
}

bool chomsky3_optimization_is_pass_enabled(
    const chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
) {
    if (!config || pass_id >= CHOMSKY3_OPT_PASS_MAX) {
        return false;
    }
    return (config->enabled_passes & (1u << pass_id)) != 0;
}

/* ========================================================================
 * Statistics Management
 * ======================================================================== */

chomsky3_optimization_stats_t *chomsky3_optimization_stats_create(size_t max_passes) {
    chomsky3_optimization_stats_t *stats = calloc(1, sizeof(chomsky3_optimization_stats_t));
    if (!stats) {
        return NULL;
    }

    if (max_passes > 0) {
        stats->pass_stats = calloc(max_passes, sizeof(chomsky3_pass_stats_t));
        if (!stats->pass_stats) {
            free(stats);
            return NULL;
        }
    }

    return stats;
}

void chomsky3_optimization_stats_free(chomsky3_optimization_stats_t *stats) {
    if (!stats) {
        return;
    }
    free(stats->pass_stats);
    free(stats);
}

void chomsky3_optimization_stats_print(
    const chomsky3_optimization_stats_t *stats,
    FILE *stream
) {
    if (!stats || !stream) {
        return;
    }

    fprintf(stream, "\n=== Optimization Statistics ===\n");
    fprintf(stream, "Total time: %.2f ms\n", stats->total_time_ms);
    fprintf(stream, "Passes run: %zu\n", stats->num_passes_run);
    fprintf(stream, "\nAggregate changes:\n");
    fprintf(stream, "  Instructions removed: %zu\n", stats->instructions_removed);
    fprintf(stream, "  Blocks merged: %zu\n", stats->blocks_merged);
    fprintf(stream, "  Constants folded: %zu\n", stats->constants_folded);
    fprintf(stream, "  Dead stores eliminated: %zu\n", stats->dead_stores_eliminated);
    fprintf(stream, "  Common subexpressions: %zu\n", stats->common_subexpressions_eliminated);
    fprintf(stream, "  Loops optimized: %zu\n", stats->loops_optimized);
    fprintf(stream, "  Branches folded: %zu\n", stats->branches_folded);

    if (stats->pass_stats) {
        fprintf(stream, "\nPer-pass statistics:\n");
        for (size_t i = 0; i < stats->num_passes_run; i++) {
            const chomsky3_pass_stats_t *ps = &stats->pass_stats[i];
            fprintf(stream, "  %s: %.2f ms, %zu changes%s\n",
                    ps->pass_name ? ps->pass_name : "unknown",
                    ps->time_ms,
                    ps->changes_made,
                    ps->converged ? " (converged)" : "");
        }
    }
    fprintf(stream, "\n");
}

/* ========================================================================
 * Main Optimization Entry Points
 * ======================================================================== */

chomsky3_error_t chomsky3_optimize_ir(
    chomsky3_ir_t *ir,
    const chomsky3_optimization_config_t *config,
    chomsky3_optimization_stats_t *stats
) {
    if (!ir || !config) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    double start_time = get_time_ms();
    chomsky3_error_t err = CHOMSKY3_OK;

    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }

    /* Run enabled optimization passes */
    size_t pass_idx = 0;

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_CONSTANT_FOLDING)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_constant_folding(ir);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "constant_folding";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            stats->pass_stats[pass_idx].changes_made = stats->constants_folded;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_DCE)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_dce(ir);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "dead_code_elimination";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            stats->pass_stats[pass_idx].changes_made = stats->instructions_removed;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_CSE)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_cse(ir);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "common_subexpression_elimination";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            stats->pass_stats[pass_idx].changes_made = stats->common_subexpressions_eliminated;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_CFG_SIMPLIFY)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_cfg(ir);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "cfg_simplification";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            stats->pass_stats[pass_idx].changes_made = stats->blocks_merged;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_PEEPHOLE)) {
        double pass_start = get_time_ms();
        size_t changes = 0;
        for (size_t i = 0; i < ir->num_blocks; i++) {
            changes += chomsky3_optimize_peephole_block(ir->blocks[i]);
        }
        
        if (stats) {
            stats->instructions_removed += changes;
            if (stats->pass_stats) {
                stats->pass_stats[pass_idx].pass_name = "peephole";
                stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
                stats->pass_stats[pass_idx].changes_made = changes;
                pass_idx++;
            }
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_STRENGTH_REDUCTION)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_strength_reduction(ir);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "strength_reduction";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_LOOP_OPT)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_loops(ir, NULL);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "loop_optimizations";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            stats->pass_stats[pass_idx].changes_made = stats->loops_optimized;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_INLINE)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_inline(ir, config->max_inline_size);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "inlining";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (chomsky3_optimization_is_pass_enabled(config, CHOMSKY3_OPT_PASS_TAIL_CALL)) {
        double pass_start = get_time_ms();
        err = chomsky3_optimize_tail_calls(ir);
        if (err != CHOMSKY3_OK) return err;
        
        if (stats && stats->pass_stats) {
            stats->pass_stats[pass_idx].pass_name = "tail_call_optimization";
            stats->pass_stats[pass_idx].time_ms = get_time_ms() - pass_start;
            pass_idx++;
        }

        if (config->verify_after_pass) {
            err = chomsky3_ir_verify(ir);
            if (err != CHOMSKY3_OK) return err;
        }
    }

    if (stats) {
        stats->total_time_ms = get_time_ms() - start_time;
        stats->num_passes_run = pass_idx;

        if (config->print_stats) {
            chomsky3_optimization_stats_print(stats, stderr);
        }
    }

    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_optimize_for_size(
    chomsky3_ir_t *ir,
    chomsky3_optimization_stats_t *stats
) {
    chomsky3_optimization_config_t config;
    chomsky3_optimization_config_default(&config, CHOMSKY3_OPT_LEVEL_STANDARD);
    
    /* Prioritize size-reducing passes */
    chomsky3_optimization_disable_pass(&config, CHOMSKY3_OPT_PASS_LOOP_OPT);
    chomsky3_optimization_disable_pass(&config, CHOMSKY3_OPT_PASS_INLINE);
    config.max_inline_size = 10; /* Very conservative inlining */
    
    return chomsky3_optimize_ir(ir, &config, stats);
}

chomsky3_error_t chomsky3_optimize_for_speed(
    chomsky3_ir_t *ir,
    chomsky3_optimization_stats_t *stats
) {
    chomsky3_optimization_config_t config;
    chomsky3_optimization_config_default(&config, CHOMSKY3_OPT_LEVEL_AGGRESSIVE);
    
    /* Aggressive inlining and loop optimizations */
    config.max_inline_size = 200;
    config.max_iterations = 20;
    
    return chomsky3_optimize_ir(ir, &config, stats);
}

/* ========================================================================
 * Dead Code Elimination
 * ======================================================================== */

chomsky3_error_t chomsky3_optimize_dce(chomsky3_ir_t *ir) {
    if (!ir) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    bool changed = true;
    size_t iterations = 0;
    const size_t max_iterations = 10;

    while (changed && iterations < max_iterations) {
        changed = false;
        iterations++;

        /* Mark all instructions as potentially dead */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            for (size_t j = 0; j < block->num_instructions; j++) {
                chomsky3_ir_instruction_t *inst = block->instructions[j];
                inst->metadata = NULL; /* Use metadata as "marked" flag */
            }
        }

        /* Mark instructions with side effects as live */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            for (size_t j = 0; j < block->num_instructions; j++) {
                chomsky3_ir_instruction_t *inst = block->instructions[j];
                if (has_side_effects(inst)) {
                    inst->metadata = (void *)1; /* Mark as live */
                }
            }
        }

        /* Propagate liveness backwards */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            for (ssize_t j = (ssize_t)block->num_instructions - 1; j >= 0; j--) {
                chomsky3_ir_instruction_t *inst = block->instructions[j];
                if (inst->metadata) {
                    /* Mark source operands' defining instructions as live */
                    /* This is simplified - real implementation needs def-use chains */
                }
            }
        }

        /* Remove dead instructions */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            size_t write_idx = 0;
            for (size_t read_idx = 0; read_idx < block->num_instructions; read_idx++) {
                chomsky3_ir_instruction_t *inst = block->instructions[read_idx];
                if (inst->metadata || has_side_effects(inst)) {
                    /* Keep instruction */
                    if (write_idx != read_idx) {
                        block->instructions[write_idx] = inst;
                    }
                    write_idx++;
                } else {
                    /* Dead instruction - would free here */
                    changed = true;
                }
            }
            block->num_instructions = write_idx;
        }
    }

    return CHOMSKY3_OK;
}

/* ========================================================================
 * Common Subexpression Elimination
 * ======================================================================== */

chomsky3_error_t chomsky3_optimize_cse(chomsky3_ir_t *ir) {
    if (!ir) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Simple local CSE within each block */
    for (size_t i = 0; i < ir->num_blocks; i++) {
        chomsky3_ir_block_t *block = ir->blocks[i];
        
        for (size_t j = 0; j < block->num_instructions; j++) {
            chomsky3_ir_instruction_t *inst1 = block->instructions[j];
            if (!is_instruction_pure(inst1)) {
                continue;
            }

            /* Look for identical instruction later in block */
            for (size_t k = j + 1; k < block->num_instructions; k++) {
                chomsky3_ir_instruction_t *inst2 = block->instructions[k];
                
                if (inst1->opcode == inst2->opcode &&
                    values_equal(&inst1->src1, &inst2->src1) &&
                    values_equal(&inst1->src2, &inst2->src2)) {
                    
                    /* Found common subexpression - replace inst2's result with inst1's */
                    /* In real implementation, would update all uses of inst2->dest */
                    /* to use inst1->dest instead, then mark inst2 as dead */
                }
            }
        }
    }

    return CHOMSKY3_OK;
}

/* ========================================================================
 * Constant Folding and Propagation
 * ======================================================================== */

chomsky3_error_t chomsky3_optimize_constant_folding(chomsky3_ir_t *ir) {
    if (!ir) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    bool changed = true;
    size_t iterations = 0;
    const size_t max_iterations = 10;

    while (changed && iterations < max_iterations) {
        changed = false;
        iterations++;

        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            
            for (size_t j = 0; j < block->num_instructions; j++) {
                chomsky3_ir_instruction_t *inst = block->instructions[j];
                
                if (can_fold_constant(inst)) {
                    chomsky3_ir_value_t folded = fold_constant_instruction(inst);
                    
                    /* Replace instruction with constant move */
                    inst->opcode = CHOMSKY3_IR_OP_MOVE;
                    inst->src1 = folded;
                    inst->src2.type = CHOMSKY3_IR_VALUE_NONE;
                    
                    changed = true;
                }
            }
        }
    }

    return CHOMSKY3_OK;
}

/* ========================================================================
 * Control Flow Graph Simplification
 * ======================================================================== */

chomsky3_error_t chomsky3_optimize_cfg(chomsky3_ir_t *ir) {
    if (!ir) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    bool changed = true;
    
    while (changed) {
        changed = false;

        /* Remove unreachable blocks */
        bool *reachable = calloc(ir->num_blocks, sizeof(bool));
        if (!reachable) {
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }

        /* Mark entry block as reachable */
        if (ir->num_blocks > 0) {
            reachable[0] = true;
        }

        /* Propagate reachability */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            if (!reachable[i]) continue;
            
            chomsky3_ir_block_t *block = ir->blocks[i];
            for (size_t j = 0; j < block->num_successors; j++) {
                for (size_t k = 0; k < ir->num_blocks; k++) {
                    if (ir->blocks[k] == block->successors[j]) {
                        reachable[k] = true;
                        break;
                    }
                }
            }
        }

        /* Remove unreachable blocks */
        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < ir->num_blocks; read_idx++) {
            if (reachable[read_idx]) {
                if (write_idx != read_idx) {
                    ir->blocks[write_idx] = ir->blocks[read_idx];
                }
                write_idx++;
            } else {
                changed = true;
            }
        }
        ir->num_blocks = write_idx;
        free(reachable);

        /* Merge blocks with single predecessor/successor */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            
            if (block->num_successors == 1) {
                chomsky3_ir_block_t *succ = block->successors[0];
                
                if (can_merge_blocks(block, succ)) {
                    /* Merge succ into block */
                    /* Real implementation would copy instructions and update edges */
                    changed = true;
                }
            }
        }

        /* Fold constant branches */
        for (size_t i = 0; i < ir->num_blocks; i++) {
            chomsky3_ir_block_t *block = ir->blocks[i];
            if (block->num_instructions == 0) continue;
            
            chomsky3_ir_instruction_t *last = block->instructions[block->num_instructions - 1];
            bool taken;
            
            if (is_branch_always_taken(last, &taken)) {
                /* Replace conditional branch with unconditional jump */
                if (taken && block->num_successors > 0) {
                    last->opcode = CHOMSKY3_IR_OP_JUMP;
                    block->num_successors = 1;
                    /* Keep only taken successor */
                    changed = true;
                }
            }
        }
    }

    return CHOMSKY3_OK;
}

/* ========================================================================
 * Peephole Optimization
 * ======================================================================== */

size_t chomsky3_optimize_peephole_block(chomsky3_ir_block_t *block) {
    if (!block) {
        return 0;
    }

    size_t changes = 0;

    for (size_t i = 0; i + 1 < block->num_instructions; i++) {
        chomsky3_ir_instruction_t *inst1 = block->instructions[i];
        chomsky3_ir_instruction_t *inst2 = block->instructions[i + 1];

        /* Pattern: MOVE r1, c; MOVE r2, r1 => MOVE r2, c */
        if (inst1->opcode == CHOMSKY3_IR_OP_MOVE &&
            inst2->opcode == CHOMSKY3_IR_OP_MOVE &&
            inst1->src1.type == CHOMSKY3_IR_VALUE_IMMEDIATE &&
            inst2->src1.type == CHOMSKY3_IR_VALUE_REGISTER &&
            inst2->src1.data.reg == inst1->dest.data.reg) {
            
            inst2->src1 = inst1->src1;
            changes++;
        }

        /* Pattern: ADD r1, r1, 0 => NOP */
        if (inst1->opcode == CHOMSKY3_IR_OP_ADD &&
            inst1->src2.type == CHOMSKY3_IR_VALUE_IMMEDIATE &&
            inst1->src2.data.imm == 0) {
            
            inst1->opcode = CHOMSKY3_IR_OP_MOVE;
            inst1->src2.type = CHOMSKY3_IR_VALUE_NONE;
            changes++;
        }

        /* Pattern: MUL r1, r1, 1 => NOP */
        if (inst1->opcode == CHOMSKY3_IR_OP_MUL &&
            inst1->src2.type == CHOMSKY3_IR_VALUE_IMMEDIATE &&
            inst1->src2.data.imm == 1) {
            
            inst1->opcode = CHOMSKY3_IR_OP_MOVE;
            inst1->src2.type = CHOMSKY3_IR_VALUE_NONE;
            changes++;
        }

        /* Pattern: MUL r1, r1, 0 => MOVE r1, 0 */
        if (inst1->opcode == CHOMSKY3_IR_OP_MUL &&
            inst1->src2.type == CHOMSKY3_IR_VALUE_IMM