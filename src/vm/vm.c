/* src/vm/vm.c - Chomsky3 Virtual Machine Implementation */

#include "chomsky3/vm.h"
#include "chomsky3/bytecode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

/* Note: Types are defined in headers */
/* VM execution state */
typedef struct vm_state {
    const chomsky3_bytecode_t *bytecode;
    const char *input;
    size_t input_len;
    size_t input_pos;
    
    /* Execution stack */
    uint64_t *stack;
    size_t stack_size;
    size_t stack_capacity;
    
    /* Call stack for function calls */
    size_t *call_stack;
    size_t call_stack_size;
    size_t call_stack_capacity;
    
    /* Capture groups */
    size_t *captures;
    size_t num_captures;
    
    /* Backtracking state */
    struct backtrack_point {
        size_t pc;
        size_t input_pos;
        size_t stack_size;
    } *backtrack_stack;
    size_t backtrack_size;
    size_t backtrack_capacity;
    
    /* Statistics */
    chomsky3_vm_stats_t stats;
    
    /* Limits */
    size_t max_stack_depth;
    size_t max_backtrack;
    
    /* Flags */
    uint32_t flags;
    int matched;
} vm_state_t;

/* Helper: Initialize VM state */
static int __attribute__((unused)) vm_state_init(vm_state_t *state, const chomsky3_bytecode_t *bytecode,
                         const char *input, size_t input_len, uint32_t flags) {
    memset(state, 0, sizeof(*state));
    
    state->bytecode = bytecode;
    state->input = input;
    state->input_len = input_len;
    state->input_pos = 0;
    state->flags = flags;
    
    /* Initialize stacks */
    state->stack_capacity = 256;
    state->stack = malloc(state->stack_capacity * sizeof(uint64_t));
    if (!state->stack) return -1;
    
    state->call_stack_capacity = 64;
    state->call_stack = malloc(state->call_stack_capacity * sizeof(size_t));
    if (!state->call_stack) {
        free(state->stack);
        return -1;
    }
    
    state->backtrack_capacity = 128;
    state->backtrack_stack = malloc(state->backtrack_capacity * sizeof(struct backtrack_point));
    if (!state->backtrack_stack) {
        free(state->stack);
        free(state->call_stack);
        return -1;
    }
    
    /* Initialize captures (assume max 32 groups) */
    state->num_captures = 32;
    state->captures = calloc(state->num_captures * 2, sizeof(size_t));
    if (!state->captures) {
        free(state->stack);
        free(state->call_stack);
        free(state->backtrack_stack);
        return -1;
    }
    
    /* Set limits */
    state->max_stack_depth = 10000;
    state->max_backtrack = 100000;
    
    return 0;
}

/* Helper: Cleanup VM state */
static void __attribute__((unused)) vm_state_cleanup(vm_state_t *state) {
    free(state->stack);
    free(state->call_stack);
    free(state->backtrack_stack);
    free(state->captures);
}

/* Helper: Push to stack */
static inline int stack_push(vm_state_t *state, uint64_t value) {
    if (state->stack_size >= state->max_stack_depth) {
        return -1;  /* Stack overflow */
    }
    
    if (state->stack_size >= state->stack_capacity) {
        size_t new_capacity = state->stack_capacity * 2;
        uint64_t *new_stack = realloc(state->stack, new_capacity * sizeof(uint64_t));
        if (!new_stack) return -1;
        state->stack = new_stack;
        state->stack_capacity = new_capacity;
    }
    
    state->stack[state->stack_size++] = value;
    return 0;
}

/* Helper: Pop from stack */
static inline uint64_t stack_pop(vm_state_t *state) {
    if (state->stack_size == 0) return 0;
    return state->stack[--state->stack_size];
}

/* Helper: Peek stack */
static inline uint64_t stack_peek(vm_state_t *state, size_t offset) {
    if (offset >= state->stack_size) return 0;
    return state->stack[state->stack_size - 1 - offset];
}

