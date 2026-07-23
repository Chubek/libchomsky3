/**
 * libchomsky3 - Optimization Pass Implementation
 *
 * Implements the optimization pipeline declared in chomsky3/optimize.h:
 * pass configuration, statistics, and the concrete passes that transform
 * the IR (constant folding, dead code elimination, peephole, CFG
 * simplification, strength reduction, CSE, and loop handling).
 */

#include "chomsky3/optimize.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

void chomsky3_optimization_config_default(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_level_t level
) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->level = level;
    config->max_iterations = 4;
    config->max_inline_size = 64;
    config->verify_after_pass = false;
    config->print_stats = false;

    switch (level) {
        case CHOMSKY3_OPT_LEVEL_AGGRESSIVE:
            config->enabled_passes |= (1u << CHOMSKY3_OPT_PASS_INLINE) |
                                      (1u << CHOMSKY3_OPT_PASS_LOOP_OPT) |
                                      (1u << CHOMSKY3_OPT_PASS_TAIL_CALL);
            /* fallthrough */
        case CHOMSKY3_OPT_LEVEL_STANDARD:
            config->enabled_passes |= (1u << CHOMSKY3_OPT_PASS_CSE) |
                                      (1u << CHOMSKY3_OPT_PASS_STRENGTH_REDUCTION);
            /* fallthrough */
        case CHOMSKY3_OPT_LEVEL_BASIC:
            config->enabled_passes |= (1u << CHOMSKY3_OPT_PASS_DCE) |
                                      (1u << CHOMSKY3_OPT_PASS_CONSTANT_FOLDING) |
                                      (1u << CHOMSKY3_OPT_PASS_CFG_SIMPLIFY) |
                                      (1u << CHOMSKY3_OPT_PASS_PEEPHOLE);
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
    if (!config || pass_id >= CHOMSKY3_OPT_PASS_MAX) return;
    config->enabled_passes |= (1u << pass_id);
}

void chomsky3_optimization_disable_pass(
    chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
) {
    if (!config || pass_id >= CHOMSKY3_OPT_PASS_MAX) return;
    config->enabled_passes &= ~(1u << pass_id);
}

bool chomsky3_optimization_is_pass_enabled(
    const chomsky3_optimization_config_t *config,
    chomsky3_opt_pass_id_t pass_id
) {
    if (!config || pass_id >= CHOMSKY3_OPT_PASS_MAX) return false;
    return (config->enabled_passes & (1u << pass_id)) != 0;
}

/* ------------------------------------------------------------------ */
/* Statistics                                                          */
/* ------------------------------------------------------------------ */

chomsky3_optimization_stats_t *chomsky3_optimization_stats_create(size_t max_passes) {
    chomsky3_optimization_stats_t *stats = calloc(1, sizeof(*stats));
    if (!stats) return NULL;

    if (max_passes > 0) {
        stats->pass_stats = calloc(max_passes, sizeof(*stats->pass_stats));
        if (!stats->pass_stats) {
            free(stats);
            return NULL;
        }
    }
    return stats;
}

void chomsky3_optimization_stats_free(chomsky3_optimization_stats_t *stats) {
    if (!stats) return;
    free(stats->pass_stats);
    free(stats);
}

void chomsky3_optimization_stats_print(
    const chomsky3_optimization_stats_t *stats,
    FILE *stream
) {
    if (!stats || !stream) return;

    fprintf(stream, "Optimization statistics:\n");
    fprintf(stream, "  total time:                  %.3f ms\n", stats->total_time_ms);
    fprintf(stream, "  passes run:                  %zu\n", stats->num_passes_run);
    fprintf(stream, "  instructions removed:        %zu\n", stats->instructions_removed);
    fprintf(stream, "  blocks merged:               %zu\n", stats->blocks_merged);
    fprintf(stream, "  constants folded:            %zu\n", stats->constants_folded);
    fprintf(stream, "  dead stores eliminated:      %zu\n", stats->dead_stores_eliminated);
    fprintf(stream, "  common subexprs eliminated:  %zu\n", stats->common_subexpressions_eliminated);
    fprintf(stream, "  loops optimized:             %zu\n", stats->loops_optimized);
    fprintf(stream, "  branches folded:             %zu\n", stats->branches_folded);

    if (stats->pass_stats) {
        for (size_t i = 0; i < stats->num_passes_run; i++) {
            const chomsky3_pass_stats_t *p = &stats->pass_stats[i];
            fprintf(stream, "  pass %-28s %6zu changes, %8.3f ms%s\n",
                    p->pass_name ? p->pass_name : "(unnamed)",
                    p->changes_made, p->time_ms,
                    p->converged ? "" : " (not converged)");
        }
    }
}

static double opt_now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* ------------------------------------------------------------------ */
/* Pass helpers                                                        */
/* ------------------------------------------------------------------ */

static bool value_is_register(const chomsky3_ir_value_t *v) {
    return v->type == CHOMSKY3_IR_VALUE_REGISTER;
}

static bool value_is_immediate(const chomsky3_ir_value_t *v) {
    return v->type == CHOMSKY3_IR_VALUE_IMMEDIATE;
}

/* Opcodes that produce a value but have no externally visible effect when
 * their destination register is dead. */
static bool opcode_is_pure(chomsky3_ir_opcode_t op) {
    switch (op) {
        case CHOMSKY3_IR_OP_ADD:
        case CHOMSKY3_IR_OP_SUB:
        case CHOMSKY3_IR_OP_MUL:
        case CHOMSKY3_IR_OP_DIV:
        case CHOMSKY3_IR_OP_MOD:
        case CHOMSKY3_IR_OP_AND:
        case CHOMSKY3_IR_OP_OR:
        case CHOMSKY3_IR_OP_XOR:
        case CHOMSKY3_IR_OP_NOT:
        case CHOMSKY3_IR_OP_SHL:
        case CHOMSKY3_IR_OP_SHR:
        case CHOMSKY3_IR_OP_CMP_EQ:
        case CHOMSKY3_IR_OP_CMP_NE:
        case CHOMSKY3_IR_OP_CMP_LT:
        case CHOMSKY3_IR_OP_CMP_LE:
        case CHOMSKY3_IR_OP_CMP_GT:
        case CHOMSKY3_IR_OP_CMP_GE:
        case CHOMSKY3_IR_OP_LOAD:
            return true;
        default:
            return false;
    }
}

