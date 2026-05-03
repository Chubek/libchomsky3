/**
 * libchomsky3 - Code Generation Interface
 * 
 * Header file providing the code generation interface for transforming
 * optimized IR into executable bytecode, JIT code, or C source.
 */

#ifndef CHOMSKY3_CODEGEN_H
#define CHOMSKY3_CODEGEN_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_codegen chomsky3_codegen_t;
typedef struct chomsky3_ir chomsky3_ir_t;
typedef struct chomsky3_code_buffer chomsky3_code_buffer_t;

/* Code generation target types */
typedef enum {
    CHOMSKY3_CODEGEN_BYTECODE = 0,  /* Generate bytecode for interpreter */
    CHOMSKY3_CODEGEN_JIT = 1,       /* Generate native code via JIT */
    CHOMSKY3_CODEGEN_C = 2          /* Generate C source code */
} chomsky3_codegen_target_t;

/* Code buffer for generated output */
struct chomsky3_code_buffer {
    uint8_t *data;              /* Buffer data */
    size_t size;                /* Current size */
    size_t capacity;            /* Allocated capacity */
    bool owns_data;             /* Whether buffer owns the data */
};

/* Code generation statistics */
typedef struct {
    size_t instructions_generated; /* Number of instructions/ops generated */
    size_t code_size;           /* Size of generated code in bytes */
    size_t data_size;           /* Size of data section in bytes */
    size_t total_size;          /* Total size (code + data + metadata) */
    size_t num_labels;          /* Number of labels/jump targets */
    size_t num_relocations;     /* Number of relocations (JIT only) */
    double codegen_time_ms;     /* Code generation time in milliseconds */
} chomsky3_codegen_stats_t;

/* Code generation options */
typedef struct {
    chomsky3_codegen_target_t target; /* Target type */
    
    /* Bytecode options */
    struct {
        bool emit_debug_info;   /* Emit debug information */
        bool compress;          /* Compress bytecode */
        uint8_t version;        /* Bytecode format version */
    } bytecode;
    
    /* JIT options */
    struct {
        bool emit_debug_symbols; /* Emit debug symbols */
        bool enable_profiling;  /* Enable profiling hooks */
        size_t stack_size;      /* Stack size for JIT code */
        bool use_register_allocation; /* Use register allocator */
    } jit;
    
    /* C source options */
    struct {
        const char *function_name; /* Generated function name */
        bool emit_comments;     /* Emit comments */
        bool emit_line_directives; /* Emit #line directives */
        bool static_function;   /* Make function static */
        const char *header_guard; /* Header guard name (NULL = auto) */
    } c_source;
    
    /* General options */
    bool optimize_jumps;        /* Optimize jump instructions */
    bool optimize_size;         /* Optimize for size over speed */
    size_t alignment;           /* Code alignment (0 = default) */
} chomsky3_codegen_options_t;

/* Code generator context */
struct chomsky3_codegen {
    chomsky3_context_t *ctx;    /* Parent context */
    chomsky3_codegen_options_t options; /* Code generation options */
    chomsky3_codegen_stats_t stats; /* Code generation statistics */
    chomsky3_code_buffer_t *buffer; /* Output buffer */
    
    /* Internal state */
    void *internal;             /* Opaque internal state */
};

/**
 * Create a new code generator instance.
 * 
 * @param ctx Parent context
 * @param options Code generation options (NULL for defaults)
 * @return New code generator or NULL on failure
 */
chomsky3_codegen_t *chomsky3_codegen_new(
    chomsky3_context_t *ctx,
    const chomsky3_codegen_options_t *options
);

/**
 * Free a code generator instance.
 * 
 * @param codegen Code generator to free
 */
void chomsky3_codegen_free(chomsky3_codegen_t *codegen);

/**
 * Get default code generation options.
 * 
 * @param options Options structure to fill
 * @param target Target type
 */
void chomsky3_codegen_options_default(
    chomsky3_codegen_options_t *options,
    chomsky3_codegen_target_t target
);

/**
 * Generate code from IR.
 * 
 * @param codegen Code generator instance
 * @param ir Intermediate representation
 * @param buffer Output buffer (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_generate(
    chomsky3_codegen_t *codegen,
    const chomsky3_ir_t *ir,
    chomsky3_code_buffer_t **buffer
);

/**
 * Generate code and create a pattern directly.
 * 
 * @param codegen Code generator instance
 * @param ir Intermediate representation
 * @param pattern Output pattern (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_generate_pattern(
    chomsky3_codegen_t *codegen,
    const chomsky3_ir_t *ir,
    chomsky3_pattern_t **pattern
);

/**
 * Get code generation statistics.
 * 
 * @param codegen Code generator instance
 * @param stats Output statistics structure
 */
