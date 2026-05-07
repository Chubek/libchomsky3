/**
 * libchomsky3 - Extended Regular Expression Compiler and Runtime
 * 
 * Main header file providing the public API for compiling ERE patterns
 * into C code or bytecode, and executing them via JIT-compiled VM.
 */

#ifndef CHOMSKY3_H
#define CHOMSKY3_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define CHOMSKY3_VERSION_MAJOR 3
#define CHOMSKY3_VERSION_MINOR 0
#define CHOMSKY3_VERSION_PATCH 0

/* Forward declarations */
typedef struct chomsky3_context chomsky3_context_t;
typedef struct chomsky3_pattern chomsky3_pattern_t;
typedef struct chomsky3_match chomsky3_match_t;

/* Compilation flags */
typedef enum {
    CHOMSKY3_FLAG_NONE          = 0,
    CHOMSKY3_FLAG_CASE_INSENSITIVE = 1 << 0,
    CHOMSKY3_FLAG_MULTILINE     = 1 << 1,
    CHOMSKY3_FLAG_DOTALL        = 1 << 2,
    CHOMSKY3_FLAG_OPTIMIZE      = 1 << 3,
    CHOMSKY3_FLAG_DEBUG         = 1 << 4,
} chomsky3_flags_t;

/* Compilation target */
typedef enum {
    CHOMSKY3_TARGET_BYTECODE,   /* Compile to bytecode for VM */
    CHOMSKY3_TARGET_JIT,        /* Compile to JIT-compiled bytecode */
    CHOMSKY3_TARGET_C_SOURCE,   /* Generate C source code */
} chomsky3_target_t;

/* Error codes */
typedef enum {
    CHOMSKY3_OK = 0,
    CHOMSKY3_ERROR_INVALID_PATTERN,
    CHOMSKY3_ERROR_PARSE_FAILED,
    CHOMSKY3_ERROR_COMPILE_FAILED,
    CHOMSKY3_ERROR_OUT_OF_MEMORY,
    CHOMSKY3_ERROR_INVALID_ARGUMENT,
    CHOMSKY3_ERROR_JIT_UNAVAILABLE,
    CHOMSKY3_ERROR_SERIALIZATION_FAILED,
    CHOMSKY3_ERROR_DESERIALIZATION_FAILED,
} chomsky3_error_t;

/* Match result */
struct chomsky3_match {
    const char *start;          /* Start of match in input string */
    const char *end;            /* End of match in input string */
    size_t length;              /* Length of match */
    size_t num_groups;          /* Number of capture groups */
    struct {
        const char *start;
        const char *end;
        size_t length;
    } *groups;                  /* Capture group data */
};

/**
 * Initialize a new chomsky3 context.
 * 
 * @return New context or NULL on failure
 */
chomsky3_context_t *chomsky3_context_new(void);

/**
 * Free a chomsky3 context and all associated resources.
 * 
 * @param ctx Context to free
 */
void chomsky3_context_free(chomsky3_context_t *ctx);

/**
 * Compile an ERE pattern.
 * 
 * @param ctx Context
 * @param pattern ERE pattern string
 * @param target Compilation target
 * @param flags Compilation flags
 * @param error Output error code (optional)
 * @return Compiled pattern or NULL on failure
 */
chomsky3_pattern_t *chomsky3_compile(
    chomsky3_context_t *ctx,
    const char *pattern,
    chomsky3_target_t target,
    chomsky3_flags_t flags,
    chomsky3_error_t *error
);

/**
 * Free a compiled pattern.
 * 
 * @param pattern Pattern to free
 */
void chomsky3_pattern_free(chomsky3_pattern_t *pattern);

/**
 * Execute a compiled pattern against input text.
 * 
 * @param pattern Compiled pattern
 * @param input Input string
 * @param length Length of input string
 * @param match Output match result (optional)
 * @return true if match found, false otherwise
 */
bool chomsky3_exec(
    chomsky3_pattern_t *pattern,
    const char *input,
    size_t length,
    chomsky3_match_t *match
);

/**
 * Free match result resources.
 * 
 * @param match Match result to free
 */
void chomsky3_match_free(chomsky3_match_t *match);

/**
 * Generate C source code from a compiled pattern.
 * 
 * @param pattern Compiled pattern
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return CHOMSKY3_OK on success, error code otherwise
 */
chomsky3_error_t chomsky3_generate_c(
    chomsky3_pattern_t *pattern,
    char *output,
    size_t output_size
);

/**
 * Serialize a compiled pattern to ERE-Bincode format.
 * 
 * @param pattern Compiled pattern
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @param bytes_written Number of bytes written (output)
 * @return CHOMSKY3_OK on success, error code otherwise
 */
chomsky3_error_t chomsky3_serialize(
    chomsky3_pattern_t *pattern,
    uint8_t *output,
    size_t output_size,
    size_t *bytes_written
);

/**
 * Deserialize a pattern from ERE-Bincode format.
 * 
 * @param ctx Context
 * @param input Input buffer containing bincode
 * @param input_size Size of input buffer
 * @param error Output error code (optional)
 * @return Deserialized pattern or NULL on failure
 */
chomsky3_pattern_t *chomsky3_deserialize(
    chomsky3_context_t *ctx,
    const uint8_t *input,
    size_t input_size,
    chomsky3_error_t *error
);

/**
 * Get human-readable error message.
 * 
 * @param error Error code
 * @return Error message string
 */
const char *chomsky3_error_string(chomsky3_error_t error);

/**
 * Get library version string.
 * 
 * @return Version string (e.g., "3.0.0")
 */
const char *chomsky3_version(void);

/**
 * Check if JIT compilation is available.
 * 
 * @return true if JIT is available, false otherwise
 */
bool chomsky3_jit_available(void);

/**
 * @brief Get the Unicode character name for a codepoint
 * @param codepoint Unicode codepoint (U+0000 to U+10FFFF)
 * @return Character name string, or NULL if not found
 * 
 * Example: chomsky3_unicode_name(0x0041) returns "LATIN CAPITAL LETTER A"
 */
const char* chomsky3_unicode_name(uint32_t codepoint);

/**
 * @brief Get the codepoint for a Unicode character name
 * @param name Official Unicode character name
 * @return Codepoint value, or 0xFFFFFFFF if not found
 * 
 * Example: chomsky3_unicode_codepoint("SPACE") returns 0x0020
 */
CHOMSKY3_API uint32_t chomsky3_unicode_codepoint(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_H */
