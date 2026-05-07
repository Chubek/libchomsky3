/**
 * @file jit_sljit.c
 * @brief SLJIT backend for chomsky3 JIT compiler
 * 
 * This module implements native code generation using the Stack-Less JIT
 * compiler library (SLJIT). It translates chomsky3 bytecode instructions
 * into native machine code for the target architecture.
 */

#include "chomsky3/jit.h"
#include "chomsky3/util/memory.h"
#include "chomsky3/error.h"
#include <sljit/sljitLir.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * SLJIT Backend Configuration
 * ======================================================================== */

/**
 * Register allocation strategy:
 * - SLJIT_R0: Accumulator / primary working register
 * - SLJIT_R1: Secondary operand register
 * - SLJIT_R2: Temporary for complex operations
 * - SLJIT_S0: VM stack pointer (preserved across calls)
 * - SLJIT_S1: VM instruction pointer (preserved)
 * - SLJIT_S2: VM context pointer (preserved)
 */
#define REG_ACC     SLJIT_R0  /* Accumulator (scratch) */
#define REG_TMP1    SLJIT_R1  /* Temporary 1 (scratch) */
#define REG_TMP2    SLJIT_R2  /* Temporary 2 (scratch) */
#define REG_STACK   SLJIT_S0  /* VM stack pointer (saved) */
#define REG_IP      SLJIT_S1  /* Instruction pointer (saved) */
#define REG_CTX     SLJIT_S2  /* VM context (saved) */

/* Stack frame layout (in words, grows downward):
 * [SP + 0]  : Local variable 0
 * [SP + 8]  : Local variable 1
 * [SP + 16] : Local variable 2
 * ...
 */
#define STACK_WORD_SIZE 8
#define MAX_LOCALS 256

/* ========================================================================
 * Bytecode Instruction Set (Standard VM Operations)
 * ======================================================================== */

/**
 * Standard bytecode opcodes for VM operations.
 * These should be mapped to your actual chomsky3_bytecode_t structure.
 */
typedef enum {
    /* Arithmetic operations */
    OP_ADD = 0x01,      /* add r1, r2 -> r1 */
    OP_SUB = 0x02,      /* sub r1, r2 -> r1 */
    OP_MUL = 0x03,      /* mul r1, r2 -> r1 */
    OP_DIV = 0x04,      /* div r1, r2 -> r1 */
    OP_MOD = 0x05,      /* mod r1, r2 -> r1 */
    OP_NEG = 0x06,      /* neg r1 -> r1 */
    
    /* Bitwise operations */
    OP_AND = 0x10,      /* and r1, r2 -> r1 */
    OP_OR  = 0x11,      /* or r1, r2 -> r1 */
    OP_XOR = 0x12,      /* xor r1, r2 -> r1 */
    OP_NOT = 0x13,      /* not r1 -> r1 */
    OP_SHL = 0x14,      /* shl r1, r2 -> r1 */
    OP_SHR = 0x15,      /* shr r1, r2 -> r1 */
    
    /* Comparison operations */
    OP_CMP_EQ  = 0x20,  /* cmp_eq r1, r2 -> flags */
    OP_CMP_NE  = 0x21,  /* cmp_ne r1, r2 -> flags */
    OP_CMP_LT  = 0x22,  /* cmp_lt r1, r2 -> flags */
    OP_CMP_LE  = 0x23,  /* cmp_le r1, r2 -> flags */
    OP_CMP_GT  = 0x24,  /* cmp_gt r1, r2 -> flags */
    OP_CMP_GE  = 0x25,  /* cmp_ge r1, r2 -> flags */
    
    /* Memory operations */
    OP_LOAD_IMM   = 0x30,  /* load immediate value */
    OP_LOAD_LOCAL = 0x31,  /* load from local variable */
    OP_STORE_LOCAL = 0x32, /* store to local variable */
    OP_LOAD_GLOBAL = 0x33, /* load from global variable */
    OP_STORE_GLOBAL = 0x34,/* store to global variable */
    OP_LOAD_MEM   = 0x35,  /* load from memory address */
    OP_STORE_MEM  = 0x36,  /* store to memory address */
    
    /* Stack operations */
    OP_PUSH = 0x40,     /* push value onto stack */
    OP_POP  = 0x41,     /* pop value from stack */
    OP_DUP  = 0x42,     /* duplicate top of stack */
    OP_SWAP = 0x43,     /* swap top two stack values */
    
    /* Control flow */
    OP_JUMP     = 0x50, /* unconditional jump */
    OP_JUMP_IF  = 0x51, /* conditional jump if true */
    OP_JUMP_NOT = 0x52, /* conditional jump if false */
    OP_CALL     = 0x53, /* function call */
    OP_RET      = 0x54, /* return from function */
    OP_ICALL    = 0x55, /* indirect call */
    
    /* Special operations */
    OP_NOP  = 0x00,     /* no operation */
    OP_HALT = 0xFF,     /* halt execution */
} bytecode_opcode_t;