void chomsky3_codegen_get_stats(
    const chomsky3_codegen_t *codegen,
    chomsky3_codegen_stats_t *stats
);

/**
 * Reset code generator state for reuse.
 * 
 * @param codegen Code generator instance
 */
void chomsky3_codegen_reset(chomsky3_codegen_t *codegen);

/**
 * Validate code generation options.
 * 
 * @param options Options to validate
 * @return true if valid, false otherwise
 */
bool chomsky3_codegen_options_validate(const chomsky3_codegen_options_t *options);

/* Code buffer management */

/**
 * Create a new code buffer.
 * 
 * @param initial_capacity Initial capacity in bytes
 * @return New buffer or NULL on failure
 */
chomsky3_code_buffer_t *chomsky3_code_buffer_new(size_t initial_capacity);

/**
 * Create a code buffer from existing data.
 * 
 * @param data Existing data
 * @param size Data size
 * @param owns_data Whether buffer should take ownership
 * @return New buffer or NULL on failure
 */
chomsky3_code_buffer_t *chomsky3_code_buffer_from_data(
    uint8_t *data,
    size_t size,
    bool owns_data
);

/**
 * Free a code buffer.
 * 
 * @param buffer Buffer to free
 */
void chomsky3_code_buffer_free(chomsky3_code_buffer_t *buffer);

/**
 * Append data to a code buffer.
 * 
 * @param buffer Buffer to append to
 * @param data Data to append
 * @param size Size of data
 * @return true on success, false on failure
 */
bool chomsky3_code_buffer_append(
    chomsky3_code_buffer_t *buffer,
    const void *data,
    size_t size
);

/**
 * Reserve space in a code buffer.
 * 
 * @param buffer Buffer to reserve in
 * @param size Size to reserve
 * @return true on success, false on failure
 */
bool chomsky3_code_buffer_reserve(
    chomsky3_code_buffer_t *buffer,
    size_t size
);

/**
 * Clear a code buffer.
 * 
 * @param buffer Buffer to clear
 */
void chomsky3_code_buffer_clear(chomsky3_code_buffer_t *buffer);

/**
 * Get a pointer to the buffer data.
 * 
 * @param buffer Buffer
 * @return Pointer to data
 */
const uint8_t *chomsky3_code_buffer_data(const chomsky3_code_buffer_t *buffer);

/**
 * Get the size of the buffer.
 * 
 * @param buffer Buffer
 * @return Size in bytes
 */
size_t chomsky3_code_buffer_size(const chomsky3_code_buffer_t *buffer);

/**
 * Detach data from buffer (caller takes ownership).
 * 
 * @param buffer Buffer
 * @param size Output size
 * @return Detached data (must be freed by caller)
 */
uint8_t *chomsky3_code_buffer_detach(
    chomsky3_code_buffer_t *buffer,
    size_t *size
);

/* Target-specific code generation */

/**
 * Generate bytecode from IR.
 * 
 * @param codegen Code generator instance
 * @param ir Intermediate representation
 * @param buffer Output buffer (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_bytecode(
    chomsky3_codegen_t *codegen,
    const chomsky3_ir_t *ir,
    chomsky3_code_buffer_t **buffer
);

/**
 * Generate JIT code from IR.
 * 
 * @param codegen Code generator instance
 * @param ir Intermediate representation
 * @param buffer Output buffer (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_jit(
    chomsky3_codegen_t *codegen,
    const chomsky3_ir_t *ir,
    chomsky3_code_buffer_t **buffer
);

/**
 * Generate C source from IR.
 * 
 * @param codegen Code generator instance
 * @param ir Intermediate representation
 * @param buffer Output buffer (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_c(
    chomsky3_codegen_t *codegen,
    const chomsky3_ir_t *ir,
    chomsky3_code_buffer_t **buffer
);

/**
 * Disassemble bytecode to human-readable format.
 * 
 * @param bytecode Bytecode buffer
 * @param size Bytecode size
 * @param output Output buffer for disassembly
 * @return Error code
 */
chomsky3_error_t chomsky3_disassemble_bytecode(
    const uint8_t *bytecode,
    size_t size,
    chomsky3_code_buffer_t **output
);

/**
 * Verify bytecode integrity.
 * 
 * @param bytecode Bytecode buffer
 * @param size Bytecode size
 * @return true if valid, false otherwise
 */
bool chomsky3_verify_bytecode(
    const uint8_t *bytecode,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_CODEGEN_H */
