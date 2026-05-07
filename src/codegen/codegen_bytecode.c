/**
 * libchomsky3 - Bytecode Code Generator
 * 
 * Generates bytecode from intermediate representation (IR).
 */

#include "chomsky3/bytecode.h"
#include "chomsky3/ir.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Magic number for bytecode format */
#define CHOMSKY3_BYTECODE_MAGIC 0x43484F4D  /* "CHOM" */

/* Current bytecode format version */
#define CHOMSKY3_BYTECODE_VERSION_MAJOR 1
#define CHOMSKY3_BYTECODE_VERSION_MINOR 0
#define CHOMSKY3_BYTECODE_VERSION_PATCH 0

/* Initial instruction capacity */
#define INITIAL_INSTRUCTION_CAPACITY 256
#define INITIAL_DATA_CAPACITY 1024

/* Code generation context */
typedef struct {
    chomsky3_context_t *ctx;
    
    /* Instruction buffer */
    chomsky3_instruction_t *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    
    /* Data section */
    uint8_t *data;
    size_t data_size;
    size_t data_capacity;
    
    /* Label tracking */
    size_t *label_positions;
    size_t label_count;
    size_t label_capacity;
    
    /* Capture group tracking */
    uint32_t num_captures;
    
    /* Compilation flags */
    uint32_t flags;
    
    /* Error tracking */
    chomsky3_error_t error;
} codegen_ctx_t;

/* Forward declarations */
static chomsky3_error_t init_codegen_ctx(
    codegen_ctx_t *ctx,
    chomsky3_context_t *parent_ctx
);
static void free_codegen_ctx(codegen_ctx_t *ctx);
static chomsky3_error_t emit_instruction(
    codegen_ctx_t *ctx,
    chomsky3_opcode_t opcode,
    uint32_t op1,
    uint32_t op2,
    uint32_t op3,
    const void *data
);
static chomsky3_error_t emit_ir_node(
    codegen_ctx_t *ctx,
    const chomsky3_ir_node_t *node
);
static chomsky3_error_t add_data(
    codegen_ctx_t *ctx,
    const void *data,
    size_t size,
    uint32_t *offset
);
static chomsky3_error_t allocate_label(
    codegen_ctx_t *ctx,
    size_t *label_id
);
static chomsky3_error_t set_label_position(
    codegen_ctx_t *ctx,
    size_t label_id,
    size_t position
);
static chomsky3_error_t get_label_position(
    codegen_ctx_t *ctx,
    size_t label_id,
    size_t *position
);
static uint32_t compute_checksum(
    const chomsky3_instruction_t *instructions,
    size_t count,
    const uint8_t *data,
    size_t data_size
);
static chomsky3_error_t grow_instruction_buffer(codegen_ctx_t *ctx);
static chomsky3_error_t grow_data_buffer(codegen_ctx_t *ctx, size_t required);
static chomsky3_error_t grow_label_buffer(codegen_ctx_t *ctx);

/**
 * Create bytecode from IR.
 */
