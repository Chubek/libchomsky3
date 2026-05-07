/** Minimal IR implementation scaffolding. */

#include "chomsky3/ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

void chomsky3_ir_remove_edge(chomsky3_ir_block_t *from, chomsky3_ir_block_t *to) { (void)from; (void)to; }

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

void chomsky3_ir_insert_instruction_before(chomsky3_ir_instruction_t *before, chomsky3_ir_instruction_t *inst) { (void)before; (void)inst; }
void chomsky3_ir_insert_instruction_after(chomsky3_ir_instruction_t *after, chomsky3_ir_instruction_t *inst) { (void)after; (void)inst; }
void chomsky3_ir_remove_instruction(chomsky3_ir_instruction_t *inst) { (void)inst; }
void chomsky3_ir_replace_instruction(chomsky3_ir_instruction_t *old, chomsky3_ir_instruction_t *new) { (void)old; (void)new; }

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

chomsky3_error_t chomsky3_ir_run_pass(chomsky3_ir_t *ir, const chomsky3_ir_pass_t *pass) { (void)pass; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_ir_run_passes(chomsky3_ir_t *ir, const chomsky3_ir_pass_t *passes, size_t num_passes) { (void)passes; (void)num_passes; return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
const chomsky3_ir_pass_t *chomsky3_ir_get_pass(chomsky3_ir_pass_type_t type) { (void)type; return NULL; }
chomsky3_ir_pass_t *chomsky3_ir_pass_create(const char *name, chomsky3_ir_pass_func_t func, void *user_data) { chomsky3_ir_pass_t *p = calloc(1, sizeof(*p)); if (p) { p->name = name; p->func = func; p->user_data = user_data; } return p; }
void chomsky3_ir_pass_destroy(chomsky3_ir_pass_t *pass) { free(pass); }
chomsky3_error_t chomsky3_ir_analyze(const chomsky3_ir_t *ir, chomsky3_ir_analysis_t **analysis) { if (!ir || !analysis) return CHOMSKY3_ERR_INVALID_ARGUMENT; *analysis = calloc(1, sizeof(**analysis)); return *analysis ? CHOMSKY3_OK : CHOMSKY3_ERR_OUT_OF_MEMORY; }
void chomsky3_ir_analysis_free(chomsky3_ir_analysis_t *analysis) { free(analysis); }
chomsky3_error_t chomsky3_ir_compute_dominators(chomsky3_ir_t *ir) { return ir ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_ir_compute_liveness(const chomsky3_ir_t *ir, chomsky3_ir_analysis_t *analysis) { return (ir && analysis) ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
chomsky3_error_t chomsky3_ir_detect_loops(const chomsky3_ir_t *ir, chomsky3_ir_analysis_t *analysis) { return (ir && analysis) ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT; }
bool chomsky3_ir_verify(const chomsky3_ir_t *ir, char **error_msg) { if (error_msg) *error_msg = NULL; return ir != NULL; }
bool chomsky3_ir_verify_block(const chomsky3_ir_block_t *block, char **error_msg) { if (error_msg) *error_msg = NULL; return block != NULL; }
bool chomsky3_ir_verify_instruction(const chomsky3_ir_instruction_t *inst, char **error_msg) { if (error_msg) *error_msg = NULL; return inst != NULL; }
chomsky3_error_t chomsky3_ir_print(const chomsky3_ir_t *ir, char **output) { if (!ir || !output) return CHOMSKY3_ERR_INVALID_ARGUMENT; int n = snprintf(NULL, 0, "IR %s: %zu blocks\n", ir->name ? ir->name : "(unnamed)", ir->num_blocks); *output = malloc((size_t)n + 1); if (!*output) return CHOMSKY3_ERR_OUT_OF_MEMORY; snprintf(*output, (size_t)n + 1, "IR %s: %zu blocks\n", ir->name ? ir->name : "(unnamed)", ir->num_blocks); return CHOMSKY3_OK; }
chomsky3_error_t chomsky3_ir_print_file(const chomsky3_ir_t *ir, const char *filename) { if (!ir || !filename) return CHOMSKY3_ERR_INVALID_ARGUMENT; char *out = NULL; chomsky3_error_t err = chomsky3_ir_print(ir, &out); if (err != CHOMSKY3_OK) return err; FILE *fp = fopen(filename, "w"); if (!fp) { free(out); return CHOMSKY3_ERR_IO_ERROR; } fputs(out, fp); fclose(fp); free(out); return CHOMSKY3_OK; }
