/**
 * libchomsky3 - Intermediate Representation (IR)
 * 
 * Header file providing the intermediate representation used in the
 * compilation pipeline between the regex AST and target code generation.
 * The IR is a low-level, architecture-independent representation optimized
 * for analysis and transformation.
 */

#ifndef CHOMSKY3_IR_H
#define CHOMSKY3_IR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "regex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_ir chomsky3_ir_t;
typedef struct chomsky3_ir_block chomsky3_ir_block_t;
typedef struct chomsky3_ir_instruction chomsky3_ir_instruction_t;
typedef struct chomsky3_ir_value chomsky3_ir_value_t;
typedef struct chomsky3_ir_builder chomsky3_ir_builder_t;
typedef struct chomsky3_ir_pass chomsky3_ir_pass_t;
typedef struct chomsky3_ir_analysis chomsky3_ir_analysis_t;

/* IR instruction opcodes */
typedef enum {
    /* Character matching */
    CHOMSKY3_IR_OP_MATCH_CHAR = 0,      /* Match single character */
    CHOMSKY3_IR_OP_MATCH_RANGE,         /* Match character range */
    CHOMSKY3_IR_OP_MATCH_CLASS,         /* Match character class */
    CHOMSKY3_IR_OP_MATCH_ANY,           /* Match any character */
    CHOMSKY3_IR_OP_MATCH_STRING,        /* Match literal string */
    
    /* Control flow */
    CHOMSKY3_IR_OP_JUMP,                /* Unconditional jump */
    CHOMSKY3_IR_OP_BRANCH,              /* Conditional branch */
    CHOMSKY3_IR_OP_SPLIT,               /* Split execution (NFA fork) */
    CHOMSKY3_IR_OP_CALL,                /* Call subroutine */
    CHOMSKY3_IR_OP_RETURN,              /* Return from subroutine */
    CHOMSKY3_IR_OP_MATCH,               /* Match success */
    CHOMSKY3_IR_OP_FAIL,                /* Match failure */
    
    /* Position and anchors */
    CHOMSKY3_IR_OP_ADVANCE,             /* Advance input position */
    CHOMSKY3_IR_OP_SAVE_POS,            /* Save current position */
    CHOMSKY3_IR_OP_RESTORE_POS,         /* Restore saved position */
    CHOMSKY3_IR_OP_CHECK_BEGIN,         /* Check start of input (^) */
    CHOMSKY3_IR_OP_CHECK_END,           /* Check end of input ($) */
    CHOMSKY3_IR_OP_CHECK_WORD_BOUNDARY, /* Check word boundary (\b) */
    
    /* Capture groups */
    CHOMSKY3_IR_OP_CAPTURE_START,       /* Start capture group */
    CHOMSKY3_IR_OP_CAPTURE_END,         /* End capture group */
    CHOMSKY3_IR_OP_BACKREFERENCE,       /* Match backreference */
    
    /* Lookahead/lookbehind */
    CHOMSKY3_IR_OP_LOOKAHEAD_START,     /* Start positive lookahead */
    CHOMSKY3_IR_OP_LOOKAHEAD_END,       /* End lookahead */
    CHOMSKY3_IR_OP_LOOKBEHIND_START,    /* Start positive lookbehind */
    CHOMSKY3_IR_OP_LOOKBEHIND_END,      /* End lookbehind */
    CHOMSKY3_IR_OP_NEG_LOOKAHEAD_START, /* Start negative lookahead */
    CHOMSKY3_IR_OP_NEG_LOOKBEHIND_START,/* Start negative lookbehind */
    
    /* Quantifiers */
    CHOMSKY3_IR_OP_REPEAT_START,        /* Start repeat loop */
    CHOMSKY3_IR_OP_REPEAT_END,          /* End repeat loop */
    CHOMSKY3_IR_OP_CHECK_COUNT,         /* Check repeat count */
    CHOMSKY3_IR_OP_INCREMENT_COUNT,     /* Increment repeat counter */
    
    /* Stack operations */
    CHOMSKY3_IR_OP_PUSH,                /* Push value to stack */
    CHOMSKY3_IR_OP_POP,                 /* Pop value from stack */
    CHOMSKY3_IR_OP_LOAD,                /* Load from memory */
    CHOMSKY3_IR_OP_STORE,               /* Store to memory */
    
    /* Arithmetic and logic */
    CHOMSKY3_IR_OP_ADD,                 /* Addition */
    CHOMSKY3_IR_OP_SUB,                 /* Subtraction */
    CHOMSKY3_IR_OP_MUL,                 /* Multiplication */
    CHOMSKY3_IR_OP_DIV,                 /* Division */
    CHOMSKY3_IR_OP_MOD,                 /* Modulo */
    CHOMSKY3_IR_OP_AND,                 /* Bitwise AND */
    CHOMSKY3_IR_OP_OR,                  /* Bitwise OR */
    CHOMSKY3_IR_OP_XOR,                 /* Bitwise XOR */
    CHOMSKY3_IR_OP_NOT,                 /* Bitwise NOT */
    CHOMSKY3_IR_OP_SHL,                 /* Shift left */
    CHOMSKY3_IR_OP_SHR,                 /* Shift right */
    
    /* Comparison */
    CHOMSKY3_IR_OP_CMP_EQ,              /* Compare equal */
    CHOMSKY3_IR_OP_CMP_NE,              /* Compare not equal */
    CHOMSKY3_IR_OP_CMP_LT,              /* Compare less than */
    CHOMSKY3_IR_OP_CMP_LE,              /* Compare less or equal */
    CHOMSKY3_IR_OP_CMP_GT,              /* Compare greater than */
    CHOMSKY3_IR_OP_CMP_GE,              /* Compare greater or equal */
    
    /* Special */
    CHOMSKY3_IR_OP_NOP,                 /* No operation */
    CHOMSKY3_IR_OP_PHI,                 /* SSA phi node */
    CHOMSKY3_IR_OP_COMMENT,             /* Comment (debug) */
    CHOMSKY3_IR_OP_LABEL,               /* Label (target for jumps) */
    
    CHOMSKY3_IR_OP_COUNT                /* Number of opcodes */
} chomsky3_ir_opcode_t;

