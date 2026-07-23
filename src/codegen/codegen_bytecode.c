/**
 * libchomsky3 - Bytecode Code Generator
 *
 * Compiles regex ASTs (via IR source references) into the Thompson-style
 * backtracking bytecode executed by the libchomsky3 VM.
 *
 * The generator walks the AST referenced by the IR (ir->source_ast) and
 * emits instructions using the standard NFA construction:
 *
 *   literal 'c'      -> CHAR c
 *   [a-z]            -> CHAR_RANGE a z (or CHAR_CLASS for multi-range)
 *   A|B              -> SPLIT L1, L2; L1: A; JUMP L3; L2: B; L3:
 *   A*               -> L1: SPLIT L2, L3; L2: A; JUMP L1; L3:
 *   (A)              -> SAVE_START n; A; SAVE_END n
 *   (?=A)            -> LOOK_AHEAD L1, L2; L1: A; MATCH; L2:
 *   A{m,n}           -> counted REPEAT op or unrolled expansion
 */

#include "chomsky3/bytecode.h"
#include "chomsky3/pattern.h"
#include "chomsky3/ir.h"
#include "chomsky3/regex.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CHOMSKY3_BYTECODE_MAGIC 0x43484F4D
#define CHOMSKY3_BYTECODE_VERSION_MAJOR 1
#define CHOMSKY3_BYTECODE_VERSION_MINOR 0
#define CHOMSKY3_BYTECODE_VERSION_PATCH 0

/* Maximum bounded-repeat expansion; larger {m,n} uses REPEAT op */
#define EMIT_UNROLL_LIMIT 16

/* ========================================================================
 * Emitter state
 * ======================================================================== */

typedef struct {
    chomsky3_instruction_t *insts;
    size_t count;
    size_t capacity;

    uint8_t *data;              /* Constant data section (classes, strings) */
    size_t data_size;
    size_t data_capacity;

    uint32_t flags;             /* Compilation flags */
    chomsky3_error_t error;
} emitter_t;

static chomsky3_error_t emit_inst(emitter_t *e, chomsky3_opcode_t opcode,
                                  uint32_t op1, uint32_t op2, uint32_t op3) {
    if (e->count >= e->capacity) {
        size_t new_capacity = e->capacity ? e->capacity * 2 : 64;
        void *grown = realloc(e->insts, new_capacity * sizeof(*e->insts));
        if (!grown) return CHOMSKY3_ERR_OUT_OF_MEMORY;
        e->insts = grown;
        e->capacity = new_capacity;
    }
    chomsky3_instruction_t *inst = &e->insts[e->count++];
    memset(inst, 0, sizeof(*inst));
    inst->opcode = opcode;
    inst->operand1 = op1;
    inst->operand2 = op2;
    inst->operand3 = op3;
    return CHOMSKY3_OK;
}

/* Append bytes to the constant data section; returns offset or SIZE_MAX. */
static size_t emit_data(emitter_t *e, const void *bytes, size_t size) {
    if (e->data_size + size > e->data_capacity) {
        size_t new_capacity = e->data_capacity ? e->data_capacity * 2 : 128;
        while (new_capacity < e->data_size + size) new_capacity *= 2;
        void *grown = realloc(e->data, new_capacity);
        if (!grown) return SIZE_MAX;
        e->data = grown;
        e->data_capacity = new_capacity;
    }
    size_t offset = e->data_size;
    memcpy(e->data + e->data_size, bytes, size);
    e->data_size += size;
    return offset;
}

/* ========================================================================
 * Character class encoding
 *
 * Multi-range classes are stored in the data section as a sequence of
 * (lo, hi) byte pairs terminated by (0,0) when 0 is not a member, or a
 * count-prefixed list. We use: [count:u16][lo,hi pairs as u8] for byte
 * ranges, which covers ASCII classes. Flags word encodes negation and
 * named classes (\d \w \s membership).
 * ======================================================================== */

#define CLASS_FLAG_NEGATED 0x01
#define CLASS_FLAG_DIGIT   0x02
#define CLASS_FLAG_WORD    0x04
#define CLASS_FLAG_SPACE   0x08

