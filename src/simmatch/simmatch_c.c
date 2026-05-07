/**
 * libchomsky3 - C-based Simulation and Matching
 * 
 * Direct C implementation of regex matching without VM overhead.
 * Optimized for simple patterns and small inputs.
 */

#include "simmatch.h"
#include "chomsky3/bytecode.h"
#include <stdlib.h>
#include <string.h>

/* Match context management */

chomsky3_match_context_t *chomsky3_match_context_new(
    const uint8_t *input,
    size_t input_length,
    size_t num_captures
) {
    chomsky3_match_context_t *ctx = calloc(1, sizeof(chomsky3_match_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->input = input;
    ctx->input_length = input_length;
    ctx->position = 0;
    
    /* Allocate capture arrays */
    if (num_captures > 0) {
        ctx->capture_starts = calloc(num_captures, sizeof(size_t));
        ctx->capture_ends = calloc(num_captures, sizeof(size_t));
        
        if (!ctx->capture_starts || !ctx->capture_ends) {
            free(ctx->capture_starts);
            free(ctx->capture_ends);
            free(ctx);
            return NULL;
        }
        
        ctx->capture_capacity = num_captures;
    }
    
    /* Set default limits */
    ctx->max_steps = 1000000;
    ctx->max_backtracks = 100000;
    
    /* Initialize flags */
    ctx->anchored = false;
    ctx->multiline = false;
    ctx->dotall = false;
    ctx->case_insensitive = false;
    
    return ctx;
}

void chomsky3_match_context_free(chomsky3_match_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    free(ctx->capture_starts);
    free(ctx->capture_ends);
    free(ctx->stack);
    free(ctx);
}

void chomsky3_match_context_reset(
    chomsky3_match_context_t *ctx,
    const uint8_t *input,
    size_t input_length
) {
    if (!ctx) {
        return;
    }
    
    ctx->input = input;
    ctx->input_length = input_length;
    ctx->position = 0;
    ctx->stack_size = 0;
    ctx->steps = 0;
    ctx->backtracks = 0;
    ctx->num_captures = 0;
    
    /* Clear capture arrays */
    if (ctx->capture_starts) {
        memset(ctx->capture_starts, 0, ctx->capture_capacity * sizeof(size_t));
    }
    if (ctx->capture_ends) {
        memset(ctx->capture_ends, 0, ctx->capture_capacity * sizeof(size_t));
    }
}

/* C-based interpreter (simplified, optimized version) */

typedef struct {
    size_t pc;
    size_t sp;
    size_t num_captures;
} c_stack_frame_t;

typedef struct {
    const chomsky3_bytecode_t *bytecode;
    chomsky3_match_context_t *ctx;
    c_stack_frame_t *stack;
    size_t stack_top;
    size_t stack_capacity;
    size_t match_start;
    size_t match_end;
    bool matched;
} c_exec_state_t;

static inline int c_push_frame(c_exec_state_t *state, size_t pc, size_t sp) {
    if (state->stack_top >= state->stack_capacity) {
        /* Grow stack */
        size_t new_capacity = state->stack_capacity * 2;
        c_stack_frame_t *new_stack = realloc(state->stack, new_capacity * sizeof(c_stack_frame_t));
        if (!new_stack) {
            return -1;
        }
        state->stack = new_stack;
        state->stack_capacity = new_capacity;
    }
    
    c_stack_frame_t *frame = &state->stack[state->stack_top++];
    frame->pc = pc;
    frame->sp = sp;
    frame->num_captures = state->ctx->num_captures;
    
    return 0;
}

static inline int c_pop_frame(c_exec_state_t *state, size_t *pc, size_t *sp) {
    if (state->stack_top == 0) {
        return -1;
    }
    
    c_stack_frame_t *frame = &state->stack[--state->stack_top];
    *pc = frame->pc;
    *sp = frame->sp;
    state->ctx->num_captures = frame->num_captures;
    state->ctx->backtracks++;
    
    return 0;
}

static inline bool c_check_limits(chomsky3_match_context_t *ctx) {
    if (ctx->max_steps > 0 && ctx->steps >= ctx->max_steps) {
        return false;
    }
    if (ctx->max_backtracks > 0 && ctx->backtracks >= ctx->max_backtracks) {
        return false;
    }
    return true;
}

/* Fast path for common operations */
static inline bool c_match_char(
    const chomsky3_match_context_t *ctx,
    size_t sp,
    uint8_t expected
) {
    if (sp >= ctx->input_length) {
        return false;
    }
    
    uint8_t c = ctx->input[sp];
    
    if (ctx->case_insensitive) {
        return chomsky3_char_equal_icase(c, expected);
    }
    
    return c == expected;
}

static inline bool c_match_range(
    const chomsky3_match_context_t *ctx,
    size_t sp,
    uint8_t start,
    uint8_t end
) {
    if (sp >= ctx->input_length) {
        return false;
    }
    
    uint8_t c = ctx->input[sp];
    
    if (ctx->case_insensitive) {
        c = chomsky3_to_lower(c);
        start = chomsky3_to_lower(start);
        end = chomsky3_to_lower(end);
    }
    
    return c >= start && c <= end;
}

static inline bool c_match_any(
    const chomsky3_match_context_t *ctx,
    size_t sp
) {
    if (sp >= ctx->input_length) {
        return false;
    }
    
    if (!ctx->dotall && ctx->input[sp] == '\n') {
        return false;
    }
    
    return true;
}

/* Main C interpreter loop */
static int c_execute(c_exec_state_t *state) {
    size_t pc = 0;
    size_t sp = state->ctx->position;
    
    while (pc < state->bytecode->header.num_instructions) {
        const chomsky3_instruction_t *inst = &state->bytecode->instructions[pc];
        
        state->ctx->steps++;
        
        if (!c_check_limits(state->ctx)) {
            return -1;
        }
        
        switch (inst->opcode) {
            case CHOMSKY3_OP_CHAR:
                if (c_match_char(state->ctx, sp, (uint8_t)inst->operand1)) {
                    sp++;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_CHAR_RANGE:
                if (c_match_range(state->ctx, sp, (uint8_t)inst->operand1, (uint8_t)inst->operand2)) {
                    sp++;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_CHAR_CLASS:
                if (sp < state->ctx->input_length &&
                    chomsky3_char_in_class(state->ctx->input[sp], (const uint8_t *)inst->data)) {
                    sp++;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_ANY:
                if (c_match_any(state->ctx, sp)) {
                    sp++;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_ANY_NL:
                if (sp < state->ctx->input_length) {
                    sp++;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_STRING: {
                const char *str = (const char *)inst->data;
                size_t len = inst->operand1;
                
                if (sp + len <= state->ctx->input_length &&
                    memcmp(&state->ctx->input[sp], str, len) == 0) {
                    sp += len;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
            }
            
            case CHOMSKY3_OP_STRING_ICASE: {
                const char *str = (const char *)inst->data;
                size_t len = inst->operand1;
                bool match = true;
                
                if (sp + len > state->ctx->input_length) {
                    goto backtrack;
                }
                
                for (size_t i = 0; i < len; i++) {
                    if (!chomsky3_char_equal_icase(state->ctx->input[sp + i], (uint8_t)str[i])) {
                        match = false;
                        break;
                    }
                }
                
                if (match) {
                    sp += len;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
            }
            
            case CHOMSKY3_OP_JUMP:
                pc = inst->operand1;
                break;
                
            case CHOMSKY3_OP_SPLIT:
                if (c_push_frame(state, inst->operand2, sp) < 0) {
                    return -1;
                }
                pc = inst->operand1;
                break;
                
            case CHOMSKY3_OP_MATCH:
                state->matched = true;
                state->match_end = sp;
                return 0;
                
            case CHOMSKY3_OP_FAIL:
                goto backtrack;
                
            case CHOMSKY3_OP_ANCHOR_START:
                if (sp == 0) {
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_ANCHOR_END:
                if (sp == state->ctx->input_length) {
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_ANCHOR_LINE_START:
                if (sp == 0 || (state->ctx->multiline && state->ctx->input[sp - 1] == '\n')) {
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_ANCHOR_LINE_END:
                if (sp == state->ctx->input_length ||
                    (state->ctx->multiline && state->ctx->input[sp] == '\n')) {
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
                
            case CHOMSKY3_OP_ANCHOR_WORD: {
                bool before = (sp > 0) && chomsky3_is_word_char(state->ctx->input[sp - 1]);
                bool after = (sp < state->ctx->input_length) &&
                            chomsky3_is_word_char(state->ctx->input[sp]);
                
                if (before != after) {
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_NWORD: {
                bool before = (sp > 0) && chomsky3_is_word_char(state->ctx->input[sp - 1]);
                bool after = (sp < state->ctx->input_length) &&
                            chomsky3_is_word_char(state->ctx->input[sp]);
                
                if (before == after) {
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
            }
            
            case CHOMSKY3_OP_SAVE_START: {
                uint32_t group = inst->operand1;
                if (group < state->ctx->capture_capacity) {
                    state->ctx->capture_starts[group] = sp;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_SAVE_END: {
                uint32_t group = inst->operand1;
                if (group < state->ctx->capture_capacity) {
                    state->ctx->capture_ends[group] = sp;
                    if (group >= state->ctx->num_captures) {
                        state->ctx->num_captures = group + 1;
                    }
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_BACKREF: {
                uint32_t group = inst->operand1;
                
                if (group >= state->ctx->num_captures) {
                    goto backtrack;
                }
                
                size_t start = state->ctx->capture_starts[group];
                size_t end = state->ctx->capture_ends[group];
                size_t len = end - start;
                
                if (sp + len <= state->ctx->input_length &&
                    memcmp(&state->ctx->input[sp], &state->ctx->input[start], len) == 0) {
                    sp += len;
                    pc++;
                } else {
                    goto backtrack;
                }
                break;
            }
            
            case CHOMSKY3_OP_NOP:
                pc++;
                break;
                
            default:
                return -1;
        }
        
        continue;
        
backtrack:
        if (c_pop_frame(state, &pc, &sp) < 0) {
            return -1;
        }
    }
    
    return -1;
}

/* Public C interpreter API */

int chomsky3_c_simulate(
    const chomsky3_bytecode_t *bytecode,
    chomsky3_match_context_t *ctx,
    chomsky3_match_result_t *result
) {
    if (!bytecode || !ctx || !result) {
        return -1;
    }
    
    /* Initialize execution state */
    c_exec_state_t state = {
        .bytecode = bytecode,
        .ctx = ctx,
        .stack = NULL,
        .stack_top = 0,
        .stack_capacity = 256,
        .match_start = ctx->position,
        .match_end = 0,
        .matched = false
    };
    
    /* Allocate stack */
    state.stack = malloc(state.stack_capacity * sizeof(c_stack_frame_t));
    if (!state.stack) {
        return -1;
    }
    
    /* Reset statistics */
    ctx->steps = 0;
    ctx->backtracks = 0;
    
    /* Execute */
    int ret = c_execute(&state);
    
    /* Fill result */
    result->matched = state.matched;
    result->start = state.match_start;
    result->end = state.match_end;
    result->capture_starts = ctx->capture_starts;
    result->capture_ends = ctx->capture_ends;
    result->num_captures = ctx->num_captures;
    result->steps = ctx->steps;
    result->backtracks = ctx->backtracks;
    
    free(state.stack);
    return ret;
}

int chomsky3_c_match(
    const chomsky3_bytecode_t *bytecode,
    const uint8_t *input,
    size_t input_length,
    chomsky3_match_result_t *result
) {
    chomsky3_match_context_t *ctx = chomsky3_match_context_new(
        input, input_length, bytecode->header.num_captures
    );
    
    if (!ctx) {
        return -1;
    }
    
    ctx->anchored = true;
    int ret = chomsky3_c_simulate(bytecode, ctx, result);
    
    chomsky3_match_context_free(ctx);
    return ret;
}

int chomsky3_c_search(
    const chomsky3_bytecode_t *bytecode,
    const uint8_t *input,
    size_t input_length,
    chomsky3_match_result_t *result
) {
    chomsky3_match_context_t *ctx = chomsky3_match_context_new(
        input, input_length, bytecode->header.num_captures
    );
    
    if (!ctx) {
        return -1;
    }
    
    /* Try matching at each position */
    for (size_t pos = 0; pos <= input_length; pos++) {
        ctx->position = pos;
        
        int ret = chomsky3_c_simulate(bytecode, ctx, result);
        
        if (ret == 0 && result->matched) {
            chomsky3_match_context_free(ctx);
            return 0;
        }
    }
    
    chomsky3_match_context_free(ctx);
    result->matched = false;
    return -1;
}
