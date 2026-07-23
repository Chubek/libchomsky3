/** IR construction, mutation, and verification. */

#include "chomsky3/ir.h"
#include "chomsky3/optimize.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static char *dup_string(const char *str) {
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (copy) memcpy(copy, str, len + 1);
    return copy;
}

chomsky3_ir_t *chomsky3_ir_create(chomsky3_context_t *ctx, const char *name) {
    chomsky3_ir_t *ir = calloc(1, sizeof(*ir));
    if (!ir) return NULL;
    ir->ctx = ctx;
    ir->name = name ? dup_string(name) : NULL;
    return ir;
}

void chomsky3_ir_destroy(chomsky3_ir_t *ir) {
    if (!ir) return;
    chomsky3_ir_block_t *block = ir->first_block;
    while (block) {
        chomsky3_ir_block_t *next_block = block->next;
        chomsky3_ir_instruction_t *inst = block->first;
        while (inst) {
            chomsky3_ir_instruction_t *next_inst = inst->next;
            free(inst);
            inst = next_inst;
        }
        free((char *)block->label);
        free(block->predecessors);
        free(block->successors);
        free(block->dominated);
        free(block);
        block = next_block;
    }
    free((char *)ir->name);
    free(ir->string_constants);
    free(ir);
}

chomsky3_ir_builder_t *chomsky3_ir_builder_create(chomsky3_context_t *ctx) {
    chomsky3_ir_builder_t *builder = calloc(1, sizeof(*builder));
    if (builder) builder->ctx = ctx;
    return builder;
}

void chomsky3_ir_builder_destroy(chomsky3_ir_builder_t *builder) {
    free(builder);
}