/* IR value types */
typedef enum {
    CHOMSKY3_IR_VALUE_NONE = 0,         /* No value */
    CHOMSKY3_IR_VALUE_IMMEDIATE,        /* Immediate constant */
    CHOMSKY3_IR_VALUE_REGISTER,         /* Virtual register */
    CHOMSKY3_IR_VALUE_MEMORY,           /* Memory location */
    CHOMSKY3_IR_VALUE_LABEL,            /* Label reference */
    CHOMSKY3_IR_VALUE_BLOCK,            /* Basic block reference */
    CHOMSKY3_IR_VALUE_STRING,           /* String constant */
    CHOMSKY3_IR_VALUE_CHARCLASS         /* Character class */
} chomsky3_ir_value_type_t;

/* IR data types */
typedef enum {
    CHOMSKY3_IR_TYPE_VOID = 0,          /* No type */
    CHOMSKY3_IR_TYPE_I8,                /* 8-bit integer */
    CHOMSKY3_IR_TYPE_I16,               /* 16-bit integer */
    CHOMSKY3_IR_TYPE_I32,               /* 32-bit integer */
    CHOMSKY3_IR_TYPE_I64,               /* 64-bit integer */
    CHOMSKY3_IR_TYPE_U8,                /* 8-bit unsigned */
    CHOMSKY3_IR_TYPE_U16,               /* 16-bit unsigned */
    CHOMSKY3_IR_TYPE_U32,               /* 32-bit unsigned */
    CHOMSKY3_IR_TYPE_U64,               /* 64-bit unsigned */
    CHOMSKY3_IR_TYPE_PTR,               /* Pointer */
    CHOMSKY3_IR_TYPE_BOOL               /* Boolean */
} chomsky3_ir_type_t;