/* GCC's -Wmaybe-uninitialized fires a false positive on cc->ranges reads
 * when emit_char_class is inlined into emit_node and the caller passed an
 * anonymous-struct array via void*. The ranges are always fully written
 * before the call; suppress locally rather than contort the code. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

static chomsky3_error_t emit_char_class(emitter_t *e, const chomsky3_char_class_t *cc) {
    /* Compute effective flags: for negated \d/\w/\s (\D \W \S) the parser
     * sets both NEGATED and the named-class flag; membership in the named
     * set is complementary, so clear the named flag and keep NEGATED —
     * the parser already folds the complementary ranges in. */
    uint32_t named = cc->flags & (CHOMSKY3_CCLASS_DIGIT |
                                  CHOMSKY3_CCLASS_WORD |
                                  CHOMSKY3_CCLASS_SPACE);
    uint32_t effective_named = named;
    if ((cc->flags & CHOMSKY3_CCLASS_NEGATED) && named) {
        effective_named = 0;
    }

    /* Fast path: single range, no named-class flags -> CHAR_RANGE or CHAR. */
    if (cc->num_ranges == 1 && !(cc->flags & CHOMSKY3_CCLASS_NEGATED) &&
        !effective_named &&
        cc->ranges[0].start <= 0xFF && cc->ranges[0].end <= 0xFF) {
        uint32_t lo = cc->ranges[0].start;
        uint32_t hi = cc->ranges[0].end;
        if (lo == hi) {
            return emit_inst(e, CHOMSKY3_OP_CHAR, lo, 0, 0);
        }
        return emit_inst(e, CHOMSKY3_OP_CHAR_RANGE, lo, hi, 0);
    }

    /* General path: serialize the class into the data section.
     * Layout: [num_ranges:u16][flags:u16][lo:u8 hi:u8]... (ranges clamped
     * to the byte domain; ranges wholly above 0xFF are dropped). */
    size_t range_count = 0;
    for (size_t i = 0; i < cc->num_ranges; i++) {
        if (cc->ranges[i].start <= 0xFF) range_count++;
    }

    size_t offset = emit_data(e, NULL, 0); /* current offset */
    uint8_t header[4];
    header[0] = (uint8_t)(range_count & 0xFF);
    header[1] = (uint8_t)((range_count >> 8) & 0xFF);
    uint32_t flags_word = 0;
    if (cc->flags & CHOMSKY3_CCLASS_NEGATED) flags_word |= CLASS_FLAG_NEGATED;
    if (effective_named & CHOMSKY3_CCLASS_DIGIT) flags_word |= CLASS_FLAG_DIGIT;
    if (effective_named & CHOMSKY3_CCLASS_WORD)  flags_word |= CLASS_FLAG_WORD;
    if (effective_named & CHOMSKY3_CCLASS_SPACE) flags_word |= CLASS_FLAG_SPACE;
    uint16_t flags = (uint16_t)flags_word;
    header[2] = (uint8_t)(flags & 0xFF);
    header[3] = (uint8_t)((flags >> 8) & 0xFF);
    if (emit_data(e, header, sizeof(header)) == SIZE_MAX) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < cc->num_ranges; i++) {
        if (cc->ranges[i].start > 0xFF) continue;
        uint32_t hi = cc->ranges[i].end > 0xFF ? 0xFF : cc->ranges[i].end;
        uint8_t pair[2] = { (uint8_t)cc->ranges[i].start, (uint8_t)hi };
        if (emit_data(e, pair, sizeof(pair)) == SIZE_MAX) {
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
    }

    return emit_inst(e, CHOMSKY3_OP_CHAR_CLASS, (uint32_t)offset,
                     (uint32_t)(4 + range_count * 2), 0);
}

/* ========================================================================
 * AST emission
 * ======================================================================== */

static chomsky3_error_t emit_node(emitter_t *e, const chomsky3_regex_node_t *node);

/* Emit `node` `count` times in sequence (bounded repeat expansion). */
static chomsky3_error_t emit_repeated_copies(emitter_t *e,
                                             const chomsky3_regex_node_t *node,
                                             uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        chomsky3_error_t err = emit_node(e, node);
        if (err != CHOMSKY3_OK) return err;
    }
    return CHOMSKY3_OK;
}

/* Emit an optional occurrence of `node` (greedy or lazy). */
static chomsky3_error_t emit_optional(emitter_t *e,
                                      const chomsky3_regex_node_t *node,
                                      bool greedy) {
    size_t split = e->count;
    chomsky3_error_t err = emit_inst(e, CHOMSKY3_OP_SPLIT, 0, 0, 0);
    if (err != CHOMSKY3_OK) return err;

    size_t body = e->count;
    err = emit_node(e, node);
    if (err != CHOMSKY3_OK) return err;

    size_t end = e->count;
    if (greedy) {
        e->insts[split].operand1 = (uint32_t)body;
        e->insts[split].operand2 = (uint32_t)end;
    } else {
        e->insts[split].operand1 = (uint32_t)end;
        e->insts[split].operand2 = (uint32_t)body;
    }
    return CHOMSKY3_OK;
}

