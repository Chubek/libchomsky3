/**
 * libchomsky3 - Extended Regular Expression Interface
 * 
 * Header file providing ERE pattern representation and manipulation functions.
 */

#ifndef CHOMSKY3_REGEX_H
#define CHOMSKY3_REGEX_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_regex chomsky3_regex_t;
typedef struct chomsky3_regex_node chomsky3_regex_node_t;

/* ERE node types */
typedef enum {
    CHOMSKY3_NODE_LITERAL,      /* Literal character */
    CHOMSKY3_NODE_CHAR_CLASS,   /* Character class [abc] */
    CHOMSKY3_NODE_DOT,          /* Any character . */
    CHOMSKY3_NODE_ANCHOR_START, /* Start anchor ^ */
    CHOMSKY3_NODE_ANCHOR_END,   /* End anchor $ */
    CHOMSKY3_NODE_CONCAT,       /* Concatenation */
    CHOMSKY3_NODE_ALTERNATION,  /* Alternation | */
    CHOMSKY3_NODE_STAR,         /* Zero or more * */
    CHOMSKY3_NODE_PLUS,         /* One or more + */
    CHOMSKY3_NODE_QUESTION,     /* Zero or one ? */
    CHOMSKY3_NODE_REPEAT,       /* Bounded repetition {m,n} */
    CHOMSKY3_NODE_GROUP,        /* Capturing group (...) */
    CHOMSKY3_NODE_BACKREF,      /* Backreference \1 */
    CHOMSKY3_NODE_LOOKAHEAD,    /* Positive lookahead (?=...) */
    CHOMSKY3_NODE_LOOKAHEAD_NEG,/* Negative lookahead (?!...) */
    CHOMSKY3_NODE_WORD_BOUNDARY,/* Word boundary \b */
    CHOMSKY3_NODE_NWORD_BOUNDARY,/* Non-word boundary \B */
} chomsky3_node_type_t;

/* Character class flags */
typedef enum {
    CHOMSKY3_CCLASS_NONE     = 0,
    CHOMSKY3_CCLASS_NEGATED  = 1 << 0,  /* Negated class [^...] */
    CHOMSKY3_CCLASS_DIGIT    = 1 << 1,  /* \d */
    CHOMSKY3_CCLASS_WORD     = 1 << 2,  /* \w */
    CHOMSKY3_CCLASS_SPACE    = 1 << 3,  /* \s */
} chomsky3_cclass_flags_t;

/* Repetition bounds */
typedef struct {
    uint32_t min;
    uint32_t max;               /* UINT32_MAX for unbounded */
} chomsky3_repeat_t;

/* Character class representation */
typedef struct {
    chomsky3_cclass_flags_t flags;
    uint8_t *bitmap;            /* 256-bit bitmap for character membership */
    size_t num_ranges;
    struct {
        uint32_t start;
        uint32_t end;
    } *ranges;                  /* Unicode ranges */
} chomsky3_char_class_t;

/* Regex node structure */
struct chomsky3_regex_node {
    chomsky3_node_type_t type;
    union {
        uint32_t literal;       /* For LITERAL */
        chomsky3_char_class_t char_class; /* For CHAR_CLASS */
        chomsky3_repeat_t repeat; /* For REPEAT */
        uint32_t group_id;      /* For GROUP */
        uint32_t backref_id;    /* For BACKREF */
    } data;
    
    chomsky3_regex_node_t *left;
    chomsky3_regex_node_t *right;
    
    /* Metadata */
    bool greedy;                /* Greedy vs lazy quantifier */
    uint32_t position;          /* Position in original pattern */
};

/* Regex structure */
struct chomsky3_regex {
    chomsky3_regex_node_t *root;
    size_t num_groups;
    char *pattern;              /* Original pattern string */
    size_t pattern_length;
    chomsky3_error_t parse_error; /* Parse error code (CHOMSKY3_OK if none) */
    size_t parse_error_pos;     /* Byte offset of parse error */
};

/**
 * Create a new regex structure from a pattern string.
 * 
 * @param pattern ERE pattern string
 * @param length Length of pattern
 * @return New regex structure or NULL on failure
 */
chomsky3_regex_t *chomsky3_regex_new(const char *pattern, size_t length);

/**
 * Free a regex structure and all associated nodes.
 * 
 * @param regex Regex to free
 */
void chomsky3_regex_free(chomsky3_regex_t *regex);

/**
 * Create a literal node.
 * 
 * @param codepoint Unicode codepoint
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_literal(uint32_t codepoint);

/**
 * Create a character class node.
 * 
 * @param flags Character class flags
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_char_class(chomsky3_cclass_flags_t flags);

/**
 * Add a character range to a character class node.
 * 
 * @param node Character class node
 * @param start Start of range (inclusive)
 * @param end End of range (inclusive)
 * @return true on success, false on failure
 */
bool chomsky3_char_class_add_range(
    chomsky3_regex_node_t *node,
    uint32_t start,
    uint32_t end
);

/**
 * Create a concatenation node.
 * 
 * @param left Left child
 * @param right Right child
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_concat(
    chomsky3_regex_node_t *left,
    chomsky3_regex_node_t *right
);

/**
 * Create an alternation node.
 * 
 * @param left Left alternative
 * @param right Right alternative
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_alternation(
    chomsky3_regex_node_t *left,
    chomsky3_regex_node_t *right
);

/**
 * Create a repetition node.
 * 
 * @param child Child node to repeat
 * @param min Minimum repetitions
 * @param max Maximum repetitions (UINT32_MAX for unbounded)
 * @param greedy Greedy vs lazy matching
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_repeat(
    chomsky3_regex_node_t *child,
    uint32_t min,
    uint32_t max,
    bool greedy
);

/**
 * Create a capturing group node.
 * 
 * @param child Child node
 * @param group_id Group identifier
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_group(
    chomsky3_regex_node_t *child,
    uint32_t group_id
);

/**
 * Create a backreference node.
 * 
 * @param backref_id Backreference identifier
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_backref(uint32_t backref_id);

/**
 * Create an anchor node.
 * 
 * @param type ANCHOR_START or ANCHOR_END
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_anchor(chomsky3_node_type_t type);

/**
 * Create a dot (any character) node.
 * 
 * @return New node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_dot(void);

/**
 * Free a regex node and all its children.
 * 
 * @param node Node to free
 */
void chomsky3_node_free(chomsky3_regex_node_t *node);

/**
 * Clone a regex node tree.
 * 
 * @param node Node to clone
 * @return Cloned node or NULL on failure
 */
chomsky3_regex_node_t *chomsky3_node_clone(const chomsky3_regex_node_t *node);

/**
 * Print a regex node tree for debugging.
 * 
 * @param node Node to print
 * @param indent Indentation level
 */
void chomsky3_node_print(const chomsky3_regex_node_t *node, int indent);

/**
 * Get the number of nodes in a regex tree.
 * 
 * @param node Root node
 * @return Number of nodes
 */
size_t chomsky3_node_count(const chomsky3_regex_node_t *node);

/**
 * Validate a regex tree for correctness.
 * 
 * @param regex Regex to validate
 * @return true if valid, false otherwise
 */
bool chomsky3_regex_validate(const chomsky3_regex_t *regex);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_REGEX_H */
