/**
 * libchomsky3 - Bytecode-based Simulation and Matching
 * 
 * Implementation of regex matching using bytecode interpretation.
 * Uses a backtracking VM with resource limits to prevent ReDoS.
 */

#include "simmatch.h"
#include "chomsky3/bytecode.h"
#include <stdlib.h>
#include <string.h>

/* Stack frame for backtracking */
typedef struct {
    size_t pc;                      /* Program counter */
    size_t sp;                      /* String position */
    size_t capture_checkpoint;      /* Capture state checkpoint */
} stack_frame_t;

/* Internal execution state */
typedef struct {
    const chomsky3_bytecode_t *bytecode;
    chomsky3_match_context_t *ctx;
    stack_frame_t *stack;
    size_t stack_top;
    size_t match_start;
    size_t match_end;
    bool matched;
} exec_state_t;

/* Helper: Push stack frame */
static inline int push_frame(exec_state_t *state, size_t pc, size_t sp) {
    if (state->stack_top >= state->ctx->stack_capacity) {
        return -1; /* Stack overflow */
    }
    
    stack_frame_t *frame = &state->stack[state->stack_top++];
    frame->pc = pc;
    frame->sp = sp;
    frame->capture_checkpoint = state->ctx->num_captures;
    
    return 0;
}

/* Helper: Pop stack frame */
static inline int pop_frame(exec_state_t *state, size_t *pc, size_t *sp) {
    if (state->stack_top == 0) {
        return -1; /* Stack underflow */
    }
    
    stack_frame_t *frame = &state->stack[--state->stack_top];
    *pc = frame->pc;
    *sp = frame->sp;
    
    /* Restore capture state */
    state->ctx->num_captures = frame->capture_checkpoint;
    
    state->ctx->backtracks++;
    return 0;
}

/* Helper: Check resource limits */
static inline int check_limits(chomsky3_match_context_t *ctx) {
    if (ctx->max_steps > 0 && ctx->steps >= ctx->max_steps) {
        return -1; /* Step limit exceeded */
    }
    if (ctx->max_backtracks > 0 && ctx->backtracks >= ctx->max_backtracks) {
        return -1; /* Backtrack limit exceeded */
    }
    return 0;
}

