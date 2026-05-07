/**
 * libchomsky3 - Intermediate Representation (IR) Implementation
 * 
 * Implements the IR structure used in the compilation pipeline between
 * the regex AST and target code generation.
 */

#include "chomsky3/ir.h"
#include "chomsky3/error.h"
#include "chomsky3/ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Helper macros */
#define INITIAL_CAPACITY 16
#define MAX_OPERANDS 4

/* Forward declarations for internal helpers */
static void free_block(chomsky3_ir_block_t *block);
static void free_instruction(chomsky3_ir_instruction_t *inst);
static chomsky3_error_t build_from_ast_node(
    chomsky3_ir_builder_t *builder,
    const chomsky3_ast_node_t *node,
    chomsky3_ir_block_t **entry,
    chomsky3_ir_block_t **exit
);
static const char *opcode_to_string(chomsky3_ir_opcode_t opcode);
static const char *type_to_string(chomsky3_ir_type_t type);
static void print_value(const chomsky3_ir_value_t *value, FILE *fp);
static void print_instruction(const chomsky3_ir_instruction_t *inst, FILE *fp);
static void print_block(const chomsky3_ir_block_t *block, FILE *fp);

/**
 * Create new IR.
 */
chomsky3_ir_t *chomsky3_ir_create(
    chomsky3_context_t *ctx,
    const char *name
) {
    if (!ctx) {
        return NULL;
    }

    chomsky3_ir_t *ir = calloc(1, sizeof(chomsky3_ir_t));
    if (!ir) {
        return NULL;
    }

    ir->ctx = ctx;
    ir->name = name ? strdup(name) : NULL;
    ir->num_registers = 0;
    ir->num_physical_registers = 0;
    ir->num_blocks = 0;
    ir->total_instructions = 0;
    ir->max_stack_depth = 0;
    ir->flags = 0;

    return ir;
}

/**
 * Destroy IR.
 */
void chomsky3_ir_destroy(chomsky3_ir_t *ir) {
    if (!ir) {
        return;
    }

    /* Free all blocks */
    chomsky3_ir_block_t *block = ir->first_block;
    while (block) {
        chomsky3_ir_block_t *next = block->next;
        free_block(block);
        block = next;
    }

    /* Free string constants */
    for (size_t i = 0; i < ir->num_string_constants; i++) {
        free((void *)ir->string_constants[i]);
    }
    free(ir->string_constants);

    free((void *)ir->name);
    free((void *)ir->source_pattern);
    free(ir);
}

/**
 * Free a basic block.
 */
static void free_block(chomsky3_ir_block_t *block) {
    if (!block) {
        return;
    }

    /* Free all instructions */
    chomsky3_ir_instruction_t *inst = block->first;
    while (inst) {
        chomsky3_ir_instruction_t *next = inst->next;
        free_instruction(inst);
        inst = next;
    }

    free((void *)block->label);
    free(block->predecessors);
    free(block->successors);
    free(block->dominated);
    free(block);
}

/**
 * Free an instruction.
 */
static void free_instruction(chomsky3_ir_instruction_t *inst) {
    if (!inst) {
        return;
    }

    free((void *)inst->comment);
    
    /* Free string values if owned */
    if (inst->dest.type == CHOMSKY3_IR_VALUE_STRING && inst->dest.data.string) {
        /* String constants are managed by IR, not individual instructions */
    }
    if (inst->src1.type == CHOMSKY3_IR_VALUE_STRING && inst->src1.data.string) {
        /* String constants are managed by IR */
    }
    
    free(inst);
}

/**
 * Create IR builder.
 */
chomsky3_ir_builder_t *chomsky3_ir_builder_create(chomsky3_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }

    chomsky3_ir_builder_t *builder = calloc(1, sizeof(chomsky3_ir_builder_t));
    if (!builder) {
        return NULL;
    }

    builder->ctx = ctx;
    builder->next_register = 0;
    builder->next_label = 0;
    builder->next_block_id = 0;
    builder->in_ssa_form = false;
    builder->flags = 0;

    return builder;
}

/**
 * Destroy IR builder.
 */
void chomsky3_ir_builder_destroy(chomsky3_ir_builder_t *builder) {
    if (!builder) {
        return;
    }

    /* IR is owned separately, don't free it here */
    free(builder);
}

/**
 * Build IR from regex AST.
 */