/* IR value */
struct chomsky3_ir_value {
    chomsky3_ir_value_type_t type;      /* Value type */
    chomsky3_ir_type_t data_type;       /* Data type */
    
    union {
        int64_t immediate;              /* Immediate value */
        uint32_t reg;                   /* Register number */
        struct {
            uint32_t base_reg;          /* Base register */
            int32_t offset;             /* Offset */
        } memory;
        uint32_t label_id;              /* Label ID */
        chomsky3_ir_block_t *block;     /* Block pointer */
        const char *string;             /* String constant */
        void *charclass;                /* Character class data */
    } data;
    
    /* Metadata */
    const char *name;                   /* Optional name (for debugging) */
    uint32_t flags;                     /* Value flags */
};

/* IR instruction */
struct chomsky3_ir_instruction {
    chomsky3_ir_opcode_t opcode;        /* Instruction opcode */
    
    chomsky3_ir_value_t dest;           /* Destination operand */
    chomsky3_ir_value_t src1;           /* Source operand 1 */
    chomsky3_ir_value_t src2;           /* Source operand 2 */
    chomsky3_ir_value_t src3;           /* Source operand 3 (for special ops) */
    
    /* Metadata */
    uint32_t id;                        /* Instruction ID */
    uint32_t flags;                     /* Instruction flags */
    const char *comment;                /* Optional comment */
    
    /* Source location (for debugging) */
    uint32_t line;                      /* Source line */
    uint32_t column;                    /* Source column */
    
    /* Links */
    chomsky3_ir_instruction_t *next;    /* Next instruction */
    chomsky3_ir_instruction_t *prev;    /* Previous instruction */
    chomsky3_ir_block_t *parent;        /* Parent block */
};

/* IR basic block */
struct chomsky3_ir_block {
    uint32_t id;                        /* Block ID */
    const char *label;                  /* Block label */
    
    /* Instructions */
    chomsky3_ir_instruction_t *first;   /* First instruction */
    chomsky3_ir_instruction_t *last;    /* Last instruction */
    size_t num_instructions;            /* Number of instructions */
    
    /* Control flow graph */
    chomsky3_ir_block_t **predecessors; /* Predecessor blocks */
    size_t num_predecessors;            /* Number of predecessors */
    chomsky3_ir_block_t **successors;   /* Successor blocks */
    size_t num_successors;              /* Number of successors */
    
    /* Dominator tree */
    chomsky3_ir_block_t *idom;          /* Immediate dominator */
    chomsky3_ir_block_t **dominated;    /* Dominated blocks */
    size_t num_dominated;               /* Number of dominated blocks */
    
    /* Loop information */
    chomsky3_ir_block_t *loop_header;   /* Loop header (if in loop) */
    uint32_t loop_depth;                /* Loop nesting depth */
    
    /* Metadata */
    uint32_t flags;                     /* Block flags */
    void *user_data;                    /* User data */
    
    /* Links */
    chomsky3_ir_block_t *next;          /* Next block */
    chomsky3_ir_block_t *prev;          /* Previous block */
};

/* IR function/program */
struct chomsky3_ir {
    chomsky3_context_t *ctx;            /* Context */
    
    /* Basic blocks */
    chomsky3_ir_block_t *entry;         /* Entry block */
    chomsky3_ir_block_t *exit;          /* Exit block */
    chomsky3_ir_block_t *first_block;   /* First block */
    chomsky3_ir_block_t *last_block;    /* Last block */
    size_t num_blocks;                  /* Number of blocks */
    
    /* Register allocation */
    uint32_t num_registers;             /* Number of virtual registers */
    uint32_t num_physical_registers;    /* Number of physical registers */
    