chomsky3_error_t chomsky3_ir_build_from_ast(
    chomsky3_ir_builder_t *builder,
    const chomsky3_regex_t *ast,
    chomsky3_ir_t **ir
) {
    if (!builder || !ast || !ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    *ir = chomsky3_ir_create(builder->ctx, "regex");
    if (!*ir) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    (*ir)->source_ast = ast;
    (*ir)->source_pattern = ast->pattern;
    builder->ir = *ir;
    chomsky3_ir_block_t *entry = chomsky3_ir_block_create(builder, "entry");
    if (!entry) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    chomsky3_ir_append_block(*ir, entry);
    (*ir)->entry = entry;
    (*ir)->exit = entry;
    builder->current_block = entry;
    return CHOMSKY3_OK;
}

chomsky3_ir_block_t *chomsky3_ir_block_create(chomsky3_ir_builder_t *builder, const char *label) {
    chomsky3_ir_block_t *block = calloc(1, sizeof(*block));
    if (!block) return NULL;
    block->id = builder ? builder->next_block_id++ : 0;
    block->label = label ? dup_string(label) : NULL;
    return block;
}

void chomsky3_ir_append_block(chomsky3_ir_t *ir, chomsky3_ir_block_t *block) {
    if (!ir || !block) return;
    block->prev = ir->last_block;
    if (ir->last_block) ir->last_block->next = block;
    else ir->first_block = block;
    ir->last_block = block;
    ir->num_blocks++;
}

void chomsky3_ir_insert_block_after(chomsky3_ir_t *ir, chomsky3_ir_block_t *after, chomsky3_ir_block_t *block) {
    if (!ir || !block) return;
    if (!after) { chomsky3_ir_append_block(ir, block); return; }
    block->prev = after;
    block->next = after->next;
    if (after->next) after->next->prev = block;
    else ir->last_block = block;
    after->next = block;
    ir->num_blocks++;
}

void chomsky3_ir_remove_block(chomsky3_ir_t *ir, chomsky3_ir_block_t *block) {
    if (!ir || !block) return;
    if (block->prev) block->prev->next = block->next;
    else ir->first_block = block->next;
    if (block->next) block->next->prev = block->prev;
    else ir->last_block = block->prev;
    ir->num_blocks--;
}

chomsky3_error_t chomsky3_ir_add_edge(chomsky3_ir_block_t *from, chomsky3_ir_block_t *to) {
    if (!from || !to) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    chomsky3_ir_block_t **successors = realloc(from->successors, (from->num_successors + 1) * sizeof(*successors));
    if (!successors) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    from->successors = successors;
    from->successors[from->num_successors++] = to;
    chomsky3_ir_block_t **predecessors = realloc(to->predecessors, (to->num_predecessors + 1) * sizeof(*predecessors));
    if (!predecessors) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    to->predecessors = predecessors;
    to->predecessors[to->num_predecessors++] = from;
    return CHOMSKY3_OK;
}

void chomsky3_ir_remove_edge(chomsky3_ir_block_t *from, chomsky3_ir_block_t *to) {
    if (!from || !to) return;
    for (size_t i = 0; i < from->num_successors; i++) {
        if (from->successors[i] == to) {
            memmove(&from->successors[i], &from->successors[i + 1],
                    (from->num_successors - i - 1) * sizeof(*from->successors));
            from->num_successors--;
            break;
        }
    }
    for (size_t i = 0; i < to->num_predecessors; i++) {
        if (to->predecessors[i] == from) {
            memmove(&to->predecessors[i], &to->predecessors[i + 1],
                    (to->num_predecessors - i - 1) * sizeof(*to->predecessors));
            to->num_predecessors--;
            break;
        }
    }
}

chomsky3_ir_instruction_t *chomsky3_ir_instruction_create(chomsky3_ir_builder_t *builder, chomsky3_ir_opcode_t opcode) {
    chomsky3_ir_instruction_t *inst = calloc(1, sizeof(*inst));
    if (!inst) return NULL;
    inst->opcode = opcode;
    inst->id = builder ? builder->next_register++ : 0;
    return inst;
}

void chomsky3_ir_append_instruction(chomsky3_ir_block_t *block, chomsky3_ir_instruction_t *inst) {
    if (!block || !inst) return;
    inst->parent = block;
    inst->prev = block->last;
    if (block->last) block->last->next = inst;
    else block->first = inst;
    block->last = inst;
    block->num_instructions++;
}

void chomsky3_ir_insert_instruction_before(chomsky3_ir_instruction_t *before, chomsky3_ir_instruction_t *inst) {
    if (!before || !inst || !before->parent) return;
    chomsky3_ir_block_t *block = before->parent;
    inst->parent = block;
    inst->next = before;
    inst->prev = before->prev;
    if (before->prev) before->prev->next = inst;
    else block->first = inst;
    before->prev = inst;
    block->num_instructions++;
}

void chomsky3_ir_insert_instruction_after(chomsky3_ir_instruction_t *after, chomsky3_ir_instruction_t *inst) {
    if (!after || !inst || !after->parent) return;
    chomsky3_ir_block_t *block = after->parent;
    inst->parent = block;
    inst->prev = after;
    inst->next = after->next;
    if (after->next) after->next->prev = inst;
    else block->last = inst;
    after->next = inst;
    block->num_instructions++;
}

void chomsky3_ir_remove_instruction(chomsky3_ir_instruction_t *inst) {
    if (!inst || !inst->parent) return;
    chomsky3_ir_block_t *block = inst->parent;
    if (inst->prev) inst->prev->next = inst->next;
    else block->first = inst->next;
    if (inst->next) inst->next->prev = inst->prev;
    else block->last = inst->prev;
    block->num_instructions--;
    inst->next = NULL;
    inst->prev = NULL;
    inst->parent = NULL;
}

void chomsky3_ir_replace_instruction(chomsky3_ir_instruction_t *old, chomsky3_ir_instruction_t *new) {
    if (!old || !new || !old->parent) return;
    chomsky3_ir_block_t *block = old->parent;
    new->parent = block;
    new->prev = old->prev;
    new->next = old->next;
    if (old->prev) old->prev->next = new;
    else block->first = new;
    if (old->next) old->next->prev = new;
    else block->last = new;
    old->prev = NULL;
    old->next = NULL;
    old->parent = NULL;
}

chomsky3_ir_value_t chomsky3_ir_value_immediate(int64_t value, chomsky3_ir_type_t type) {
    chomsky3_ir_value_t v = { .type = CHOMSKY3_IR_VALUE_IMMEDIATE, .data_type = type };
    v.data.immediate = value;
    return v;
}

chomsky3_ir_value_t chomsky3_ir_value_register(chomsky3_ir_builder_t *builder, chomsky3_ir_type_t type) {
    chomsky3_ir_value_t v = { .type = CHOMSKY3_IR_VALUE_REGISTER, .data_type = type };
    v.data.reg = builder ? builder->next_register++ : 0;
    return v;
}

chomsky3_ir_value_t chomsky3_ir_value_label(chomsky3_ir_builder_t *builder) {
    chomsky3_ir_value_t v = { .type = CHOMSKY3_IR_VALUE_LABEL, .data_type = CHOMSKY3_IR_TYPE_U32 };
    v.data.label_id = builder ? builder->next_label++ : 0;
    return v;
}

chomsky3_ir_value_t chomsky3_ir_value_block(chomsky3_ir_block_t *block) {
    chomsky3_ir_value_t v = { .type = CHOMSKY3_IR_VALUE_BLOCK, .data_type = CHOMSKY3_IR_TYPE_PTR };
    v.data.block = block;
    return v;
}

chomsky3_ir_value_t chomsky3_ir_value_string(chomsky3_ir_builder_t *builder, const char *string) {
    (void)builder;
    chomsky3_ir_value_t v = { .type = CHOMSKY3_IR_VALUE_STRING, .data_type = CHOMSKY3_IR_TYPE_PTR };
    v.data.string = string;
    return v;
}

static chomsky3_ir_instruction_t *build_simple(chomsky3_ir_builder_t *builder, chomsky3_ir_opcode_t opcode) {
    chomsky3_ir_instruction_t *inst = chomsky3_ir_instruction_create(builder, opcode);
    if (builder && builder->current_block && inst) chomsky3_ir_append_instruction(builder->current_block, inst);
    if (builder && builder->ir) builder->ir->total_instructions++;
    return inst;
}

chomsky3_ir_instruction_t *chomsky3_ir_build_match_char(chomsky3_ir_builder_t *builder, uint32_t ch) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_MATCH_CHAR); if (i) i->src1 = chomsky3_ir_value_immediate(ch, CHOMSKY3_IR_TYPE_U32); return i; }
chomsky3_ir_instruction_t *chomsky3_ir_build_match_range(chomsky3_ir_builder_t *builder, uint32_t start, uint32_t end) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_MATCH_RANGE); if (i) { i->src1 = chomsky3_ir_value_immediate(start, CHOMSKY3_IR_TYPE_U32); i->src2 = chomsky3_ir_value_immediate(end, CHOMSKY3_IR_TYPE_U32); } return i; }
chomsky3_ir_instruction_t *chomsky3_ir_build_jump(chomsky3_ir_builder_t *builder, chomsky3_ir_block_t *target) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_JUMP); if (i) i->src1 = chomsky3_ir_value_block(target); return i; }
chomsky3_ir_instruction_t *chomsky3_ir_build_branch(chomsky3_ir_builder_t *builder, chomsky3_ir_value_t condition, chomsky3_ir_block_t *true_block, chomsky3_ir_block_t *false_block) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_BRANCH); if (i) { i->src1 = condition; i->src2 = chomsky3_ir_value_block(true_block); i->src3 = chomsky3_ir_value_block(false_block); } return i; }
chomsky3_ir_instruction_t *chomsky3_ir_build_split(chomsky3_ir_builder_t *builder, chomsky3_ir_block_t *target1, chomsky3_ir_block_t *target2) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_SPLIT); if (i) { i->src1 = chomsky3_ir_value_block(target1); i->src2 = chomsky3_ir_value_block(target2); } return i; }
chomsky3_ir_instruction_t *chomsky3_ir_build_capture_start(chomsky3_ir_builder_t *builder, uint32_t group_id) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_CAPTURE_START); if (i) i->src1 = chomsky3_ir_value_immediate(group_id, CHOMSKY3_IR_TYPE_U32); return i; }
chomsky3_ir_instruction_t *chomsky3_ir_build_capture_end(chomsky3_ir_builder_t *builder, uint32_t group_id) { chomsky3_ir_instruction_t *i = build_simple(builder, CHOMSKY3_IR_OP_CAPTURE_END); if (i) i->src1 = chomsky3_ir_value_immediate(group_id, CHOMSKY3_IR_TYPE_U32); return i; }