chomsky3_error_t chomsky3_ir_build_from_ast(
    chomsky3_ir_builder_t *builder,
    const chomsky3_regex_t *ast,
    chomsky3_ir_t **ir
) {
    if (!builder || !ast || !ir) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Create new IR */
    chomsky3_ir_t *new_ir = chomsky3_ir_create(builder->ctx, "regex_match");
    if (!new_ir) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    builder->ir = new_ir;
    new_ir->source_ast = ast;

    /* Create entry and exit blocks */
    chomsky3_ir_block_t *entry = chomsky3_ir_block_create(builder, "entry");
    chomsky3_ir_block_t *exit = chomsky3_ir_block_create(builder, "exit");
    
    if (!entry || !exit) {
        chomsky3_ir_destroy(new_ir);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    chomsky3_ir_append_block(new_ir, entry);
    chomsky3_ir_append_block(new_ir, exit);
    
    new_ir->entry = entry;
    new_ir->exit = exit;
    builder->current_block = entry;

    /* Build IR from AST root */
    chomsky3_ir_block_t *pattern_entry = NULL;
    chomsky3_ir_block_t *pattern_exit = NULL;
    
    chomsky3_error_t err = build_from_ast_node(
        builder,
        ast->root,
        &pattern_entry,
        &pattern_exit
    );
    
    if (err != CHOMSKY3_OK) {
        chomsky3_ir_destroy(new_ir);
        return err;
    }

    /* Connect entry to pattern */
    if (pattern_entry) {
        chomsky3_ir_add_edge(entry, pattern_entry);
    }

    /* Connect pattern to exit with match instruction */
    if (pattern_exit) {
        chomsky3_ir_instruction_t *match_inst = 
            chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_MATCH);
        if (match_inst) {
            chomsky3_ir_append_instruction(pattern_exit, match_inst);
        }
        chomsky3_ir_add_edge(pattern_exit, exit);
    }

    /* Add fail instruction to exit */
    chomsky3_ir_instruction_t *fail_inst = 
        chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_FAIL);
    if (fail_inst) {
        chomsky3_ir_append_instruction(exit, fail_inst);
    }

    *ir = new_ir;
    return CHOMSKY3_OK;
}

/**
 * Build IR from AST node recursively.
 */