/**
 * Bytecode instruction structure (placeholder).
 * Replace with actual chomsky3_bytecode_t when available.
 */
typedef struct {
    uint8_t opcode;
    uint8_t flags;
    uint16_t reserved;
    union {
        struct { int32_t operand1, operand2; } binary;
        struct { int32_t operand; } unary;
        struct { int64_t value; } immediate;
        struct { int32_t offset; } jump;
        struct { uint32_t index; } local;
    } data;
} bytecode_instruction_t;

/* ========================================================================
 * SLJIT Backend Context
 * ======================================================================== */

/**
 * SLJIT compilation context.
 */
typedef struct {
    struct sljit_compiler *compiler;
    chomsky3_jit_config_t *config;
    
    /* Jump label tracking */
    struct sljit_label **labels;
    size_t label_count;
    size_t label_capacity;
    
    /* Jump fixup list */
    struct {
        struct sljit_jump *jump;
        size_t target_offset;
    } *fixups;
    size_t fixup_count;
    size_t fixup_capacity;
    
    /* Local variable tracking */
    size_t local_count;
    size_t stack_size;
    
    /* Statistics */
    size_t instructions_compiled;
    size_t native_code_size;
} sljit_context_t;

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/**
 * Create SLJIT compilation context.
 */
static sljit_context_t *sljit_context_create(chomsky3_jit_config_t *config) {
    sljit_context_t *ctx = chomsky3_malloc(sizeof(sljit_context_t));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(sljit_context_t));
    ctx->config = config;
    
    ctx->compiler = sljit_create_compiler(NULL, NULL);
    if (!ctx->compiler) {
        chomsky3_free(ctx);
        return NULL;
    }
    
    /* Initialize label tracking */
    ctx->label_capacity = 64;
    ctx->labels = chomsky3_calloc(ctx->label_capacity, sizeof(struct sljit_label *));
    
    /* Initialize fixup tracking */
    ctx->fixup_capacity = 64;
    ctx->fixups = chomsky3_calloc(ctx->fixup_capacity, sizeof(*ctx->fixups));
    
    return ctx;
}

/**
 * Destroy SLJIT compilation context.
 */
static void sljit_context_destroy(sljit_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->compiler) {
        sljit_free_compiler(ctx->compiler);
    }
    
    chomsky3_free(ctx->labels);
    chomsky3_free(ctx->fixups);
    chomsky3_free(ctx);
}

/**
 * Get or create label for bytecode offset.
 */
static struct sljit_label *get_label(sljit_context_t *ctx, size_t offset) {
    /* Expand label array if needed */
    if (offset >= ctx->label_capacity) {
        size_t new_capacity = ctx->label_capacity * 2;
        while (new_capacity <= offset) new_capacity *= 2;
        
        struct sljit_label **new_labels = chomsky3_realloc(
            ctx->labels, new_capacity * sizeof(struct sljit_label *)
        );
        if (!new_labels) return NULL;
        
        memset(new_labels + ctx->label_capacity, 0,
               (new_capacity - ctx->label_capacity) * sizeof(struct sljit_label *));
        
        ctx->labels = new_labels;
        ctx->label_capacity = new_capacity;
    }
    