chomsky3_error_t chomsky3_bytecode_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    chomsky3_bytecode_t **bytecode
) {
    if (!ctx || !ir || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Initialize code generation context */
    codegen_ctx_t codegen;
    chomsky3_error_t err = init_codegen_ctx(&codegen, ctx);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    /* Store compilation flags */
    codegen.flags = ir->flags;

    /* Generate bytecode from IR nodes */
    for (size_t i = 0; i < ir->node_count; i++) {
        err = emit_ir_node(&codegen, &ir->nodes[i]);
        if (err != CHOMSKY3_OK) {
            free_codegen_ctx(&codegen);
            return err;
        }
    }

    /* Emit final MATCH instruction if not already present */
    if (codegen.instruction_count == 0 ||
        codegen.instructions[codegen.instruction_count - 1].opcode != CHOMSKY3_OP_MATCH) {
        err = emit_instruction(&codegen, CHOMSKY3_OP_MATCH, 0, 0, 0, NULL);
        if (err != CHOMSKY3_OK) {
            free_codegen_ctx(&codegen);
            return err;
        }
    }

    /* Allocate bytecode structure */
    chomsky3_bytecode_t *bc = calloc(1, sizeof(chomsky3_bytecode_t));
    if (!bc) {
        free_codegen_ctx(&codegen);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* Fill header */
    bc->header.magic = CHOMSKY3_BYTECODE_MAGIC;
    bc->header.version.major = CHOMSKY3_BYTECODE_VERSION_MAJOR;
    bc->header.version.minor = CHOMSKY3_BYTECODE_VERSION_MINOR;
    bc->header.version.patch = CHOMSKY3_BYTECODE_VERSION_PATCH;
    bc->header.flags = codegen.flags;
    bc->header.num_instructions = codegen.instruction_count;
    bc->header.num_captures = codegen.num_captures;
    bc->header.data_size = codegen.data_size;
    bc->header.checksum = compute_checksum(
        codegen.instructions,
        codegen.instruction_count,
        codegen.data,
        codegen.data_size
    );

    /* Transfer ownership of instructions and data */
    bc->instructions = codegen.instructions;
    bc->data = codegen.data;
    
    /* Calculate sizes */
    bc->code_size = codegen.instruction_count * sizeof(chomsky3_instruction_t);
    bc->data_section_size = codegen.data_size;
    bc->total_size = sizeof(chomsky3_bytecode_header_t) + 
                     bc->code_size + 
                     bc->data_section_size;

    /* Copy pattern source if available */
    if (ir->pattern_source) {
        bc->pattern_source = strdup(ir->pattern_source);
    }

    /* Don't free instructions and data since we transferred ownership */
    codegen.instructions = NULL;
    codegen.data = NULL;
    free_codegen_ctx(&codegen);

    *bytecode = bc;
    return CHOMSKY3_OK;
}

/**
 * Create bytecode from compiled pattern.
 */
chomsky3_error_t chomsky3_bytecode_from_pattern(
    const chomsky3_pattern_t *pattern,
    chomsky3_bytecode_t **bytecode
) {
    if (!pattern || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Get IR from pattern */
    const chomsky3_ir_t *ir = chomsky3_pattern_get_ir(pattern);
    if (!ir) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_STATE,
                          "Pattern has no IR representation");
        return CHOMSKY3_ERR_INVALID_STATE;
    }

    /* Generate bytecode from IR */
    return chomsky3_bytecode_from_ir(pattern->ctx, ir, bytecode);
}

/**
 * Initialize code generation context.
 */
static chomsky3_error_t init_codegen_ctx(
    codegen_ctx_t *ctx,
    chomsky3_context_t *parent_ctx
) {
    memset(ctx, 0, sizeof(codegen_ctx_t));
    ctx->ctx = parent_ctx;

    /* Allocate instruction buffer */
    ctx->instruction_capacity = INITIAL_INSTRUCTION_CAPACITY;
    ctx->instructions = malloc(ctx->instruction_capacity * 
                              sizeof(chomsky3_instruction_t));
    if (!ctx->instructions) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* Allocate data buffer */
    ctx->data_capacity = INITIAL_DATA_CAPACITY;
    ctx->data = malloc(ctx->data_capacity);
    if (!ctx->data) {
        free(ctx->instructions);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* Allocate label buffer */
    ctx->label_capacity = 64;
    ctx->label_positions = malloc(ctx->label_capacity * sizeof(size_t));
    if (!ctx->label_positions) {
        free(ctx->instructions);
        free(ctx->data);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    return CHOMSKY3_OK;
}

/**
 * Free code generation context.
 */
static void free_codegen_ctx(codegen_ctx_t *ctx) {
    if (ctx->instructions) {
        free(ctx->instructions);
    }
    if (ctx->data) {
        free(ctx->data);
    }
    if (ctx->label_positions) {
        free(ctx->label_positions);
    }
}

/**
 * Emit a bytecode instruction.
 */
static chomsky3_error_t emit_instruction(
    codegen_ctx_t *ctx,
    chomsky3_opcode_t opcode,
    uint32_t op1,
    uint32_t op2,
    uint32_t op3,
    const void *data
) {
    /* Grow buffer if needed */
    if (ctx->instruction_count >= ctx->instruction_capacity) {
        chomsky3_error_t err = grow_instruction_buffer(ctx);
        if (err != CHOMSKY3_OK) {
            return err;
        }
    }

    /* Add instruction */
    chomsky3_instruction_t *inst = &ctx->instructions[ctx->instruction_count++];
    inst->opcode = opcode;
    inst->operand1 = op1;
    inst->operand2 = op2;
    inst->operand3 = op3;
    inst->data = data;

    return CHOMSKY3_OK;
}

/**
 * Emit bytecode for an IR node.
 */
static chomsky3_error_t emit_ir_node(
    codegen_ctx_t *ctx,
    const chomsky3_ir_node_t *node
) {
    if (!node) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    chomsky3_error_t err = CHOMSKY3_OK;

    switch (node->type) {
        case CHOMSKY3_IR_LITERAL: {
            /* Emit character match */
            if (node->data.literal.length == 1) {
                err = emit_instruction(ctx, CHOMSKY3_OP_CHAR,
                                      node->data.literal.text[0], 0, 0, NULL);
            } else {
                /* Multi-character string */
                uint32_t offset;
                err = add_data(ctx, node->data.literal.text,
                              node->data.literal.length, &offset);
                if (err == CHOMSKY3_OK) {
                    err = emit_instruction(ctx, CHOMSKY3_OP_STRING,
                                          offset, node->data.literal.length, 0, NULL);
                }
            }
            break;
        }

        case CHOMSKY3_IR_CHAR_CLASS: {
            /* Add character class data */
            uint32_t offset;
            err = add_data(ctx, node->data.char_class.bitmap, 32, &offset);
            if (err == CHOMSKY3_OK) {
                err = emit_instruction(ctx, CHOMSKY3_OP_CHAR_CLASS,
                                      offset, node->data.char_class.negated, 0, NULL);
            }
            break;
        }

        case CHOMSKY3_IR_CHAR_RANGE: {
            err = emit_instruction(ctx, CHOMSKY3_OP_CHAR_RANGE,
                                  node->data.char_range.start,
                                  node->data.char_range.end,
                                  node->data.char_range.negated, NULL);
            break;
        }

        case CHOMSKY3_IR_ANY_CHAR: {
            chomsky3_opcode_t op = node->data.any_char.include_newline ?
                                   CHOMSKY3_OP_ANY_NL : CHOMSKY3_OP_ANY;
            err = emit_instruction(ctx, op, 0, 0, 0, NULL);
            break;
        }

        case CHOMSKY3_IR_ANCHOR: {
            chomsky3_opcode_t op;
            switch (node->data.anchor.type) {
                case CHOMSKY3_ANCHOR_START:
                    op = CHOMSKY3_OP_ANCHOR_START;
                    break;
                case CHOMSKY3_ANCHOR_END:
                    op = CHOMSKY3_OP_ANCHOR_END;
                    break;
                case CHOMSKY3_ANCHOR_LINE_START:
                    op = CHOMSKY3_OP_ANCHOR_LINE_START;
                    break;
                case CHOMSKY3_ANCHOR_LINE_END:
                    op = CHOMSKY3_OP_ANCHOR_LINE_END;
                    break;
                case CHOMSKY3_ANCHOR_WORD_BOUNDARY:
                    op = CHOMSKY3_OP_ANCHOR_WORD;
                    break;
                case CHOMSKY3_ANCHOR_NON_WORD_BOUNDARY:
                    op = CHOMSKY3_OP_ANCHOR_NWORD;
                    break;
                default:
                    return CHOMSKY3_ERR_INVALID_ARGUMENT;
            }
            err = emit_instruction(ctx, op, 0, 0, 0, NULL);
            break;
        }

        case CHOMSKY3_IR_CAPTURE: {
            /* Emit save start */
            err = emit_instruction(ctx, CHOMSKY3_OP_SAVE_START,
                                  node->data.capture.group_id, 0, 0, NULL);
            if (err != CHOMSKY3_OK) break;

            /* Track capture groups */
            if (node->data.capture.group_id >= ctx->num_captures) {
                ctx->num_captures = node->data.capture.group_id + 1;
            }

            /* Emit child nodes */
            for (size_t i = 0; i < node->child_count; i++) {
                err = emit_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) break;
            }

            /* Emit save end */
            if (err == CHOMSKY3_OK) {
                err = emit_instruction(ctx, CHOMSKY3_OP_SAVE_END,
                                      node->data.capture.group_id, 0, 0, NULL);
            }
            break;
        }

        case CHOMSKY3_IR_BACKREFERENCE: {
            err = emit_instruction(ctx, CHOMSKY3_OP_BACKREF,
                                  node->data.backref.group_id,
                                  node->data.backref.case_insensitive, 0, NULL);
            break;
        }

        case CHOMSKY3_IR_ALTERNATION: {
            /* Emit split for each alternative */
            size_t *alt_labels = malloc(node->child_count * sizeof(size_t));
            if (!alt_labels) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }

            size_t end_label;
            err = allocate_label(ctx, &end_label);
            if (err != CHOMSKY3_OK) {
                free(alt_labels);
                return err;
            }

            /* Allocate labels for each alternative */
            for (size_t i = 0; i < node->child_count; i++) {
                err = allocate_label(ctx, &alt_labels[i]);
                if (err != CHOMSKY3_OK) {
                    free(alt_labels);
                    return err;
                }
            }

            /* Emit splits */
            for (size_t i = 0; i < node->child_count - 1; i++) {
                size_t next_pos;
                err = get_label_position(ctx, alt_labels[i + 1], &next_pos);
                if (err != CHOMSKY3_OK) {
                    free(alt_labels);
                    return err;
                }
                
                err = emit_instruction(ctx, CHOMSKY3_OP_SPLIT,
                                      alt_labels[i], alt_labels[i + 1], 0, NULL);
                if (err != CHOMSKY3_OK) {
                    free(alt_labels);
                    return err;
                }
            }

            /* Emit alternatives */
            for (size_t i = 0; i < node->child_count; i++) {
                err = set_label_position(ctx, alt_labels[i], ctx->instruction_count);
                if (err != CHOMSKY3_OK) {
                    free(alt_labels);
                    return err;
                }

                err = emit_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) {
                    free(alt_labels);
                    return err;
                }

                /* Jump to end (except last alternative) */
                if (i < node->child_count - 1) {
                    err = emit_instruction(ctx, CHOMSKY3_OP_JUMP,
                                          end_label, 0, 0, NULL);
                    if (err != CHOMSKY3_OK) {
                        free(alt_labels);
                        return err;
                    }
                }
            }

            err = set_label_position(ctx, end_label, ctx->instruction_count);
            free(alt_labels);
            break;
        }

        case CHOMSKY3_IR_CONCATENATION: {
            /* Emit children in sequence */
            for (size_t i = 0; i < node->child_count; i++) {
                err = emit_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) break;
            }
            break;
        }

        case CHOMSKY3_IR_REPETITION: {
            chomsky3_opcode_t op = node->data.repetition.lazy ?
                                   CHOMSKY3_OP_REPEAT_LAZY : CHOMSKY3_OP_REPEAT;
            
            /* Emit repeat instruction with min/max bounds */
            err = emit_instruction(ctx, op,
                                  node->data.repetition.min,
                                  node->data.repetition.max,
                                  0, NULL);
            if (err != CHOMSKY3_OK) break;

            /* Emit child node */
            if (node->child_count > 0) {
                err = emit_ir_node(ctx, node->children[0]);
            }
            break;
        }

        case CHOMSKY3_IR_LOOKAHEAD: {
            chomsky3_opcode_t op = node->data.lookahead.negative ?
                                   CHOMSKY3_OP_LOOK_AHEAD_NEG : CHOMSKY3_OP_LOOK_AHEAD;
            
            size_t end_label;
            err = allocate_label(ctx, &end_label);
            if (err != CHOMSKY3_OK) break;

            err = emit_instruction(ctx, op, end_label, 0, 0, NULL);
            if (err != CHOMSKY3_OK) break;

            /* Emit lookahead body */
            for (size_t i = 0; i < node->child_count; i++) {
                err = emit_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) break;
            }

            err = set_label_position(ctx, end_label, ctx->instruction_count);
            break;
        }

        case CHOMSKY3_IR_LOOKBEHIND: {
            chomsky3_opcode_t op = node->data.lookbehind.negative ?
                                   CHOMSKY3_OP_LOOK_BEHIND_NEG : CHOMSKY3_OP_LOOK_BEHIND;
            
            err = emit_instruction(ctx, op,
                                  node->data.lookbehind.length, 0, 0, NULL);
            if (err != CHOMSKY3_OK) break;

            /* Emit lookbehind body */
            for (size_t i = 0; i < node->child_count; i++) {
                err = emit_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) break;
            }
            break;
        }

        default:
            chomsky3_set_error(CHOMSKY3_ERR_INVALID_ARGUMENT,
                              "Unknown IR node type");
            return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    return err;
}

