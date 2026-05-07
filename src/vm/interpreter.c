// src/vm/interpreter.c - Bytecode interpreter for libchomsky3
// Production regex VM with backtracking, captures, and resource limits

#include "chomsky3/vm.h"
#include "chomsky3/bytecode.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// Backtracking stack frame
typedef struct {
    uint32_t pc;              // Program counter
    const char *input_pos;    // Input position
    uint32_t capture_count;   // Number of active captures
    uint32_t repeat_count;    // Repeat counter for quantifiers
} vm_stack_frame_t;

// Capture slot
typedef struct {
    const char *start;
    const char *end;
} vm_capture_t;

// Internal VM execution state
typedef struct {
    const chomsky3_bytecode_t *bytecode;
    chomsky3_vm_state_t *state;
    const chomsky3_vm_config_t *config;
    
    // Backtracking stack
    vm_stack_frame_t *stack;
    uint32_t stack_size;
    uint32_t stack_capacity;
    
    // Capture groups
    vm_capture_t *captures;
    uint32_t capture_capacity;
    
    // Resource tracking
    uint64_t step_count;
    uint64_t backtrack_count;
    
    // Execution state
    uint32_t pc;
    const char *input_pos;
    const char *input_end;
    bool matched;
} vm_exec_context_t;

// Forward declarations
static bool vm_exec_instruction(vm_exec_context_t *ctx);
static bool vm_push_frame(vm_exec_context_t *ctx, uint32_t pc, const char *pos);
static bool vm_pop_frame(vm_exec_context_t *ctx);
static bool vm_check_limits(vm_exec_context_t *ctx);

// Initialize execution context
static bool vm_init_context(vm_exec_context_t *ctx,
                            const chomsky3_bytecode_t *bytecode,
                            chomsky3_vm_state_t *state,
                            const chomsky3_vm_config_t *config) {
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->bytecode = bytecode;
    ctx->state = state;
    ctx->config = config;
    
    // Allocate backtracking stack
    ctx->stack_capacity = config->limits.max_stack_depth > 0 
                         ? config->limits.max_stack_depth 
                         : 1024;
    ctx->stack = calloc(ctx->stack_capacity, sizeof(vm_stack_frame_t));
    if (!ctx->stack) return false;
    
    // Allocate capture slots
    ctx->capture_capacity = bytecode->header.capture_count * 2; // start + end
    if (ctx->capture_capacity > 0) {
        ctx->captures = calloc(ctx->capture_capacity, sizeof(vm_capture_t));
        if (!ctx->captures) {
            free(ctx->stack);
            return false;
        }
    }
    
    // Initialize execution state
    ctx->pc = 0;
    ctx->input_pos = state->input;
    ctx->input_end = state->input + state->input_length;
    ctx->matched = false;
    
    return true;
}

// Cleanup execution context
static void vm_cleanup_context(vm_exec_context_t *ctx) {
    free(ctx->stack);
    free(ctx->captures);
}

// Main interpreter loop
int chomsky3_vm_execute(const chomsky3_bytecode_t *bytecode,
                        chomsky3_vm_state_t *state,
                        const chomsky3_vm_config_t *config) {
    if (!bytecode || !state || !config) {
        return CHOMSKY3_ERROR_INVALID_ARGUMENT;
    }
    
    // Validate bytecode
    if (bytecode->header.magic != CHOMSKY3_BYTECODE_MAGIC) {
        return CHOMSKY3_ERROR_INVALID_BYTECODE;
    }
    
    vm_exec_context_t ctx;
    if (!vm_init_context(&ctx, bytecode, state, config)) {
        return CHOMSKY3_ERROR_OUT_OF_MEMORY;
    }
    
    int result = CHOMSKY3_ERROR_NO_MATCH;
    
    // Main execution loop
    while (ctx.pc < bytecode->header.instruction_count) {
        // Check resource limits
        if (!vm_check_limits(&ctx)) {
            result = CHOMSKY3_ERROR_RESOURCE_LIMIT;
            break;
        }
        
        // Execute instruction
        if (!vm_exec_instruction(&ctx)) {
            // Backtrack
            if (ctx.stack_size == 0) {
                // No more backtrack points
                result = CHOMSKY3_ERROR_NO_MATCH;
                break;
            }
            
            if (!vm_pop_frame(&ctx)) {
                result = CHOMSKY3_ERROR_INTERNAL;
                break;
            }
            ctx.backtrack_count++;
            continue;
        }
        
        // Check for match
        if (ctx.matched) {
            result = CHOMSKY3_SUCCESS;
            
            // Copy captures to state
            state->capture_count = bytecode->header.capture_count;
            for (uint32_t i = 0; i < state->capture_count && i < ctx.capture_capacity / 2; i++) {
                state->captures[i].start = ctx.captures[i * 2].start;
                state->captures[i].end = ctx.captures[i * 2 + 1].end;
            }
            
            state->match_start = state->input;
            state->match_end = ctx.input_pos;
            break;
        }
        
        ctx.step_count++;
    }
    
    // Update statistics
    state->steps_executed = ctx.step_count;
    state->backtracks = ctx.backtrack_count;
    
    vm_cleanup_context(&ctx);
    return result;
}

