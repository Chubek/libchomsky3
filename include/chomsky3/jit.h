/**
 * libchomsky3 - JIT Compilation Interface
 * 
 * Header file providing the Just-In-Time compilation interface for
 * compiling bytecode to native machine code using SLJIT backend.
 * Enables high-performance regex execution on supported architectures.
 */

#ifndef CHOMSKY3_JIT_H
#define CHOMSKY3_JIT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_jit_compiler chomsky3_jit_compiler_t;
typedef struct chomsky3_jit_code chomsky3_jit_code_t;
typedef struct chomsky3_jit_config chomsky3_jit_config_t;
typedef struct chomsky3_jit_stats chomsky3_jit_stats_t;

/* Target architecture */
typedef enum {
    CHOMSKY3_ARCH_UNKNOWN = 0,
    CHOMSKY3_ARCH_X86 = 1,          /* x86 32-bit */
    CHOMSKY3_ARCH_X86_64 = 2,       /* x86-64 */
    CHOMSKY3_ARCH_ARM = 3,          /* ARM 32-bit */
    CHOMSKY3_ARCH_ARM64 = 4,        /* ARM 64-bit (AArch64) */
    CHOMSKY3_ARCH_ARM_THUMB2 = 5,   /* ARM Thumb-2 */
    CHOMSKY3_ARCH_MIPS = 6,         /* MIPS 32-bit */
    CHOMSKY3_ARCH_MIPS64 = 7,       /* MIPS 64-bit */
    CHOMSKY3_ARCH_PPC = 8,          /* PowerPC 32-bit */
    CHOMSKY3_ARCH_PPC64 = 9,        /* PowerPC 64-bit */
    CHOMSKY3_ARCH_SPARC = 10,       /* SPARC 32-bit */
    CHOMSKY3_ARCH_S390X = 11,       /* IBM S390x */
    CHOMSKY3_ARCH_RISCV = 12,       /* RISC-V 32-bit */
    CHOMSKY3_ARCH_RISCV64 = 13      /* RISC-V 64-bit */
} chomsky3_arch_t;

/* JIT compilation flags */
typedef enum {
    CHOMSKY3_JIT_FLAG_NONE = 0,
    CHOMSKY3_JIT_FLAG_OPTIMIZE = (1 << 0),      /* Enable optimizations */
    CHOMSKY3_JIT_FLAG_FAST_COMPILE = (1 << 1),  /* Fast compilation, less optimization */
    CHOMSKY3_JIT_FLAG_DEBUG = (1 << 2),         /* Include debug information */
    CHOMSKY3_JIT_FLAG_PROFILE = (1 << 3),       /* Include profiling hooks */
    CHOMSKY3_JIT_FLAG_BOUNDS_CHECK = (1 << 4),  /* Enable bounds checking */
    CHOMSKY3_JIT_FLAG_STACK_CHECK = (1 << 5),   /* Enable stack overflow checks */
    CHOMSKY3_JIT_FLAG_INLINE = (1 << 6),        /* Aggressive inlining */
    CHOMSKY3_JIT_FLAG_UNROLL = (1 << 7),        /* Loop unrolling */
    CHOMSKY3_JIT_FLAG_VECTORIZE = (1 << 8),     /* SIMD vectorization */
    CHOMSKY3_JIT_FLAG_CACHE_ALIGN = (1 << 9),   /* Cache-aligned code */
    CHOMSKY3_JIT_FLAG_PIC = (1 << 10),          /* Position-independent code */
    CHOMSKY3_JIT_FLAG_THREAD_SAFE = (1 << 11)   /* Thread-safe code generation */
} chomsky3_jit_flags_t;

/* JIT optimization level */
typedef enum {
    CHOMSKY3_JIT_OPT_NONE = 0,      /* No optimization */
    CHOMSKY3_JIT_OPT_BASIC = 1,     /* Basic optimizations */
    CHOMSKY3_JIT_OPT_STANDARD = 2,  /* Standard optimizations */
    CHOMSKY3_JIT_OPT_AGGRESSIVE = 3 /* Aggressive optimizations */
} chomsky3_jit_opt_level_t;

/* JIT code cache policy */
typedef enum {
    CHOMSKY3_JIT_CACHE_NONE = 0,    /* No caching */
    CHOMSKY3_JIT_CACHE_MEMORY = 1,  /* In-memory cache */
    CHOMSKY3_JIT_CACHE_DISK = 2,    /* Disk-based cache */
    CHOMSKY3_JIT_CACHE_HYBRID = 3   /* Hybrid memory + disk */
} chomsky3_jit_cache_policy_t;

