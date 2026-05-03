/**
 * libchomsky3 - Virtual Machine Interface
 * 
 * Header file providing the high-level virtual machine interface for
 * executing compiled regular expressions. This wraps the lower-level
 * bytecode execution engine with a more convenient API.
 */

#ifndef CHOMSKY3_VM_H
#define CHOMSKY3_VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_vm_config chomsky3_vm_config_t;
typedef struct chomsky3_vm_state chomsky3_vm_state_t;
typedef struct chomsky3_vm_snapshot chomsky3_vm_snapshot_t;
typedef struct chomsky3_vm_trace chomsky3_vm_trace_t;

/* VM execution mode */
typedef enum {
    CHOMSKY3_VM_MODE_NORMAL = 0,    /* Normal execution */
    CHOMSKY3_VM_MODE_TRACE = 1,     /* Trace execution */
    CHOMSKY3_VM_MODE_DEBUG = 2,     /* Debug mode with breakpoints */
    CHOMSKY3_VM_MODE_PROFILE = 3,   /* Profiling mode */
    CHOMSKY3_VM_MODE_SAFE = 4       /* Safe mode with extra checks */
} chomsky3_vm_mode_t;

/* VM execution flags */
typedef enum {
    CHOMSKY3_VM_FLAG_NONE = 0,
    CHOMSKY3_VM_FLAG_ANCHORED = (1 << 0),      /* Match must start at position 0 */
    CHOMSKY3_VM_FLAG_NOTBOL = (1 << 1),        /* First char is not beginning of line */
    CHOMSKY3_VM_FLAG_NOTEOL = (1 << 2),        /* Last char is not end of line */
    CHOMSKY3_VM_FLAG_NOTEMPTY = (1 << 3),      /* Don't match empty string */
    CHOMSKY3_VM_FLAG_PARTIAL = (1 << 4),       /* Allow partial matches */
    CHOMSKY3_VM_FLAG_MULTILINE = (1 << 5),     /* Multiline mode */
    CHOMSKY3_VM_FLAG_DOTALL = (1 << 6),        /* Dot matches newline */
    CHOMSKY3_VM_FLAG_CASELESS = (1 << 7),      /* Case-insensitive matching */
    CHOMSKY3_VM_FLAG_UNGREEDY = (1 << 8),      /* Ungreedy quantifiers */
    CHOMSKY3_VM_FLAG_CAPTURE = (1 << 9),       /* Enable capture groups */
    CHOMSKY3_VM_FLAG_OPTIMIZE = (1 << 10)      /* Enable runtime optimizations */
} chomsky3_vm_flags_t;

/* VM resource limits */
typedef struct {
    size_t max_stack_depth;         /* Maximum stack depth */
    size_t max_backtrack_count;     /* Maximum backtrack operations */
    size_t max_steps;               /* Maximum execution steps */
    uint64_t max_time_ns;           /* Maximum execution time (nanoseconds) */
    size_t max_memory;              /* Maximum memory usage */
} chomsky3_vm_limits_t;

/* VM configuration */
struct chomsky3_vm_config {
    chomsky3_vm_mode_t mode;        /* Execution mode */
    chomsky3_vm_flags_t flags;      /* Execution flags */
    chomsky3_vm_limits_t limits;    /* Resource limits */
    
    /* Memory management */
    size_t stack_size;              /* Initial stack size */
    size_t capture_slots;           /* Number of capture slots */
    
    /* Performance tuning */
    bool use_jit;                   /* Use JIT if available */
    bool cache_enabled;             /* Enable result caching */
    int optimization_level;         /* Optimization level (0-3) */
    
    /* Debugging */
    bool collect_stats;             /* Collect execution statistics */
    bool trace_enabled;             /* Enable execution tracing */
    
    /* Callbacks */
    void (*trace_callback)(const char *msg, void *user_data);
    void (*error_callback)(chomsky3_error_t err, void *user_data);
    void *user_data;                /* User data for callbacks */
};