    /* Create label if it doesn't exist */
    if (!ctx->labels[offset]) {
        ctx->labels[offset] = sljit_emit_label(ctx->compiler);
        if (offset >= ctx->label_count) {
            ctx->label_count = offset + 1;
        }
    }
    
    return ctx->labels[offset];
}

/**
 * Add jump fixup for later resolution.
 */
static int add_fixup(sljit_context_t *ctx, struct sljit_jump *jump, size_t target) {
    if (ctx->fixup_count >= ctx->fixup_capacity) {
        size_t new_capacity = ctx->fixup_capacity * 2;
        void *new_fixups = chomsky3_realloc(
            ctx->fixups, new_capacity * sizeof(*ctx->fixups)
        );
        if (!new_fixups) return -1;
        
        ctx->fixups = new_fixups;
        ctx->fixup_capacity = new_capacity;
    }
    
    ctx->fixups[ctx->fixup_count].jump = jump;
    ctx->fixups[ctx->fixup_count].target_offset = target;
    ctx->fixup_count++;
    
    return 0;
}

/* ========================================================================
 * Instruction Compilation Functions
 * ======================================================================== */

/**
 * Emit function prologue.
 */
static int emit_prologue(sljit_context_t *ctx, size_t local_count) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Enter function with saved registers and local space */
    int err = sljit_emit_enter(c,
        0,  /* options */
        SLJIT_ARGS3(P, P, P, P),  /* 3 pointer arguments */
        3,  /* 3 scratch registers (R0-R2) */
        3,  /* 3 saved registers (S0-S2) */
        0,  /* no float registers */
        0,  /* no float saved registers */
        local_count * STACK_WORD_SIZE  /* local variable space */
    );
    
    if (err != SLJIT_SUCCESS) return -1;
    
    /* Initialize saved registers:
     * S0 (REG_STACK) = arg1 (stack pointer)
     * S1 (REG_IP) = arg2 (instruction pointer)
     * S2 (REG_CTX) = arg3 (context pointer)
     */
    sljit_emit_op1(c, SLJIT_MOV_P, REG_STACK, 0, SLJIT_S0, 0);
    sljit_emit_op1(c, SLJIT_MOV_P, REG_IP, 0, SLJIT_S1, 0);
    sljit_emit_op1(c, SLJIT_MOV_P, REG_CTX, 0, SLJIT_S2, 0);
    
    ctx->local_count = local_count;
    ctx->stack_size = local_count * STACK_WORD_SIZE;
    
    return 0;
}

/**
 * Emit function epilogue.
 */
static int emit_epilogue(sljit_context_t *ctx) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Return with R0 as return value */
    return sljit_emit_return(c, SLJIT_MOV, REG_ACC, 0);
}

/**
 * Compile arithmetic operation.
 */
static int compile_arithmetic(sljit_context_t *ctx, bytecode_opcode_t op) {
    struct sljit_compiler *c = ctx->compiler;
    int sljit_op;
    
    switch (op) {
        case OP_ADD: sljit_op = SLJIT_ADD; break;
        case OP_SUB: sljit_op = SLJIT_SUB; break;
        case OP_MUL: sljit_op = SLJIT_MUL; break;
        case OP_DIV: sljit_op = SLJIT_SDIV; break;
        case OP_NEG: sljit_op = SLJIT_NEG; break;
        default: return -1;
    }
    
    if (op == OP_NEG) {
        /* Unary operation: REG_ACC = -REG_ACC */
        return sljit_emit_op1(c, sljit_op, REG_ACC, 0, REG_ACC, 0);
    } else {
        /* Binary operation: REG_ACC = REG_ACC op REG_TMP1 */
        return sljit_emit_op2(c, sljit_op, REG_ACC, 0, REG_ACC, 0, REG_TMP1, 0);
    }
}

/**
 * Compile bitwise operation.
 */