chomsky3_error_t chomsky3_ir_run_pass(chomsky3_ir_t *ir, const chomsky3_ir_pass_t *pass) {
    if (!ir || !pass) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    /* Custom passes carry their own function; standard passes dispatch on
     * the pass type to the built-in implementations in optimize.c. */
    if (pass->func) {
        return pass->func(ir, pass->user_data);
    }

    switch (pass->type) {
        case CHOMSKY3_IR_PASS_DCE:            return chomsky3_optimize_dce(ir);
        case CHOMSKY3_IR_PASS_CSE:            return chomsky3_optimize_cse(ir);
        case CHOMSKY3_IR_PASS_CONSTANT_FOLD:
        case CHOMSKY3_IR_PASS_CONSTANT_PROP:  return chomsky3_optimize_constant_folding(ir);
        case CHOMSKY3_IR_PASS_LOOP_UNROLL:
        case CHOMSKY3_IR_PASS_LOOP_INVARIANT: return chomsky3_optimize_loops(ir, NULL);
        case CHOMSKY3_IR_PASS_STRENGTH_REDUCE:return chomsky3_optimize_strength_reduction(ir);
        case CHOMSKY3_IR_PASS_PEEPHOLE: {
            size_t changed = 0;
            for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
                changed += chomsky3_optimize_peephole_block(b);
            }
            (void)changed;
            return CHOMSKY3_OK;
        }
        case CHOMSKY3_IR_PASS_INLINE:          return chomsky3_optimize_inline(ir, 64);
        case CHOMSKY3_IR_PASS_TAIL_CALL:       return chomsky3_optimize_tail_calls(ir);
        case CHOMSKY3_IR_PASS_BRANCH_FOLD:
        case CHOMSKY3_IR_PASS_JUMP_THREAD:
        case CHOMSKY3_IR_PASS_BLOCK_MERGE:
        case CHOMSKY3_IR_PASS_SIMPLIFY_CFG:    return chomsky3_optimize_cfg(ir);
        case CHOMSKY3_IR_PASS_SSA_CONSTRUCT:   return chomsky3_ir_to_ssa(ir);
        case CHOMSKY3_IR_PASS_SSA_DESTRUCT:    return chomsky3_ir_from_ssa(ir);
        case CHOMSKY3_IR_PASS_REGISTER_ALLOC:  return chomsky3_optimize_register_allocation(ir, NULL);
        default: return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
}

chomsky3_error_t chomsky3_ir_run_passes(chomsky3_ir_t *ir, const chomsky3_ir_pass_t *passes, size_t num_passes) {
    if (!ir || (!passes && num_passes > 0)) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    for (size_t i = 0; i < num_passes; i++) {
        chomsky3_error_t err = chomsky3_ir_run_pass(ir, &passes[i]);
        if (err != CHOMSKY3_OK) return err;
    }
    return CHOMSKY3_OK;
}