/* Helper: Save backtrack point */
static int save_backtrack(vm_state_t *state, size_t pc, size_t input_pos) {
    if (state->backtrack_size >= state->max_backtrack) {
        return -1;  /* Too much backtracking */
    }
    
    if (state->backtrack_size >= state->backtrack_capacity) {
        size_t new_capacity = state->backtrack_capacity * 2;
        struct backtrack_point *new_stack = realloc(state->backtrack_stack,
                                                     new_capacity * sizeof(struct backtrack_point));
        if (!new_stack) return -1;
        state->backtrack_stack = new_stack;
        state->backtrack_capacity = new_capacity;
    }
    
    state->backtrack_stack[state->backtrack_size].pc = pc;
    state->backtrack_stack[state->backtrack_size].input_pos = input_pos;
    state->backtrack_stack[state->backtrack_size].stack_size = state->stack_size;
    state->backtrack_size++;
    
    return 0;
}

/* Helper: Restore backtrack point */
static int restore_backtrack(vm_state_t *state, size_t *pc, size_t *input_pos) {
    if (state->backtrack_size == 0) return -1;
    
    state->backtrack_size--;
    *pc = state->backtrack_stack[state->backtrack_size].pc;
    *input_pos = state->backtrack_stack[state->backtrack_size].input_pos;
    state->stack_size = state->backtrack_stack[state->backtrack_size].stack_size;
    
    return 0;
}

/* Helper: Check character class */
static int check_char_class(char c, uint32_t class_id) {
    switch (class_id) {
        case 0: return isalnum(c);   /* \w */
        case 1: return isspace(c);   /* \s */
        case 2: return isdigit(c);   /* \d */
        case 3: return isalpha(c);   /* alpha */
        case 4: return islower(c);   /* lower */
        case 5: return isupper(c);   /* upper */
        case 6: return isxdigit(c);  /* xdigit */
        default: return 0;
    }
}

