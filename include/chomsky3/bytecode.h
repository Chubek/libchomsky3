/**
 * libchomsky3 - Bytecode Interface
 * 
 * Header file providing the bytecode representation and execution interface
 * for compiled regular expressions.
 */

#ifndef CHOMSKY3_BYTECODE_H
#define CHOMSKY3_BYTECODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_bytecode chomsky3_bytecode_t;
typedef struct chomsky3_vm chomsky3_vm_t;
typedef struct chomsky3_vm_state chomsky3_vm_state_t;
typedef struct chomsky3_ir chomsky3_ir_t;

/* Bytecode instruction opcodes */
typedef enum {
    /* Character matching */
    CHOMSKY3_OP_CHAR = 0x00,        /* Match single character */
    CHOMSKY3_OP_CHAR_RANGE = 0x01,  /* Match character range */
    CHOMSKY3_OP_CHAR_CLASS = 0x02,  /* Match character class */
    CHOMSKY3_OP_ANY = 0x03,         /* Match any character */
    CHOMSKY3_OP_ANY_NL = 0x04,      /* Match any including newline */
    
    /* String matching */
    CHOMSKY3_OP_STRING = 0x10,      /* Match literal string */
    CHOMSKY3_OP_STRING_ICASE = 0x11, /* Match string case-insensitive */
    
    /* Control flow */
    CHOMSKY3_OP_JUMP = 0x20,        /* Unconditional jump */
    CHOMSKY3_OP_SPLIT = 0x21,       /* Split execution (fork) */
    CHOMSKY3_OP_MATCH = 0x22,       /* Successful match */
    CHOMSKY3_OP_FAIL = 0x23,        /* Explicit failure */
    
    /* Anchors */
    CHOMSKY3_OP_ANCHOR_START = 0x30, /* Match start of string */
    CHOMSKY3_OP_ANCHOR_END = 0x31,   /* Match end of string */
    CHOMSKY3_OP_ANCHOR_LINE_START = 0x32, /* Match start of line */
    CHOMSKY3_OP_ANCHOR_LINE_END = 0x33,   /* Match end of line */
    CHOMSKY3_OP_ANCHOR_WORD = 0x34,  /* Word boundary */
    CHOMSKY3_OP_ANCHOR_NWORD = 0x35, /* Non-word boundary */
    
    /* Capture groups */
    CHOMSKY3_OP_SAVE_START = 0x40,  /* Save capture group start */
    CHOMSKY3_OP_SAVE_END = 0x41,    /* Save capture group end */
    CHOMSKY3_OP_BACKREF = 0x42,     /* Backreference */
    
    /* Lookahead/lookbehind */
    CHOMSKY3_OP_LOOK_AHEAD = 0x50,  /* Positive lookahead */
    CHOMSKY3_OP_LOOK_AHEAD_NEG = 0x51, /* Negative lookahead */
    CHOMSKY3_OP_LOOK_BEHIND = 0x52, /* Positive lookbehind */
    CHOMSKY3_OP_LOOK_BEHIND_NEG = 0x53, /* Negative lookbehind */
    
    /* Quantifiers (optimized) */
    CHOMSKY3_OP_REPEAT = 0x60,      /* Generic repeat */
    CHOMSKY3_OP_REPEAT_LAZY = 0x61, /* Lazy repeat */
    CHOMSKY3_OP_REPEAT_NG = 0x62,   /* Non-greedy repeat */
    
    /* Special */
    CHOMSKY3_OP_NOP = 0xF0,         /* No operation */
    CHOMSKY3_OP_DEBUG = 0xF1,       /* Debug breakpoint */
    CHOMSKY3_OP_CHECKPOINT = 0xF2   /* Execution checkpoint */
} chomsky3_opcode_t;

/* Bytecode instruction */
typedef struct {
    chomsky3_opcode_t opcode;       /* Instruction opcode */
    uint32_t operand1;              /* First operand */
    uint32_t operand2;              /* Second operand */
    uint32_t operand3;              /* Third operand */
    const void *data;               /* Additional data pointer */
} chomsky3_instruction_t;

/* Bytecode format version */
typedef struct {
    uint16_t major;                 /* Major version */
    uint16_t minor;                 /* Minor version */
    uint16_t patch;                 /* Patch version */
} chomsky3_bytecode_version_t;

/* Bytecode header */
typedef struct {
    uint32_t magic;                 /* Magic number (0x43484F4D = "CHOM") */
    chomsky3_bytecode_version_t version; /* Bytecode format version */
    uint32_t flags;                 /* Compilation flags */
    uint32_t num_instructions;      /* Number of instructions */
    uint32_t num_captures;          /* Number of capture groups */
    uint32_t data_size;             /* Size of constant data section */
    uint32_t checksum;              /* CRC32 checksum */
} chomsky3_bytecode_header_t;