    /* Constants */
    const char **string_constants;      /* String constants */
    size_t num_string_constants;        /* Number of string constants */
    
    /* Metadata */
    const char *name;                   /* Function name */
    uint32_t flags;                     /* IR flags */
    
    /* Statistics */
    size_t total_instructions;          /* Total instructions */
    uint32_t max_stack_depth;           /* Maximum stack depth */
    
    /* Source information */
    const chomsky3_regex_t *source_ast; /* Source AST */
    const char *source_pattern;         /* Source pattern string */
};

/* IR builder for constructing IR */
struct chomsky3_ir_builder {
    chomsky3_context_t *ctx;            /* Context */
    chomsky3_ir_t *ir;                  /* IR being built */
    
    chomsky3_ir_block_t *current_block; /* Current block */
    uint32_t next_register;             /* Next register number */
    uint32_t next_label;                /* Next label ID */
    uint32_t next_block_id;             /* Next block ID */
    
    /* Builder state */
    bool in_ssa_form;                   /* Whether IR is in SSA form */
    uint32_t flags;                     /* Builder flags */
};

/* IR optimization pass */
typedef enum {
    CHOMSKY3_IR_PASS_DCE,               /* Dead code elimination */
    CHOMSKY3_IR_PASS_CSE,               /* Common subexpression elimination */
    CHOMSKY3_IR_PASS_CONSTANT_FOLD,     /* Constant folding */
    CHOMSKY3_IR_PASS_CONSTANT_PROP,     /* Constant propagation */
    CHOMSKY3_IR_PASS_COPY_PROP,         /* Copy propagation */
    CHOMSKY3_IR_PASS_INLINE,            /* Inlining */
    CHOMSKY3_IR_PASS_LOOP_UNROLL,       /* Loop unrolling */
    CHOMSKY3_IR_PASS_LOOP_INVARIANT,    /* Loop-invariant code motion */
    CHOMSKY3_IR_PASS_STRENGTH_REDUCE,   /* Strength reduction */
    CHOMSKY3_IR_PASS_PEEPHOLE,          /* Peephole optimization */
    CHOMSKY3_IR_PASS_TAIL_CALL,         /* Tail call optimization */
    CHOMSKY3_IR_PASS_BRANCH_FOLD,       /* Branch folding */
    CHOMSKY3_IR_PASS_JUMP_THREAD,       /* Jump threading */
    CHOMSKY3_IR_PASS_BLOCK_MERGE,       /* Basic block merging */
    CHOMSKY3_IR_PASS_SIMPLIFY_CFG,      /* CFG simplification */
    CHOMSKY3_IR_PASS_SSA_CONSTRUCT,     /* SSA construction */
    CHOMSKY3_IR_PASS_SSA_DESTRUCT,      /* SSA destruction */
    CHOMSKY3_IR_PASS_REGISTER_ALLOC,    /* Register allocation */
    CHOMSKY3_IR_PASS_CUSTOM             /* Custom pass */
} chomsky3_ir_pass_type_t;

/* IR pass function signature */
typedef chomsky3_error_t (*chomsky3_ir_pass_func_t)(
    chomsky3_ir_t *ir,
    void *user_data
);

/* IR optimization pass */
struct chomsky3_ir_pass {
    chomsky3_ir_pass_type_t type;       /* Pass type */
    const char *name;                   /* Pass name */
    chomsky3_ir_pass_func_t func;       /* Pass function */
    void *user_data;                    /* User data */
    uint32_t flags;                     /* Pass flags */
};

/* IR analysis results */
struct chomsky3_ir_analysis {
    /* Control flow */
    bool *reachable_blocks;             /* Reachable blocks bitmap */
    uint32_t *postorder;                /* Postorder traversal */
    uint32_t *reverse_postorder;        /* Reverse postorder */
    