const chomsky3_ir_pass_t *chomsky3_ir_get_pass(chomsky3_ir_pass_type_t type) {
    switch (type) {
        case CHOMSKY3_IR_PASS_DCE:              return chomsky3_get_opt_pass_dce();
        case CHOMSKY3_IR_PASS_CSE:              return chomsky3_get_opt_pass_cse();
        case CHOMSKY3_IR_PASS_CONSTANT_FOLD:    return chomsky3_get_opt_pass_constant_folding();
        case CHOMSKY3_IR_PASS_LOOP_UNROLL:
        case CHOMSKY3_IR_PASS_LOOP_INVARIANT:   return chomsky3_get_opt_pass_loop_optimizations();
        case CHOMSKY3_IR_PASS_STRENGTH_REDUCE:  return chomsky3_get_opt_pass_strength_reduction();
        case CHOMSKY3_IR_PASS_PEEPHOLE:         return chomsky3_get_opt_pass_peephole();
        case CHOMSKY3_IR_PASS_SIMPLIFY_CFG:
        case CHOMSKY3_IR_PASS_BLOCK_MERGE:
        case CHOMSKY3_IR_PASS_BRANCH_FOLD:      return chomsky3_get_opt_pass_cfg_simplify();
        case CHOMSKY3_IR_PASS_REGISTER_ALLOC:   return chomsky3_get_opt_pass_register_allocation();
        case CHOMSKY3_IR_PASS_INLINE:           return chomsky3_get_opt_pass_inline();
        case CHOMSKY3_IR_PASS_TAIL_CALL:        return chomsky3_get_opt_pass_tail_calls();
        default: return NULL;
    }
}
chomsky3_ir_pass_t *chomsky3_ir_pass_create(const char *name, chomsky3_ir_pass_func_t func, void *user_data) { chomsky3_ir_pass_t *p = calloc(1, sizeof(*p)); if (p) { p->name = name; p->func = func; p->user_data = user_data; } return p; }
void chomsky3_ir_pass_destroy(chomsky3_ir_pass_t *pass) { free(pass); }
chomsky3_error_t chomsky3_ir_analyze(const chomsky3_ir_t *ir, chomsky3_ir_analysis_t **analysis) {
    if (!ir || !analysis) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    chomsky3_ir_analysis_t *a = calloc(1, sizeof(*a));
    if (!a) return CHOMSKY3_ERR_OUT_OF_MEMORY;

    a->num_blocks = ir->num_blocks;
    a->num_instructions = ir->total_instructions;
    a->num_registers = ir->num_registers;

    /* Reachability: iterative DFS from the entry block over the CFG. */
    if (ir->num_blocks > 0) {
        a->reachable_blocks = calloc(ir->num_blocks, sizeof(bool));
        if (!a->reachable_blocks) {
            free(a);
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
        if (ir->entry) {
            chomsky3_ir_block_t **stack = malloc(ir->num_blocks * sizeof(*stack));
            if (!stack) {
                free(a->reachable_blocks);
                free(a);
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            size_t sp = 0;
            stack[sp++] = ir->entry;
            while (sp > 0) {
                chomsky3_ir_block_t *block = stack[--sp];
                if (block->id >= ir->num_blocks || a->reachable_blocks[block->id]) {
                    continue;
                }
                a->reachable_blocks[block->id] = true;
                for (size_t i = 0; i < block->num_successors && sp < ir->num_blocks; i++) {
                    if (block->successors[i] && !a->reachable_blocks[block->successors[i]->id]) {
                        stack[sp++] = block->successors[i];
                    }
                }
            }
            free(stack);
        }
    }

    *analysis = a;
    return CHOMSKY3_OK;
}

void chomsky3_ir_analysis_free(chomsky3_ir_analysis_t *analysis) {
    if (!analysis) return;
    free(analysis->reachable_blocks);
    free(analysis->postorder);
    free(analysis->reverse_postorder);
    if (analysis->live_in) {
        for (size_t i = 0; i < analysis->num_blocks; i++) free(analysis->live_in[i]);
        free(analysis->live_in);
    }
    if (analysis->live_out) {
        for (size_t i = 0; i < analysis->num_blocks; i++) free(analysis->live_out[i]);
        free(analysis->live_out);
    }
    if (analysis->def) {
        for (size_t i = 0; i < analysis->num_blocks; i++) free(analysis->def[i]);
        free(analysis->def);
    }
    if (analysis->use) {
        for (size_t i = 0; i < analysis->num_blocks; i++) free(analysis->use[i]);
        free(analysis->use);
    }
    free(analysis->dominators);
    free(analysis->loop_depth);
    free(analysis->loop_headers);
    free(analysis);
}

/* Iterative immediate-dominator computation (Cooper-Harvey-Kennedy). */
chomsky3_error_t chomsky3_ir_compute_dominators(chomsky3_ir_t *ir) {
    if (!ir) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    if (!ir->entry || ir->num_blocks == 0) return CHOMSKY3_OK;

    /* Order blocks by list position; ids are dense from the builder. */
    size_t n = ir->num_blocks;
    chomsky3_ir_block_t **order = malloc(n * sizeof(*order));
    if (!order) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    size_t count = 0;
    for (chomsky3_ir_block_t *b = ir->first_block; b && count < n; b = b->next) {
        order[count++] = b;
    }

    ir->entry->idom = ir->entry;
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < count; i++) {
            chomsky3_ir_block_t *b = order[i];
            if (b == ir->entry) continue;

            /* New idom = first processed predecessor, then intersect rest. */
            chomsky3_ir_block_t *new_idom = NULL;
            for (size_t p = 0; p < b->num_predecessors; p++) {
                chomsky3_ir_block_t *pred = b->predecessors[p];
                if (!pred->idom) continue;
                if (!new_idom) {
                    new_idom = pred;
                    continue;
                }
                /* Intersect: walk both idom chains until they meet. Block
                 * list position is a valid approximation of RPO for this
                 * purpose when ids are unset; use ids when available. */
                chomsky3_ir_block_t *f1 = pred;
                chomsky3_ir_block_t *f2 = new_idom;
                size_t guard = n * n + 2;
                while (f1 != f2 && guard-- > 0) {
                    while (f1 && f2 && f1->id > f2->id) f1 = f1->idom;
                    while (f1 && f2 && f2->id > f1->id) f2 = f2->idom;
                    if (!f1 || !f2) break;
                    if (f1->id == f2->id) break;
                    if (f1->idom == f1 || f2->idom == f2) break;
                }
                new_idom = (f1 == f2) ? f1 : new_idom;
            }
            if (new_idom && new_idom != b->idom) {
                b->idom = new_idom;
                changed = true;
            }
        }
    }

    free(order);
    return CHOMSKY3_OK;
}