/* Bytecode structure */
struct chomsky3_bytecode {
    chomsky3_bytecode_header_t header; /* Bytecode header */
    chomsky3_instruction_t *instructions; /* Instruction array */
    uint8_t *data;                  /* Constant data section */
    
    /* Metadata */
    char *pattern_source;           /* Original pattern (optional) */
    uint32_t *line_map;             /* Instruction to pattern line mapping */
    
    /* Statistics */
    size_t total_size;              /* Total bytecode size in bytes */
    size_t code_size;               /* Code section size */
    size_t data_section_size;       /* Data section size */
};

/* VM execution mode */
typedef enum {
    CHOMSKY3_VM_MODE_MATCH = 0,     /* Match mode (find first match) */
    CHOMSKY3_VM_MODE_SEARCH = 1,    /* Search mode (find all matches) */
    CHOMSKY3_VM_MODE_SPLIT = 2,     /* Split mode (split on matches) */
    CHOMSKY3_VM_MODE_REPLACE = 3    /* Replace mode */
} chomsky3_vm_mode_t;

/* VM execution limits */
typedef struct {
    size_t max_steps;               /* Maximum execution steps */
    size_t max_stack_depth;         /* Maximum stack depth */
    size_t max_backtrack;           /* Maximum backtrack count */
    uint64_t timeout_ns;            /* Timeout in nanoseconds */
} chomsky3_vm_limits_t;

/* VM statistics */
typedef struct {
    uint64_t steps_executed;        /* Total steps executed */
    uint64_t backtracks;            /* Number of backtracks */
    uint64_t splits;                /* Number of splits */
    size_t max_stack_used;          /* Maximum stack depth used */
    uint64_t execution_time_ns;     /* Execution time in nanoseconds */
} chomsky3_vm_stats_t;

/* VM state (for resumable execution) */
struct chomsky3_vm_state {
    size_t pc;                      /* Program counter */
    size_t sp;                      /* String position */
    void *stack;                    /* Execution stack */
    size_t stack_size;              /* Current stack size */
    chomsky3_vm_stats_t stats;      /* Execution statistics */
    bool suspended;                 /* Execution suspended flag */
};

/* VM instance */
struct chomsky3_vm {
    chomsky3_context_t *ctx;        /* Parent context */
    const chomsky3_bytecode_t *bytecode; /* Bytecode to execute */
    chomsky3_vm_mode_t mode;        /* Execution mode */
    chomsky3_vm_limits_t limits;    /* Execution limits */
    
    /* Internal state */
    void *internal;                 /* Opaque internal state */
};

/**
 * Create bytecode from IR.
 * 
 * @param ctx Context
 * @param ir Intermediate representation
 * @param bytecode Output bytecode (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    chomsky3_bytecode_t **bytecode
);

/**
 * Create bytecode from compiled pattern.
 * 
 * @param pattern Compiled pattern
 * @param bytecode Output bytecode (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_from_pattern(
    const chomsky3_pattern_t *pattern,
    chomsky3_bytecode_t **bytecode
);

/**
 * Free bytecode structure.
 * 
 * @param bytecode Bytecode to free
 */
void chomsky3_bytecode_free(chomsky3_bytecode_t *bytecode);

/**
 * Serialize bytecode to buffer.
 * 
 * @param bytecode Bytecode to serialize
 * @param buffer Output buffer (allocated by function)
 * @param size Output buffer size
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_serialize(
    const chomsky3_bytecode_t *bytecode,
    uint8_t **buffer,
    size_t *size
);

/**
 * Deserialize bytecode from buffer.
 * 
 * @param ctx Context
 * @param buffer Input buffer
 * @param size Buffer size
 * @param bytecode Output bytecode (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_deserialize(
    chomsky3_context_t *ctx,
    const uint8_t *buffer,
    size_t size,
    chomsky3_bytecode_t **bytecode
);

/**
 * Save bytecode to file.
 * 
 * @param bytecode Bytecode to save
 * @param path File path
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_save(
    const chomsky3_bytecode_t *bytecode,
    const char *path
);

/**
 * Load bytecode from file.
 * 
 * @param ctx Context
 * @param path File path
 * @param bytecode Output bytecode (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_load(
    chomsky3_context_t *ctx,
    const char *path,
    chomsky3_bytecode_t **bytecode
);

/**
 * Validate bytecode integrity.
 * 
 * @param bytecode Bytecode to validate
 * @return true if valid, false otherwise
 */