static chomsky3_error_t emit_node(emitter_t *e, const chomsky3_regex_node_t *node) {
    if (!node) return CHOMSKY3_OK;

    switch (node->type) {
        case CHOMSKY3_NODE_LITERAL:
            if (node->data.literal > 0xFF) {
                /* Out of byte domain: encode as single-range class */
                chomsky3_char_class_t cc = {0};
                struct { uint32_t start; uint32_t end; } r =
                    { node->data.literal, node->data.literal };
                /* Anonymous-struct members can't share a type across TUs;
                 * the layout is two u32s either way, so copy via void*. */
                cc.ranges = (void *)&r;
                cc.num_ranges = 1;
                return emit_char_class(e, &cc);
            }
            if (e->flags & CHOMSKY3_FLAG_CASE_INSENSITIVE &&
                node->data.literal < 0x80) {
                uint32_t ch = node->data.literal;
                uint32_t lo = ch, hi = ch;
                if (ch >= 'a' && ch <= 'z') hi = ch - 32;
                else if (ch >= 'A' && ch <= 'Z') lo = ch + 32;
                if (lo != hi) {
                    /* Emit class with both cases. Heap-allocate the range
                     * list: passing a local array through the anonymous
                     * struct pointer trips -Wmaybe-uninitialized under
                     * inlining, and emit copies are trivially small. */
                    struct { uint32_t start; uint32_t end; } *rs =
                        malloc(2 * sizeof(*rs));
                    if (!rs) return CHOMSKY3_ERR_OUT_OF_MEMORY;
                    rs[0].start = lo; rs[0].end = lo;
                    rs[1].start = hi; rs[1].end = hi;
                    chomsky3_char_class_t cc = {0};
                    cc.ranges = (void *)rs;
                    cc.num_ranges = 2;
                    chomsky3_error_t err = emit_char_class(e, &cc);
                    free(rs);
                    return err;
                }
            }
            return emit_inst(e, CHOMSKY3_OP_CHAR, node->data.literal, 0, 0);

        case CHOMSKY3_NODE_CHAR_CLASS:
            return emit_char_class(e, &node->data.char_class);

        case CHOMSKY3_NODE_DOT:
            return emit_inst(e, CHOMSKY3_OP_ANY,
                             (e->flags & CHOMSKY3_FLAG_DOTALL) ? 0 : 1, 0, 0);

        case CHOMSKY3_NODE_ANCHOR_START:
            return emit_inst(e,
                             (e->flags & CHOMSKY3_FLAG_MULTILINE)
                                 ? CHOMSKY3_OP_ANCHOR_LINE_START
                                 : CHOMSKY3_OP_ANCHOR_START,
                             0, 0, 0);

        case CHOMSKY3_NODE_ANCHOR_END:
            return emit_inst(e,
                             (e->flags & CHOMSKY3_FLAG_MULTILINE)
                                 ? CHOMSKY3_OP_ANCHOR_LINE_END
                                 : CHOMSKY3_OP_ANCHOR_END,
                             0, 0, 0);

        case CHOMSKY3_NODE_WORD_BOUNDARY:
            return emit_inst(e, CHOMSKY3_OP_ANCHOR_WORD, 0, 0, 0);

        case CHOMSKY3_NODE_NWORD_BOUNDARY:
            return emit_inst(e, CHOMSKY3_OP_ANCHOR_NWORD, 0, 0, 0);

        case CHOMSKY3_NODE_CONCAT: {
            chomsky3_error_t err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;
            return emit_node(e, node->right);
        }

        case CHOMSKY3_NODE_ALTERNATION: {
            /* SPLIT L1, L2; L1: left; JUMP L3; L2: right; L3: */
            size_t split = e->count;
            chomsky3_error_t err = emit_inst(e, CHOMSKY3_OP_SPLIT, 0, 0, 0);
            if (err != CHOMSKY3_OK) return err;

            size_t l1 = e->count;
            err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;

            size_t jump = e->count;
            err = emit_inst(e, CHOMSKY3_OP_JUMP, 0, 0, 0);
            if (err != CHOMSKY3_OK) return err;

            size_t l2 = e->count;
            err = emit_node(e, node->right);
            if (err != CHOMSKY3_OK) return err;

            size_t l3 = e->count;
            e->insts[split].operand1 = (uint32_t)l1;
            e->insts[split].operand2 = (uint32_t)l2;
            e->insts[jump].operand1 = (uint32_t)l3;
            return CHOMSKY3_OK;
        }

        case CHOMSKY3_NODE_STAR: {
            if (node->greedy) {
                /* L1: SPLIT L2, L3; L2: body; JUMP L1; L3: */
                size_t l1 = e->count;
                chomsky3_error_t err = emit_inst(e, CHOMSKY3_OP_SPLIT, 0, 0, 0);
                if (err != CHOMSKY3_OK) return err;
                size_t l2 = e->count;
                err = emit_node(e, node->left);
                if (err != CHOMSKY3_OK) return err;
                err = emit_inst(e, CHOMSKY3_OP_JUMP, (uint32_t)l1, 0, 0);
                if (err != CHOMSKY3_OK) return err;
                size_t l3 = e->count;
                e->insts[l1].operand1 = (uint32_t)l2;
                e->insts[l1].operand2 = (uint32_t)l3;
                return CHOMSKY3_OK;
            }
            /* Lazy: L1: SPLIT L3, L2; L2: body; JUMP L1; L3: */
            size_t l1 = e->count;
            chomsky3_error_t err = emit_inst(e, CHOMSKY3_OP_SPLIT, 0, 0, 0);
            if (err != CHOMSKY3_OK) return err;
            size_t l2 = e->count;
            err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;
            err = emit_inst(e, CHOMSKY3_OP_JUMP, (uint32_t)l1, 0, 0);
            if (err != CHOMSKY3_OK) return err;
            size_t l3 = e->count;
            e->insts[l1].operand1 = (uint32_t)l3;
            e->insts[l1].operand2 = (uint32_t)l2;
            return CHOMSKY3_OK;
        }

        case CHOMSKY3_NODE_PLUS: {
            /* body first, then greedy/lazy loop back */
            size_t l1 = e->count;
            chomsky3_error_t err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;
            size_t split = e->count;
            err = emit_inst(e, CHOMSKY3_OP_SPLIT, 0, 0, 0);
            if (err != CHOMSKY3_OK) return err;
            size_t l3 = e->count;
            if (node->greedy) {
                e->insts[split].operand1 = (uint32_t)l1;
                e->insts[split].operand2 = (uint32_t)l3;
            } else {
                e->insts[split].operand1 = (uint32_t)l3;
                e->insts[split].operand2 = (uint32_t)l1;
            }
            return CHOMSKY3_OK;
        }

        case CHOMSKY3_NODE_QUESTION:
            return emit_optional(e, node->left, node->greedy);

        case CHOMSKY3_NODE_REPEAT: {
            uint32_t min = node->data.repeat.min;
            uint32_t max = node->data.repeat.max;

            if (max == UINT32_MAX) {
                /* {min,} : min copies then star/plus of body */
                if (min <= EMIT_UNROLL_LIMIT) {
                    chomsky3_error_t err = emit_repeated_copies(e, node->left, min);
                    if (err != CHOMSKY3_OK) return err;
                    /* Then star of body */
                    chomsky3_regex_node_t star = {
                        .type = CHOMSKY3_NODE_STAR,
                        .left = (chomsky3_regex_node_t *)node->left,
                        .greedy = node->greedy
                    };
                    return emit_node(e, &star);
                }
                /* Large min: use REPEAT op */
                return emit_inst(e, node->greedy ? CHOMSKY3_OP_REPEAT : CHOMSKY3_OP_REPEAT_LAZY,
                                 min, UINT32_MAX, 0);
            }

            if (max <= EMIT_UNROLL_LIMIT) {
                /* {min,max}: min mandatory copies + (max-min) optionals */
                chomsky3_error_t err = emit_repeated_copies(e, node->left, min);
                if (err != CHOMSKY3_OK) return err;
                /* Optionals nest: (a(a(a)?)?)? — emit iteratively */
                for (uint32_t i = min; i < max; i++) {
                    err = emit_optional(e, node->left, node->greedy);
                    if (err != CHOMSKY3_OK) return err;
                }
                return CHOMSKY3_OK;
            }

            /* Large bounded repeat: REPEAT op with body inline is not
             * expressible; emit REPEAT with min/max and let the VM loop
             * over the following body region marked by operand3. */
            size_t rep = e->count;
            chomsky3_error_t err = emit_inst(e,
                                             node->greedy ? CHOMSKY3_OP_REPEAT : CHOMSKY3_OP_REPEAT_LAZY,
                                             min, max, 0);
            if (err != CHOMSKY3_OK) return err;
            size_t body = e->count;
            err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;
            e->insts[rep].operand3 = (uint32_t)body;
            return CHOMSKY3_OK;
        }

        case CHOMSKY3_NODE_GROUP: {
            chomsky3_error_t err = emit_inst(e, CHOMSKY3_OP_SAVE_START,
                                             node->data.group_id, 0, 0);
            if (err != CHOMSKY3_OK) return err;
            err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;
            return emit_inst(e, CHOMSKY3_OP_SAVE_END, node->data.group_id, 0, 0);
        }

        case CHOMSKY3_NODE_BACKREF:
            return emit_inst(e, CHOMSKY3_OP_BACKREF, node->data.backref_id, 0, 0);

        case CHOMSKY3_NODE_LOOKAHEAD:
        case CHOMSKY3_NODE_LOOKAHEAD_NEG: {
            /* LOOK_AHEAD body_pc, cont_pc; body ... ; MATCH; cont: */
            size_t la = e->count;
            chomsky3_error_t err = emit_inst(e,
                                             node->type == CHOMSKY3_NODE_LOOKAHEAD
                                                 ? CHOMSKY3_OP_LOOK_AHEAD
                                                 : CHOMSKY3_OP_LOOK_AHEAD_NEG,
                                             0, 0, 0);
            if (err != CHOMSKY3_OK) return err;

            size_t body = e->count;
            err = emit_node(e, node->left);
            if (err != CHOMSKY3_OK) return err;

            err = emit_inst(e, CHOMSKY3_OP_MATCH, 0, 0, 0);
            if (err != CHOMSKY3_OK) return err;

            size_t cont = e->count;
            e->insts[la].operand1 = (uint32_t)body;
            e->insts[la].operand2 = (uint32_t)cont;
            return CHOMSKY3_OK;
        }
    }

    return CHOMSKY3_ERR_INTERNAL;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

static chomsky3_bytecode_t *bytecode_create(void) {
    chomsky3_bytecode_t *bytecode = calloc(1, sizeof(*bytecode));
    if (!bytecode) return NULL;
    bytecode->header.magic = CHOMSKY3_BYTECODE_MAGIC;
    bytecode->header.version.major = CHOMSKY3_BYTECODE_VERSION_MAJOR;
    bytecode->header.version.minor = CHOMSKY3_BYTECODE_VERSION_MINOR;
    bytecode->header.version.patch = CHOMSKY3_BYTECODE_VERSION_PATCH;
    return bytecode;
}

chomsky3_error_t chomsky3_bytecode_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    chomsky3_bytecode_t **bytecode
) {
    (void)ctx;
    if (!ir || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    *bytecode = NULL;

    const chomsky3_regex_t *ast = ir->source_ast;
    if (!ast) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    if (ast->parse_error != CHOMSKY3_OK) {
        return ast->parse_error;
    }

    emitter_t e;
    memset(&e, 0, sizeof(e));
    e.flags = ir->flags;

    /* Unanchored search: prepend implicit .*? prefix unless the pattern
     * is anchored at the start. */
    bool anchored = ast->root && ast->root->type == CHOMSKY3_NODE_ANCHOR_START;
    if (!anchored) {
        /* L1: SPLIT L3, L2 (lazy); L2: ANY_NL; JUMP L1; L3: <pattern> */
        size_t l1 = e.count;
        chomsky3_error_t err = emit_inst(&e, CHOMSKY3_OP_SPLIT, 0, 0, 0);
        if (err != CHOMSKY3_OK) goto fail;
        size_t l2 = e.count;
        err = emit_inst(&e, CHOMSKY3_OP_ANY_NL, 0, 0, 0);
        if (err != CHOMSKY3_OK) goto fail;
        err = emit_inst(&e, CHOMSKY3_OP_JUMP, (uint32_t)l1, 0, 0);
        if (err != CHOMSKY3_OK) goto fail;
        size_t l3 = e.count;
        e.insts[l1].operand1 = (uint32_t)l3;
        e.insts[l1].operand2 = (uint32_t)l2;
    }

    /* Whole-match capture: group 0 */
    chomsky3_error_t err = CHOMSKY3_OK;
    err = emit_inst(&e, CHOMSKY3_OP_SAVE_START, 0, 0, 0);
    if (err != CHOMSKY3_OK) goto fail;
    err = emit_node(&e, ast->root);
    if (err != CHOMSKY3_OK) goto fail;
    err = emit_inst(&e, CHOMSKY3_OP_SAVE_END, 0, 0, 0);
    if (err != CHOMSKY3_OK) goto fail;
    err = emit_inst(&e, CHOMSKY3_OP_MATCH, 0, 0, 0);
    if (err != CHOMSKY3_OK) goto fail;

    {
        chomsky3_bytecode_t *bc = bytecode_create();
        if (!bc) {
            err = CHOMSKY3_ERR_OUT_OF_MEMORY;
            goto fail;
        }
        bc->instructions = e.insts;
        bc->data = e.data;
        bc->header.flags = e.flags;
        bc->header.num_instructions = (uint32_t)e.count;
        bc->header.num_captures = (uint32_t)ast->num_groups + 1;
        bc->header.data_size = (uint32_t)e.data_size;
        bc->code_size = e.count * sizeof(*e.insts);
        bc->data_section_size = e.data_size;
        bc->total_size = sizeof(bc->header) + bc->code_size + bc->data_section_size;
        if (ast->pattern) {
            bc->pattern_source = malloc(ast->pattern_length + 1);
            if (bc->pattern_source) {
                memcpy(bc->pattern_source, ast->pattern, ast->pattern_length + 1);
            }
        }
        *bytecode = bc;
        return CHOMSKY3_OK;
    }

fail:
    free(e.insts);
    free(e.data);
    return err;
}

chomsky3_error_t chomsky3_bytecode_from_pattern(
    const chomsky3_pattern_t *pattern,
    chomsky3_bytecode_t **bytecode
) {
    if (!pattern || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    *bytecode = NULL;

    /* Patterns carry their emitted instruction stream in bytecode.code. */
    if (pattern->target == CHOMSKY3_TARGET_BYTECODE && pattern->bytecode.code) {
        /* Already a compiled bytecode image: shallow-copy the container. */
        const chomsky3_bytecode_t *src = (const chomsky3_bytecode_t *)pattern->bytecode.code;
        chomsky3_bytecode_t *copy = calloc(1, sizeof(*copy));
        if (!copy) return CHOMSKY3_ERR_OUT_OF_MEMORY;
        *copy = *src;
        copy->instructions = malloc(src->code_size);
        if (!copy->instructions) {
            free(copy);
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
        memcpy(copy->instructions, src->instructions, src->code_size);
        if (src->data_section_size && src->data) {
            copy->data = malloc(src->data_section_size);
            if (!copy->data) {
                free(copy->instructions);
                free(copy);
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            memcpy(copy->data, src->data, src->data_section_size);
        }
        copy->pattern_source = NULL;
        copy->line_map = NULL;
        *bytecode = copy;
        return CHOMSKY3_OK;
    }

    return CHOMSKY3_ERR_INVALID_ARGUMENT;
}

void chomsky3_bytecode_free(chomsky3_bytecode_t *bytecode) {
    if (!bytecode) {
        return;
    }

    free(bytecode->instructions);
    free(bytecode->data);
    free(bytecode->pattern_source);
    free(bytecode->line_map);
    free(bytecode);
}

chomsky3_error_t chomsky3_bytecode_serialize(
    const chomsky3_bytecode_t *bytecode,
    uint8_t **buffer,
    size_t *size
) {
    if (!bytecode || !buffer || !size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    *size = sizeof(bytecode->header) + bytecode->code_size + bytecode->data_section_size;
    *buffer = malloc(*size);
    if (!*buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    uint8_t *ptr = *buffer;
    memcpy(ptr, &bytecode->header, sizeof(bytecode->header));
    ptr += sizeof(bytecode->header);
    if (bytecode->code_size) {
        memcpy(ptr, bytecode->instructions, bytecode->code_size);
        ptr += bytecode->code_size;
    }
    if (bytecode->data_section_size) {
        memcpy(ptr, bytecode->data, bytecode->data_section_size);
    }
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_bytecode_deserialize(
    chomsky3_context_t *ctx,
    const uint8_t *buffer,
    size_t size,
    chomsky3_bytecode_t **bytecode
) {
    (void)ctx;
    if (!buffer || size < sizeof(chomsky3_bytecode_header_t) || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    *bytecode = NULL;

    chomsky3_bytecode_header_t header;
    memcpy(&header, buffer, sizeof(header));
    if (header.magic != CHOMSKY3_BYTECODE_MAGIC) {
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    size_t code_size = (size_t)header.num_instructions * sizeof(chomsky3_instruction_t);
    if (sizeof(header) + code_size + header.data_size > size) {
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    chomsky3_bytecode_t *bc = bytecode_create();
    if (!bc) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    bc->header = header;

    const uint8_t *ptr = buffer + sizeof(header);
    bc->instructions = malloc(code_size ? code_size : 1);
    if (!bc->instructions) {
        free(bc);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    memcpy(bc->instructions, ptr, code_size);
    ptr += code_size;

    if (header.data_size) {
        bc->data = malloc(header.data_size);
        if (!bc->data) {
            free(bc->instructions);
            free(bc);
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
        memcpy(bc->data, ptr, header.data_size);
    }

    bc->code_size = code_size;
    bc->data_section_size = header.data_size;
    bc->total_size = sizeof(header) + code_size + header.data_size;
    *bytecode = bc;
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_bytecode_save(
    const chomsky3_bytecode_t *bytecode,
    const char *path
) {
    if (!bytecode || !path) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    uint8_t *buffer = NULL;
    size_t size = 0;
    chomsky3_error_t err = chomsky3_bytecode_serialize(bytecode, &buffer, &size);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        free(buffer);
        return CHOMSKY3_ERR_IO_ERROR;
    }
    size_t written = fwrite(buffer, 1, size, file);
    fclose(file);
    free(buffer);
    return written == size ? CHOMSKY3_OK : CHOMSKY3_ERR_IO_ERROR;
}

chomsky3_error_t chomsky3_bytecode_load(
    chomsky3_context_t *ctx,
    const char *path,
    chomsky3_bytecode_t **bytecode
) {
    if (!path || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return CHOMSKY3_ERR_IO_ERROR;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return CHOMSKY3_ERR_IO_ERROR;
    }
    long file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return CHOMSKY3_ERR_IO_ERROR;
    }

    uint8_t *buffer = malloc((size_t)file_size);
    if (!buffer) {
        fclose(file);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    size_t read_size = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    if (read_size != (size_t)file_size) {
        free(buffer);
        return CHOMSKY3_ERR_IO_ERROR;
    }

    chomsky3_error_t err = chomsky3_bytecode_deserialize(ctx, buffer, read_size, bytecode);
    free(buffer);
    return err;
}

bool chomsky3_bytecode_validate(const chomsky3_bytecode_t *bytecode) {
    return bytecode && bytecode->header.magic == CHOMSKY3_BYTECODE_MAGIC &&
           bytecode->instructions && bytecode->header.num_instructions > 0;
}

chomsky3_bytecode_version_t chomsky3_bytecode_get_version(
    const chomsky3_bytecode_t *bytecode
) {
    return bytecode ? bytecode->header.version : (chomsky3_bytecode_version_t){0, 0, 0};
}

const char *chomsky3_opcode_name(chomsky3_opcode_t opcode) {
    switch (opcode) {
        case CHOMSKY3_OP_CHAR:             return "CHAR";
        case CHOMSKY3_OP_CHAR_RANGE:       return "CHAR_RANGE";
        case CHOMSKY3_OP_CHAR_CLASS:       return "CHAR_CLASS";
        case CHOMSKY3_OP_ANY:              return "ANY";
        case CHOMSKY3_OP_ANY_NL:           return "ANY_NL";
        case CHOMSKY3_OP_STRING:           return "STRING";
        case CHOMSKY3_OP_STRING_ICASE:     return "STRING_ICASE";
        case CHOMSKY3_OP_JUMP:             return "JUMP";
        case CHOMSKY3_OP_SPLIT:            return "SPLIT";
        case CHOMSKY3_OP_MATCH:            return "MATCH";
        case CHOMSKY3_OP_FAIL:             return "FAIL";
        case CHOMSKY3_OP_ANCHOR_START:     return "ANCHOR_START";
        case CHOMSKY3_OP_ANCHOR_END:       return "ANCHOR_END";
        case CHOMSKY3_OP_ANCHOR_LINE_START: return "ANCHOR_LINE_START";
        case CHOMSKY3_OP_ANCHOR_LINE_END:  return "ANCHOR_LINE_END";
        case CHOMSKY3_OP_ANCHOR_WORD:      return "ANCHOR_WORD";
        case CHOMSKY3_OP_ANCHOR_NWORD:     return "ANCHOR_NWORD";
        case CHOMSKY3_OP_SAVE_START:       return "SAVE_START";
        case CHOMSKY3_OP_SAVE_END:         return "SAVE_END";
        case CHOMSKY3_OP_BACKREF:          return "BACKREF";
        case CHOMSKY3_OP_LOOK_AHEAD:       return "LOOK_AHEAD";
        case CHOMSKY3_OP_LOOK_AHEAD_NEG:   return "LOOK_AHEAD_NEG";
        case CHOMSKY3_OP_LOOK_BEHIND:      return "LOOK_BEHIND";
        case CHOMSKY3_OP_LOOK_BEHIND_NEG:  return "LOOK_BEHIND_NEG";
        case CHOMSKY3_OP_REPEAT:           return "REPEAT";
        case CHOMSKY3_OP_REPEAT_LAZY:      return "REPEAT_LAZY";
        case CHOMSKY3_OP_REPEAT_NG:        return "REPEAT_NG";
        case CHOMSKY3_OP_NOP:              return "NOP";
        case CHOMSKY3_OP_DEBUG:            return "DEBUG";
        case CHOMSKY3_OP_CHECKPOINT:       return "CHECKPOINT";
        default:                           return "UNKNOWN";
    }
}

chomsky3_error_t chomsky3_bytecode_disassemble(
    const chomsky3_bytecode_t *bytecode,
    char **output
) {
    if (!bytecode || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Two passes: measure, then print. 64 bytes per line is a safe upper
     * bound for the formats below. */
    size_t capacity = (size_t)bytecode->header.num_instructions * 64 + 128;
    char *buffer = malloc(capacity);
    if (!buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    size_t used = 0;
    used += (size_t)snprintf(buffer + used, capacity - used,
                             "; chomsky3 bytecode: %u instructions, %u captures, data=%u bytes\n",
                             bytecode->header.num_instructions,
                             bytecode->header.num_captures,
                             bytecode->header.data_size);
    for (uint32_t i = 0; i < bytecode->header.num_instructions; i++) {
        const chomsky3_instruction_t *inst = &bytecode->instructions[i];
        used += (size_t)snprintf(buffer + used, capacity - used,
                                 "%04u: %-18s %u %u %u\n",
                                 i, chomsky3_opcode_name(inst->opcode),
                                 inst->operand1, inst->operand2, inst->operand3);
    }

    *output = buffer;
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_bytecode_optimize(
    const chomsky3_bytecode_t *bytecode,
    int level,
    chomsky3_bytecode_t **optimized
) {
    if (!bytecode || !optimized || level < 0 || level > 3) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Serialize + deserialize gives a deep, owned copy. Peephole cleanup:
     * strip NOP instructions and retarget jumps at level >= 1. */
    uint8_t *buffer = NULL;
    size_t size = 0;
    chomsky3_error_t err = chomsky3_bytecode_serialize(bytecode, &buffer, &size);
    if (err != CHOMSKY3_OK) {
        return err;
    }
    err = chomsky3_bytecode_deserialize(NULL, buffer, size, optimized);
    free(buffer);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    if (level >= 1) {
        chomsky3_bytecode_t *bc = *optimized;
        /* Count non-NOP instructions */
        size_t live = 0;
        for (uint32_t i = 0; i < bc->header.num_instructions; i++) {
            if (bc->instructions[i].opcode != CHOMSKY3_OP_NOP) live++;
        }
        if (live < bc->header.num_instructions) {
            /* Build remap table */
            uint32_t *remap = malloc(bc->header.num_instructions * sizeof(uint32_t));
            if (!remap) {
                return CHOMSKY3_OK; /* keep unoptimized copy */
            }
            chomsky3_instruction_t *compacted = malloc(live * sizeof(*compacted));
            if (!compacted) {
                free(remap);
                return CHOMSKY3_OK;
            }
            uint32_t next = 0;
            for (uint32_t i = 0; i < bc->header.num_instructions; i++) {
                remap[i] = next;
                if (bc->instructions[i].opcode != CHOMSKY3_OP_NOP) {
                    compacted[next++] = bc->instructions[i];
                }
            }
            /* Retarget control-flow operands */
            for (uint32_t i = 0; i < (uint32_t)live; i++) {
                chomsky3_instruction_t *inst = &compacted[i];
                switch (inst->opcode) {
                    case CHOMSKY3_OP_JUMP:
                        inst->operand1 = remap[inst->operand1];
                        break;
                    case CHOMSKY3_OP_SPLIT:
                        inst->operand1 = remap[inst->operand1];
                        inst->operand2 = remap[inst->operand2];
                        break;
                    case CHOMSKY3_OP_LOOK_AHEAD:
                    case CHOMSKY3_OP_LOOK_AHEAD_NEG:
                        inst->operand1 = remap[inst->operand1];
                        inst->operand2 = remap[inst->operand2];
                        break;
                    default:
                        break;
                }
            }
            free(bc->instructions);
            bc->instructions = compacted;
            bc->header.num_instructions = (uint32_t)live;
            bc->code_size = live * sizeof(*compacted);
            bc->total_size = sizeof(bc->header) + bc->code_size + bc->data_section_size;
            free(remap);
        }
    }

    return CHOMSKY3_OK;
}

bool chomsky3_bytecode_verify(
    const chomsky3_bytecode_t *bytecode,
    char **errors
) {
    if (errors) *errors = NULL;
    if (!chomsky3_bytecode_validate(bytecode)) {
        if (errors) {
            const char *message = "invalid bytecode header or empty instruction stream";
            *errors = malloc(strlen(message) + 1);
            if (*errors) strcpy(*errors, message);
        }
        return false;
    }

    /* Verify jump targets are in range */
    for (uint32_t i = 0; i < bytecode->header.num_instructions; i++) {
        const chomsky3_instruction_t *inst = &bytecode->instructions[i];
        uint32_t limit = bytecode->header.num_instructions;
        bool bad = false;
        switch (inst->opcode) {
            case CHOMSKY3_OP_JUMP:
                bad = inst->operand1 > limit;
                break;
            case CHOMSKY3_OP_SPLIT:
                bad = inst->operand1 > limit || inst->operand2 > limit;
                break;
            case CHOMSKY3_OP_LOOK_AHEAD:
            case CHOMSKY3_OP_LOOK_AHEAD_NEG:
                bad = inst->operand1 > limit || inst->operand2 > limit;
                break;
            default:
                break;
        }
        if (bad) {
            if (errors) {
                char message[96];
                snprintf(message, sizeof(message),
                         "instruction %u has out-of-range jump target", i);
                *errors = malloc(strlen(message) + 1);
                if (*errors) strcpy(*errors, message);
            }
            return false;
        }
    }
    return true;
}

uint32_t chomsky3_bytecode_complexity(const chomsky3_bytecode_t *bytecode) {
    if (!bytecode) return 0;
    uint32_t score = bytecode->header.num_instructions;
    for (uint32_t i = 0; i < bytecode->header.num_instructions; i++) {
        switch (bytecode->instructions[i].opcode) {
            case CHOMSKY3_OP_SPLIT:
            case CHOMSKY3_OP_REPEAT:
            case CHOMSKY3_OP_REPEAT_LAZY:
                score += 2; /* backtracking constructs cost extra */
                break;
            case CHOMSKY3_OP_BACKREF:
            case CHOMSKY3_OP_LOOK_AHEAD:
            case CHOMSKY3_OP_LOOK_AHEAD_NEG:
                score += 4;
                break;
            default:
                break;
        }
    }
    return score;
}