/* Main VM interpreter loop */
static int __attribute__((unused)) vm_execute_internal(vm_state_t *state) {
    size_t pc = 0;
    const chomsky3_instruction_t *instructions = state->bytecode->instructions;
    size_t num_instructions = state->bytecode->header.num_instructions;
    
    while (pc < num_instructions) {
        const chomsky3_instruction_t *inst = &instructions[pc];
        state->stats.steps_executed++;
        
        switch (inst->opcode) {
            case CHOMSKY3_OP_CHAR: {
                /* Match single character */
                if (state->input_pos >= state->input_len ||
                    state->input[state->input_pos] != (char)inst->operand1) {
                    /* Backtrack */
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;  /* No match */
                    }
                    continue;
                }
                state->input_pos++;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_CHAR_RANGE: {
                /* Match character in range [operand1, operand2] */
                if (state->input_pos >= state->input_len) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                char c = state->input[state->input_pos];
                if (c < (char)inst->operand1 || c > (char)inst->operand2) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                state->input_pos++;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_CHAR_CLASS: {
                /* Match character class */
                if (state->input_pos >= state->input_len) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                char c = state->input[state->input_pos];
                if (!check_char_class(c, inst->operand1)) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                state->input_pos++;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANY: {
                /* Match any character except newline */
                if (state->input_pos >= state->input_len ||
                    state->input[state->input_pos] == '\n') {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                state->input_pos++;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_STRING: {
                /* Match string literal */
                const char *str = (const char *)inst->data;
                size_t len = inst->operand1;
                if (state->input_pos + len > state->input_len ||
                    memcmp(state->input + state->input_pos, str, len) != 0) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                state->input_pos += len;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_STRING_ICASE: {
                /* Match string case-insensitive */
                const char *str = (const char *)inst->data;
                size_t len = inst->operand1;
                if (state->input_pos + len > state->input_len) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                int match = 1;
                for (size_t i = 0; i < len; i++) {
                    if (tolower(state->input[state->input_pos + i]) != tolower(str[i])) {
                        match = 0;
                        break;
                    }
                }
                if (!match) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                state->input_pos += len;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_JUMP: {
                /* Unconditional jump */
                pc = inst->operand1;
                break;
            }
            
            case CHOMSKY3_OP_SPLIT: {
                /* Non-deterministic split - save backtrack point */
                if (save_backtrack(state, inst->operand2, state->input_pos) < 0) {
                    return -1;  /* Backtrack limit exceeded */
                }
                pc = inst->operand1;
                break;
            }
            
            case CHOMSKY3_OP_MATCH: {
                /* Successful match */
                state->matched = 1;
                return 1;
            }
            
            case CHOMSKY3_OP_FAIL: {
                /* Explicit failure - backtrack */
                if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                    return 0;
                }
                break;
            }
            
            case CHOMSKY3_OP_SAVE_START: {
                /* Save capture group start */
                size_t group = inst->operand1;
                if (group < state->num_captures) {
                    state->captures[group * 2] = state->input_pos;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_SAVE_END: {
                /* Save capture group end */
                size_t group = inst->operand1;
                if (group < state->num_captures) {
                    state->captures[group * 2 + 1] = state->input_pos;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_BACKREF: {
                /* Match backreference */
                size_t group = inst->operand1;
                if (group >= state->num_captures) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                size_t start = state->captures[group * 2];
                size_t end = state->captures[group * 2 + 1];
                size_t len = end - start;
                
                if (state->input_pos + len > state->input_len ||
                    memcmp(state->input + state->input_pos, state->input + start, len) != 0) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                state->input_pos += len;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_START: {
                /* Match start of string */
                if (state->input_pos != 0) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_END: {
                /* Match end of string */
                if (state->input_pos != state->input_len) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_LINE_START: {
                /* Match start of line */
                if (state->input_pos != 0 && state->input[state->input_pos - 1] != '\n') {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_LINE_END: {
                /* Match end of line */
                if (state->input_pos < state->input_len && state->input[state->input_pos] != '\n') {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_WORD: {
                /* Match word boundary */
                int before_word = (state->input_pos > 0 && isalnum(state->input[state->input_pos - 1]));
                int after_word = (state->input_pos < state->input_len && isalnum(state->input[state->input_pos]));
                if (before_word == after_word) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_ANCHOR_NWORD: {
                /* Match non-word boundary */
                int before_word = (state->input_pos > 0 && isalnum(state->input[state->input_pos - 1]));
                int after_word = (state->input_pos < state->input_len && isalnum(state->input[state->input_pos]));
                if (before_word != after_word) {
                    if (restore_backtrack(state, &pc, &state->input_pos) < 0) {
                        return 0;
                    }
                    continue;
                }
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_LOOK_AHEAD: {
                /* Positive lookahead */
                size_t saved_pos = state->input_pos;
                size_t target_pc = inst->operand1;
                /* Execute lookahead sub-pattern */
                /* For simplicity, just jump and restore position */
                pc = target_pc;
                /* TODO: Proper lookahead implementation with sub-execution */
                state->input_pos = saved_pos;
                break;
            }
            
            case CHOMSKY3_OP_LOOK_AHEAD_NEG: {
                /* Negative lookahead */
                size_t saved_pos = state->input_pos;
                /* TODO: Execute and invert result */
                state->input_pos = saved_pos;
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_REPEAT: {
                /* Greedy repeat {min, max} */
                uint32_t min = inst->operand1;
                uint32_t max = inst->operand2;
                uint32_t count = 0;
                (void)inst->operand3;  /* body_pc - TODO: use in implementation */
                
                /* Match minimum required */
                for (count = 0; count < min; count++) {
                    /* TODO: Execute body pattern */
                }
                
                /* Match up to maximum greedily */
                for (; count < max; count++) {
                    (void)state->input_pos;  /* saved_pos - TODO: use for backtracking */
                    /* Try to match body */
                    /* If fails, restore and break */
                    /* TODO: Proper repeat implementation */
                }
                
                pc++;
                break;
            }
            
            case CHOMSKY3_OP_NOP:
                pc++;
                break;
            
            default:
                /* Unknown opcode */
                return -1;
        }
    }
    
    return 1;
}