static chomsky3_error_t build_from_ast_node(
    chomsky3_ir_builder_t *builder,
    const chomsky3_ast_node_t *node,
    chomsky3_ir_block_t **entry,
    chomsky3_ir_block_t **exit
) {
    if (!builder || !node || !entry || !exit) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    chomsky3_error_t err = CHOMSKY3_OK;

    switch (node->type) {
        case CHOMSKY3_AST_LITERAL: {
            /* Create block with match instruction */
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_build_match_char(builder, node->data.literal);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_STRING: {
            /* Create block with string match */
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_MATCH_STRING);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            /* Store string length in src1 */
            inst->src1 = chomsky3_ir_value_immediate(
                node->data.string.length,
                CHOMSKY3_IR_TYPE_U32
            );
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_CHAR_CLASS:
        case CHOMSKY3_AST_NEGATED_CHAR_CLASS: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_MATCH_CLASS);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            /* Store negation flag */
            inst->src1 = chomsky3_ir_value_immediate(
                node->data.char_class.negated ? 1 : 0,
                CHOMSKY3_IR_TYPE_BOOL
            );
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_DOT: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_MATCH_ANY);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_CONCATENATION: {
            /* Chain children sequentially */
            chomsky3_ir_block_t *first_entry = NULL;
            chomsky3_ir_block_t *last_exit = NULL;
            
            for (size_t i = 0; i < node->data.concatenation.num_children; i++) {
                chomsky3_ir_block_t *child_entry = NULL;
                chomsky3_ir_block_t *child_exit = NULL;
                
                err = build_from_ast_node(
                    builder,
                    node->data.concatenation.children[i],
                    &child_entry,
                    &child_exit
                );
                
                if (err != CHOMSKY3_OK) {
                    return err;
                }
                
                if (i == 0) {
                    first_entry = child_entry;
                } else if (last_exit && child_entry) {
                    chomsky3_ir_add_edge(last_exit, child_entry);
                }
                
                last_exit = child_exit;
            }
            
            *entry = first_entry;
            *exit = last_exit;
            break;
        }

        case CHOMSKY3_AST_ALTERNATION: {
            /* Create split for alternatives */
            chomsky3_ir_block_t *split_block = 
                chomsky3_ir_block_create(builder, "alt_split");
            chomsky3_ir_block_t *join_block = 
                chomsky3_ir_block_create(builder, "alt_join");
            
            if (!split_block || !join_block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, split_block);
            chomsky3_ir_append_block(builder->ir, join_block);
            
            /* Build each alternative */
            for (size_t i = 0; i < node->data.alternation.num_alternatives; i++) {
                chomsky3_ir_block_t *alt_entry = NULL;
                chomsky3_ir_block_t *alt_exit = NULL;
                
                err = build_from_ast_node(
                    builder,
                    node->data.alternation.alternatives[i],
                    &alt_entry,
                    &alt_exit
                );
                
                if (err != CHOMSKY3_OK) {
                    return err;
                }
                
                /* Connect split to alternative entry */
                if (alt_entry) {
                    chomsky3_ir_add_edge(split_block, alt_entry);
                }
                
                /* Connect alternative exit to join */
                if (alt_exit) {
                    chomsky3_ir_add_edge(alt_exit, join_block);
                }
            }
            
            *entry = split_block;
            *exit = join_block;
            break;
        }

        case CHOMSKY3_AST_ZERO_OR_MORE:
        case CHOMSKY3_AST_ONE_OR_MORE:
        case CHOMSKY3_AST_ZERO_OR_ONE:
        case CHOMSKY3_AST_REPEAT:
        case CHOMSKY3_AST_ZERO_OR_MORE_LAZY:
        case CHOMSKY3_AST_ONE_OR_MORE_LAZY:
        case CHOMSKY3_AST_ZERO_OR_ONE_LAZY:
        case CHOMSKY3_AST_REPEAT_LAZY: {
            /* Build child pattern */
            chomsky3_ir_block_t *child_entry = NULL;
            chomsky3_ir_block_t *child_exit = NULL;
            
            err = build_from_ast_node(
                builder,
                node->data.quantifier.child,
                &child_entry,
                &child_exit
            );
            
            if (err != CHOMSKY3_OK) {
                return err;
            }
            
            /* Create loop structure */
            chomsky3_ir_block_t *loop_entry = 
                chomsky3_ir_block_create(builder, "loop_entry");
            chomsky3_ir_block_t *loop_exit = 
                chomsky3_ir_block_create(builder, "loop_exit");
            
            if (!loop_entry || !loop_exit) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, loop_entry);
            chomsky3_ir_append_block(builder->ir, loop_exit);
            
            /* Add repeat start instruction */
            chomsky3_ir_instruction_t *start_inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_REPEAT_START);
            if (start_inst) {
                start_inst->src1 = chomsky3_ir_value_immediate(
                    node->data.quantifier.bounds.min,
                    CHOMSKY3_IR_TYPE_U32
                );
                start_inst->src2 = chomsky3_ir_value_immediate(
                    node->data.quantifier.bounds.max,
                    CHOMSKY3_IR_TYPE_U32
                );
                chomsky3_ir_append_instruction(loop_entry, start_inst);
            }
            
            /* Connect loop structure */
            chomsky3_ir_add_edge(loop_entry, child_entry);
            
            if (child_exit) {
                /* Add check count and loop back or exit */
                chomsky3_ir_instruction_t *check_inst = 
                    chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_CHECK_COUNT);
                if (check_inst) {
                    chomsky3_ir_append_instruction(child_exit, check_inst);
                }
                
                /* Loop back to entry or exit */
                chomsky3_ir_add_edge(child_exit, loop_entry);
                chomsky3_ir_add_edge(child_exit, loop_exit);
            }
            
            /* For zero-or-more/zero-or-one, allow skipping */
            if (node->type == CHOMSKY3_AST_ZERO_OR_MORE ||
                node->type == CHOMSKY3_AST_ZERO_OR_ONE ||
                node->type == CHOMSKY3_AST_ZERO_OR_MORE_LAZY ||
                node->type == CHOMSKY3_AST_ZERO_OR_ONE_LAZY) {
                chomsky3_ir_add_edge(loop_entry, loop_exit);
            }
            
            *entry = loop_entry;
            *exit = loop_exit;
            break;
        }

        case CHOMSKY3_AST_GROUP:
        case CHOMSKY3_AST_NON_CAPTURING_GROUP:
        case CHOMSKY3_AST_NAMED_GROUP: {
            /* Build child */
            chomsky3_ir_block_t *child_entry = NULL;
            chomsky3_ir_block_t *child_exit = NULL;
            
            err = build_from_ast_node(
                builder,
                node->data.group.child,
                &child_entry,
                &child_exit
            );
            
            if (err != CHOMSKY3_OK) {
                return err;
            }
            
            if (node->data.group.info.capturing) {
                /* Add capture start before child */
                chomsky3_ir_block_t *start_block = 
                    chomsky3_ir_block_create(builder, "capture_start");
                if (!start_block) {
                    return CHOMSKY3_ERR_OUT_OF_MEMORY;
                }
                
                chomsky3_ir_append_block(builder->ir, start_block);
                
                chomsky3_ir_instruction_t *start_inst = 
                    chomsky3_ir_build_capture_start(
                        builder,
                        node->data.group.info.group_id
                    );
                if (start_inst) {
                    chomsky3_ir_append_instruction(start_block, start_inst);
                }
                
                chomsky3_ir_add_edge(start_block, child_entry);
                
                /* Add capture end after child */
                chomsky3_ir_block_t *end_block = 
                    chomsky3_ir_block_create(builder, "capture_end");
                if (!end_block) {
                    return CHOMSKY3_ERR_OUT_OF_MEMORY;
                }
                
                chomsky3_ir_append_block(builder->ir, end_block);
                
                chomsky3_ir_instruction_t *end_inst = 
                    chomsky3_ir_build_capture_end(
                        builder,
                        node->data.group.info.group_id
                    );
                if (end_inst) {
                    chomsky3_ir_append_instruction(end_block, end_inst);
                }
                
                chomsky3_ir_add_edge(child_exit, end_block);
                
                *entry = start_block;
                *exit = end_block;
            } else {
                /* Non-capturing, just pass through */
                *entry = child_entry;
                *exit = child_exit;
            }
            break;
        }

        case CHOMSKY3_AST_BACKREF:
        case CHOMSKY3_AST_NAMED_BACKREF: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_BACKREFERENCE);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            inst->src1 = chomsky3_ir_value_immediate(
                node->data.backref.group_id,
                CHOMSKY3_IR_TYPE_U32
            );
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_START_ANCHOR: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_CHECK_BEGIN);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_END_ANCHOR: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_CHECK_END);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_WORD_BOUNDARY: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_CHECK_WORD_BOUNDARY);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        case CHOMSKY3_AST_LOOKAHEAD:
        case CHOMSKY3_AST_NEGATIVE_LOOKAHEAD: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, "lookahead");
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_opcode_t opcode = node->data.lookaround.positive ?
                CHOMSKY3_IR_OP_LOOKAHEAD_START :
                CHOMSKY3_IR_OP_NEG_LOOKAHEAD_START;
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, opcode);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            /* Build lookahead child */
            chomsky3_ir_block_t *child_entry = NULL;
            chomsky3_ir_block_t *child_exit = NULL;
            
            err = build_from_ast_node(
                builder,
                node->data.lookaround.child,
                &child_entry,
                &child_exit
            );
            
            if (err != CHOMSKY3_OK) {
                return err;
            }
            
            if (child_entry) {
                chomsky3_ir_add_edge(block, child_entry);
            }
            
            /* Add lookahead end */
            if (child_exit) {
                chomsky3_ir_instruction_t *end_inst = 
                    chomsky3_ir_instruction_create(builder, CHOMSKY3_IR_OP_LOOKAHEAD_END);
                if (end_inst) {
                    chomsky3_ir_append_instruction(child_exit, end_inst);
                }
            }
            
            *entry = block;
            *exit = child_exit ? child_exit : block;
            break;
        }

        case CHOMSKY3_AST_LOOKBEHIND:
        case CHOMSKY3_AST_NEGATIVE_LOOKBEHIND: {
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, "lookbehind");
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            chomsky3_ir_opcode_t opcode = node->data.lookaround.positive ?
                CHOMSKY3_IR_OP_LOOKBEHIND_START :
                CHOMSKY3_IR_OP_NEG_LOOKBEHIND_START;
            
            chomsky3_ir_instruction_t *inst = 
                chomsky3_ir_instruction_create(builder, opcode);
            if (!inst) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_instruction(block, inst);
            
            *entry = block;
            *exit = block;
            break;
        }

        default:
            /* Unsupported node type, create NOP */
            chomsky3_ir_block_t *block = chomsky3_ir_block_create(builder, NULL);
            if (!block) {
                return CHOMSKY3_ERR_OUT_OF_MEMORY;
            }
            
            chomsky3_ir_append_block(builder->ir, block);
            
            *entry = block;
            *exit = block;
            break;
    }

    return CHOMSKY3_OK;
}

/**
 * Create new basic block.
 */
chomsky3_ir_block_t *chomsky3_ir_block_create(
    chomsky3_ir_builder_t *builder,
    const char *label
) {
    if (!builder) {
        return NULL;
