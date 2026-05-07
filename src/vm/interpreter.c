// src/vm/interpreter.c - Backtracking interpreter for libchomsky3 bytecode

#include "chomsky3/bytecode.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHOMSKY3_BYTECODE_MAGIC 0x43484F4D

#ifndef CHOMSKY3_SUCCESS
#define CHOMSKY3_SUCCESS CHOMSKY3_OK
#endif

#ifndef CHOMSKY3_ERROR_NO_MATCH
#define CHOMSKY3_ERROR_NO_MATCH CHOMSKY3_ERR_NO_MATCH
#endif

#ifndef CHOMSKY3_ERROR_INVALID_BYTECODE
#define CHOMSKY3_ERROR_INVALID_BYTECODE CHOMSKY3_ERR_INVALID_BYTECODE
#endif

#ifndef CHOMSKY3_ERROR_RESOURCE_LIMIT
#define CHOMSKY3_ERROR_RESOURCE_LIMIT CHOMSKY3_ERROR_LIMIT_COMPLEXITY
#endif

#ifndef CHOMSKY3_OP_ANY_CHAR
#define CHOMSKY3_OP_ANY_CHAR CHOMSKY3_OP_ANY
#endif

#ifndef CHOMSKY3_OP_ANCHOR_WORD_BOUNDARY
#define CHOMSKY3_OP_ANCHOR_WORD_BOUNDARY CHOMSKY3_OP_ANCHOR_WORD
#endif

typedef struct {
    chomsky3_vm_stats_t stats;
} vm_runtime_state_t;

/* Compatibility runtime capture representation (temporary/interpreter-only). */
typedef struct {
    const char *start;
    const char *end;
} vm_capture_t;

/* Backtracking stack frame. */
typedef struct {
    uint32_t pc;
    const char *input_pos;
    uint32_t capture_count;
    uint32_t repeat_count;
} vm_stack_frame_t;

/* Internal execution context. */
typedef struct {
    const chomsky3_bytecode_t *bytecode;
    const char *input_start;
    const char *input_pos;
    const char *input_end;
    chomsky3_vm_limits_t limits;
    chomsky3_vm_mode_t mode;

    vm_stack_frame_t *stack;
    uint32_t stack_size;
    uint32_t stack_capacity;

    vm_capture_t *captures;
    uint32_t capture_capacity;

    uint64_t step_count;
    uint64_t backtrack_count;

    uint32_t pc;
    bool matched;
} vm_exec_context_t;

static bool vm_exec_instruction(vm_exec_context_t *ctx);
static bool vm_push_frame(vm_exec_context_t *ctx, uint32_t pc, const char *pos);
static bool vm_pop_frame(vm_exec_context_t *ctx);
static bool vm_check_limits(const vm_exec_context_t *ctx);

static bool vm_init_context(vm_exec_context_t *ctx,
                           const chomsky3_bytecode_t *bytecode,
                           const char *input,
                           size_t input_length,
                           const chomsky3_vm_limits_t *limits,
                           chomsky3_vm_mode_t mode)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->bytecode = bytecode;
    ctx->input_start = input ? input : "";
    ctx->input_pos = ctx->input_start;
    ctx->input_end = ctx->input_start + (input_length > 0 ? input_length : 0);
    ctx->mode = mode;
    ctx->limits = limits ? *limits : (chomsky3_vm_limits_t){0, 0, 0, 0};

    ctx->stack_capacity = (ctx->limits.max_stack_depth > 0) ? (uint32_t)ctx->limits.max_stack_depth : 1024;
    ctx->stack = calloc(ctx->stack_capacity, sizeof(vm_stack_frame_t));
    if (!ctx->stack) {
        return false;
    }

    ctx->capture_capacity = (uint32_t)(bytecode->header.num_captures * 2u);
    if (ctx->capture_capacity > 0) {
        ctx->captures = calloc(ctx->capture_capacity, sizeof(vm_capture_t));
        if (!ctx->captures) {
            free(ctx->stack);
            return false;
        }
    }

    ctx->pc = 0;
    ctx->matched = false;
    return true;
}

static void vm_cleanup_context(vm_exec_context_t *ctx)
{
    free(ctx->stack);
    free(ctx->captures);
}