/* Standard iterative liveness over the block list. */
chomsky3_error_t chomsky3_ir_compute_liveness(const chomsky3_ir_t *ir, chomsky3_ir_analysis_t *analysis) {
    if (!ir || !analysis) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    size_t n = ir->num_blocks;
    size_t regs = ir->num_registers ? ir->num_registers : 1;
    size_t words = (regs + 63) / 64;

    analysis->live_in = calloc(n, sizeof(uint64_t *));
    analysis->live_out = calloc(n, sizeof(uint64_t *));
    analysis->def = calloc(n, sizeof(uint64_t *));
    analysis->use = calloc(n, sizeof(uint64_t *));
    if (!analysis->live_in || !analysis->live_out || !analysis->def || !analysis->use) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    chomsky3_ir_block_t **blocks = malloc((n ? n : 1) * sizeof(*blocks));
    if (!blocks) return CHOMSKY3_ERR_OUT_OF_MEMORY;
    size_t count = 0;
    for (chomsky3_ir_block_t *b = ir->first_block; b && count < n; b = b->next) {
        blocks[count++] = b;
    }

    for (size_t i = 0; i < count; i++) {
        analysis->live_in[i] = calloc(words, sizeof(uint64_t));
        analysis->live_out[i] = calloc(words, sizeof(uint64_t));
        analysis->def[i] = calloc(words, sizeof(uint64_t));
        analysis->use[i] = calloc(words, sizeof(uint64_t));
        if (!analysis->live_in[i] || !analysis->live_out[i] ||
            !analysis->def[i] || !analysis->use[i]) {
            free(blocks);
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }

        /* Local def/use: use before def counts as use. */
        chomsky3_ir_block_t *b = blocks[i];
        for (chomsky3_ir_instruction_t *inst = b->first; inst; inst = inst->next) {
            uint32_t defs[3], uses[3];
            size_t ndefs = 0, nuses = 0;
            if (inst->dest.type == CHOMSKY3_IR_VALUE_REGISTER && ndefs < 3)
                defs[ndefs++] = inst->dest.data.reg;
            if (inst->src1.type == CHOMSKY3_IR_VALUE_REGISTER && nuses < 3)
                uses[nuses++] = inst->src1.data.reg;
            if (inst->src2.type == CHOMSKY3_IR_VALUE_REGISTER && nuses < 3)
                uses[nuses++] = inst->src2.data.reg;
            if (inst->src3.type == CHOMSKY3_IR_VALUE_REGISTER && nuses < 3)
                uses[nuses++] = inst->src3.data.reg;

            for (size_t u = 0; u < nuses; u++) {
                uint32_t r = uses[u];
                if (r >= regs) continue;
                if (!(analysis->def[i][r / 64] & (UINT64_C(1) << (r % 64))))
                    analysis->use[i][r / 64] |= UINT64_C(1) << (r % 64);
            }
            for (size_t d = 0; d < ndefs; d++) {
                uint32_t r = defs[d];
                if (r < regs) analysis->def[i][r / 64] |= UINT64_C(1) << (r % 64);
            }
        }
    }

    /* Iterate live sets to a fixed point. */
    bool changed = true;
    size_t guard = count * 8 + 16;
    while (changed && guard-- > 0) {
        changed = false;
        for (size_t i = count; i-- > 0;) {
            chomsky3_ir_block_t *b = blocks[i];

            /* live_out = union of successors' live_in */
            for (size_t s = 0; s < b->num_successors; s++) {
                /* Successor index in `blocks`: match by id (dense). */
                size_t sid = b->successors[s] ? b->successors[s]->id : 0;
                if (sid >= count) continue;
                for (size_t w = 0; w < words; w++) {
                    uint64_t v = analysis->live_out[i][w] | analysis->live_in[sid][w];
                    if (v != analysis->live_out[i][w]) {
                        analysis->live_out[i][w] = v;
                        changed = true;
                    }
                }
            }

            /* live_in = use | (live_out - def) */
            for (size_t w = 0; w < words; w++) {
                uint64_t v = analysis->use[i][w] |
                             (analysis->live_out[i][w] & ~analysis->def[i][w]);
                if (v != analysis->live_in[i][w]) {
                    analysis->live_in[i][w] = v;
                    changed = true;
                }
            }
        }
    }

    free(blocks);
    return CHOMSKY3_OK;
}