/* ------------------------------------------------------------------ */
/* Constant folding                                                    */
/* ------------------------------------------------------------------ */

/* Fold a binary arithmetic/logic op with two immediate sources into the
 * destination as an immediate. Returns true when the instruction was
 * replaced by an immediate load. */
static bool fold_binary_immediates(chomsky3_ir_instruction_t *inst) {
    if (!value_is_register(&inst->dest) ||
        !value_is_immediate(&inst->src1) ||
        !value_is_immediate(&inst->src2)) {
        return false;
    }

    int64_t a = inst->src1.data.immediate;
    int64_t b = inst->src2.data.immediate;
    int64_t result;

    switch (inst->opcode) {
        case CHOMSKY3_IR_OP_ADD: result = a + b; break;
        case CHOMSKY3_IR_OP_SUB: result = a - b; break;
        case CHOMSKY3_IR_OP_MUL: result = a * b; break;
        case CHOMSKY3_IR_OP_DIV:
            if (b == 0) return false;
            result = a / b;
            break;
        case CHOMSKY3_IR_OP_MOD:
            if (b == 0) return false;
            result = a % b;
            break;
        case CHOMSKY3_IR_OP_AND: result = a & b; break;
        case CHOMSKY3_IR_OP_OR:  result = a | b; break;
        case CHOMSKY3_IR_OP_XOR: result = a ^ b; break;
        case CHOMSKY3_IR_OP_SHL:
            if (b < 0 || b >= 64) return false;
            result = a << b;
            break;
        case CHOMSKY3_IR_OP_SHR:
            if (b < 0 || b >= 64) return false;
            result = (int64_t)((uint64_t)a >> (uint64_t)b);
            break;
        case CHOMSKY3_IR_OP_CMP_EQ: result = a == b; break;
        case CHOMSKY3_IR_OP_CMP_NE: result = a != b; break;
        case CHOMSKY3_IR_OP_CMP_LT: result = a < b;  break;
        case CHOMSKY3_IR_OP_CMP_LE: result = a <= b; break;
        case CHOMSKY3_IR_OP_CMP_GT: result = a > b;  break;
        case CHOMSKY3_IR_OP_CMP_GE: result = a >= b; break;
        default: return false;
    }

    inst->opcode = CHOMSKY3_IR_OP_ADD; /* becomes dest = 0 + result */
    inst->src1 = chomsky3_ir_value_immediate(0, inst->dest.data_type);
    inst->src2 = chomsky3_ir_value_immediate(result, inst->dest.data_type);
    return true;
}

static size_t fold_block(chomsky3_ir_block_t *block) {
    size_t folded = 0;
    for (chomsky3_ir_instruction_t *inst = block->first; inst; inst = inst->next) {
        if (fold_binary_immediates(inst)) {
            folded++;
            continue;
        }

        /* Identity folds: x+0, x-0, x*1, x/1, x|0, x^0, x&-1 keep src1. */
        if (value_is_immediate(&inst->src2)) {
            int64_t b = inst->src2.data.immediate;
            bool identity = false;
            switch (inst->opcode) {
                case CHOMSKY3_IR_OP_ADD:
                case CHOMSKY3_IR_OP_SUB:
                case CHOMSKY3_IR_OP_OR:
                case CHOMSKY3_IR_OP_XOR:
                case CHOMSKY3_IR_OP_SHL:
                case CHOMSKY3_IR_OP_SHR:
                    identity = (b == 0);
                    break;
                case CHOMSKY3_IR_OP_MUL:
                case CHOMSKY3_IR_OP_DIV:
                    identity = (b == 1);
                    break;
                case CHOMSKY3_IR_OP_AND:
                    identity = (b == -1);
                    break;
                default:
                    break;
            }
            if (identity && value_is_register(&inst->dest)) {
                inst->opcode = CHOMSKY3_IR_OP_ADD;
                inst->src2 = chomsky3_ir_value_immediate(0, inst->dest.data_type);
                folded++;
            }
        }

        /* Annihilator folds: x*0, x&0, 0*x. */
        if (value_is_register(&inst->dest)) {
            if ((inst->opcode == CHOMSKY3_IR_OP_MUL || inst->opcode == CHOMSKY3_IR_OP_AND) &&
                ((value_is_immediate(&inst->src2) && inst->src2.data.immediate == 0) ||
                 (value_is_immediate(&inst->src1) && inst->src1.data.immediate == 0))) {
                inst->opcode = CHOMSKY3_IR_OP_ADD;
                inst->src1 = chomsky3_ir_value_immediate(0, inst->dest.data_type);
                inst->src2 = chomsky3_ir_value_immediate(0, inst->dest.data_type);
                folded++;
            }
        }
    }
    return folded;
}

chomsky3_error_t chomsky3_optimize_constant_folding(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        fold_block(b);
    }
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Dead code elimination                                               */
/* ------------------------------------------------------------------ */

/* Backwards sweep per block: track live registers; a pure instruction whose
 * destination is never used later in the block (or after the block, per a
 * simple any-successor-use approximation) is removed. */