/* VM execution state */
struct chomsky3_vm_state {
    /* Current position */
    const char *input;              /* Input string */
    size_t input_length;            /* Input length */
    size_t position;                /* Current position */
    
    /* Match state */
    size_t match_start;             /* Match start position */
    size_t match_end;               /* Match end position */
    bool matched;                   /* Match found flag */
    
    /* Capture groups */
    size_t *capture_starts;         /* Capture start positions */
    size_t *capture_ends;           /* Capture end positions */
    size_t num_captures;            /* Number of captures */
    
    /* Execution state */
    size_t pc;                      /* Program counter */
    size_t stack_depth;             /* Current stack depth */
    size_t backtrack_count;         /* Backtrack count */
    size_t step_count;              /* Step count */
    
    /* Flags */
    bool partial_match;             /* Partial match flag */
    bool timeout;                   /* Timeout flag */
    bool error;                     /* Error flag */
};

/* VM execution statistics */
typedef struct {
    uint64_t total_steps;           /* Total steps executed */
    uint64_t total_backtracks;      /* Total backtracks */
    uint64_t max_stack_depth;       /* Maximum stack depth reached */
    uint64_t execution_time_ns;     /* Execution time */
    
    /* Instruction counts */
    uint64_t *opcode_counts;        /* Count per opcode */
    size_t num_opcodes;             /* Number of opcodes */
    
    /* Memory usage */
    size_t peak_memory;             /* Peak memory usage */
    size_t current_memory;          /* Current memory usage */
    
    /* Cache statistics */
    uint64_t cache_hits;            /* Cache hits */
    uint64_t cache_misses;          /* Cache misses */
} chomsky3_vm_stats_t;

/* VM snapshot for state save/restore */
struct chomsky3_vm_snapshot {
    chomsky3_vm_state_t state;      /* VM state */
    void *stack_data;               /* Stack data copy */
    size_t stack_size;              /* Stack size */
    uint64_t timestamp;             /* Snapshot timestamp */
};

/* VM trace entry */
typedef struct {
    size_t pc;                      /* Program counter */
    size_t position;                /* Input position */
    uint8_t opcode;                 /* Opcode */
    const char *opcode_name;        /* Opcode name */
    char *description;              /* Human-readable description */
    uint64_t timestamp_ns;          /* Timestamp */
} chomsky3_vm_trace_entry_t;

/* VM trace */
struct chomsky3_vm_trace {
    chomsky3_vm_trace_entry_t *entries; /* Trace entries */
    size_t count;                   /* Number of entries */
    size_t capacity;                /* Capacity */
};

/**
 * Get default VM configuration.
 * 
 * @param config Configuration structure to fill
 */
void chomsky3_vm_config_default(chomsky3_vm_config_t *config);

/**
 * Get default VM limits.
 * 
 * @param limits Limits structure to fill
 */
void chomsky3_vm_limits_default(chomsky3_vm_limits_t *limits);

/**
 * Create a new VM instance.
 * 
 * @param ctx Context
 * @param bytecode Bytecode to execute
 * @param config VM configuration (NULL for defaults)
 * @return New VM instance or NULL on failure
 */
chomsky3_vm_t *chomsky3_vm_create(
    chomsky3_context_t *ctx,
    const chomsky3_bytecode_t *bytecode,
    const chomsky3_vm_config_t *config
);

/**
 * Destroy a VM instance.
 * 
 * @param vm VM instance to destroy
 */
void chomsky3_vm_destroy(chomsky3_vm_t *vm);

/**
 * Reset VM to initial state.
 * 
 * @param vm VM instance
 */
void chomsky3_vm_reset(chomsky3_vm_t *vm);