    /* Data flow */
    uint64_t **live_in;                 /* Live-in sets per block */
    uint64_t **live_out;                /* Live-out sets per block */
    uint64_t **def;                     /* Definition sets */
    uint64_t **use;                     /* Use sets */
    
    /* Dominance */
    chomsky3_ir_block_t **dominators;   /* Dominator tree */
    uint64_t **dominance_frontier;      /* Dominance frontiers */
    
    /* Loop analysis */
    uint32_t *loop_depth;               /* Loop depth per block */
    chomsky3_ir_block_t **loop_headers; /* Loop headers */
    size_t num_loops;                   /* Number of loops */
    
    /* Alias analysis */
    bool **may_alias;                   /* May-alias matrix */
    
    /* Statistics */
    size_t num_blocks;                  /* Number of blocks */
    size_t num_instructions;            /* Number of instructions */
    size_t num_registers;               /* Number of registers */
};

/**
 * Create new IR.
 * 
 * @param ctx Context
 * @param name IR name (optional)
 * @return New IR or NULL on failure
 */
chomsky3_ir_t *chomsky3_ir_create(
    chomsky3_context_t *ctx,
    const char *name
);

/**
 * Destroy IR.
 * 
 * @param ir IR to destroy
 */
void chomsky3_ir_destroy(chomsky3_ir_t *ir);

/**
 * Create IR builder.
 * 
 * @param ctx Context
 * @return New IR builder or NULL on failure
 */
chomsky3_ir_builder_t *chomsky3_ir_builder_create(chomsky3_context_t *ctx);

/**
 * Destroy IR builder.
 * 
 * @param builder IR builder to destroy
 */
void chomsky3_ir_builder_destroy(chomsky3_ir_builder_t *builder);

/**
 * Build IR from regex AST.
 * 
 * @param builder IR builder
 * @param ast Regex AST
 * @param ir Output IR
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_build_from_ast(
    chomsky3_ir_builder_t *builder,
    const chomsky3_regex_t *ast,
    chomsky3_ir_t **ir
);

/* Basic block operations */

/**
 * Create new basic block.
 * 
 * @param builder IR builder
 * @param label Block label (optional)
 * @return New block or NULL on failure
 */
chomsky3_ir_block_t *chomsky3_ir_block_create(
    chomsky3_ir_builder_t *builder,
    const char *label
);

/**
 * Append block to IR.
 * 
 * @param ir IR
 * @param block Block to append
 */
void chomsky3_ir_append_block(chomsky3_ir_t *ir, chomsky3_ir_block_t *block);

/**
 * Insert block after another block.
 * 
 * @param ir IR
 * @param after Insert after this block
 * @param block Block to insert
 */
void chomsky3_ir_insert_block_after(
    chomsky3_ir_t *ir,
    chomsky3_ir_block_t *after,
    chomsky3_ir_block_t *block
);

/**
 * Remove block from IR.
 * 
 * @param ir IR
 * @param block Block to remove
 */
void chomsky3_ir_remove_block(chomsky3_ir_t *ir, chomsky3_ir_block_t *block);

/**
 * Add edge between blocks.
 * 
 * @param from Source block
 * @param to Destination block
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_add_edge(
    chomsky3_ir_block_t *from,
    chomsky3_ir_block_t *to
);

/**
 * Remove edge between blocks.
 * 
 * @param from Source block
 * @param to Destination block
 */
void chomsky3_ir_remove_edge(
    chomsky3_ir_block_t *from,
    chomsky3_ir_block_t *to
);

/* Instruction operations */

/**
 * Create new instruction.
 * 
 * @param builder IR builder
 * @param opcode Instruction opcode
 * @return New instruction or NULL on failure
 */
chomsky3_ir_instruction_t *chomsky3_ir_instruction_create(
    chomsky3_ir_builder_t *builder,
    chomsky3_ir_opcode_t opcode
);

/**
 * Append instruction to block.
 * 
 * @param block Block
 * @param inst Instruction to append
 */