static size_t dce_block(chomsky3_ir_block_t *block) {
    size_t regs_cap = 64;
    size_t n_removed = 0;

    /* Live set as a growable bitmap. */
    uint64_t *live = calloc(regs_cap / 64, sizeof(uint64_t));
    if (!live) return 0;
    size_t live_words = regs_cap / 64;

    /* Seed: registers used by terminator are live. */
    if (block->last) {
        chomsky3_ir_instruction_t *t = block->last;
        for (int i = 0; i < 3; i++) {
            const chomsky3_ir_value_t *v = (i == 0) ? &t->src1 : (i == 1) ? &t->src2 : &t->src3;
            if (value_is_register(v)) {
                uint32_t r = v->data.reg;
                while (r / 64 >= live_words) {
                    size_t new_words = live_words * 2;
                    uint64_t *p = realloc(live, new_words * sizeof(uint64_t));
                    if (!p) { free(live); return 0; }
                    memset(p + live_words, 0, (new_words - live_words) * sizeof(uint64_t));
                    live = p;
                    live_words = new_words;
                }
                live[r / 64] |= UINT64_C(1) << (r % 64);
            }
        }
    }

    /* Backwards sweep. */
    for (chomsky3_ir_instruction_t *inst = block->last; inst; inst = inst->prev) {
        if (value_is_register(&inst->dest) && opcode_is_pure(inst->opcode)) {
            uint32_t r = inst->dest.data.reg;
            bool is_live = (r / 64 < live_words) &&
                           (live[r / 64] & (UINT64_C(1) << (r % 64)));
            if (!is_live) {
                chomsky3_ir_instruction_t *prev = inst->prev;
                chomsky3_ir_remove_instruction(inst);
                free(inst);
                n_removed++;
                inst = prev;
                continue;
            }
            /* dest is overwritten: kill it, then sources become live. */
            if (r / 64 < live_words) {
                live[r / 64] &= ~(UINT64_C(1) << (r % 64));
            }
        }

        for (int i = 0; i < 3; i++) {
            const chomsky3_ir_value_t *v = (i == 0) ? &inst->src1 : (i == 1) ? &inst->src2 : &inst->src3;
            if (!value_is_register(v)) continue;
            uint32_t r = v->data.reg;
            if (r / 64 >= live_words) {
                size_t new_words = live_words * 2;
                while (r / 64 >= new_words) new_words *= 2;
                uint64_t *p = realloc(live, new_words * sizeof(uint64_t));
                if (!p) { free(live); return n_removed; }
                memset(p + live_words, 0, (new_words - live_words) * sizeof(uint64_t));
                live = p;
                live_words = new_words;
            }
            live[r / 64] |= UINT64_C(1) << (r % 64);
        }
    }

    free(live);
    return n_removed;
}

chomsky3_error_t chomsky3_optimize_dce(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    size_t total_removed = 0;
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        total_removed += dce_block(b);
    }

    /* Remove unreachable blocks (not entry, no predecessors, not referenced
     * as a jump/split/branch target from anywhere). */
    chomsky3_ir_block_t *b = ir->first_block;
    while (b) {
        chomsky3_ir_block_t *next = b->next;
        if (b != ir->entry && b->num_predecessors == 0) {
            bool referenced = false;
            for (chomsky3_ir_block_t *other = ir->first_block; other && !referenced; other = other->next) {
                if (other == b) continue;
                for (chomsky3_ir_instruction_t *inst = other->first; inst; inst = inst->next) {
                    if ((inst->src1.type == CHOMSKY3_IR_VALUE_BLOCK && inst->src1.data.block == b) ||
                        (inst->src2.type == CHOMSKY3_IR_VALUE_BLOCK && inst->src2.data.block == b) ||
                        (inst->src3.type == CHOMSKY3_IR_VALUE_BLOCK && inst->src3.data.block == b)) {
                        referenced = true;
                        break;
                    }
                }
            }
            if (!referenced) {
                size_t before = ir->total_instructions;
                for (chomsky3_ir_instruction_t *inst = b->first; inst; ) {
                    chomsky3_ir_instruction_t *next_inst = inst->next;
                    free(inst);
                    inst = next_inst;
                }
                if (ir->total_instructions >= b->num_instructions) {
                    ir->total_instructions -= b->num_instructions;
                } else {
                    ir->total_instructions = before > b->num_instructions ? before - b->num_instructions : 0;
                }
                total_removed += b->num_instructions;
                free((char *)b->label);
                free(b->predecessors);
                free(b->successors);
                free(b->dominated);
                chomsky3_ir_remove_block(ir, b);
                free(b);
            }
        }
        b = next;
    }

    (void)total_removed;
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Common subexpression elimination (block-local)                      */
/* ------------------------------------------------------------------ */

typedef struct {
    chomsky3_ir_opcode_t opcode;
    int64_t imm1, imm2;
    bool has_imm1, has_imm2;
    uint32_t reg1, reg2;
    bool has_reg1, has_reg2;
    uint32_t dest_reg;
} cse_expr_t;

static bool cse_expr_matches(const cse_expr_t *e, const chomsky3_ir_instruction_t *inst) {
    if (e->opcode != inst->opcode) return false;
    if (e->has_imm1 && (!value_is_immediate(&inst->src1) || inst->src1.data.immediate != e->imm1)) return false;
    if (e->has_reg1 && (!value_is_register(&inst->src1) || inst->src1.data.reg != e->reg1)) return false;
    if (e->has_imm2 && (!value_is_immediate(&inst->src2) || inst->src2.data.immediate != e->imm2)) return false;
    if (e->has_reg2 && (!value_is_register(&inst->src2) || inst->src2.data.reg != e->reg2)) return false;
    return true;
}