/* Native function signature for JIT-compiled code */
typedef int (*chomsky3_jit_func_t)(
    const char *input,              /* Input string */
    size_t length,                  /* Input length */
    size_t offset,                  /* Starting offset */
    size_t *match_start,            /* Output: match start */
    size_t *match_end,              /* Output: match end */
    size_t *captures,               /* Output: capture positions */
    size_t num_captures,            /* Number of capture slots */
    void *context                   /* Execution context */
);

/* JIT configuration */
struct chomsky3_jit_config {
    chomsky3_arch_t target_arch;    /* Target architecture */
    chomsky3_jit_flags_t flags;     /* Compilation flags */
    chomsky3_jit_opt_level_t opt_level; /* Optimization level */
    
    /* Code generation options */
    size_t max_code_size;           /* Maximum code size (0 = unlimited) */
    size_t stack_size;              /* Stack size for execution */
    bool use_scratch_registers;     /* Use scratch registers */
    bool preserve_flags;            /* Preserve CPU flags */
    
    /* Optimization options */
    int inline_threshold;           /* Inlining threshold */
    int unroll_factor;              /* Loop unroll factor */
    bool eliminate_dead_code;       /* Dead code elimination */
    bool constant_folding;          /* Constant folding */
    bool common_subexpr_elim;       /* Common subexpression elimination */
    
    /* Cache options */
    chomsky3_jit_cache_policy_t cache_policy;
    size_t cache_size;              /* Cache size in bytes */
    const char *cache_dir;          /* Cache directory (for disk cache) */
    
    /* Performance tuning */
    bool prefetch_enabled;          /* Enable prefetching */
    int branch_prediction_hint;     /* Branch prediction hint (-1, 0, 1) */
    bool align_loops;               /* Align loop entry points */
    
    /* Debugging */
    bool emit_comments;             /* Emit assembly comments */
    bool emit_line_info;            /* Emit line number info */
    const char *dump_asm_file;      /* Dump assembly to file (NULL = no dump) */
    
    /* Callbacks */
    void (*compile_callback)(const char *msg, void *user_data);
    void (*error_callback)(chomsky3_error_t err, void *user_data);
    void *user_data;                /* User data for callbacks */
};

/* JIT-compiled native code */
struct chomsky3_jit_code {
    chomsky3_jit_func_t func;       /* Native function pointer */
    void *code_ptr;                 /* Raw code pointer */
    size_t code_size;               /* Code size in bytes */
    
    chomsky3_arch_t arch;           /* Target architecture */
    uint32_t flags;                 /* Compilation flags */
    
    /* Metadata */
    uint64_t compile_time_ns;       /* Compilation time */
    size_t bytecode_size;           /* Original bytecode size */
    float speedup_factor;           /* Estimated speedup vs bytecode */
    
    /* Debug info */
    void *debug_info;               /* Debug information */
    size_t debug_info_size;         /* Debug info size */
    
    /* Cache info */
    char *cache_key;                /* Cache key (hash) */
    uint64_t cache_timestamp;       /* Cache timestamp */
    
    /* Reference counting */
    int ref_count;                  /* Reference count */
};

/* JIT compilation statistics */
struct chomsky3_jit_stats {
    uint64_t total_compilations;    /* Total compilations */
    uint64_t cache_hits;            /* Cache hits */
    uint64_t cache_misses;          /* Cache misses */
    
    uint64_t total_compile_time_ns; /* Total compilation time */
    uint64_t total_code_size;       /* Total code size generated */
    
    uint64_t total_executions;      /* Total executions */
    uint64_t total_exec_time_ns;    /* Total execution time */
    
    float avg_speedup;              /* Average speedup vs bytecode */
    float avg_compile_time_ms;      /* Average compile time */
    
    size_t peak_memory;             /* Peak memory usage */
    size_t current_memory;          /* Current memory usage */
};

/* JIT benchmark result */
typedef struct {
    uint64_t bytecode_time_ns;      /* Bytecode execution time */
    uint64_t jit_time_ns;           /* JIT execution time */
    float speedup;                  /* Speedup factor */
    
    uint64_t compile_time_ns;       /* JIT compilation time */
    size_t code_size;               /* Generated code size */
    
    size_t iterations;              /* Number of iterations */
    const char *input;              /* Test input */
    size_t input_length;            /* Input length */
} chomsky3_jit_benchmark_t;

/**
 * Get default JIT configuration.
 * 
 * @param config Configuration structure to fill
 */
void chomsky3_jit_config_default(chomsky3_jit_config_t *config);

/**
 * Detect current CPU architecture.
 * 
 * @return Detected architecture
 */
chomsky3_arch_t chomsky3_jit_detect_arch(void);

/**
 * Check if JIT is available for architecture.
 * 
 * @param arch Architecture to check
 * @return true if JIT is available, false otherwise
 */