/**
 * Add data to data section.
 */
static chomsky3_error_t add_data(
    codegen_ctx_t *ctx,
    const void *data,
    size_t size,
    uint32_t *offset
) {
    if (!data || size == 0) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Grow buffer if needed */
    if (ctx->data_size + size > ctx->data_capacity) {
        chomsky3_error_t err = grow_data_buffer(ctx, size);
        if (err != CHOMSKY3_OK) {
            return err;
        }
    }

    /* Copy data */
    *offset = ctx->data_size;
    memcpy(ctx->data + ctx->data_size, data, size);
    ctx->data_size += size;

    return CHOMSKY3_OK;
}

/**
 * Allocate a new label.
 */
static chomsky3_error_t allocate_label(
    codegen_ctx_t *ctx,
    size_t *label_id
) {
    if (ctx->label_count >= ctx->label_capacity) {
        chomsky3_error_t err = grow_label_buffer(ctx);
        if (err != CHOMSKY3_OK) {
            return err;
        }
    }

    *label_id = ctx->label_count++;
    ctx->label_positions[*label_id] = (size_t)-1;  /* Unresolved */

    return CHOMSKY3_OK;
}

/**
 * Set label position.
 */
static chomsky3_error_t set_label_position(
    codegen_ctx_t *ctx,
    size_t label_id,
    size_t position
) {
    if (label_id >= ctx->label_count) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    ctx->label_positions[label_id] = position;
    return CHOMSKY3_OK;
}