chomsky3_error_t chomsky3_optimize_cse(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        cse_expr_t exprs[64];
        size_t num_exprs = 0;

        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            if (!opcode_is_pure(inst->opcode) || !value_is_register(&inst->dest)) {
                /* Side-effecting instruction: invalidate exprs that read
                 * memory (LOAD) since a STORE may have clobbered it. */
                if (inst->opcode == CHOMSKY3_IR_OP_STORE) num_exprs = 0;
                continue;
            }

            /* Any prior expression whose operand register this instruction
             * redefines is dead. */
            uint32_t dst = inst->dest.data.reg;
            size_t w = 0;
            for (size_t r = 0; r < num_exprs; r++) {
                bool clobbered =
                    (exprs[r].has_reg1 && exprs[r].reg1 == dst) ||
                    (exprs[r].has_reg2 && exprs[r].reg2 == dst) ||
                    exprs[r].dest_reg == dst;
                if (!clobbered) exprs[w++] = exprs[r];
            }
            num_exprs = w;

            /* Match against surviving expressions. */
            bool found = false;
            for (size_t r = 0; r < num_exprs; r++) {
                if (cse_expr_matches(&exprs[r], inst)) {
                    /* Replace with a copy of the first result. */
                    inst->opcode = CHOMSKY3_IR_OP_ADD;
                    inst->src1.type = CHOMSKY3_IR_VALUE_REGISTER;
                    inst->src1.data_type = inst->dest.data_type;
                    inst->src1.data.reg = exprs[r].dest_reg;
                    inst->src2 = chomsky3_ir_value_immediate(0, inst->dest.data_type);
                    found = true;
                    break;
                }
            }

            if (!found && num_exprs < 64) {
                cse_expr_t *e = &exprs[num_exprs++];
                memset(e, 0, sizeof(*e));
                e->opcode = inst->opcode;
                e->dest_reg = dst;
                if (value_is_immediate(&inst->src1)) { e->has_imm1 = true; e->imm1 = inst->src1.data.immediate; }
                if (value_is_register(&inst->src1))  { e->has_reg1 = true; e->reg1 = inst->src1.data.reg; }
                if (value_is_immediate(&inst->src2)) { e->has_imm2 = true; e->imm2 = inst->src2.data.immediate; }
                if (value_is_register(&inst->src2))  { e->has_reg2 = true; e->reg2 = inst->src2.data.reg; }
            }
        }
    }
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Strength reduction                                                  */
/* ------------------------------------------------------------------ */

chomsky3_error_t chomsky3_optimize_strength_reduction(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            if (!value_is_immediate(&inst->src2)) continue;
            int64_t c = inst->src2.data.immediate;

            /* Multiplication by a power of two -> shift. */
            if (inst->opcode == CHOMSKY3_IR_OP_MUL && c > 0 && (c & (c - 1)) == 0) {
                int shift = 0;
                while ((1LL << shift) < c) shift++;
                inst->opcode = CHOMSKY3_IR_OP_SHL;
                inst->src2 = chomsky3_ir_value_immediate(shift, inst->src2.data_type);
                continue;
            }

            /* Unsigned power-of-two division/modulo -> shift/mask. Only safe
             * for non-negative types; restrict to unsigned data types. */
            bool is_unsigned = (inst->src1.data_type == CHOMSKY3_IR_TYPE_U8) ||
                               (inst->src1.data_type == CHOMSKY3_IR_TYPE_U16) ||
                               (inst->src1.data_type == CHOMSKY3_IR_TYPE_U32) ||
                               (inst->src1.data_type == CHOMSKY3_IR_TYPE_U64);
            if (is_unsigned && c > 0 && (c & (c - 1)) == 0) {
                int shift = 0;
                while ((1LL << shift) < c) shift++;
                if (inst->opcode == CHOMSKY3_IR_OP_DIV) {
                    inst->opcode = CHOMSKY3_IR_OP_SHR;
                    inst->src2 = chomsky3_ir_value_immediate(shift, inst->src2.data_type);
                } else if (inst->opcode == CHOMSKY3_IR_OP_MOD) {
                    inst->opcode = CHOMSKY3_IR_OP_AND;
                    inst->src2 = chomsky3_ir_value_immediate(c - 1, inst->src2.data_type);
                }
            }
        }
    }
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Peephole                                                            */
/* ------------------------------------------------------------------ */

size_t chomsky3_optimize_peephole_block(chomsky3_ir_block_t *block) {
    if (!block) return 0;
    size_t changed = 0;

    for (chomsky3_ir_instruction_t *inst = block->first; inst; ) {
        chomsky3_ir_instruction_t *next = inst->next;

        /* Consecutive jumps: JUMP L; JUMP L' -> JUMP L (second is dead until
         * another block targets it; conservatively remove back-to-back). */
        if (inst->opcode == CHOMSKY3_IR_OP_JUMP && next &&
            next->opcode == CHOMSKY3_IR_OP_JUMP) {
            chomsky3_ir_remove_instruction(next);
            free(next);
            changed++;
            continue; /* recheck same inst against its new successor */
        }

        /* NOP elimination. */
        if (inst->opcode == CHOMSKY3_IR_OP_NOP) {
            chomsky3_ir_remove_instruction(inst);
            free(inst);
            changed++;
            inst = next;
            continue;
        }

        /* Copy to self: rX = rX + 0 (and similar) is removable. */
        if (inst->opcode == CHOMSKY3_IR_OP_ADD &&
            value_is_register(&inst->dest) &&
            value_is_register(&inst->src1) &&
            inst->dest.data.reg == inst->src1.data.reg &&
            value_is_immediate(&inst->src2) &&
            inst->src2.data.immediate == 0) {
            chomsky3_ir_remove_instruction(inst);
            free(inst);
            changed++;
            inst = next;
            continue;
        }

        /* Jump to the immediately following block: JUMP bb; bb: -> remove. */
        if (inst->opcode == CHOMSKY3_IR_OP_JUMP &&
            inst->src1.type == CHOMSKY3_IR_VALUE_BLOCK &&
            inst->src1.data.block && inst->src1.data.block == block->next &&
            inst == block->last) {
            chomsky3_ir_remove_instruction(inst);
            free(inst);
            changed++;
            inst = next;
            continue;
        }

        inst = next;
    }

    return changed;
}

/* ------------------------------------------------------------------ */
/* CFG simplification                                                  */
/* ------------------------------------------------------------------ */

