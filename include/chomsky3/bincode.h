/**
 * libchomsky3 - Binary Code Generation Interface
 * 
 * Header file providing the interface for generating native machine code
 * from compiled regular expressions using SLJIT (Stack-Less Just-In-Time compiler).
 */

#ifndef CHOMSKY3_BINCODE_H
#define CHOMSKY3_BINCODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_bincode chomsky3_bincode_t;
typedef struct chomsky3_jit_compiler chomsky3_jit_compiler_t;
typedef struct chomsky3_native_func chomsky3_native_func_t;

/* JIT compilation target architecture */
typedef enum {
    CHOMSKY3_ARCH_AUTO = 0,         /* Auto-detect */
    CHOMSKY3_ARCH_X86 = 1,          /* x86 32-bit */
    CHOMSKY3_ARCH_X86_64 = 2,       /* x86-64 */
    CHOMSKY3_ARCH_ARM = 3,          /* ARM 32-bit */
    CHOMSKY3_ARCH_ARM64 = 4,        /* ARM 64-bit (AArch64) */
    CHOMSKY3_ARCH_MIPS = 5,         /* MIPS */
    CHOMSKY3_ARCH_PPC = 6,          /* PowerPC */
    CHOMSKY3_ARCH_RISCV = 7         /* RISC-V */
} chomsky3_arch_t;

/* JIT compilation flags */
typedef enum {
    CHOMSKY3_JIT_FLAG_NONE = 0,
    CHOMSKY3_JIT_FLAG_FAST = (1 << 0),      /* Optimize for speed */
    CHOMSKY3_JIT_FLAG_SMALL = (1 << 1),     /* Optimize for size */
    CHOMSKY3_JIT_FLAG_DEBUG = (1 << 2),     /* Include debug info */
    CHOMSKY3_JIT_FLAG_PROFILE = (1 << 3),   /* Enable profiling */
    CHOMSKY3_JIT_FLAG_BOUNDS_CHECK = (1 << 4), /* Enable bounds checking */
    CHOMSKY3_JIT_FLAG_INLINE = (1 << 5),    /* Aggressive inlining */
    CHOMSKY3_JIT_FLAG_UNROLL = (1 << 6),    /* Loop unrolling */
    CHOMSKY3_JIT_FLAG_SIMD = (1 << 7)       /* Use SIMD instructions */
} chomsky3_jit_flags_t;

/* JIT optimization level */
typedef enum {
    CHOMSKY3_JIT_OPT_NONE = 0,      /* No optimization */
    CHOMSKY3_JIT_OPT_BASIC = 1,     /* Basic optimizations */
    CHOMSKY3_JIT_OPT_STANDARD = 2,  /* Standard optimizations */
    CHOMSKY3_JIT_OPT_AGGRESSIVE = 3 /* Aggressive optimizations */
} chomsky3_jit_opt_level_t;

/* JIT compilation options */
typedef struct {
    chomsky3_arch_t target_arch;    /* Target architecture */
    chomsky3_jit_flags_t flags;     /* Compilation flags */
    chomsky3_jit_opt_level_t opt_level; /* Optimization level */
    
    /* Code generation options */
    bool use_computed_goto;         /* Use computed goto (if supported) */
    bool use_tail_calls;            /* Use tail call optimization */
    bool emit_debug_symbols;        /* Emit debug symbols */
    
    /* Memory options */
    size_t code_cache_size;         /* Code cache size (0 = default) */
    size_t stack_size;              /* Stack size for execution */
    
    /* Performance tuning */
    int inline_threshold;           /* Inlining threshold */
    int unroll_factor;              /* Loop unroll factor */
} chomsky3_jit_options_t;

/* Native function signature */
typedef int (*chomsky3_native_match_fn)(
    const char *input,
    size_t length,
    size_t *match_start,
    size_t *match_end,
    size_t **captures,
    void *user_data
);

/* Native function structure */
struct chomsky3_native_func {
    chomsky3_native_match_fn func;  /* Function pointer */
    void *code;                     /* Executable code pointer */
    size_t code_size;               /* Code size in bytes */
    
    /* Metadata */
    chomsky3_arch_t arch;           /* Target architecture */
    uint32_t flags;                 /* Compilation flags */
    
    /* Statistics */
    uint64_t invocation_count;      /* Number of invocations */
    uint64_t total_time_ns;         /* Total execution time */
};