void chomsky3_ir_append_instruction(
    chomsky3_ir_block_t *block,
    chomsky3_ir_instruction_t *inst
);

/**
 * Insert instruction before another instruction.
 * 
 * @param before Insert before this instruction
 * @param inst Instruction to insert
 */
void chomsky3_ir_insert_instruction_before(
    chomsky3_ir_instruction_t *before,
    chomsky3_ir_instruction_t *inst
);

/**
 * Insert instruction after another instruction.
 * 
 * @param after Insert after this instruction
 * @param inst Instruction to insert
 */
void chomsky3_ir_insert_instruction_after(
    chomsky3_ir_instruction_t *after,
    chomsky3_ir_instruction_t *inst
);

/**
 * Remove instruction from block.
 * 
 * @param inst Instruction to remove
 */
void chomsky3_ir_remove_instruction(chomsky3_ir_instruction_t *inst);

/**
 * Replace instruction.
 * 
 * @param old Old instruction
 * @param new New instruction
 */
void chomsky3_ir_replace_instruction(
    chomsky3_ir_instruction_t *old,
    chomsky3_ir_instruction_t *new
);

/* Value operations */

/**
 * Create immediate value.
 * 
 * @param value Immediate value
 * @param type Data type
 * @return IR value
 */
chomsky3_ir_value_t chomsky3_ir_value_immediate(
    int64_t value,
    chomsky3_ir_type_t type
);

/**
 * Create register value.
 * 
 * @param builder IR builder
 * @param type Data type
 * @return IR value
 */
chomsky3_ir_value_t chomsky3_ir_value_register(
    chomsky3_ir_builder_t *builder,
    chomsky3_ir_type_t type
);

/**
 * Create label value.
 * 
 * @param builder IR builder
 * @return IR value
 */
chomsky3_ir_value_t chomsky3_ir_value_label(chomsky3_ir_builder_t *builder);

/**
 * Create block reference value.
 * 
 * @param block Block
 * @return IR value
 */
chomsky3_ir_value_t chomsky3_ir_value_block(chomsky3_ir_block_t *block);

/**
 * Create string constant value.
 * 
 * @param builder IR builder
 * @param string String constant
 * @return IR value
 */
chomsky3_ir_value_t chomsky3_ir_value_string(
    chomsky3_ir_builder_t *builder,
    const char *string
);

/* High-level instruction builders */

/**
 * Build match character instruction.
 * 
 * @param builder IR builder
 * @param ch Character to match
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_match_char(
    chomsky3_ir_builder_t *builder,
    uint32_t ch
);

/**
 * Build match range instruction.
 * 
 * @param builder IR builder
 * @param start Range start
 * @param end Range end
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_match_range(
    chomsky3_ir_builder_t *builder,
    uint32_t start,
    uint32_t end
);

/**
 * Build jump instruction.
 * 
 * @param builder IR builder
 * @param target Target block
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_jump(
    chomsky3_ir_builder_t *builder,
    chomsky3_ir_block_t *target
);

/**
 * Build conditional branch instruction.
 * 
 * @param builder IR builder
 * @param condition Condition value
 * @param true_block True branch target
 * @param false_block False branch target
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_branch(
    chomsky3_ir_builder_t *builder,
    chomsky3_ir_value_t condition,
    chomsky3_ir_block_t *true_block,
    chomsky3_ir_block_t *false_block
);

/**
 * Build split instruction (NFA fork).
 * 
 * @param builder IR builder
 * @param target1 First target
 * @param target2 Second target
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_split(
    chomsky3_ir_builder_t *builder,
    chomsky3_ir_block_t *target1,
    chomsky3_ir_block_t *target2
);

/**
 * Build capture start instruction.
 * 
 * @param builder IR builder
 * @param group_id Capture group ID
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_capture_start(
    chomsky3_ir_builder_t *builder,
    uint32_t group_id
);

/**
 * Build capture end instruction.
 * 
 * @param builder IR builder
 * @param group_id Capture group ID
 * @return Instruction
 */