bool chomsky3_jit_is_available(chomsky3_arch_t arch);

/**
 * Get architecture name.
 * 
 * @param arch Architecture
 * @return Architecture name string
 */
const char *chomsky3_jit_arch_name(chomsky3_arch_t arch);

/**
 * Create JIT compiler instance.
 * 
 * @param ctx Context
 * @param config JIT configuration (NULL for defaults)
 * @return New JIT compiler or NULL on failure
 */
chomsky3_jit_compiler_t *chomsky3_jit_compiler_create(
    chomsky3_context_t *ctx,
    const chomsky3_jit_config_t *config
);

/**
 * Destroy JIT compiler instance.
 * 
 * @param compiler JIT compiler to destroy
 */
void chomsky3_jit_compiler_destroy(chomsky3_jit_compiler_t *compiler);

/**
 * Compile bytecode to native code.
 * 
 * @param compiler JIT compiler
 * @param bytecode Bytecode to compile
 * @param code Output JIT code (must be freed with chomsky3_jit_code_free)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_compile(
    chomsky3_jit_compiler_t *compiler,
    const chomsky3_bytecode_t *bytecode,
    chomsky3_jit_code_t **code
);

/**
 * Compile bytecode with explicit options.
 * 
 * @param compiler JIT compiler
 * @param bytecode Bytecode to compile
 * @param flags Compilation flags
 * @param opt_level Optimization level
 * @param code Output JIT code
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_compile_ex(
    chomsky3_jit_compiler_t *compiler,
    const chomsky3_bytecode_t *bytecode,
    chomsky3_jit_flags_t flags,
    chomsky3_jit_opt_level_t opt_level,
    chomsky3_jit_code_t **code
);

/**
 * Execute JIT-compiled code.
 * 
 * @param code JIT code
 * @param input Input string
 * @param length Input length
 * @param offset Starting offset
 * @param match Output match result (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_execute(
    const chomsky3_jit_code_t *code,
    const char *input,
    size_t length,
    size_t offset,
    chomsky3_match_t **match
);

/**
 * Execute JIT code with capture groups.
 * 
 * @param code JIT code
 * @param input Input string
 * @param length Input length
 * @param offset Starting offset
 * @param captures Output capture positions (array of start/end pairs)
 * @param num_captures Number of capture slots
 * @param match Output match result
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_execute_captures(
    const chomsky3_jit_code_t *code,
    const char *input,
    size_t length,
    size_t offset,
    size_t *captures,
    size_t num_captures,
    chomsky3_match_t **match
);

/**
 * Free JIT-compiled code.
 * 
 * @param code JIT code to free
 */
void chomsky3_jit_code_free(chomsky3_jit_code_t *code);

/**
 * Increment reference count.
 * 
 * @param code JIT code
 */
void chomsky3_jit_code_ref(chomsky3_jit_code_t *code);

/**
 * Decrement reference count and free if zero.
 * 
 * @param code JIT code
 */
void chomsky3_jit_code_unref(chomsky3_jit_code_t *code);

/* Code cache management */

/**
 * Enable code caching.
 * 
 * @param compiler JIT compiler
 * @param policy Cache policy
 * @param cache_size Cache size in bytes
 * @param cache_dir Cache directory (for disk cache, NULL for default)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_cache_enable(
    chomsky3_jit_compiler_t *compiler,
    chomsky3_jit_cache_policy_t policy,
    size_t cache_size,
    const char *cache_dir
);

/**
 * Disable code caching.
 * 
 * @param compiler JIT compiler
 */
void chomsky3_jit_cache_disable(chomsky3_jit_compiler_t *compiler);

/**
 * Clear code cache.
 * 
 * @param compiler JIT compiler
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_cache_clear(chomsky3_jit_compiler_t *compiler);

/**
 * Get cached code by key.
 * 
 * @param compiler JIT compiler
 * @param cache_key Cache key
 * @param code Output cached code (NULL if not found)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_cache_get(
    chomsky3_jit_compiler_t *compiler,
    const char *cache_key,
    chomsky3_jit_code_t **code
);

/**
 * Store code in cache.
 * 
 * @param compiler JIT compiler
 * @param cache_key Cache key
 * @param code Code to cache
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_cache_put(
    chomsky3_jit_compiler_t *compiler,
    const char *cache_key,
    const chomsky3_jit_code_t *code
);

/**
 * Compute cache key for bytecode.
 * 
 * @param bytecode Bytecode
 * @param flags Compilation flags
 * @param opt_level Optimization level
 * @param key Output key buffer (at least 65 bytes for SHA-256 hex)
 * @param key_size Key buffer size
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_cache_key(
    const chomsky3_bytecode_t *bytecode,
    chomsky3_jit_flags_t flags,
    chomsky3_jit_opt_level_t opt_level,
    char *key,
    size_t key_size
);

/* Statistics and profiling */