/**
 * Execute VM on input string.
 * 
 * @param vm VM instance
 * @param input Input string
 * @param length Input length
 * @param offset Starting offset
 * @param match Output match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_exec(
    chomsky3_vm_t *vm,
    const char *input,
    size_t length,
    size_t offset,
    chomsky3_match_t **match
);

/**
 * Execute VM with explicit flags.
 * 
 * @param vm VM instance
 * @param input Input string
 * @param length Input length
 * @param offset Starting offset
 * @param flags Execution flags
 * @param match Output match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_exec_flags(
    chomsky3_vm_t *vm,
    const char *input,
    size_t length,
    size_t offset,
    chomsky3_vm_flags_t flags,
    chomsky3_match_t **match
);

/**
 * Execute single step (for debugging).
 * 
 * @param vm VM instance
 * @param state Output current state
 * @return Error code (CHOMSKY3_ERROR_NONE if more steps remain)
 */
chomsky3_error_t chomsky3_vm_step(
    chomsky3_vm_t *vm,
    chomsky3_vm_state_t *state
);

/**
 * Continue execution from current state.
 * 
 * @param vm VM instance
 * @param match Output match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_continue(
    chomsky3_vm_t *vm,
    chomsky3_match_t **match
);

/**
 * Get current VM state.
 * 
 * @param vm VM instance
 * @param state Output state structure
 */
void chomsky3_vm_get_state(
    const chomsky3_vm_t *vm,
    chomsky3_vm_state_t *state
);

/**
 * Set VM state (for resumption).
 * 
 * @param vm VM instance
 * @param state State to restore
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_set_state(
    chomsky3_vm_t *vm,
    const chomsky3_vm_state_t *state
);

/**
 * Create a snapshot of current VM state.
 * 
 * @param vm VM instance
 * @param snapshot Output snapshot (must be freed with chomsky3_vm_snapshot_free)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_snapshot_create(
    const chomsky3_vm_t *vm,
    chomsky3_vm_snapshot_t **snapshot
);

/**
 * Restore VM from snapshot.
 * 
 * @param vm VM instance
 * @param snapshot Snapshot to restore
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_snapshot_restore(
    chomsky3_vm_t *vm,
    const chomsky3_vm_snapshot_t *snapshot
);

/**
 * Free a snapshot.
 * 
 * @param snapshot Snapshot to free
 */
void chomsky3_vm_snapshot_free(chomsky3_vm_snapshot_t *snapshot);

/**
 * Get VM execution statistics.
 * 
 * @param vm VM instance
 * @param stats Output statistics structure
 */
void chomsky3_vm_get_stats(
    const chomsky3_vm_t *vm,
    chomsky3_vm_stats_t *stats
);

/**
 * Reset VM statistics.
 * 
 * @param vm VM instance
 */
void chomsky3_vm_reset_stats(chomsky3_vm_t *vm);

/**
 * Get VM execution trace.
 * 
 * @param vm VM instance
 * @param trace Output trace (must be freed with chomsky3_vm_trace_free)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_get_trace(
    const chomsky3_vm_t *vm,
    chomsky3_vm_trace_t **trace
);

/**
 * Free a trace.
 * 
 * @param trace Trace to free
 */
void chomsky3_vm_trace_free(chomsky3_vm_trace_t *trace);

/**
 * Format trace as string.
 * 
 * @param trace Trace
 * @param output Output string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_trace_format(
    const chomsky3_vm_trace_t *trace,
    char **output
);

/* Breakpoint management */

/**
 * Set breakpoint at program counter.
 * 
 * @param vm VM instance
 * @param pc Program counter
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_breakpoint_set(
    chomsky3_vm_t *vm,
    size_t pc
);

/**
 * Clear breakpoint at program counter.
 * 
 * @param vm VM instance
 * @param pc Program counter
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_breakpoint_clear(
    chomsky3_vm_t *vm,
    size_t pc
);

/**
 * Clear all breakpoints.
 * 
 * @param vm VM instance
 */
void chomsky3_vm_breakpoint_clear_all(chomsky3_vm_t *vm);

/**
 * Check if breakpoint is set.
 * 
 * @param vm VM instance
 * @param pc Program counter
 * @return true if breakpoint is set, false otherwise
 */
bool chomsky3_vm_breakpoint_is_set(
    const chomsky3_vm_t *vm,
    size_t pc
);