chomsky3_error_t chomsky3_optimize_cfg(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    /* 1. Fold branches on constant conditions into unconditional jumps. */
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        chomsky3_ir_instruction_t *t = b->last;
        if (!t || t->opcode != CHOMSKY3_IR_OP_BRANCH) continue;
        if (!value_is_immediate(&t->src1)) continue;

        bool cond = t->src1.data.immediate != 0;
        chomsky3_ir_block_t *target = cond ? t->src2.data.block : t->src3.data.block;
        chomsky3_ir_block_t *dead = cond ? t->src3.data.block : t->src2.data.block;
        if (!target) continue;

        t->opcode = CHOMSKY3_IR_OP_JUMP;
        t->src1 = chomsky3_ir_value_block(target);
        t->src2.type = CHOMSKY3_IR_VALUE_NONE;
        t->src3.type = CHOMSKY3_IR_VALUE_NONE;

        /* Repair edges: b now points only at target. */
        for (size_t i = 0; i < b->num_successors; i++) {
            if (b->successors[i] == dead) {
                chomsky3_ir_remove_edge(b, dead);
                break;
            }
        }
    }

    /* 2. Merge single-successor/single-predecessor block pairs. */
    bool merged_any = true;
    while (merged_any) {
        merged_any = false;
        for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
            if (b->num_successors != 1) continue;
            chomsky3_ir_block_t *succ = b->successors[0];
            if (!succ || succ == b || succ->num_predecessors != 1) continue;
            if (succ == ir->entry) continue;

            /* b must end in an unconditional jump (or fall through). */
            chomsky3_ir_instruction_t *t = b->last;
            if (t && t->opcode != CHOMSKY3_IR_OP_JUMP) continue;
            if (t && t->opcode == CHOMSKY3_IR_OP_JUMP &&
                t->src1.type == CHOMSKY3_IR_VALUE_BLOCK &&
                t->src1.data.block != succ) continue;

            /* Remove the terminator jump, splice succ's instructions. */
            if (t) {
                chomsky3_ir_remove_instruction(t);
                free(t);
            }
            while (succ->first) {
                chomsky3_ir_instruction_t *inst = succ->first;
                chomsky3_ir_remove_instruction(inst);
                chomsky3_ir_append_instruction(b, inst);
            }

            /* Rewire edges: b inherits succ's successors. */
            while (succ->num_successors > 0) {
                chomsky3_ir_block_t *ss = succ->successors[0];
                chomsky3_ir_remove_edge(succ, ss);
                chomsky3_ir_add_edge(b, ss);
            }
            /* Redirect any jump targets that pointed at succ. */
            for (chomsky3_ir_block_t *other = ir->first_block; other; other = other->next) {
                for (chomsky3_ir_instruction_t *inst = other->first; inst; inst = inst->next) {
                    if (inst->src1.type == CHOMSKY3_IR_VALUE_BLOCK && inst->src1.data.block == succ)
                        inst->src1.data.block = b;
                    if (inst->src2.type == CHOMSKY3_IR_VALUE_BLOCK && inst->src2.data.block == succ)
                        inst->src2.data.block = b;
                    if (inst->src3.type == CHOMSKY3_IR_VALUE_BLOCK && inst->src3.data.block == succ)
                        inst->src3.data.block = b;
                }
            }

            if (ir->exit == succ) ir->exit = b;
            free((char *)succ->label);
            free(succ->predecessors);
            free(succ->successors);
            free(succ->dominated);
            chomsky3_ir_remove_block(ir, succ);
            free(succ);
            merged_any = true;
            break; /* restart scan: lists have changed */
        }
    }

    /* 3. Thread jumps through empty jump-only blocks. */
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            for (int i = 0; i < 3; i++) {
                chomsky3_ir_value_t *v = (i == 0) ? &inst->src1 : (i == 1) ? &inst->src2 : &inst->src3;
                if (v->type != CHOMSKY3_IR_VALUE_BLOCK) continue;
                chomsky3_ir_block_t *target = v->data.block;
                size_t guard = 32;
                while (target && target != ir->entry &&
                       target->num_instructions == 1 && target->first &&
                       target->first->opcode == CHOMSKY3_IR_OP_JUMP &&
                       target->first->src1.type == CHOMSKY3_IR_VALUE_BLOCK &&
                       guard-- > 0) {
                    chomsky3_ir_block_t *next_target = target->first->src1.data.block;
                    if (next_target == target) break;
                    target = next_target;
                }
                v->data.block = target;
            }
        }
    }

    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Loop optimizations                                                  */
/* ------------------------------------------------------------------ */

chomsky3_error_t chomsky3_optimize_loops(
    chomsky3_ir_t *ir,
    chomsky3_ir_analysis_t *analysis
) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    chomsky3_ir_analysis_t *own_analysis = NULL;
    if (!analysis) {
        if (chomsky3_ir_analyze(ir, &own_analysis) != CHOMSKY3_OK) {
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
        analysis = own_analysis;
    }

    /* Compute loop info when absent. */
    if (!analysis->loop_headers) {
        chomsky3_ir_compute_dominators(ir);
        chomsky3_ir_detect_loops(ir, analysis);
    }

    /* Loop-invariant code motion: in each detected loop, hoist pure
     * instructions whose source registers are defined outside the loop into
     * the loop header's predecessor chain is non-trivial in general; the
     * conservative version hoists instructions whose sources are all
     * immediates (loop-invariant by construction) to the header front. */
    for (size_t i = 0; i < analysis->num_loops; i++) {
        chomsky3_ir_block_t *header = analysis->loop_headers[i];
        if (!header) continue;

        /* Collect the natural loop: header plus blocks dominated by it that
         * can reach the header again. Conservative: blocks whose id is in
         * [header->id, max back-edge source id]. */
        uint32_t max_id = header->id;
        for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
            for (size_t s = 0; s < b->num_successors; s++) {
                if (b->successors[s] == header && b->id > max_id) max_id = b->id;
            }
        }

        for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
            if (b->id < header->id || b->id > max_id || b == header) continue;

            for (chomsky3_ir_instruction_t *inst = b->first; inst; ) {
                chomsky3_ir_instruction_t *next = inst->next;
                bool invariant = opcode_is_pure(inst->opcode) &&
                                 value_is_register(&inst->dest) &&
                                 !value_is_register(&inst->src1) &&
                                 !value_is_register(&inst->src2) &&
                                 !value_is_register(&inst->src3);
                if (invariant) {
                    chomsky3_ir_remove_instruction(inst);
                    if (header->first) {
                        chomsky3_ir_insert_instruction_before(header->first, inst);
                    } else {
                        chomsky3_ir_append_instruction(header, inst);
                    }
                }
                inst = next;
            }
        }
    }

    if (own_analysis) chomsky3_ir_analysis_free(own_analysis);
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Inlining                                                            */
/* ------------------------------------------------------------------ */