/* Execute single instruction */
static int execute_instruction(exec_state_t *state, size_t *pc, size_t *sp) {
    chomsky3_match_context_t *ctx = state->ctx;
    const chomsky3_instruction_t *inst = &state->bytecode->instructions[*pc];
    
    ctx->steps++;
    
    if (check_limits(ctx) < 0) {
        return -1;
    }
    
    switch (inst->opcode) {
        case CHOMSKY3_OP_CHAR: {
            /* Match single character */
            if (*sp >= ctx->input_length) {
                return 0; /* Backtrack */
            }
            
            uint8_t c = ctx->input[*sp];
            uint8_t expected = (uint8_t)inst->operand1;
            
            if (ctx->case_insensitive) {
                if (!chomsky3_char_equal_icase(c, expected)) {
                    return 0;
                }
            } else {
                if (c != expected) {
                    return 0;
                }
            }
            
            (*sp)++;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_CHAR_RANGE: {
            /* Match character range */
            if (*sp >= ctx->input_length) {
                return 0;
            }
            
            uint8_t c = ctx->input[*sp];
            uint8_t start = (uint8_t)inst->operand1;
            uint8_t end = (uint8_t)inst->operand2;
            
            if (ctx->case_insensitive) {
                c = chomsky3_to_lower(c);
                start = chomsky3_to_lower(start);
                end = chomsky3_to_lower(end);
            }
            
            if (c < start || c > end) {
                return 0;
            }
            
            (*sp)++;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_CHAR_CLASS: {
            /* Match character class */
            if (*sp >= ctx->input_length) {
                return 0;
            }
            
            uint8_t c = ctx->input[*sp];
            const uint8_t *bitmap = (const uint8_t *)inst->data;
            
            if (!chomsky3_char_in_class(c, bitmap)) {
                return 0;
            }
            
            (*sp)++;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_ANY: {
            /* Match any character except newline */
            if (*sp >= ctx->input_length) {
                return 0;
            }
            
            uint8_t c = ctx->input[*sp];
            if (!ctx->dotall && c == '\n') {
                return 0;
            }
            
            (*sp)++;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_ANY_NL: {
            /* Match any character including newline */
            if (*sp >= ctx->input_length) {
                return 0;
            }
            
            (*sp)++;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_STRING: {
            /* Match literal string */
            const char *str = (const char *)inst->data;
            size_t len = inst->operand1;
            
            if (*sp + len > ctx->input_length) {
                return 0;
            }
            
            for (size_t i = 0; i < len; i++) {
                if (ctx->input[*sp + i] != (uint8_t)str[i]) {
                    return 0;
                }
            }
            
            *sp += len;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_STRING_ICASE: {
            /* Match string case-insensitive */
            const char *str = (const char *)inst->data;
            size_t len = inst->operand1;
            
            if (*sp + len > ctx->input_length) {
                return 0;
            }
            
            for (size_t i = 0; i < len; i++) {
                if (!chomsky3_char_equal_icase(ctx->input[*sp + i], (uint8_t)str[i])) {
                    return 0;
                }
            }
            
            *sp += len;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_JUMP: {
            /* Unconditional jump */
            *pc = inst->operand1;
            return 1;
        }
        
        case CHOMSKY3_OP_SPLIT: {
            /* Split execution (fork) */
            size_t target1 = inst->operand1;
            size_t target2 = inst->operand2;
            
            /* Push second alternative onto stack */
            if (push_frame(state, target2, *sp) < 0) {
                return -1;
            }
            
            /* Continue with first alternative */
            *pc = target1;
            return 1;
        }
        
        case CHOMSKY3_OP_MATCH: {
            /* Successful match */
            state->matched = true;
            state->match_end = *sp;
            return 2; /* Signal match found */
        }
        
        case CHOMSKY3_OP_FAIL: {
            /* Explicit failure */
            return 0;
        }
        
        case CHOMSKY3_OP_ANCHOR_START: {
            /* Match start of string */
            if (*sp != 0) {
                return 0;
            }
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_ANCHOR_END: {
            /* Match end of string */
            if (*sp != ctx->input_length) {
                return 0;
            }
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_ANCHOR_LINE_START: {
            /* Match start of line */
            if (*sp == 0 || (ctx->multiline && ctx->input[*sp - 1] == '\n')) {
                (*pc)++;
                return 1;
            }
            return 0;
        }
        
        case CHOMSKY3_OP_ANCHOR_LINE_END: {
            /* Match end of line */
            if (*sp == ctx->input_length || (ctx->multiline && ctx->input[*sp] == '\n')) {
                (*pc)++;
                return 1;
            }
            return 0;
        }
        
        case CHOMSKY3_OP_ANCHOR_WORD: {
            /* Word boundary */
            bool before = (*sp > 0) && chomsky3_is_word_char(ctx->input[*sp - 1]);
            bool after = (*sp < ctx->input_length) && chomsky3_is_word_char(ctx->input[*sp]);
            
            if (before != after) {
                (*pc)++;
                return 1;
            }
            return 0;
        }
        
        case CHOMSKY3_OP_ANCHOR_NWORD: {
            /* Non-word boundary */
            bool before = (*sp > 0) && chomsky3_is_word_char(ctx->input[*sp - 1]);
            bool after = (*sp < ctx->input_length) && chomsky3_is_word_char(ctx->input[*sp]);
            
            if (before == after) {
                (*pc)++;
                return 1;
            }
            return 0;
        }
        
        case CHOMSKY3_OP_SAVE_START: {
            /* Save capture group start */
            uint32_t group = inst->operand1;
            if (group < ctx->capture_capacity) {
                ctx->capture_starts[group] = *sp;
            }
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_SAVE_END: {
            /* Save capture group end */
            uint32_t group = inst->operand1;
            if (group < ctx->capture_capacity) {
                ctx->capture_ends[group] = *sp;
                if (group >= ctx->num_captures) {
                    ctx->num_captures = group + 1;
                }
            }
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_BACKREF: {
            /* Backreference */
            uint32_t group = inst->operand1;
            
            if (group >= ctx->num_captures) {
                return 0; /* Invalid backreference */
            }
            
            size_t start = ctx->capture_starts[group];
            size_t end = ctx->capture_ends[group];
            size_t len = end - start;
            
            if (*sp + len > ctx->input_length) {
                return 0;
            }
            
            for (size_t i = 0; i < len; i++) {
                if (ctx->input[*sp + i] != ctx->input[start + i]) {
                    return 0;
                }
            }
            
            *sp += len;
            (*pc)++;
            return 1;
        }
        
        case CHOMSKY3_OP_NOP: {
            /* No operation */
            (*pc)++;
            return 1;
        }
        
        default:
            /* Unknown opcode */
            return -1;
    }
}

/* Main execution loop */
static int execute_bytecode(exec_state_t *state) {
    size_t pc = 0;
    size_t sp = state->ctx->position;
    
    while (pc < state->bytecode->header.num_instructions) {
        int result = execute_instruction(state, &pc, &sp);
        
        if (result == 2) {
            /* Match found */
            return 0;
        } else if (result == 1) {
            /* Continue execution */
            continue;
        } else if (result == 0) {
            /* Backtrack */
            if (pop_frame(state, &pc, &sp) < 0) {
                /* No more alternatives */
                return -1;
            }
        } else {
            /* Error */
            return result;
        }
    }
    
    return -1; /* No match */
}

/* Public API implementation */

int chomsky3_bytecode_simulate(
    const chomsky3_bytecode_t *bytecode,
    chomsky3_match_context_t *ctx,
    chomsky3_match_result_t *result
) {
    if (!bytecode || !ctx || !result) {
        return -1;
    }
    
    /* Allocate stack if needed */
    if (!ctx->stack) {
        ctx->stack_capacity = 1024;
        ctx->stack = malloc(ctx->stack_capacity * sizeof(stack_frame_t));
        if (!ctx->stack) {
            return -1;
        }
    }
    
    /* Initialize execution state */
    exec_state_t state = {
        .bytecode = bytecode,
        .ctx = ctx,
        .stack = (stack_frame_t *)ctx->stack,
        .stack_top = 0,
        .match_start = ctx->position,
        .match_end = 0,
        .matched = false
    };
    
    /* Reset statistics */
    ctx->steps = 0;
    ctx->backtracks = 0;
    
    /* Execute bytecode */
    int ret = execute_bytecode(&state);
    
    /* Fill result */
    result->matched = state.matched;
    result->start = state.match_start;
    result->end = state.match_end;
    result->capture_starts = ctx->capture_starts;
    result->capture_ends = ctx->capture_ends;
    result->num_captures = ctx->num_captures;
    result->steps = ctx->steps;
    result->backtracks = ctx->backtracks;
    
    return ret;
}

int chomsky3_bytecode_match(
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
    int ret = chomsky3_bytecode_simulate(bytecode, ctx, result);
    
    chomsky3_match_context_free(ctx);
    return ret;
}

int chomsky3_bytecode_search(
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
        
        int ret = chomsky3_bytecode_simulate(bytecode, ctx, result);
        
        if (ret == 0 && result->matched) {
            chomsky3_match_context_free(ctx);
            return 0;
        }
    }
    
    chomsky3_match_context_free(ctx);
    result->matched = false;
    return -1;
}