/**
 * Get JIT compiler statistics.
 * 
 * @param compiler JIT compiler
 * @param stats Output statistics structure
 */
void chomsky3_jit_get_stats(
    const chomsky3_jit_compiler_t *compiler,
    chomsky3_jit_stats_t *stats
);

/**
 * Reset JIT compiler statistics.
 * 
 * @param compiler JIT compiler
 */
void chomsky3_jit_reset_stats(chomsky3_jit_compiler_t *compiler);

/**
 * Benchmark JIT vs bytecode execution.
 * 
 * @param compiler JIT compiler
 * @param bytecode Bytecode to benchmark
 * @param input Test input
 * @param length Input length
 * @param iterations Number of iterations
 * @param result Output benchmark result
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_benchmark(
    chomsky3_jit_compiler_t *compiler,
    const chomsky3_bytecode_t *bytecode,
    const char *input,
    size_t length,
    size_t iterations,
    chomsky3_jit_benchmark_t *result
);

/* Disassembly and debugging */

/**
 * Disassemble JIT code to assembly string.
 * 
 * @param code JIT code
 * @param output Output assembly string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_disassemble(
    const chomsky3_jit_code_t *code,
    char **output
);

/**
 * Dump JIT code to file.
 * 
 * @param code JIT code
 * @param filename Output filename
 * @param format Format ("asm", "hex", "binary")
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_dump(
    const chomsky3_jit_code_t *code,
    const char *filename,
    const char *format
);

/**
 * Get JIT code metadata as string.
 * 
 * @param code JIT code
 * @param output Output string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_code_info(
    const chomsky3_jit_code_t *code,
    char **output
);

/* Utility functions */

/**
 * Check if JIT code is valid.
 * 
 * @param code JIT code
 * @return true if valid, false otherwise
 */
bool chomsky3_jit_code_is_valid(const chomsky3_jit_code_t *code);

/**
 * Get JIT code size.
 * 
 * @param code JIT code
 * @return Code size in bytes
 */
size_t chomsky3_jit_code_size(const chomsky3_jit_code_t *code);

/**
 * Get JIT code architecture.
 * 
 * @param code JIT code
 * @return Architecture
 */
chomsky3_arch_t chomsky3_jit_code_arch(const chomsky3_jit_code_t *code);

/**
 * Estimate JIT speedup for bytecode.
 * 
 * @param compiler JIT compiler
 * @param bytecode Bytecode
 * @return Estimated speedup factor (0.0 if cannot estimate)
 */
float chomsky3_jit_estimate_speedup(
    const chomsky3_jit_compiler_t *compiler,
    const chomsky3_bytecode_t *bytecode
);

/**
 * Check if bytecode is suitable for JIT compilation.
 * 
 * @param bytecode Bytecode
 * @return true if suitable, false otherwise
 */
bool chomsky3_jit_is_suitable(const chomsky3_bytecode_t *bytecode);

/**
 * Get JIT compiler version.
 * 
 * @return Version string
 */
const char *chomsky3_jit_version(void);

/**
 * Get SLJIT backend version.
 * 
 * @return SLJIT version string
 */
const char *chomsky3_jit_backend_version(void);

/**
 * Get supported architectures.
 * 
 * @param archs Output array of architectures
 * @param count Output number of architectures
 */
void chomsky3_jit_supported_archs(
    chomsky3_arch_t **archs,
    size_t *count
);

/**
 * Get CPU features for architecture.
 * 
 * @param arch Architecture
 * @param features Output features string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_cpu_features(
    chomsky3_arch_t arch,
    char **features
);

/**
 * Clone JIT code.
 * 
 * @param code JIT code to clone
 * @param cloned Output cloned code
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_code_clone(
    const chomsky3_jit_code_t *code,
    chomsky3_jit_code_t **cloned
);

/**
 * Serialize JIT code to buffer.
 * 
 * @param code JIT code
 * @param buffer Output buffer (allocated by function)
 * @param size Output buffer size
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_code_serialize(
    const chomsky3_jit_code_t *code,
    uint8_t **buffer,
    size_t *size
);

/**
 * Deserialize JIT code from buffer.
 * 
 * @param ctx Context
 * @param buffer Input buffer
 * @param size Buffer size
 * @param code Output JIT code
 * @return Error code
 */
chomsky3_error_t chomsky3_jit_code_deserialize(
    chomsky3_context_t *ctx,
    const uint8_t *buffer,
    size_t size,
    chomsky3_jit_code_t **code
);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_JIT_H */