/* Binary code structure */
struct chomsky3_bincode {
    chomsky3_context_t *ctx;        /* Parent context */
    chomsky3_native_func_t *native; /* Native function */
    
    /* Source information */
    const chomsky3_bytecode_t *bytecode; /* Source bytecode */
    char *pattern_source;           /* Original pattern */
    
    /* Code sections */
    void *code_section;             /* Executable code */
    void *data_section;             /* Read-only data */
    void *reloc_section;            /* Relocation table */
    
    size_t code_size;               /* Code section size */
    size_t data_size;               /* Data section size */
    size_t reloc_size;              /* Relocation table size */
    
    /* Metadata */
    chomsky3_jit_options_t options; /* Compilation options */
    uint32_t num_captures;          /* Number of capture groups */
    
    /* Internal state */
    void *internal;                 /* Opaque internal state */
};

/* JIT compiler structure */
struct chomsky3_jit_compiler {
    chomsky3_context_t *ctx;        /* Parent context */
    chomsky3_jit_options_t options; /* Compilation options */
    
    /* SLJIT compiler state */
    void *sljit_compiler;           /* SLJIT compiler instance */
    
    /* Code generation state */
    void *label_table;              /* Label table */
    void *constant_pool;            /* Constant pool */
    
    /* Statistics */
    size_t instructions_emitted;    /* Number of instructions emitted */
    size_t labels_created;          /* Number of labels created */
    
    /* Internal state */
    void *internal;                 /* Opaque internal state */
};

/* JIT compilation statistics */
typedef struct {
    uint64_t compile_time_ns;       /* Compilation time */
    size_t code_size;               /* Generated code size */
    size_t data_size;               /* Data section size */
    size_t instructions_emitted;    /* Instructions emitted */
    size_t basic_blocks;            /* Number of basic blocks */
    size_t branches;                /* Number of branches */
    size_t memory_allocated;        /* Total memory allocated */
} chomsky3_jit_stats_t;

/**
 * Get default JIT options.
 * 
 * @param options Options structure to fill
 */
void chomsky3_jit_options_default(chomsky3_jit_options_t *options);

/**
 * Create a new JIT compiler instance.
 * 
 * @param ctx Context
 * @param options Compilation options (NULL for defaults)
 * @return New JIT compiler or NULL on failure
 */
chomsky3_jit_compiler_t *chomsky3_jit_compiler_new(
    chomsky3_context_t *ctx,
    const chomsky3_jit_options_t *options
);

/**
 * Free a JIT compiler instance.
 * 
 * @param compiler JIT compiler to free
 */
void chomsky3_jit_compiler_free(chomsky3_jit_compiler_t *compiler);

/**
 * Compile bytecode to native code.
 * 
 * @param compiler JIT compiler
 * @param bytecode Bytecode to compile
 * @param bincode Output binary code (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_compile(
    chomsky3_jit_compiler_t *compiler,
    const chomsky3_bytecode_t *bytecode,
    chomsky3_bincode_t **bincode
);

/**
 * Compile pattern directly to native code.
 * 
 * @param ctx Context
 * @param pattern Compiled pattern
 * @param options JIT options (NULL for defaults)
 * @param bincode Output binary code (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_from_pattern(
    chomsky3_context_t *ctx,
    const chomsky3_pattern_t *pattern,
    const chomsky3_jit_options_t *options,
    chomsky3_bincode_t **bincode
);

/**
 * Free binary code structure.
 * 
 * @param bincode Binary code to free
 */
void chomsky3_bincode_free(chomsky3_bincode_t *bincode);

/**
 * Execute native code on input string.
 * 
 * @param bincode Binary code
 * @param input Input string
 * @param length Input length
 * @param match Match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_execute(
    chomsky3_bincode_t *bincode,
    const char *input,
    size_t length,
    chomsky3_match_t **match
);

/**
 * Get native function pointer.
 * 
 * @param bincode Binary code
 * @return Native function pointer or NULL
 */
chomsky3_native_match_fn chomsky3_bincode_get_function(
    const chomsky3_bincode_t *bincode
);

/**
 * Check if JIT compilation is available.
 * 
 * @return true if JIT is available, false otherwise
 */
bool chomsky3_jit_available(void);

/**
 * Get supported architectures.
 * 
 * @param archs Output array of supported architectures
 * @param count Output count of architectures
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_supported_archs(
    chomsky3_arch_t **archs,
    size_t *count
);

/**
 * Get current architecture.
 * 
 * @return Current architecture
 */