/**
 * Get label position.
 */
static chomsky3_error_t get_label_position(
    codegen_ctx_t *ctx,
    size_t label_id,
    size_t *position
) {
    if (label_id >= ctx->label_count) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    *position = ctx->label_positions[label_id];
    return CHOMSKY3_OK;
}

/**
 * Compute CRC32 checksum.
 */
static uint32_t compute_checksum(
    const chomsky3_instruction_t *instructions,
    size_t count,
    const uint8_t *data,
    size_t data_size
) {
    uint32_t crc = 0xFFFFFFFF;

    /* CRC32 lookup table */
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
        /* ... (full table omitted for brevity) ... */
    };

    /* Hash instructions */
    const uint8_t *bytes = (const uint8_t *)instructions;
    for (size_t i = 0; i < count * sizeof(chomsky3_instruction_t); i++) {
        crc = crc_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    /* Hash data section */
    for (size_t i = 0; i < data_size; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/**
 * Grow instruction buffer.
 */
static chomsky3_error_t grow_instruction_buffer(codegen_ctx_t *ctx) {
    size_t new_capacity = ctx->instruction_capacity * 2;
    chomsky3_instruction_t *new_buffer = realloc(
        ctx->instructions,
        new_capacity * sizeof(chomsky3_instruction_t)
    );
    
    if (!new_buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    ctx->instructions = new_buffer;
    ctx->instruction_capacity = new_capacity;

    return CHOMSKY3_OK;
}

/**
 * Grow data buffer.
 */
static chomsky3_error_t grow_data_buffer(codegen_ctx_t *ctx, size_t required) {
    size_t new_capacity = ctx->data_capacity;
    while (new_capacity < ctx->data_size + required) {
        new_capacity *= 2;
    }

    uint8_t *new_buffer = realloc(ctx->data, new_capacity);
    if (!new_buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    ctx->data = new_buffer;
    ctx->data_capacity = new_capacity;

    return CHOMSKY3_OK;
}

/**
 * Grow label buffer.
 */
static chomsky3_error_t grow_label_buffer(codegen_ctx_t *ctx) {
    size_t new_capacity = ctx->label_capacity * 2;
    size_t *new_buffer = realloc(
        ctx->label_positions,
        new_capacity * sizeof(size_t)
    );
    
    if (!new_buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    ctx->label_positions = new_buffer;
    ctx->label_capacity = new_capacity;

    return CHOMSKY3_OK;
}