static int compile_bitwise(sljit_context_t *ctx, bytecode_opcode_t op) {
    struct sljit_compiler *c = ctx->compiler;
    int sljit_op;
    
    switch (op) {
        case OP_AND: sljit_op = SLJIT_AND; break;
        case OP_OR:  sljit_op = SLJIT_OR; break;
        case OP_XOR: sljit_op = SLJIT_XOR; break;
        case OP_NOT: sljit_op = SLJIT_NOT; break;
        case OP_SHL: sljit_op = SLJIT_SHL; break;
        case OP_SHR: sljit_op = SLJIT_LSHR; break;
        default: return -1;
    }
    
    if (op == OP_NOT) {
        /* Unary operation */
        return sljit_emit_op1(c, sljit_op, REG_ACC, 0, REG_ACC, 0);
    } else {
        /* Binary operation */
        return sljit_emit_op2(c, sljit_op, REG_ACC, 0, REG_ACC, 0, REG_TMP1, 0);
    }
}

/**
 * Compile comparison operation.
 */
static int compile_comparison(sljit_context_t *ctx, bytecode_opcode_t op) {
    struct sljit_compiler *c = ctx->compiler;
    int cmp_type;
    
    switch (op) {
        case OP_CMP_EQ: cmp_type = SLJIT_EQUAL; break;
        case OP_CMP_NE: cmp_type = SLJIT_NOT_EQUAL; break;
        case OP_CMP_LT: cmp_type = SLJIT_LESS; break;
        case OP_CMP_LE: cmp_type = SLJIT_LESS_EQUAL; break;
        case OP_CMP_GT: cmp_type = SLJIT_GREATER; break;
        case OP_CMP_GE: cmp_type = SLJIT_GREATER_EQUAL; break;
        default: return -1;
    }
    
    /* Compare REG_ACC with REG_TMP1 */
    int err = sljit_emit_op2(c, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS | SLJIT_SET_GREATER,
                             SLJIT_UNUSED, 0, REG_ACC, 0, REG_TMP1, 0);
    if (err != SLJIT_SUCCESS) return err;
    
    /* Set REG_ACC to 1 if condition true, 0 otherwise */
    return sljit_emit_op_flags(c, SLJIT_MOV, REG_ACC, 0, cmp_type);
}

/**
 * Compile load immediate.
 */
static int compile_load_imm(sljit_context_t *ctx, int64_t value) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Load immediate value into REG_ACC */
    return sljit_emit_op1(c, SLJIT_MOV, REG_ACC, 0, SLJIT_IMM, value);
}

/**
 * Compile load from local variable.
 */
static int compile_load_local(sljit_context_t *ctx, uint32_t index) {
    struct sljit_compiler *c = ctx->compiler;
    
    if (index >= ctx->local_count) return -1;
    
    /* Load from stack frame: REG_ACC = [SP + index * 8] */
    return sljit_emit_op1(c, SLJIT_MOV, REG_ACC, 0,
                          SLJIT_MEM1(SLJIT_SP), index * STACK_WORD_SIZE);
}

/**
 * Compile store to local variable.
 */
static int compile_store_local(sljit_context_t *ctx, uint32_t index) {
    struct sljit_compiler *c = ctx->compiler;
    
    if (index >= ctx->local_count) return -1;
    
    /* Store to stack frame: [SP + index * 8] = REG_ACC */
    return sljit_emit_op1(c, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), index * STACK_WORD_SIZE,
                          REG_ACC, 0);
}

/**
 * Compile memory load.
 */
static int compile_load_mem(sljit_context_t *ctx) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Load from address in REG_ACC: REG_ACC = [REG_ACC] */
    return sljit_emit_op1(c, SLJIT_MOV, REG_ACC, 0, SLJIT_MEM1(REG_ACC), 0);
}

/**
 * Compile memory store.
 */
static int compile_store_mem(sljit_context_t *ctx) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Store REG_TMP1 to address in REG_ACC: [REG_ACC] = REG_TMP1 */
    return sljit_emit_op1(c, SLJIT_MOV, SLJIT_MEM1(REG_ACC), 0, REG_TMP1, 0);
}