chomsky3_ir_instruction_t *chomsky3_ir_build_capture_end(
    chomsky3_ir_builder_t *builder,
    uint32_t group_id
);

/* Optimization passes */

/**
 * Run optimization pass on IR.
 * 
 * @param ir IR
 * @param pass Pass to run
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_run_pass(
    chomsky3_ir_t *ir,
    const chomsky3_ir_pass_t *pass
);

/**
 * Run multiple optimization passes.
 * 
 * @param ir IR
 * @param passes Array of passes
 * @param num_passes Number of passes
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_run_passes(
    chomsky3_ir_t *ir,
    const chomsky3_ir_pass_t *passes,
    size_t num_passes
);

/**
 * Get standard optimization pass.
 * 
 * @param type Pass type
 * @return Pass or NULL if not available
 */
const chomsky3_ir_pass_t *chomsky3_ir_get_pass(chomsky3_ir_pass_type_t type);

/**
 * Create custom optimization pass.
 * 
 * @param name Pass name
 * @param func Pass function
 * @param user_data User data
 * @return New pass or NULL on failure
 */
chomsky3_ir_pass_t *chomsky3_ir_pass_create(
    const char *name,
    chomsky3_ir_pass_func_t func,
    void *user_data
);

/**
 * Destroy optimization pass.
 * 
 * @param pass Pass to destroy
 */
void chomsky3_ir_pass_destroy(chomsky3_ir_pass_t *pass);

/* Analysis */

/**
 * Perform IR analysis.
 * 
 * @param ir IR
 * @param analysis Output analysis results
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_analyze(
    const chomsky3_ir_t *ir,
    chomsky3_ir_analysis_t **analysis
);

/**
 * Free analysis results.
 * 
 * @param analysis Analysis to free
 */
void chomsky3_ir_analysis_free(chomsky3_ir_analysis_t *analysis);

/**
 * Compute dominators.
 * 
 * @param ir IR
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_compute_dominators(chomsky3_ir_t *ir);

/**
 * Compute liveness information.
 * 
 * @param ir IR
 * @param analysis Analysis results
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_compute_liveness(
    const chomsky3_ir_t *ir,
    chomsky3_ir_analysis_t *analysis
);

/**
 * Detect loops.
 * 
 * @param ir IR
 * @param analysis Analysis results
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_detect_loops(
    const chomsky3_ir_t *ir,
    chomsky3_ir_analysis_t *analysis
);

/* Verification */

/**
 * Verify IR correctness.
 * 
 * @param ir IR to verify
 * @param error_msg Output error message (if verification fails)
 * @return true if valid, false otherwise
 */
bool chomsky3_ir_verify(const chomsky3_ir_t *ir, char **error_msg);

/**
 * Verify basic block.
 * 
 * @param block Block to verify
 * @param error_msg Output error message
 * @return true if valid, false otherwise
 */
bool chomsky3_ir_verify_block(
    const chomsky3_ir_block_t *block,
    char **error_msg
);

/**
 * Verify instruction.
 * 
 * @param inst Instruction to verify
 * @param error_msg Output error message
 * @return true if valid, false otherwise
 */
bool chomsky3_ir_verify_instruction(
    const chomsky3_ir_instruction_t *inst,
    char **error_msg
);

/* Serialization and debugging */

/**
 * Print IR to string.
 * 
 * @param ir IR
 * @param output Output string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_print(const chomsky3_ir_t *ir, char **output);

/**
 * Print IR to file.
 * 
 * @param ir IR
 * @param filename Output filename
 * @return Error code
 */
chomsky3_error_t chomsky3_ir_print_file(const chomsky3_ir_t *ir, const char *filename);

#endif /* CHOMSKY3_IR_H */