/* Inline single-call targets: a block consisting of a CALL to a block that
 * is smaller than max_size and ends in RETURN is replaced by the callee's
 * body. */
chomsky3_error_t chomsky3_optimize_inline(chomsky3_ir_t *ir, size_t max_size) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            if (inst->opcode != CHOMSKY3_IR_OP_CALL) continue;
            if (inst->src1.type != CHOMSKY3_IR_VALUE_BLOCK || !inst->src1.data.block) continue;

            chomsky3_ir_block_t *callee = inst->src1.data.block;
            if (callee == b || callee->num_instructions == 0 ||
                callee->num_instructions > max_size) continue;
            if (!callee->last || callee->last->opcode != CHOMSKY3_IR_OP_RETURN) continue;

            /* Copy callee body (minus the RETURN) at the call site. */
            for (chomsky3_ir_instruction_t *ci = callee->first; ci && ci != callee->last; ci = ci->next) {
                chomsky3_ir_instruction_t *copy = calloc(1, sizeof(*copy));
                if (!copy) return CHOMSKY3_ERR_OUT_OF_MEMORY;
                *copy = *ci;
                copy->next = NULL;
                copy->prev = NULL;
                copy->parent = NULL;
                chomsky3_ir_insert_instruction_before(inst, copy);
            }

            /* Remove the CALL itself. */
            chomsky3_ir_instruction_t *next = inst->next;
            chomsky3_ir_remove_instruction(inst);
            free(inst);
            inst = next ? next : b->last;
            if (!inst) break;
        }
    }
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Tail calls                                                          */
/* ------------------------------------------------------------------ */

/* Convert CALL immediately followed by RETURN to the same block into a
 * jump when the callee is the current block (direct self-tail-recursion). */
chomsky3_error_t chomsky3_optimize_tail_calls(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            if (inst->opcode != CHOMSKY3_IR_OP_CALL) continue;
            if (!inst->next || inst->next->opcode != CHOMSKY3_IR_OP_RETURN) continue;
            if (inst->src1.type != CHOMSKY3_IR_VALUE_BLOCK) continue;
            if (inst->src1.data.block != b) continue;

            /* Self tail call: replace CALL;RETURN with a jump to the block
             * start (the block itself). */
            chomsky3_ir_instruction_t *ret = inst->next;
            inst->opcode = CHOMSKY3_IR_OP_JUMP;
            chomsky3_ir_remove_instruction(ret);
            free(ret);
        }
    }
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* SSA                                                                 */
/* ------------------------------------------------------------------ */

/* Minimal SSA support: version registers on assignment. Full phi placement
 * requires dominance frontiers; the IR's regex-oriented control flow rarely
 * joins values, so renaming alone covers the common cases (per-block). */
chomsky3_error_t chomsky3_ir_to_ssa(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    uint32_t *versions = calloc(ir->num_registers ? ir->num_registers : 1, sizeof(uint32_t));
    if (!versions) return CHOMSKY3_ERR_OUT_OF_MEMORY;

    uint32_t next_version = ir->num_registers;

    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            /* Rename uses to the current version. */
            for (int i = 0; i < 3; i++) {
                chomsky3_ir_value_t *v = (i == 0) ? &inst->src1 : (i == 1) ? &inst->src2 : &inst->src3;
                if (value_is_register(v) && v->data.reg < ir->num_registers) {
                    uint32_t ver = versions[v->data.reg];
                    v->data.reg = ver ? ver : v->data.reg;
                }
            }
            /* Assign a fresh version to the definition. */
            if (value_is_register(&inst->dest) && inst->dest.data.reg < ir->num_registers) {
                versions[inst->dest.data.reg] = next_version;
                inst->dest.data.reg = next_version;
                next_version++;
            }
        }
    }

    ir->num_registers = next_version;
    free(versions);
    return CHOMSKY3_OK;
}

/* SSA destruction is the identity for this representation: versioned
 * registers are already plain registers; nothing maps versions back, which
 * is safe because downstream codegen treats each register independently. */
chomsky3_error_t chomsky3_ir_from_ssa(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Register allocation                                                 */
/* ------------------------------------------------------------------ */

/* Linear-scan-inspired register compaction: remap the (possibly sparse)
 * register namespace to a dense one and record the physical register count. */
chomsky3_error_t chomsky3_optimize_register_allocation(
    chomsky3_ir_t *ir,
    const chomsky3_ir_analysis_t *analysis
) {
    (void)analysis;
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    uint32_t old_n = ir->num_registers;
    if (old_n == 0) return CHOMSKY3_OK;

    uint32_t *remap = malloc(old_n * sizeof(uint32_t));
    if (!remap) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    for (uint32_t i = 0; i < old_n; i++) remap[i] = UINT32_MAX;

    uint32_t next = 0;
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            chomsky3_ir_value_t *vals[4] = { &inst->dest, &inst->src1, &inst->src2, &inst->src3 };
            for (int i = 0; i < 4; i++) {
                if (!value_is_register(vals[i])) continue;
                uint32_t r = vals[i]->data.reg;
                if (r >= old_n) continue;
                if (remap[r] == UINT32_MAX) remap[r] = next++;
                vals[i]->data.reg = remap[r];
            }
        }
    }

    ir->num_registers = next;
    free(remap);
    return CHOMSKY3_OK;
}