static bool vm_match_char_class(const char *input_pos, const char *input_end, const uint8_t *class_data, uint32_t class_size)
{
    if (input_pos >= input_end) {
        return false;
    }

    for (uint32_t i = 0; i < class_size; i++) {
        if (*input_pos == (char)class_data[i]) {
            return true;
        }
    }

    return false;
}

static chomsky3_error_t vm_execute_internal(const chomsky3_bytecode_t *bytecode,
                                          const char *input,
                                          size_t input_length,
                                          const chomsky3_vm_limits_t *limits,
                                          chomsky3_vm_mode_t mode,
                                          chomsky3_match_t **match)
{
    chomsky3_error_t result = CHOMSKY3_ERROR_NO_MATCH;
    vm_exec_context_t ctx;

    if (!vm_init_context(&ctx, bytecode, input, input_length, limits, mode)) {
        return CHOMSKY3_ERROR_OUT_OF_MEMORY;
    }

    while (ctx.pc < ctx.bytecode->header.num_instructions) {
        if (!vm_check_limits(&ctx)) {
            result = CHOMSKY3_ERROR_RESOURCE_LIMIT;
            break;
        }

        if (!vm_exec_instruction(&ctx)) {
            if (ctx.stack_size == 0) {
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

        if (ctx.matched) {
            result = CHOMSKY3_SUCCESS;
            if (match) {
                chomsky3_match_t *m = calloc(1, sizeof(chomsky3_match_t));
                if (!m) {
                    result = CHOMSKY3_ERROR_OUT_OF_MEMORY;
                    break;
                }

                m->start = ctx.input_start;
                m->end = ctx.input_pos;
                m->length = (size_t)(ctx.input_pos - ctx.input_start);
                m->num_groups = ctx.bytecode->header.num_captures;

                if (m->num_groups > 0) {
                    m->groups = calloc(m->num_groups, sizeof(*m->groups));
                    if (!m->groups) {
                        free(m);
                        result = CHOMSKY3_ERROR_OUT_OF_MEMORY;
                        break;
                    }

                    for (size_t i = 0; i < m->num_groups && (i * 2u) < ctx.capture_capacity; i++) {
                        m->groups[i].start = ctx.captures[i * 2u].start;
                        m->groups[i].end = ctx.captures[i * 2u + 1u].end;
                        m->groups[i].length =
                            (m->groups[i].start && m->groups[i].end)
                                ? (size_t)(m->groups[i].end - m->groups[i].start)
                                : 0;
                    }
                }

                *match = m;
            }

            break;
        }

        ctx.step_count++;
    }

    if (!vm_check_limits(&ctx)) {
        result = CHOMSKY3_ERROR_RESOURCE_LIMIT;
    }

    vm_cleanup_context(&ctx);
    return result;
}

static bool vm_exec_instruction(vm_exec_context_t *ctx)
{
    const chomsky3_instruction_t *inst = &ctx->bytecode->instructions[ctx->pc];
    const char *input = ctx->input_pos;
    const char *input_end = ctx->input_end;

    switch (inst->opcode) {
        case CHOMSKY3_OP_CHAR:
            if (input >= input_end || *input != (char)inst->operand1) {
                return false;
            }
            ctx->input_pos++;
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_CHAR_RANGE:
            if (input >= input_end || *input < (char)inst->operand1 || *input > (char)inst->operand2) {
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
                return false;
            }
            ctx->input_pos++;
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_CHAR_CLASS: {
            const uint8_t *class_data = (const uint8_t *)ctx->bytecode->data + inst->operand1;
            if (!vm_match_char_class(input, input_end, class_data, inst->operand2)) {
                return false;
            }
            ctx->input_pos++;
            ctx->pc++;
            return true;
        }

        case CHOMSKY3_OP_STRING: {
            const char *str = (const char *)ctx->bytecode->data + inst->operand1;
            uint32_t len = inst->operand2;
            if (input + len > input_end || memcmp(input, str, len) != 0) {
                return false;
            }

            ctx->input_pos += len;
            ctx->pc++;
            return true;
        }

        case CHOMSKY3_OP_STRING_ICASE: {
            const char *str = (const char *)ctx->bytecode->data + inst->operand1;
            uint32_t len = inst->operand2;
            if (input + len > input_end) {
                return false;
            }
            for (uint32_t i = 0; i < len; i++) {
                char c1 = input[i];
                char c2 = str[i];
                if (c1 >= 'A' && c1 <= 'Z') {
                    c1 += 32;
                }
                if (c2 >= 'A' && c2 <= 'Z') {
                    c2 += 32;
                }
                if (c1 != c2) {
                    return false;
                }
            }
            ctx->input_pos += len;
            ctx->pc++;
            return true;
        }

        case CHOMSKY3_OP_JUMP:
            ctx->pc = inst->operand1;
            return true;

        case CHOMSKY3_OP_SPLIT:
            if (!vm_push_frame(ctx, inst->operand2, ctx->input_pos)) {
                return false;
            }
            ctx->pc = inst->operand1;
            return true;

        case CHOMSKY3_OP_MATCH:
            ctx->matched = true;
            return true;

        case CHOMSKY3_OP_FAIL:
            return false;

        case CHOMSKY3_OP_ANCHOR_START:
            if (ctx->input_pos != ctx->input_start) {
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
            if (ctx->input_pos != ctx->input_start && ctx->input_pos[-1] != '\n') {
                return false;
            }
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_ANCHOR_LINE_END:
            if (ctx->input_pos != ctx->input_end && *ctx->input_pos != '\n') {
                return false;
            }
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_ANCHOR_WORD_BOUNDARY: {
            bool before_word = (ctx->input_pos > ctx->input_start) &&
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
                return false;
            }
            ctx->pc++;
            return true;
        }

        case CHOMSKY3_OP_SAVE_START:
            if (inst->operand1 * 2u < ctx->capture_capacity) {
                ctx->captures[inst->operand1 * 2u].start = ctx->input_pos;
            }
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_SAVE_END:
            if (inst->operand1 * 2u + 1u < ctx->capture_capacity) {
                ctx->captures[inst->operand1 * 2u + 1u].end = ctx->input_pos;
            }
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_BACKREF: {
            uint32_t group = inst->operand1;
            if (group * 2u + 1u >= ctx->capture_capacity) {
                return false;
            }

            const char *ref_start = ctx->captures[group * 2u].start;
            const char *ref_end = ctx->captures[group * 2u + 1u].end;
            if (!ref_start || !ref_end) {
                return false;
            }

            size_t ref_len = (size_t)(ref_end - ref_start);
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

        case CHOMSKY3_OP_LOOK_AHEAD:
        case CHOMSKY3_OP_LOOK_AHEAD_NEG:
        case CHOMSKY3_OP_LOOK_BEHIND:
        case CHOMSKY3_OP_LOOK_BEHIND_NEG:
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_REPEAT:
        case CHOMSKY3_OP_REPEAT_LAZY:
        case CHOMSKY3_OP_REPEAT_NG:
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_NOP:
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_DEBUG:
            ctx->pc++;
            return true;

        case CHOMSKY3_OP_CHECKPOINT:
            ctx->pc++;
            return true;

        default:
            return false;
    }
}

static bool vm_push_frame(vm_exec_context_t *ctx, uint32_t pc, const char *pos)
{
    if (ctx->stack_size >= ctx->stack_capacity) {
        return false;
    }

    vm_stack_frame_t *frame = &ctx->stack[ctx->stack_size++];
    frame->pc = pc;
    frame->input_pos = pos;
    frame->capture_count = ctx->capture_capacity;
    frame->repeat_count = 0;
    return true;
}

static bool vm_pop_frame(vm_exec_context_t *ctx)
{
    if (ctx->stack_size == 0) {
        return false;
    }

    vm_stack_frame_t *frame = &ctx->stack[--ctx->stack_size];
    ctx->pc = frame->pc;
    ctx->input_pos = frame->input_pos;
    return true;
}

static bool vm_check_limits(const vm_exec_context_t *ctx)
{
    const chomsky3_vm_limits_t *limits = &ctx->limits;

    if (limits->max_steps > 0 && ctx->step_count >= limits->max_steps) {
        return false;
    }

    if (limits->max_backtrack > 0 && ctx->backtrack_count >= limits->max_backtrack) {
        return false;
    }

    if (limits->max_stack_depth > 0 && ctx->stack_size >= limits->max_stack_depth) {
        return false;
    }

    return true;
}

chomsky3_vm_t *chomsky3_vm_new(chomsky3_context_t *ctx,
                               const chomsky3_bytecode_t *bytecode)
{
    (void)ctx;
    chomsky3_vm_t *vm = malloc(sizeof(chomsky3_vm_t));
    if (!vm) {
        return NULL;
    }

    vm->bytecode = bytecode;
    vm->mode = CHOMSKY3_VM_MODE_MATCH;
    vm->limits = (chomsky3_vm_limits_t){0, 0, 0, 0};
    
    vm->internal = calloc(1, sizeof(vm_runtime_state_t));
    if (!vm->internal) {
        free(vm);
        return NULL;
    }
    
    return vm;
}

void chomsky3_vm_free(chomsky3_vm_t *vm)
{
    if (vm) {
        free(vm->internal);
    }
    free(vm);
}

void chomsky3_vm_set_mode(chomsky3_vm_t *vm, chomsky3_vm_mode_t mode)
{
    if (!vm) {
        return;
    }
    vm->mode = mode;
}

void chomsky3_vm_set_limits(chomsky3_vm_t *vm, const chomsky3_vm_limits_t *limits)
{
    if (!vm) {
        return;
    }
    if (!limits) {
        vm->limits = (chomsky3_vm_limits_t){0, 0, 0, 0};
        return;
    }
    vm->limits = *limits;
}

void chomsky3_vm_limits_default(chomsky3_vm_limits_t *limits)
{
    if (!limits) {
        return;
    }
    *limits = (chomsky3_vm_limits_t){0, 0, 0, 0};
}

chomsky3_error_t chomsky3_vm_execute(chomsky3_vm_t *vm,
                                     const char *input,
                                     size_t length,
                                     chomsky3_match_t **match)
{
    if (!vm || !input) {
        return CHOMSKY3_ERROR_INVALID_ARGUMENT;
    }

    if (!vm->bytecode || !vm->bytecode->header.magic || !vm->bytecode->instructions) {
        return CHOMSKY3_ERROR_INVALID_BYTECODE;
    }

    if (vm->bytecode->header.magic != CHOMSKY3_BYTECODE_MAGIC) {
        return CHOMSKY3_ERROR_INVALID_BYTECODE;
    }

    return vm_execute_internal(vm->bytecode, input, length, &vm->limits, vm->mode, match);
}

chomsky3_error_t chomsky3_vm_execute_stateful(chomsky3_vm_t *vm,
                                              const char *input,
                                              size_t length,
                                              chomsky3_vm_state_t *state,
                                              chomsky3_match_t **match)
{
    chomsky3_error_t rc = chomsky3_vm_execute(vm, input, length, match);
    if (state) {
        memset(state, 0, sizeof(*state));
        state->pc = 0;
        state->sp = 0;
        memset(&state->stats, 0, sizeof(state->stats));
    }
    return rc;
}

void chomsky3_vm_reset(chomsky3_vm_t *vm)
{
    if (!vm) {
        return;
    }
    if (vm->internal) {
        memset(vm->internal, 0, sizeof(vm_runtime_state_t));
    }
}

void chomsky3_vm_get_stats(const chomsky3_vm_t *vm, chomsky3_vm_stats_t *stats)
{
    if (!vm || !stats) {
        return;
    }

    if (vm->internal) {
        vm_runtime_state_t *state = (vm_runtime_state_t *)vm->internal;
        *stats = state->stats;
    } else {
        memset(stats, 0, sizeof(*stats));
    }
}

chomsky3_vm_state_t *chomsky3_vm_state_new(void)
{
    return calloc(1, sizeof(chomsky3_vm_state_t));
}

void chomsky3_vm_state_free(chomsky3_vm_state_t *state)
{
    free(state);
}

chomsky3_vm_state_t *chomsky3_vm_state_clone(const chomsky3_vm_state_t *state)
{
    if (!state) {
        return NULL;
    }

    chomsky3_vm_state_t *copy = calloc(1, sizeof(chomsky3_vm_state_t));
    if (!copy) {
        return NULL;
    }

    *copy = *state;
    return copy;
}
