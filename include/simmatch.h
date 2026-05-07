/**
 * libchomsky3 - Simulation and Matching Interface
 * 
 * Abstract interface for simulating and matching automata against input strings.
 * Provides both bytecode-based and C-based implementations for flexibility.
 */

#ifndef CHOMSKY3_SIMMATCH_H
#define CHOMSKY3_SIMMATCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_match_result chomsky3_match_result_t;
typedef struct chomsky3_match_context chomsky3_match_context_t;
typedef struct chomsky3_bytecode chomsky3_bytecode_t;

/* Match result structure */
struct chomsky3_match_result {
    bool matched;                   /* Whether a match was found */
    size_t start;                   /* Start position of match */
    size_t end;                     /* End position of match (exclusive) */
    size_t *capture_starts;         /* Capture group start positions */
    size_t *capture_ends;           /* Capture group end positions */
    size_t num_captures;            /* Number of capture groups */
    
    /* Statistics */
    uint64_t steps;                 /* Number of execution steps */
    uint64_t backtracks;            /* Number of backtracks */
};

/* Match context for stateful matching */
struct chomsky3_match_context {
    const uint8_t *input;           /* Input string (null-terminated) */
    size_t input_length;            /* Length of input (excluding null) */
    size_t position;                /* Current position in input */
    
    /* Capture group storage */
    size_t *capture_starts;         /* Capture group start positions */
    size_t *capture_ends;           /* Capture group end positions */
    size_t num_captures;            /* Number of capture groups */
    size_t capture_capacity;        /* Capacity of capture arrays */
    
    /* Execution state */
    void *stack;                    /* Execution stack (implementation-specific) */
    size_t stack_size;              /* Current stack size */
    size_t stack_capacity;          /* Stack capacity */
    
    /* Statistics */
    uint64_t steps;                 /* Execution steps counter */
    uint64_t backtracks;            /* Backtrack counter */
    
    /* Limits */
    size_t max_steps;               /* Maximum execution steps */
    size_t max_backtracks;          /* Maximum backtracks */
    
    /* Flags */
    bool anchored;                  /* Match must start at position 0 */
    bool multiline;                 /* Multiline mode */
    bool dotall;                    /* Dot matches newline */
    bool case_insensitive;          /* Case-insensitive matching */
};

/**
 * Create a new match context.
 * 
 * @param input Input string (null-terminated)
 * @param input_length Length of input (excluding null terminator)
 * @param num_captures Number of capture groups
 * @return New match context or NULL on failure
 */
chomsky3_match_context_t *chomsky3_match_context_new(
    const uint8_t *input,
    size_t input_length,
    size_t num_captures
);

/**
 * Free a match context.
 * 
 * @param ctx Match context to free
 */
void chomsky3_match_context_free(chomsky3_match_context_t *ctx);

/**
 * Reset a match context for reuse.
 * 
 * @param ctx Match context to reset
 * @param input New input string
 * @param input_length Length of new input
 */
void chomsky3_match_context_reset(
    chomsky3_match_context_t *ctx,
    const uint8_t *input,
    size_t input_length
);

/* ========================================================================
 * Bytecode-based Simulation
 * ======================================================================== */

/**
 * Execute bytecode against input string.
 * 
 * @param bytecode Compiled bytecode
 * @param ctx Match context
 * @param result Result structure to fill
 * @return 0 on success, negative on error
 */
int chomsky3_bytecode_simulate(
    const chomsky3_bytecode_t *bytecode,
    chomsky3_match_context_t *ctx,
    chomsky3_match_result_t *result
);

/**
 * Match bytecode against input string (convenience function).
 * 
 * @param bytecode Compiled bytecode
 * @param input Input string (null-terminated)
 * @param input_length Length of input
 * @param result Result structure to fill
 * @return 0 on success, negative on error
 */
int chomsky3_bytecode_match(
    const chomsky3_bytecode_t *bytecode,
    const uint8_t *input,
    size_t input_length,
    chomsky3_match_result_t *result
);

/**
 * Search for first match in input string.
 * 
 * @param bytecode Compiled bytecode
 * @param input Input string (null-terminated)
 * @param input_length Length of input
 * @param result Result structure to fill
 * @return 0 on success, negative on error
 */
int chomsky3_bytecode_search(
    const chomsky3_bytecode_t *bytecode,
    const uint8_t *input,
    size_t input_length,
    chomsky3_match_result_t *result
);

/* ========================================================================
 * C-based Simulation (Direct Interpretation)
 * ======================================================================== */

/**
 * Simulate bytecode using C interpreter (no VM overhead).
 * 
 * @param bytecode Compiled bytecode
 * @param ctx Match context
 * @param result Result structure to fill
 * @return 0 on success, negative on error
 */
int chomsky3_c_simulate(
    const chomsky3_bytecode_t *bytecode,
    chomsky3_match_context_t *ctx,
    chomsky3_match_result_t *result
);

/**
 * Match using C interpreter (convenience function).
 * 
 * @param bytecode Compiled bytecode
 * @param input Input string (null-terminated)
 * @param input_length Length of input
 * @param result Result structure to fill
 * @return 0 on success, negative on error
 */
int chomsky3_c_match(
    const chomsky3_bytecode_t *bytecode,
    const uint8_t *input,
    size_t input_length,
    chomsky3_match_result_t *result
);

/**
 * Search using C interpreter.
 * 
 * @param bytecode Compiled bytecode
 * @param input Input string (null-terminated)
 * @param input_length Length of input
 * @param result Result structure to fill
 * @return 0 on success, negative on error
 */
int chomsky3_c_search(
    const chomsky3_bytecode_t *bytecode,
    const uint8_t *input,
    size_t input_length,
    chomsky3_match_result_t *result
);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Check if character is a word character (for word boundaries).
 * 
 * @param c Character to check
 * @return true if word character, false otherwise
 */
static inline bool chomsky3_is_word_char(uint8_t c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c == '_');
}

/**
 * Check if character matches a character class.
 * 
 * @param c Character to check
 * @param bitmap 256-bit bitmap (32 bytes)
 * @return true if character is in class, false otherwise
 */
static inline bool chomsky3_char_in_class(uint8_t c, const uint8_t *bitmap) {
    return (bitmap[c / 8] & (1 << (c % 8))) != 0;
}

/**
 * Convert character to lowercase (ASCII only).
 * 
 * @param c Character to convert
 * @return Lowercase character
 */
static inline uint8_t chomsky3_to_lower(uint8_t c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

/**
 * Compare characters case-insensitively (ASCII only).
 * 
 * @param a First character
 * @param b Second character
 * @return true if equal (case-insensitive), false otherwise
 */
static inline bool chomsky3_char_equal_icase(uint8_t a, uint8_t b) {
    return chomsky3_to_lower(a) == chomsky3_to_lower(b);
}

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_SIMMATCH_H */