bool chomsky3_bytecode_validate(const chomsky3_bytecode_t *bytecode);

/**
 * Get bytecode version.
 * 
 * @param bytecode Bytecode
 * @return Version structure
 */
chomsky3_bytecode_version_t chomsky3_bytecode_get_version(
    const chomsky3_bytecode_t *bytecode
);

/**
 * Disassemble bytecode to human-readable format.
 * 
 * @param bytecode Bytecode to disassemble
 * @param output Output string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_disassemble(
    const chomsky3_bytecode_t *bytecode,
    char **output
);

/**
 * Get instruction name.
 * 
 * @param opcode Instruction opcode
 * @return Instruction name string
 */
const char *chomsky3_opcode_name(chomsky3_opcode_t opcode);

/* VM functions */

/**
 * Create a new VM instance.
 * 
 * @param ctx Context
 * @param bytecode Bytecode to execute
 * @return New VM instance or NULL on failure
 */
chomsky3_vm_t *chomsky3_vm_new(
    chomsky3_context_t *ctx,
    const chomsky3_bytecode_t *bytecode
);

/**
 * Free a VM instance.
 * 
 * @param vm VM to free
 */
void chomsky3_vm_free(chomsky3_vm_t *vm);

/**
 * Set VM execution mode.
 * 
 * @param vm VM instance
 * @param mode Execution mode
 */
void chomsky3_vm_set_mode(chomsky3_vm_t *vm, chomsky3_vm_mode_t mode);

/**
 * Set VM execution limits.
 * 
 * @param vm VM instance
 * @param limits Execution limits
 */
void chomsky3_vm_set_limits(chomsky3_vm_t *vm, const chomsky3_vm_limits_t *limits);

/**
 * Get default VM limits.
 * 
 * @param limits Limits structure to fill
 */
void chomsky3_vm_limits_default(chomsky3_vm_limits_t *limits);

/**
 * Execute bytecode on input string.
 * 
 * @param vm VM instance
 * @param input Input string
 * @param length Input length
 * @param match Match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_execute(
    chomsky3_vm_t *vm,
    const char *input,
    size_t length,
    chomsky3_match_t **match
);

/**
 * Execute bytecode with state (resumable).
 * 
 * @param vm VM instance
 * @param input Input string
 * @param length Input length
 * @param state VM state (in/out)
 * @param match Match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_execute_stateful(
    chomsky3_vm_t *vm,
    const char *input,
    size_t length,
    chomsky3_vm_state_t *state,
    chomsky3_match_t **match
);

/**
 * Reset VM state.
 * 
 * @param vm VM instance
 */
void chomsky3_vm_reset(chomsky3_vm_t *vm);

/**
 * Get VM execution statistics.
 * 
 * @param vm VM instance
 * @param stats Statistics structure to fill
 */
void chomsky3_vm_get_stats(const chomsky3_vm_t *vm, chomsky3_vm_stats_t *stats);

/**
 * Create a new VM state.
 * 
 * @return New VM state or NULL on failure
 */
chomsky3_vm_state_t *chomsky3_vm_state_new(void);

/**
 * Free a VM state.
 * 
 * @param state VM state to free
 */
void chomsky3_vm_state_free(chomsky3_vm_state_t *state);

/**
 * Clone a VM state.
 * 
 * @param state VM state to clone
 * @return Cloned state or NULL on failure
 */
chomsky3_vm_state_t *chomsky3_vm_state_clone(const chomsky3_vm_state_t *state);

/* Bytecode optimization */

/**
 * Optimize bytecode.
 * 
 * @param bytecode Input bytecode
 * @param level Optimization level (0-3)
 * @param optimized Output optimized bytecode (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bytecode_optimize(
    const chomsky3_bytecode_t *bytecode,
    int level,
    chomsky3_bytecode_t **optimized
);

/**
 * Verify bytecode correctness.
 * 
 * @param bytecode Bytecode to verify
 * @param errors Output error messages (optional)
 * @return true if correct, false otherwise
 */
bool chomsky3_bytecode_verify(
    const chomsky3_bytecode_t *bytecode,
    char **errors
);

/**
 * Get bytecode complexity estimate.
 * 
 * @param bytecode Bytecode
 * @return Complexity score (higher = more complex)
 */
uint32_t chomsky3_bytecode_complexity(const chomsky3_bytecode_t *bytecode);

/**
 * Profile bytecode execution.
 * 
 * @param vm VM instance
 * @param input Input string
 * @param length Input length
 * @param profile Output profile data (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_profile(
    chomsky3_vm_t *vm,
    const char *input,
    size_t length,
    char **profile
);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_BYTECODE_H */