/* ------------------------------------------------------------------ */
/* Pass orchestration                                                  */
/* ------------------------------------------------------------------ */

typedef chomsky3_error_t (*opt_impl_fn)(chomsky3_ir_t *ir);

static chomsky3_error_t run_named_pass(
    chomsky3_ir_t *ir,
    chomsky3_opt_pass_id_t pass_id,
    const chomsky3_optimization_config_t *config
) {
    switch (pass_id) {
        case CHOMSKY3_OPT_PASS_DCE:               return chomsky3_optimize_dce(ir);
        case CHOMSKY3_OPT_PASS_CSE:               return chomsky3_optimize_cse(ir);
        case CHOMSKY3_OPT_PASS_CONSTANT_FOLDING:  return chomsky3_optimize_constant_folding(ir);
        case CHOMSKY3_OPT_PASS_LOOP_OPT:          return chomsky3_optimize_loops(ir, NULL);
        case CHOMSKY3_OPT_PASS_STRENGTH_REDUCTION:return chomsky3_optimize_strength_reduction(ir);
        case CHOMSKY3_OPT_PASS_CFG_SIMPLIFY:      return chomsky3_optimize_cfg(ir);
        case CHOMSKY3_OPT_PASS_PEEPHOLE: {
            for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
                chomsky3_optimize_peephole_block(b);
            }
            return CHOMSKY3_OK;
        }
        case CHOMSKY3_OPT_PASS_REGISTER_ALLOC:    return chomsky3_optimize_register_allocation(ir, NULL);
        case CHOMSKY3_OPT_PASS_INLINE:
            return chomsky3_optimize_inline(ir, config ? config->max_inline_size : 64);
        case CHOMSKY3_OPT_PASS_TAIL_CALL:         return chomsky3_optimize_tail_calls(ir);
        default:                                  return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
}

static const char *pass_id_name(chomsky3_opt_pass_id_t pass_id) {
    switch (pass_id) {
        case CHOMSKY3_OPT_PASS_DCE:               return "dead_code_elimination";
        case CHOMSKY3_OPT_PASS_CSE:               return "common_subexpression_elimination";
        case CHOMSKY3_OPT_PASS_CONSTANT_FOLDING:  return "constant_folding";
        case CHOMSKY3_OPT_PASS_LOOP_OPT:          return "loop_optimizations";
        case CHOMSKY3_OPT_PASS_STRENGTH_REDUCTION:return "strength_reduction";
        case CHOMSKY3_OPT_PASS_CFG_SIMPLIFY:      return "cfg_simplification";
        case CHOMSKY3_OPT_PASS_PEEPHOLE:          return "peephole";
        case CHOMSKY3_OPT_PASS_REGISTER_ALLOC:    return "register_allocation";
        case CHOMSKY3_OPT_PASS_INLINE:            return "inline";
        case CHOMSKY3_OPT_PASS_TAIL_CALL:         return "tail_call";
        default:                                  return "unknown";
    }
}

chomsky3_error_t chomsky3_optimize_ir(
    chomsky3_ir_t *ir,
    const chomsky3_optimization_config_t *config,
    chomsky3_optimization_stats_t *stats
) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    chomsky3_optimization_config_t default_config;
    if (!config) {
        chomsky3_optimization_config_default(&default_config, CHOMSKY3_OPT_LEVEL_STANDARD);
        config = &default_config;
    }

    double start = opt_now_ms();
    size_t pass_slot = 0;

    uint32_t max_iterations = config->max_iterations ? config->max_iterations : 1;

    /* Fixed-point over the enabled pipeline: repeat until a full iteration
     * makes no structural change (measured by instruction/block counts) or
     * the iteration cap is reached. */
    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        size_t blocks_before = ir->num_blocks;
        size_t insts_before = ir->total_instructions;

        for (chomsky3_opt_pass_id_t p = CHOMSKY3_OPT_PASS_DCE; p < CHOMSKY3_OPT_PASS_MAX; p++) {
            if (!chomsky3_optimization_is_pass_enabled(config, p)) continue;

            double pstart = opt_now_ms();
            chomsky3_error_t err = run_named_pass(ir, p, config);
            double ptime = opt_now_ms() - pstart;
            if (err != CHOMSKY3_OK) return err;

            if (config->verify_after_pass) {
                char *verify_error = NULL;
                if (!chomsky3_ir_verify(ir, &verify_error)) {
                    free(verify_error);
                    return CHOMSKY3_ERR_INTERNAL;
                }
            }

            if (stats) {
                if (!stats->pass_stats || pass_slot >= stats->num_passes_run) {
                    size_t new_cap = (pass_slot + 8);
                    chomsky3_pass_stats_t *grown = realloc(
                        stats->pass_stats, new_cap * sizeof(*stats->pass_stats));
                    if (grown) {
                        memset(grown + stats->num_passes_run, 0,
                               (new_cap - stats->num_passes_run) * sizeof(*grown));
                        stats->pass_stats = grown;
                    }
                }
                if (stats->pass_stats) {
                    chomsky3_pass_stats_t *ps = &stats->pass_stats[pass_slot];
                    ps->pass_name = pass_id_name(p);
                    ps->time_ms = ptime;
                    ps->converged = true;
                }
                stats->num_passes_run = pass_slot + 1;
            }
            pass_slot++;
        }

        if (ir->num_blocks == blocks_before && ir->total_instructions == insts_before) {
            break; /* converged */
        }
    }

    if (stats) {
        stats->total_time_ms = opt_now_ms() - start;
        if (stats->print_stats || config->print_stats) {
            chomsky3_optimization_stats_print(stats, stderr);
        }
    }

    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_run_optimization_pass(
    chomsky3_ir_t *ir,
    const chomsky3_ir_pass_t *pass
) {
    return chomsky3_ir_run_pass(ir, pass);
}