/**
 * Compile unconditional jump.
 */
static int compile_jump(sljit_context_t *ctx, size_t target_offset) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Create jump and add to fixup list */
    struct sljit_jump *jump = sljit_emit_jump(c, SLJIT_JUMP);
    if (!jump) return -1;
    
    return add_fixup(ctx, jump, target_offset);
}

/**
 * Compile conditional jump.
 */
static int compile_jump_cond(sljit_context_t *ctx, size_t target_offset, int condition) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Compare REG_ACC with 0 */
    int err = sljit_emit_op2(c, SLJIT_SUB | SLJIT_SET_Z,
                             SLJIT_UNUSED, 0, REG_ACC, 0, SLJIT_IMM, 0);
    if (err != SLJIT_SUCCESS) return err;
    
    /* Jump based on condition */
    int jump_type = condition ? SLJIT_NOT_ZERO : SLJIT_ZERO;
    struct sljit_jump *jump = sljit_emit_jump(c, jump_type);
    if (!jump) return -1;
    
    return add_fixup(ctx, jump, target_offset);
}

/**
 * Compile function call.
 */
static int compile_call(sljit_context_t *ctx, void *target) {
    struct sljit_compiler *c = ctx->compiler;
    
    /* Indirect call through REG_ACC */
    return sljit_emit_icall(c, SLJIT_CALL, SLJIT_ARGS0(W), REG_ACC, 0);
}

/**
 * Compile return.
 */
static int compile_return(sljit_context_t *ctx) {
    return emit_epilogue(ctx);
}

/* ========================================================================
 * Main Compilation Function
 * ======================================================================== */

/**
 * Compile single bytecode instruction.
 */
static int compile_instruction(sljit_context_t *ctx, const bytecode_instruction_t *instr,
                               size_t offset) {
    int err;
    
    /* Create label for this instruction (for jump targets) */
    get_label(ctx, offset);
    
    switch (instr->opcode) {
        /* Arithmetic */
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_NEG:
            err = compile_arithmetic(ctx, instr->opcode);
            break;
            
        /* Bitwise */
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_NOT:
        case OP_SHL:
        case OP_SHR:
            err = compile_bitwise(ctx, instr->opcode);
            break;
            
        /* Comparison */
        case OP_CMP_EQ:
        case OP_CMP_NE:
        case OP_CMP_LT:
        case OP_CMP_LE:
        case OP_CMP_GT:
        case OP_CMP_GE:
            err = compile_comparison(ctx, instr->opcode);
            break;
            
        /* Memory operations */
        case OP_LOAD_IMM:
            err = compile_load_imm(ctx, instr->data.immediate.value);
            break;
            
        case OP_LOAD_LOCAL:
            err = compile_load_local(ctx, instr->data.local.index);
            break;
            
        case OP_STORE_LOCAL:
            err = compile_store_local(ctx, instr->data.local.index);
            break;
            
        case OP_LOAD_MEM:
            err = compile_load_mem(ctx);
            break;
            
        case OP_STORE_MEM:
            err = compile_store_mem(ctx);
            break;
            
        /* Control flow */
        case OP_JUMP:
            err = compile_jump(ctx, instr->data.jump.offset);
            break;
            
        case OP_JUMP_IF:
            err = compile_jump_cond(ctx, instr->data.jump.offset, 1);
            break;
            
        case OP_JUMP_NOT:
            err = compile_jump_cond(ctx, instr->data.jump.offset, 0);
            break;
            
        case OP_RET:
            err = compile_return(ctx);
            break;
            
        /* No-op */
        case OP_NOP:
            err = SLJIT_SUCCESS;
            break;
            
        default:
            err = -1;
            break;
    }
    
    if (err == SLJIT_SUCCESS) {
        ctx->instructions_compiled++;
    }
    
    return err;
}

/**
 * Resolve all jump fixups.
 */