/* Detect natural loops via back edges using the dominator tree. */
chomsky3_error_t chomsky3_ir_detect_loops(const chomsky3_ir_t *ir, chomsky3_ir_analysis_t *analysis) {
    if (!ir || !analysis) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    size_t n = ir->num_blocks;
    if (n == 0) return CHOMSKY3_OK;

    analysis->loop_depth = calloc(n, sizeof(uint32_t));
    analysis->loop_headers = calloc(n, sizeof(chomsky3_ir_block_t *));
    if (!analysis->loop_depth || !analysis->loop_headers) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* A successor edge b -> s is a back edge when s dominates b (or, without
     * a dominator tree, when s appears earlier in the block list than b). */
    for (chomsky3_ir_block_t *b = ir->first_block; b; b = b->next) {
        for (size_t s = 0; s < b->num_successors; s++) {
            chomsky3_ir_block_t *succ = b->successors[s];
            if (!succ) continue;

            bool dominates = false;
            for (chomsky3_ir_block_t *d = b->idom; d; d = (d->idom == d ? NULL : d->idom)) {
                if (d == succ) { dominates = true; break; }
            }
            if (!dominates && succ->id <= b->id && succ->num_predecessors > 0) {
                dominates = true; /* conservative: treat earlier block as header */
            }

            if (dominates && succ->id < n) {
                analysis->loop_depth[succ->id]++;
                if (analysis->num_loops < n) {
                    bool seen = false;
                    for (size_t k = 0; k < analysis->num_loops; k++) {
                        if (analysis->loop_headers[k] == succ) { seen = true; break; }
                    }
                    if (!seen) analysis->loop_headers[analysis->num_loops++] = succ;
                }
                succ->loop_header = succ;
            }
        }
    }

    return CHOMSKY3_OK;
}
bool chomsky3_ir_verify_instruction(const chomsky3_ir_instruction_t *inst, char **error_msg) {
    if (error_msg) *error_msg = NULL;
    if (!inst) {
        if (error_msg) *error_msg = dup_string("NULL instruction");
        return false;
    }
    if (inst->opcode < 0 || inst->opcode >= CHOMSKY3_IR_OP_COUNT) {
        if (error_msg) *error_msg = dup_string("invalid instruction opcode");
        return false;
    }
    /* Control-flow instructions must reference real blocks. */
    switch (inst->opcode) {
        case CHOMSKY3_IR_OP_JUMP:
            if (inst->src1.type != CHOMSKY3_IR_VALUE_BLOCK || !inst->src1.data.block) {
                if (error_msg) *error_msg = dup_string("jump without target block");
                return false;
            }
            break;
        case CHOMSKY3_IR_OP_BRANCH:
            if (inst->src2.type != CHOMSKY3_IR_VALUE_BLOCK || !inst->src2.data.block ||
                inst->src3.type != CHOMSKY3_IR_VALUE_BLOCK || !inst->src3.data.block) {
                if (error_msg) *error_msg = dup_string("branch without target blocks");
                return false;
            }
            break;
        case CHOMSKY3_IR_OP_SPLIT:
            if (inst->src1.type != CHOMSKY3_IR_VALUE_BLOCK || !inst->src1.data.block ||
                inst->src2.type != CHOMSKY3_IR_VALUE_BLOCK || !inst->src2.data.block) {
                if (error_msg) *error_msg = dup_string("split without target blocks");
                return false;
            }
            break;
        default:
            break;
    }
    return true;
}

bool chomsky3_ir_verify_block(const chomsky3_ir_block_t *block, char **error_msg) {
    if (error_msg) *error_msg = NULL;
    if (!block) {
        if (error_msg) *error_msg = dup_string("NULL block");
        return false;
    }
    size_t count = 0;
    for (chomsky3_ir_instruction_t *inst = block->first; inst; inst = inst->next) {
        if (inst->parent != block) {
            if (error_msg) *error_msg = dup_string("instruction has wrong parent block");
            return false;
        }
        if (!chomsky3_ir_verify_instruction(inst, error_msg)) {
            return false;
        }
        count++;
    }
    if (count != block->num_instructions) {
        if (error_msg) *error_msg = dup_string("block instruction count mismatch");
        return false;
    }
    return true;
}