chomsky3_arch_t chomsky3_jit_current_arch(void);

/**
 * Get architecture name.
 * 
 * @param arch Architecture
 * @return Architecture name string
 */
const char *chomsky3_arch_name(chomsky3_arch_t arch);

/**
 * Check if architecture supports SIMD.
 * 
 * @param arch Architecture
 * @return true if SIMD is supported, false otherwise
 */
bool chomsky3_arch_has_simd(chomsky3_arch_t arch);

/**
 * Get JIT compilation statistics.
 * 
 * @param compiler JIT compiler
 * @param stats Statistics structure to fill
 */
void chomsky3_jit_get_stats(
    const chomsky3_jit_compiler_t *compiler,
    chomsky3_jit_stats_t *stats
);

/**
 * Disassemble native code.
 * 
 * @param bincode Binary code
 * @param output Output string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_disassemble(
    const chomsky3_bincode_t *bincode,
    char **output
);

/**
 * Save binary code to file.
 * 
 * @param bincode Binary code
 * @param path File path
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_save(
    const chomsky3_bincode_t *bincode,
    const char *path
);

/**
 * Load binary code from file.
 * 
 * @param ctx Context
 * @param path File path
 * @param bincode Output binary code (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_load(
    chomsky3_context_t *ctx,
    const char *path,
    chomsky3_bincode_t **bincode
);

/**
 * Validate binary code integrity.
 * 
 * @param bincode Binary code
 * @return true if valid, false otherwise
 */
bool chomsky3_bincode_validate(const chomsky3_bincode_t *bincode);

/**
 * Get binary code size.
 * 
 * @param bincode Binary code
 * @return Total size in bytes
 */
size_t chomsky3_bincode_size(const chomsky3_bincode_t *bincode);

/**
 * Get binary code complexity estimate.
 * 
 * @param bincode Binary code
 * @return Complexity score
 */
uint32_t chomsky3_bincode_complexity(const chomsky3_bincode_t *bincode);

/* Code cache management */

/**
 * Create a code cache.
 * 
 * @param ctx Context
 * @param max_size Maximum cache size in bytes
 * @return Error code
 */
chomsky3_error_t chomsky3_code_cache_create(
    chomsky3_context_t *ctx,
    size_t max_size
);

/**
 * Clear code cache.
 * 
 * @param ctx Context
 */
void chomsky3_code_cache_clear(chomsky3_context_t *ctx);

/**
 * Get code cache statistics.
 * 
 * @param ctx Context
 * @param size Output current cache size
 * @param entries Output number of cached entries
 * @param hits Output cache hit count
 * @param misses Output cache miss count
 * @return Error code
 */
chomsky3_error_t chomsky3_code_cache_stats(
    chomsky3_context_t *ctx,
    size_t *size,
    size_t *entries,
    uint64_t *hits,
    uint64_t *misses
);

/* Performance profiling */

/**
 * Profile native code execution.
 * 
 * @param bincode Binary code
 * @param input Input string
 * @param length Input length
 * @param iterations Number of iterations
 * @param profile Output profile data (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_profile(
    chomsky3_bincode_t *bincode,
    const char *input,
    size_t length,
    size_t iterations,
    char **profile
);

/**
 * Benchmark native code vs bytecode.
 * 
 * @param bincode Binary code
 * @param bytecode Bytecode
 * @param input Input string
 * @param length Input length
 * @param iterations Number of iterations
 * @param speedup Output speedup factor
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_benchmark(
    chomsky3_bincode_t *bincode,
    const chomsky3_bytecode_t *bytecode,
    const char *input,
    size_t length,
    size_t iterations,
    double *speedup
);

/* Advanced features */

/**
 * Enable/disable code patching.
 * 
 * @param bincode Binary code
 * @param enable Enable flag
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_set_patchable(
    chomsky3_bincode_t *bincode,
    bool enable
);

/**
 * Clone binary code.
 * 
 * @param bincode Binary code to clone
 * @param cloned Output cloned binary code
 * @return Error code
 */
chomsky3_error_t chomsky3_bincode_clone(
    const chomsky3_bincode_t *bincode,
    chomsky3_bincode_t **cloned
);

/**
 * Get SLJIT version information.
 * 
 * @param major Output major version
 * @param minor Output minor version
 * @param patch Output patch version
 */
void chomsky3_sljit_version(int *major, int *minor, int *patch);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_BINCODE_H */