// Execute single instruction
static bool vm_exec_instruction(vm_exec_context_t *ctx) {
    const chomsky3_instruction_t *inst = &ctx->bytecode->instructions[ctx->pc];
    const char *input = ctx->input_pos;
    const char *input_end = ctx->input_end;
    
    switch (inst->opcode) {
        // Character matching
        case CHOMSKY3_OP_CHAR:
            if (input >= input_end || *input != (char)inst->operand1) {
                return false;
            }
            ctx->input_pos++;
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_CHAR_RANGE:
            if (input >= input_end || 
                *input < (char)inst->operand1 || 
                *input > (char)inst->operand2) {
                return false;
            }
            ctx->input_pos++;
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_ANY_CHAR:
            if (input >= input_end) {
                return false;
            }
            if (inst->operand1 && *input == '\n') {
                return false; // DOTALL not set
            }
            ctx->input_pos++;
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_CHAR_CLASS: {
            if (input >= input_end) return false;
            
            // Character class data in bytecode data section
            const uint8_t *class_data = (const uint8_t *)ctx->bytecode->data + inst->operand1;
            uint32_t class_size = inst->operand2;
            
            bool matched = false;
            for (uint32_t i = 0; i < class_size; i++) {
                if (*input == class_data[i]) {
                    matched = true;
                    break;
                }
            }
            
            if (!matched) return false;
            ctx->input_pos++;
            ctx->pc++;
            return true;
        }
            
        // String matching
        case CHOMSKY3_OP_STRING: {
            const char *str = (const char *)ctx->bytecode->data + inst->operand1;
            uint32_t len = inst->operand2;
            
            if (input + len > input_end) return false;
            if (memcmp(input, str, len) != 0) return false;
            
            ctx->input_pos += len;
            ctx->pc++;
            return true;
        }
            
        case CHOMSKY3_OP_STRING_ICASE: {
            const char *str = (const char *)ctx->bytecode->data + inst->operand1;
            uint32_t len = inst->operand2;
            
            if (input + len > input_end) return false;
            
            for (uint32_t i = 0; i < len; i++) {
                char c1 = input[i];
                char c2 = str[i];
                if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if (c1 != c2) return false;
            }
            
            ctx->input_pos += len;
            ctx->pc++;
            return true;
        }
            
        // Control flow
        case CHOMSKY3_OP_JUMP:
            ctx->pc = inst->operand1;
            return true;
            
        case CHOMSKY3_OP_SPLIT:
            // Push second branch to stack
            if (!vm_push_frame(ctx, inst->operand2, ctx->input_pos)) {
                return false;
            }
            // Take first branch
            ctx->pc = inst->operand1;
            return true;
            
        case CHOMSKY3_OP_MATCH:
            ctx->matched = true;
            return true;
            
        case CHOMSKY3_OP_FAIL:
            return false;
            
        // Anchors
        case CHOMSKY3_OP_ANCHOR_START:
            if (ctx->input_pos != ctx->state->input) {
                return false;
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_ANCHOR_END:
            if (ctx->input_pos != ctx->input_end) {
                return false;
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_ANCHOR_LINE_START:
            if (ctx->input_pos != ctx->state->input && 
                ctx->input_pos[-1] != '\n') {
                return false;
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_ANCHOR_LINE_END:
            if (ctx->input_pos != ctx->input_end && 
                *ctx->input_pos != '\n') {
                return false;
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_ANCHOR_WORD_BOUNDARY: {
            bool before_word = (ctx->input_pos > ctx->state->input) &&
                              ((ctx->input_pos[-1] >= 'a' && ctx->input_pos[-1] <= 'z') ||
                               (ctx->input_pos[-1] >= 'A' && ctx->input_pos[-1] <= 'Z') ||
                               (ctx->input_pos[-1] >= '0' && ctx->input_pos[-1] <= '9') ||
                               ctx->input_pos[-1] == '_');
            bool after_word = (ctx->input_pos < ctx->input_end) &&
                             ((*ctx->input_pos >= 'a' && *ctx->input_pos <= 'z') ||
                              (*ctx->input_pos >= 'A' && *ctx->input_pos <= 'Z') ||
                              (*ctx->input_pos >= '0' && *ctx->input_pos <= '9') ||
                              *ctx->input_pos == '_');
            
            if (before_word == after_word) {
                return false; // Not a boundary
            }
            ctx->pc++;
            return true;
        }
            
        // Capture groups
        case CHOMSKY3_OP_SAVE_START:
            if (inst->operand1 * 2 < ctx->capture_capacity) {
                ctx->captures[inst->operand1 * 2].start = ctx->input_pos;
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_SAVE_END:
            if (inst->operand1 * 2 + 1 < ctx->capture_capacity) {
                ctx->captures[inst->operand1 * 2 + 1].end = ctx->input_pos;
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_BACKREF: {
            uint32_t group = inst->operand1;
            if (group * 2 + 1 >= ctx->capture_capacity) {
                return false;
            }
            
            const char *ref_start = ctx->captures[group * 2].start;
            const char *ref_end = ctx->captures[group * 2 + 1].end;
            
            if (!ref_start || !ref_end) {
                return false; // Group not captured yet
            }
            
            size_t ref_len = ref_end - ref_start;
            if (ctx->input_pos + ref_len > ctx->input_end) {
                return false;
            }
            
            if (memcmp(ctx->input_pos, ref_start, ref_len) != 0) {
                return false;
            }
            
            ctx->input_pos += ref_len;
            ctx->pc++;
            return true;
        }
            
        // Lookahead/lookbehind (simplified)
        case CHOMSKY3_OP_LOOK_AHEAD:
        case CHOMSKY3_OP_LOOK_AHEAD_NEG:
        case CHOMSKY3_OP_LOOK_BEHIND:
        case CHOMSKY3_OP_LOOK_BEHIND_NEG:
            // TODO: Implement lookaround assertions
            ctx->pc++;
            return true;
            
        // Quantifiers
        case CHOMSKY3_OP_REPEAT:
        case CHOMSKY3_OP_REPEAT_LAZY:
        case CHOMSKY3_OP_REPEAT_NG:
            // TODO: Implement repeat with min/max bounds
            ctx->pc++;
            return true;
            
        // Special
        case CHOMSKY3_OP_NOP:
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_DEBUG:
            if (ctx->config->mode == CHOMSKY3_VM_MODE_DEBUG) {
                // Trigger breakpoint or trace
            }
            ctx->pc++;
            return true;
            
        case CHOMSKY3_OP_CHECKPOINT:
            // Save checkpoint for debugging
            ctx->pc++;
            return true;
            
        default:
            return false; // Unknown opcode
    }
}

// Push backtracking frame
static bool vm_push_frame(vm_exec_context_t *ctx, uint32_t pc, const char *pos) {
    if (ctx->stack_size >= ctx->stack_capacity) {
        return false; // Stack overflow
    }
    
    vm_stack_frame_t *frame = &ctx->stack[ctx->stack_size++];
    frame->pc = pc;
    frame->input_pos = pos;
    frame->capture_count = ctx->capture_capacity;
    frame->repeat_count = 0;
    
    return true;
}

// Pop backtracking frame
static bool vm_pop_frame(vm_exec_context_t *ctx) {
    if (ctx->stack_size == 0) {
        return false;
    }
    
    vm_stack_frame_t *frame = &ctx->stack[--ctx->stack_size];
    ctx->pc = frame->pc;
    ctx->input_pos = frame->input_pos;
    
    // Restore captures (simplified - should save/restore full state)
    
    return true;
}

// Check resource limits
static bool vm_check_limits(vm_exec_context_t *ctx) {
    const chomsky3_vm_limits_t *limits = &ctx->config->limits;
    
    if (limits->max_steps > 0 && ctx->step_count >= limits->max_steps) {
        return false;
    }
    
    if (limits->max_backtracks > 0 && ctx->backtrack_count >= limits->max_backtracks) {
        return false;
    }
    
    if (limits->max_stack_depth > 0 && ctx->stack_size >= limits->max_stack_depth) {
        return false;
    }
    
    return true;
}

// Convenience function: match against input
int chomsky3_vm_match(const chomsky3_bytecode_t *bytecode,
                      const char *input,
                      size_t input_length,
                      chomsky3_vm_state_t *state,
                      const chomsky3_vm_config_t *config) {
    if (!state) {
        return CHOMSKY3_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize state
    memset(state, 0, sizeof(*state));
    state->input = input;
    state->input_length = input_length;
    state->position = 0;
    
    return chomsky3_vm_execute(bytecode, state, config);
}