bool chomsky3_ir_verify(const chomsky3_ir_t *ir, char **error_msg) {
    if (error_msg) *error_msg = NULL;
    if (!ir) {
        if (error_msg) *error_msg = dup_string("NULL IR");
        return false;
    }
    size_t num_blocks = 0;
    size_t total_instructions = 0;
    for (chomsky3_ir_block_t *block = ir->first_block; block; block = block->next) {
        if (!chomsky3_ir_verify_block(block, error_msg)) {
            return false;
        }
        total_instructions += block->num_instructions;
        num_blocks++;
    }
    if (num_blocks != ir->num_blocks) {
        if (error_msg) *error_msg = dup_string("IR block count mismatch");
        return false;
    }
    if (ir->num_blocks > 0 && !ir->entry) {
        if (error_msg) *error_msg = dup_string("IR has blocks but no entry block");
        return false;
    }
    (void)total_instructions;
    return true;
}
/* Growable string buffer used by the IR printer. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int oom;
} ir_strbuf_t;

static void ir_strbuf_init(ir_strbuf_t *sb) {
    sb->cap = 1024;
    sb->len = 0;
    sb->oom = 0;
    sb->data = malloc(sb->cap);
    if (!sb->data) {
        sb->cap = 0;
        sb->oom = 1;
        return;
    }
    sb->data[0] = '\0';
}

static void ir_strbuf_append(ir_strbuf_t *sb, const char *fmt, ...) {
    if (sb->oom) return;
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) { sb->oom = 1; return; }

    if (sb->len + (size_t)needed + 1 > sb->cap) {
        size_t new_cap = sb->cap * 2;
        while (new_cap < sb->len + (size_t)needed + 1) new_cap *= 2;
        char *p = realloc(sb->data, new_cap);
        if (!p) { sb->oom = 1; return; }
        sb->data = p;
        sb->cap = new_cap;
    }

    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);
    sb->len += (size_t)needed;
}

static const char *ir_opcode_name(chomsky3_ir_opcode_t opcode) {
    switch (opcode) {
        case CHOMSKY3_IR_OP_MATCH_CHAR:          return "match_char";
        case CHOMSKY3_IR_OP_MATCH_RANGE:         return "match_range";
        case CHOMSKY3_IR_OP_MATCH_CLASS:         return "match_class";
        case CHOMSKY3_IR_OP_MATCH_ANY:           return "match_any";
        case CHOMSKY3_IR_OP_MATCH_STRING:        return "match_string";
        case CHOMSKY3_IR_OP_JUMP:                return "jump";
        case CHOMSKY3_IR_OP_BRANCH:              return "branch";
        case CHOMSKY3_IR_OP_SPLIT:               return "split";
        case CHOMSKY3_IR_OP_CALL:                return "call";
        case CHOMSKY3_IR_OP_RETURN:              return "return";
        case CHOMSKY3_IR_OP_MATCH:               return "match";
        case CHOMSKY3_IR_OP_FAIL:                return "fail";
        case CHOMSKY3_IR_OP_ADVANCE:             return "advance";
        case CHOMSKY3_IR_OP_SAVE_POS:            return "save_pos";
        case CHOMSKY3_IR_OP_RESTORE_POS:         return "restore_pos";
        case CHOMSKY3_IR_OP_CHECK_BEGIN:         return "check_begin";
        case CHOMSKY3_IR_OP_CHECK_END:           return "check_end";
        case CHOMSKY3_IR_OP_CHECK_WORD_BOUNDARY: return "check_word_boundary";
        case CHOMSKY3_IR_OP_CAPTURE_START:       return "capture_start";
        case CHOMSKY3_IR_OP_CAPTURE_END:         return "capture_end";
        case CHOMSKY3_IR_OP_BACKREFERENCE:       return "backreference";
        case CHOMSKY3_IR_OP_LOOKAHEAD_START:     return "lookahead_start";
        case CHOMSKY3_IR_OP_LOOKAHEAD_END:       return "lookahead_end";
        case CHOMSKY3_IR_OP_LOOKBEHIND_START:    return "lookbehind_start";
        case CHOMSKY3_IR_OP_LOOKBEHIND_END:      return "lookbehind_end";
        case CHOMSKY3_IR_OP_NEG_LOOKAHEAD_START: return "neg_lookahead_start";
        case CHOMSKY3_IR_OP_NEG_LOOKBEHIND_START:return "neg_lookbehind_start";
        case CHOMSKY3_IR_OP_REPEAT_START:        return "repeat_start";
        case CHOMSKY3_IR_OP_REPEAT_END:          return "repeat_end";
        case CHOMSKY3_IR_OP_CHECK_COUNT:         return "check_count";
        case CHOMSKY3_IR_OP_INCREMENT_COUNT:     return "increment_count";
        case CHOMSKY3_IR_OP_PUSH:                return "push";
        case CHOMSKY3_IR_OP_POP:                 return "pop";
        case CHOMSKY3_IR_OP_LOAD:                return "load";
        case CHOMSKY3_IR_OP_STORE:               return "store";
        case CHOMSKY3_IR_OP_ADD:                 return "add";
        case CHOMSKY3_IR_OP_SUB:                 return "sub";
        case CHOMSKY3_IR_OP_MUL:                 return "mul";
        case CHOMSKY3_IR_OP_DIV:                 return "div";
        case CHOMSKY3_IR_OP_MOD:                 return "mod";
        case CHOMSKY3_IR_OP_AND:                 return "and";
        case CHOMSKY3_IR_OP_OR:                  return "or";
        case CHOMSKY3_IR_OP_XOR:                 return "xor";
        case CHOMSKY3_IR_OP_NOT:                 return "not";
        case CHOMSKY3_IR_OP_SHL:                 return "shl";
        case CHOMSKY3_IR_OP_SHR:                 return "shr";
        case CHOMSKY3_IR_OP_CMP_EQ:              return "cmp_eq";
        case CHOMSKY3_IR_OP_CMP_NE:              return "cmp_ne";
        case CHOMSKY3_IR_OP_CMP_LT:              return "cmp_lt";
        case CHOMSKY3_IR_OP_CMP_LE:              return "cmp_le";
        case CHOMSKY3_IR_OP_CMP_GT:              return "cmp_gt";
        case CHOMSKY3_IR_OP_CMP_GE:              return "cmp_ge";
        case CHOMSKY3_IR_OP_NOP:                 return "nop";
        case CHOMSKY3_IR_OP_PHI:                 return "phi";
        case CHOMSKY3_IR_OP_COMMENT:             return "comment";
        case CHOMSKY3_IR_OP_LABEL:               return "label";
        default:                                 return "?";
    }
}

static void ir_print_value(ir_strbuf_t *sb, const chomsky3_ir_value_t *v) {
    switch (v->type) {
        case CHOMSKY3_IR_VALUE_NONE:      return;
        case CHOMSKY3_IR_VALUE_IMMEDIATE: ir_strbuf_append(sb, " %lld", (long long)v->data.immediate); break;
        case CHOMSKY3_IR_VALUE_REGISTER:  ir_strbuf_append(sb, " r%u", v->data.reg); break;
        case CHOMSKY3_IR_VALUE_MEMORY:    ir_strbuf_append(sb, " [r%u%+d]", v->data.memory.base_reg, v->data.memory.offset); break;
        case CHOMSKY3_IR_VALUE_LABEL:     ir_strbuf_append(sb, " L%u", v->data.label_id); break;
        case CHOMSKY3_IR_VALUE_BLOCK:     ir_strbuf_append(sb, " bb%u", v->data.block ? v->data.block->id : 0); break;
        case CHOMSKY3_IR_VALUE_STRING:    ir_strbuf_append(sb, " \"%s\"", v->data.string ? v->data.string : ""); break;
        case CHOMSKY3_IR_VALUE_CHARCLASS: ir_strbuf_append(sb, " <charclass>"); break;
        default:                          ir_strbuf_append(sb, " ?"); break;
    }
}

chomsky3_error_t chomsky3_ir_print(const chomsky3_ir_t *ir, char **output) {
    if (!ir || !output) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    ir_strbuf_t sb;
    ir_strbuf_init(&sb);
    if (sb.oom) return CHOMSKY3_ERR_OUT_OF_MEMORY;

    ir_strbuf_append(&sb, "IR %s: %zu blocks, %zu instructions, %u registers\n",
                     ir->name ? ir->name : "(unnamed)",
                     ir->num_blocks, ir->total_instructions, ir->num_registers);

    for (chomsky3_ir_block_t *block = ir->first_block; block; block = block->next) {
        ir_strbuf_append(&sb, "bb%u:", block->id);
        if (block->label) ir_strbuf_append(&sb, " ; %s", block->label);
        if (block == ir->entry) ir_strbuf_append(&sb, " ; entry");
        if (block->loop_depth > 0) ir_strbuf_append(&sb, " ; loop depth %u", block->loop_depth);
        ir_strbuf_append(&sb, "\n");

        if (block->num_predecessors > 0) {
            ir_strbuf_append(&sb, "  ; preds:");
            for (size_t i = 0; i < block->num_predecessors; i++) {
                ir_strbuf_append(&sb, " bb%u", block->predecessors[i] ? block->predecessors[i]->id : 0);
            }
            ir_strbuf_append(&sb, "\n");
        }

        for (chomsky3_ir_instruction_t *inst = block->first; inst; inst = inst->next) {
            ir_strbuf_append(&sb, "  %04u %-20s", inst->id, ir_opcode_name(inst->opcode));
            ir_print_value(&sb, &inst->dest);
            ir_print_value(&sb, &inst->src1);
            ir_print_value(&sb, &inst->src2);
            ir_print_value(&sb, &inst->src3);
            if (inst->comment) ir_strbuf_append(&sb, " ; %s", inst->comment);
            ir_strbuf_append(&sb, "\n");
        }
    }

    if (sb.oom) {
        free(sb.data);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    *output = sb.data;
    return CHOMSKY3_OK;
}
chomsky3_error_t chomsky3_ir_print_file(const chomsky3_ir_t *ir, const char *filename) { if (!ir || !filename) return CHOMSKY3_ERR_INVALID_ARGUMENT; char *out = NULL; chomsky3_error_t err = chomsky3_ir_print(ir, &out); if (err != CHOMSKY3_OK) return err; FILE *fp = fopen(filename, "w"); if (!fp) { free(out); return CHOMSKY3_ERR_IO_ERROR; } fputs(out, fp); fclose(fp); free(out); return CHOMSKY3_OK; }