chomsky3_error_t chomsky3_optimize_for_size(
    chomsky3_ir_t *ir,
    chomsky3_optimization_stats_t *stats
) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    chomsky3_optimization_config_t config;
    chomsky3_optimization_config_default(&config, CHOMSKY3_OPT_LEVEL_AGGRESSIVE);
    /* Size: drop unrolling/inline, keep everything that shrinks code. */
    chomsky3_optimization_disable_pass(&config, CHOMSKY3_OPT_PASS_INLINE);
    chomsky3_optimization_disable_pass(&config, CHOMSKY3_OPT_PASS_LOOP_OPT);
    return chomsky3_optimize_ir(ir, &config, stats);
}

chomsky3_error_t chomsky3_optimize_for_speed(
    chomsky3_ir_t *ir,
    chomsky3_optimization_stats_t *stats
) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    chomsky3_optimization_config_t config;
    chomsky3_optimization_config_default(&config, CHOMSKY3_OPT_LEVEL_AGGRESSIVE);
    return chomsky3_optimize_ir(ir, &config, stats);
}

/* ------------------------------------------------------------------ */
/* Standard pass descriptors                                           */
/* ------------------------------------------------------------------ */

/* Adapter: call the corresponding implementation with an IR + user_data
 * signature. `user_data` carries an optional size limit for inline. */
static chomsky3_error_t pass_adapter_dce(chomsky3_ir_t *ir, void *ud)           { (void)ud; return chomsky3_optimize_dce(ir); }
static chomsky3_error_t pass_adapter_cse(chomsky3_ir_t *ir, void *ud)           { (void)ud; return chomsky3_optimize_cse(ir); }
static chomsky3_error_t pass_adapter_fold(chomsky3_ir_t *ir, void *ud)          { (void)ud; return chomsky3_optimize_constant_folding(ir); }
static chomsky3_error_t pass_adapter_loops(chomsky3_ir_t *ir, void *ud)         { (void)ud; return chomsky3_optimize_loops(ir, NULL); }
static chomsky3_error_t pass_adapter_strength(chomsky3_ir_t *ir, void *ud)      { (void)ud; return chomsky3_optimize_strength_reduction(ir); }
static chomsky3_error_t pass_adapter_cfg(chomsky3_ir_t *ir, void *ud)           { (void)ud; return chomsky3_optimize_cfg(ir); }
static chomsky3_error_t pass_adapter_peephole(chomsky3_ir_t *ir, void *ud) {
    (void)ud;
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        chomsky3_optimize_peephole_block(b);
    }
    return CHOMSKY3_OK;
}
static chomsky3_error_t pass_adapter_regalloc(chomsky3_ir_t *ir, void *ud)      { (void)ud; return chomsky3_optimize_register_allocation(ir, NULL); }
static chomsky3_error_t pass_adapter_inline(chomsky3_ir_t *ir, void *ud)        { return chomsky3_optimize_inline(ir, ud ? *(size_t *)ud : 64); }
static chomsky3_error_t pass_adapter_tailcall(chomsky3_ir_t *ir, void *ud)      { (void)ud; return chomsky3_optimize_tail_calls(ir); }

#define DEFINE_PASS(name_var, pass_type, pass_name, adapter) \
    static const chomsky3_ir_pass_t name_var = { \
        .type = pass_type, \
        .name = pass_name, \
        .func = adapter, \
        .user_data = NULL, \
        .flags = 0 \
    }

DEFINE_PASS(g_pass_dce,       CHOMSKY3_IR_PASS_DCE,             "dead_code_elimination",           pass_adapter_dce);
DEFINE_PASS(g_pass_cse,       CHOMSKY3_IR_PASS_CSE,             "common_subexpression_elimination", pass_adapter_cse);
DEFINE_PASS(g_pass_fold,      CHOMSKY3_IR_PASS_CONSTANT_FOLD,   "constant_folding",                pass_adapter_fold);
DEFINE_PASS(g_pass_loops,     CHOMSKY3_IR_PASS_LOOP_INVARIANT,  "loop_optimizations",              pass_adapter_loops);
DEFINE_PASS(g_pass_strength,  CHOMSKY3_IR_PASS_STRENGTH_REDUCE, "strength_reduction",              pass_adapter_strength);
DEFINE_PASS(g_pass_cfg,       CHOMSKY3_IR_PASS_SIMPLIFY_CFG,    "cfg_simplification",              pass_adapter_cfg);
DEFINE_PASS(g_pass_peephole,  CHOMSKY3_IR_PASS_PEEPHOLE,        "peephole",                        pass_adapter_peephole);
DEFINE_PASS(g_pass_regalloc,  CHOMSKY3_IR_PASS_REGISTER_ALLOC,  "register_allocation",             pass_adapter_regalloc);
DEFINE_PASS(g_pass_inline,    CHOMSKY3_IR_PASS_INLINE,          "inline",                          pass_adapter_inline);
DEFINE_PASS(g_pass_tailcall,  CHOMSKY3_IR_PASS_TAIL_CALL,       "tail_call",                       pass_adapter_tailcall);

const chomsky3_ir_pass_t *chomsky3_get_opt_pass_dce(void)                { return &g_pass_dce; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_cse(void)                { return &g_pass_cse; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_constant_folding(void)   { return &g_pass_fold; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_loop_optimizations(void) { return &g_pass_loops; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_strength_reduction(void) { return &g_pass_strength; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_cfg_simplify(void)       { return &g_pass_cfg; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_peephole(void)           { return &g_pass_peephole; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_register_allocation(void){ return &g_pass_regalloc; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_inline(void)             { return &g_pass_inline; }
const chomsky3_ir_pass_t *chomsky3_get_opt_pass_tail_calls(void)         { return &g_pass_tailcall; }