static int resolve_fixups(sljit_context_t *ctx) {
    for (size_t i = 0; i < ctx->fixup_count; i++) {
        struct sljit_jump *jump = ctx->fixups[i].jump;
        size_t target = ctx->fixups[i].target_offset;
        
        if (target >= ctx->label_count || !ctx->labels[target]) {
            return -1;  /* Invalid jump target */
        }
        
        sljit_set_label(jump, ctx->labels[target]);
    }
    
    return 0;
}

/* ========================================================================
 * Public API Implementation
 * ======================================================================== */

/**
 * Compile bytecode to native code using SLJIT.
 * 
 * @param bytecode Bytecode to compile (placeholder - replace with actual type)
 * @param bytecode_size Size of bytecode in bytes
 * @param config JIT configuration
 * @param out_code Output native code buffer
 * @param out_size Output code size
 * @return 0 on success, -1 on error
 */
int chomsky3_jit_sljit_compile(
    const void *bytecode,
    size_t bytecode_size,
    chomsky3_jit_config_t *config,
    void **out_code,
    size_t *out_size
) {
    if (!bytecode || !config || !out_code || !out_size) {
        return chomsky3_error_set(CHOMSKY3_ERROR_INVALID_ARGUMENT,
                                  "Invalid arguments to SLJIT compile");
    }
    
    /* Create compilation context */
    sljit_context_t *ctx = sljit_context_create(config);
    if (!ctx) {
        return chomsky3_error_set(CHOMSKY3_ERROR_OUT_OF_MEMORY,
                                  "Failed to create SLJIT context");
    }
    
    /* Emit function prologue */
    if (emit_prologue(ctx, MAX_LOCALS) != 0) {
        sljit_context_destroy(ctx);
        return chomsky3_error_set(CHOMSKY3_ERROR_JIT_COMPILATION_FAILED,
                                  "Failed to emit prologue");
    }
    
    /* Compile bytecode instructions
     * NOTE: This is a placeholder. Replace with actual bytecode iteration
     * when chomsky3_bytecode_t structure is available.
     */
    const bytecode_instruction_t *instructions = (const bytecode_instruction_t *)bytecode;
    size_t instruction_count = bytecode_size / sizeof(bytecode_instruction_t);
    
    for (size_t i = 0; i < instruction_count; i++) {
        if (compile_instruction(ctx, &instructions[i], i) != 0) {
            sljit_context_destroy(ctx);
            return chomsky3_error_set(CHOMSKY3_ERROR_JIT_COMPILATION_FAILED,
                                      "Failed to compile instruction %zu", i);
        }
    }
    
    /* Emit function epilogue (if not already emitted by return) */
    emit_epilogue(ctx);
    
    /* Resolve all jump targets */
    if (resolve_fixups(ctx) != 0) {
        sljit_context_destroy(ctx);
        return chomsky3_error_set(CHOMSKY3_ERROR_JIT_COMPILATION_FAILED,
                                  "Failed to resolve jump targets");
    }
    
    /* Generate final code */
    void *code = sljit_generate_code(ctx->compiler);
    if (!code) {
        sljit_context_destroy(ctx);
        return chomsky3_error_set(CHOMSKY3_ERROR_JIT_COMPILATION_FAILED,
                                  "Failed to generate native code");
    }
    
    /* Get code size */
    *out_size = sljit_get_generated_code_size(ctx->compiler);
    *out_code = code;
    
    /* Update statistics */
    ctx->native_code_size = *out_size;
    
    /* Cleanup (but don't free the generated code) */
    sljit_context_destroy(ctx);
    
    return 0;
}

/**
 * Free native code generated by SLJIT.
 */
void chomsky3_jit_sljit_free(void *code) {
    if (code) {
        sljit_free_code(code, NULL);
    }
}

/**
 * Get SLJIT version information.
 */
const char *chomsky3_jit_sljit_version(void) {
    return "SLJIT " SLJIT_VERSION_STR;
}

/**
 * Check if SLJIT supports current architecture.
 */
int chomsky3_jit_sljit_is_supported(void) {
    return sljit_has_cpu_feature(SLJIT_HAS_FPU) >= 0;
}