/* Watchpoint management */

/**
 * Set watchpoint on capture group.
 * 
 * @param vm VM instance
 * @param capture_index Capture group index
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_watchpoint_set(
    chomsky3_vm_t *vm,
    size_t capture_index
);

/**
 * Clear watchpoint on capture group.
 * 
 * @param vm VM instance
 * @param capture_index Capture group index
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_watchpoint_clear(
    chomsky3_vm_t *vm,
    size_t capture_index
);

/**
 * Clear all watchpoints.
 * 
 * @param vm VM instance
 */
void chomsky3_vm_watchpoint_clear_all(chomsky3_vm_t *vm);

/* Configuration management */

/**
 * Get VM configuration.
 * 
 * @param vm VM instance
 * @param config Output configuration structure
 */
void chomsky3_vm_get_config(
    const chomsky3_vm_t *vm,
    chomsky3_vm_config_t *config
);

/**
 * Update VM configuration.
 * 
 * @param vm VM instance
 * @param config New configuration
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_set_config(
    chomsky3_vm_t *vm,
    const chomsky3_vm_config_t *config
);

/**
 * Get VM limits.
 * 
 * @param vm VM instance
 * @param limits Output limits structure
 */
void chomsky3_vm_get_limits(
    const chomsky3_vm_t *vm,
    chomsky3_vm_limits_t *limits
);

/**
 * Set VM limits.
 * 
 * @param vm VM instance
 * @param limits New limits
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_set_limits(
    chomsky3_vm_t *vm,
    const chomsky3_vm_limits_t *limits
);

/* Utility functions */

/**
 * Check if VM is in valid state.
 * 
 * @param vm VM instance
 * @return true if valid, false otherwise
 */
bool chomsky3_vm_is_valid(const chomsky3_vm_t *vm);

/**
 * Check if VM has matched.
 * 
 * @param vm VM instance
 * @return true if matched, false otherwise
 */
bool chomsky3_vm_has_matched(const chomsky3_vm_t *vm);

/**
 * Check if VM is at end of input.
 * 
 * @param vm VM instance
 * @return true if at end, false otherwise
 */
bool chomsky3_vm_at_end(const chomsky3_vm_t *vm);

/**
 * Get current input position.
 * 
 * @param vm VM instance
 * @return Current position
 */
size_t chomsky3_vm_get_position(const chomsky3_vm_t *vm);

/**
 * Get program counter.
 * 
 * @param vm VM instance
 * @return Program counter
 */
size_t chomsky3_vm_get_pc(const chomsky3_vm_t *vm);

/**
 * Get stack depth.
 * 
 * @param vm VM instance
 * @return Current stack depth
 */
size_t chomsky3_vm_get_stack_depth(const chomsky3_vm_t *vm);

/**
 * Get bytecode reference.
 * 
 * @param vm VM instance
 * @return Bytecode reference
 */
const chomsky3_bytecode_t *chomsky3_vm_get_bytecode(const chomsky3_vm_t *vm);

/**
 * Dump VM state to string.
 * 
 * @param vm VM instance
 * @param output Output string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_dump_state(
    const chomsky3_vm_t *vm,
    char **output
);

/**
 * Get VM memory usage.
 * 
 * @param vm VM instance
 * @return Memory usage in bytes
 */
size_t chomsky3_vm_memory_usage(const chomsky3_vm_t *vm);

/**
 * Optimize VM for specific input pattern.
 * 
 * @param vm VM instance
 * @param sample_input Sample input for optimization
 * @param sample_length Sample length
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_optimize(
    chomsky3_vm_t *vm,
    const char *sample_input,
    size_t sample_length
);

/**
 * Clone VM instance.
 * 
 * @param vm VM instance to clone
 * @param cloned Output cloned VM
 * @return Error code
 */
chomsky3_error_t chomsky3_vm_clone(
    const chomsky3_vm_t *vm,
    chomsky3_vm_t **cloned
);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_VM_H */
