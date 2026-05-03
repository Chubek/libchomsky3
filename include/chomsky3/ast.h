/**
 * libchomsky3 - Abstract Syntax Tree (AST) Interface
 * 
 * Header file defining the Abstract Syntax Tree structure used to represent
 * parsed regular expression patterns before compilation to IR.
 */

#ifndef CHOMSKY3_AST_H
#define CHOMSKY3_AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "chomsky3/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_ast_node chomsky3_ast_node_t;
typedef struct chomsky3_ast chomsky3_ast_t;

/* AST node types */
typedef enum {
    /* Literal nodes */
    CHOMSKY3_AST_LITERAL = 0,           /* Single character literal */
    CHOMSKY3_AST_STRING,                /* String literal (sequence of chars) */
    
    /* Character classes */
    CHOMSKY3_AST_CHAR_CLASS,            /* Character class [abc] */
    CHOMSKY3_AST_NEGATED_CHAR_CLASS,    /* Negated character class [^abc] */
    CHOMSKY3_AST_DOT,                   /* Dot metacharacter (.) */
    
    /* Predefined character classes */
    CHOMSKY3_AST_DIGIT,                 /* \d */
    CHOMSKY3_AST_NON_DIGIT,             /* \D */
    CHOMSKY3_AST_WORD,                  /* \w */
    CHOMSKY3_AST_NON_WORD,              /* \W */
    CHOMSKY3_AST_WHITESPACE,            /* \s */
    CHOMSKY3_AST_NON_WHITESPACE,        /* \S */
    
    /* Anchors */
    CHOMSKY3_AST_START_ANCHOR,          /* ^ (start of line/string) */
    CHOMSKY3_AST_END_ANCHOR,            /* $ (end of line/string) */
    CHOMSKY3_AST_WORD_BOUNDARY,         /* \b */
    CHOMSKY3_AST_NON_WORD_BOUNDARY,     /* \B */
    CHOMSKY3_AST_START_TEXT,            /* \A (start of text) */
    CHOMSKY3_AST_END_TEXT,              /* \z (end of text) */
    CHOMSKY3_AST_END_TEXT_NEWLINE,      /* \Z (end of text or before newline) */
    
    /* Quantifiers */
    CHOMSKY3_AST_ZERO_OR_MORE,          /* * */
    CHOMSKY3_AST_ONE_OR_MORE,           /* + */
    CHOMSKY3_AST_ZERO_OR_ONE,           /* ? */
    CHOMSKY3_AST_REPEAT,                /* {n,m} */
    
    /* Lazy quantifiers */
    CHOMSKY3_AST_ZERO_OR_MORE_LAZY,     /* *? */
    CHOMSKY3_AST_ONE_OR_MORE_LAZY,      /* +? */
    CHOMSKY3_AST_ZERO_OR_ONE_LAZY,      /* ?? */
    CHOMSKY3_AST_REPEAT_LAZY,           /* {n,m}? */
    
    /* Possessive quantifiers */
    CHOMSKY3_AST_ZERO_OR_MORE_POSSESSIVE, /* *+ */
    CHOMSKY3_AST_ONE_OR_MORE_POSSESSIVE,  /* ++ */
    CHOMSKY3_AST_ZERO_OR_ONE_POSSESSIVE,  /* ?+ */
    CHOMSKY3_AST_REPEAT_POSSESSIVE,       /* {n,m}+ */
    
    /* Grouping */
    CHOMSKY3_AST_GROUP,                 /* Capturing group (...) */
    CHOMSKY3_AST_NON_CAPTURING_GROUP,   /* Non-capturing group (?:...) */
    CHOMSKY3_AST_NAMED_GROUP,           /* Named group (?<name>...) */
    CHOMSKY3_AST_ATOMIC_GROUP,          /* Atomic group (?>...) */
    
    /* Alternation */
    CHOMSKY3_AST_ALTERNATION,           /* | (or) */
    
    /* Concatenation */
    CHOMSKY3_AST_CONCATENATION,         /* Sequence of nodes */
    
    /* Backreferences */
    CHOMSKY3_AST_BACKREF,               /* Backreference \1, \2, etc. */
    CHOMSKY3_AST_NAMED_BACKREF,         /* Named backreference \k<name> */
    
    /* Lookaround assertions */
    CHOMSKY3_AST_LOOKAHEAD,             /* Positive lookahead (?=...) */
    CHOMSKY3_AST_NEGATIVE_LOOKAHEAD,    /* Negative lookahead (?!...) */
    CHOMSKY3_AST_LOOKBEHIND,            /* Positive lookbehind (?<=...) */
    CHOMSKY3_AST_NEGATIVE_LOOKBEHIND,   /* Negative lookbehind (?<!...) */
    
    /* Conditional expressions */
    CHOMSKY3_AST_CONDITIONAL,           /* (?(condition)yes|no) */
    
    /* Unicode properties */
    CHOMSKY3_AST_UNICODE_PROPERTY,      /* \p{Property} */
    CHOMSKY3_AST_NEGATED_UNICODE_PROPERTY, /* \P{Property} */
    
    /* Special nodes */
    CHOMSKY3_AST_EMPTY,                 /* Empty/epsilon node */
    CHOMSKY3_AST_FAIL,                  /* Always fails to match */
    
    /* Subroutines and recursion */
    CHOMSKY3_AST_SUBROUTINE,            /* Subroutine call (?1), (?&name) */
    CHOMSKY3_AST_RECURSION,             /* Recursion (?R) or (?0) */
    
    CHOMSKY3_AST_MAX_TYPE
} chomsky3_ast_node_type_t;

/* Character range for character classes */
typedef struct {
    uint32_t start;     /* Start codepoint */
    uint32_t end;       /* End codepoint (inclusive) */
} chomsky3_char_range_t;

/* Character class data */
typedef struct {
    chomsky3_char_range_t *ranges;  /* Array of character ranges */
    size_t num_ranges;              /* Number of ranges */
    size_t capacity;                /* Allocated capacity */
    bool negated;                   /* True if negated class */
} chomsky3_char_class_t;

/* Quantifier bounds */
typedef struct {
    uint32_t min;       /* Minimum repetitions */
    uint32_t max;       /* Maximum repetitions (UINT32_MAX for unbounded) */
} chomsky3_quantifier_bounds_t;

/* Group information */
typedef struct {
    uint32_t group_id;      /* Group number (0 for non-capturing) */
    char *name;             /* Group name (NULL if unnamed) */
    bool capturing;         /* True if capturing group */
} chomsky3_group_info_t;

/* Unicode property */
typedef struct {
    char *property_name;    /* Property name (e.g., "Letter", "Nd") */
    char *property_value;   /* Property value (NULL for binary properties) */
    bool negated;           /* True if negated property */
} chomsky3_unicode_property_t;

/* Conditional expression */
typedef struct {
    chomsky3_ast_node_t *condition;     /* Condition (can be group ref or assertion) */
    chomsky3_ast_node_t *true_branch;   /* Branch if condition is true */
    chomsky3_ast_node_t *false_branch;  /* Branch if condition is false (can be NULL) */
} chomsky3_conditional_t;

/* AST node structure */
struct chomsky3_ast_node {
    chomsky3_ast_node_type_t type;      /* Node type */
    
    /* Node-specific data (union) */
    union {
        /* Literal */
        uint32_t literal;                   /* Single character codepoint */
        
        /* String literal */
        struct {
            uint32_t *codepoints;           /* Array of codepoints */
            size_t length;                  /* String length */
        } string;
        
        /* Character class */
        chomsky3_char_class_t char_class;
        
        /* Quantifier */
        struct {
            chomsky3_ast_node_t *child;     /* Quantified expression */
            chomsky3_quantifier_bounds_t bounds;
            bool greedy;                    /* True for greedy, false for lazy */
            bool possessive;                /* True for possessive */
        } quantifier;
        
        /* Group */
        struct {
            chomsky3_ast_node_t *child;     /* Group content */
            chomsky3_group_info_t info;     /* Group information */
        } group;
        
        /* Alternation */
        struct {
            chomsky3_ast_node_t **alternatives; /* Array of alternative branches */
            size_t num_alternatives;            /* Number of alternatives */
        } alternation;
        
        /* Concatenation */
        struct {
            chomsky3_ast_node_t **children;     /* Array of child nodes */
            size_t num_children;                /* Number of children */
        } concatenation;
        
        /* Backreference */
        struct {
            uint32_t group_id;              /* Referenced group number */
            char *group_name;               /* Referenced group name (NULL if numeric) */
        } backref;
        
        /* Lookaround */
        struct {
            chomsky3_ast_node_t *child;     /* Assertion content */
            bool positive;                  /* True for positive, false for negative */
            bool ahead;                     /* True for lookahead, false for lookbehind */
        } lookaround;
        
        /* Conditional */
        chomsky3_conditional_t conditional;
        
        /* Unicode property */
        chomsky3_unicode_property_t unicode_property;
        
        /* Subroutine/recursion */
        struct {
            uint32_t group_id;              /* Target group number */
            char *group_name;               /* Target group name (NULL if numeric) */
        } subroutine;
    } data;
    
    /* Source location information */
    size_t start_offset;    /* Start offset in pattern */
    size_t end_offset;      /* End offset in pattern */
    
    /* Parent node (NULL for root) */
    chomsky3_ast_node_t *parent;
    
    /* User data */
    void *user_data;
};

/* AST structure */
struct chomsky3_ast {
    chomsky3_ast_node_t *root;      /* Root node */
    const char *pattern;            /* Original pattern string */
    size_t pattern_length;          /* Pattern length */
    uint32_t num_groups;            /* Total number of capturing groups */
    uint32_t flags;                 /* Compilation flags */
    
    /* Named groups map */
    struct {
        char **names;               /* Array of group names */
        uint32_t *group_ids;        /* Corresponding group IDs */
        size_t count;               /* Number of named groups */
    } named_groups;
    
    /* Memory management */
    void *allocator_data;           /* Custom allocator data */
};

/**
 * Create a new AST structure.
 * 
 * @param pattern The original pattern string (will be copied).
 * @param flags Compilation flags.
 * @return Allocated AST structure, or NULL on failure.
 */
chomsky3_ast_t *chomsky3_ast_create(const char *pattern, uint32_t flags);

/**
 * Free an AST structure and all its nodes.
 * 
 * @param ast The AST to free.
 */
void chomsky3_ast_free(chomsky3_ast_t *ast);

/**
 * Create a new AST node.
 * 
 * @param type The node type.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create(chomsky3_ast_node_type_t type);

/**
 * Free an AST node and all its children recursively.
 * 
 * @param node The node to free.
 */
void chomsky3_ast_node_free(chomsky3_ast_node_t *node);

/**
 * Clone an AST node and all its children.
 * 
 * @param node The node to clone.
 * @return Cloned node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_clone(const chomsky3_ast_node_t *node);

/**
 * Create a literal node.
 * 
 * @param codepoint The character codepoint.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_literal(uint32_t codepoint);

/**
 * Create a string literal node.
 * 
 * @param codepoints Array of codepoints.
 * @param length Number of codepoints.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_string(
    const uint32_t *codepoints,
    size_t length
);

/**
 * Create a character class node.
 * 
 * @param negated True for negated class.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_char_class(bool negated);

/**
 * Add a range to a character class node.
 * 
 * @param node The character class node.
 * @param start Start codepoint.
 * @param end End codepoint (inclusive).
 * @return CHOMSKY3_OK on success, error code on failure.
 */
chomsky3_error_t chomsky3_ast_char_class_add_range(
    chomsky3_ast_node_t *node,
    uint32_t start,
    uint32_t end
);

/**
 * Create a quantifier node.
 * 
 * @param child The child node to quantify.
 * @param min Minimum repetitions.
 * @param max Maximum repetitions (UINT32_MAX for unbounded).
 * @param greedy True for greedy, false for lazy.
 * @param possessive True for possessive quantifier.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_quantifier(
    chomsky3_ast_node_t *child,
    uint32_t min,
    uint32_t max,
    bool greedy,
    bool possessive
);

/**
 * Create a group node.
 * 
 * @param child The group content.
 * @param group_id Group number (0 for non-capturing).
 * @param name Group name (NULL for unnamed, will be copied).
 * @param capturing True for capturing group.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_group(
    chomsky3_ast_node_t *child,
    uint32_t group_id,
    const char *name,
    bool capturing
);

/**
 * Create an alternation node.
 * 
 * @param alternatives Array of alternative branches.
 * @param num_alternatives Number of alternatives.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_alternation(
    chomsky3_ast_node_t **alternatives,
    size_t num_alternatives
);

/**
 * Create a concatenation node.
 * 
 * @param children Array of child nodes.
 * @param num_children Number of children.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_concatenation(
    chomsky3_ast_node_t **children,
    size_t num_children
);

/**
 * Create a backreference node.
 * 
 * @param group_id Referenced group number.
 * @param group_name Referenced group name (NULL for numeric, will be copied).
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_backref(
    uint32_t group_id,
    const char *group_name
);

/**
 * Create a lookaround assertion node.
 * 
 * @param child Assertion content.
 * @param positive True for positive, false for negative.
 * @param ahead True for lookahead, false for lookbehind.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_lookaround(
    chomsky3_ast_node_t *child,
    bool positive,
    bool ahead
);

/**
 * Create a conditional expression node.
 * 
 * @param condition Condition node.
 * @param true_branch True branch.
 * @param false_branch False branch (can be NULL).
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_conditional(
    chomsky3_ast_node_t *condition,
    chomsky3_ast_node_t *true_branch,
    chomsky3_ast_node_t *false_branch
);

/**
 * Create a Unicode property node.
 * 
 * @param property_name Property name (will be copied).
 * @param property_value Property value (NULL for binary properties, will be copied).
 * @param negated True for negated property.
 * @return Allocated node, or NULL on failure.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_unicode_property(
    const char *property_name,
    const char *property_value,
    bool negated
);

/**
 * Get the type name of an AST node.
 * 
 * @param type The node type.
 * @return Constant string with the type name.
 */
const char *chomsky3_ast_node_type_name(chomsky3_ast_node_type_t type);

/**
 * Check if a node type is a quantifier.
 * 
 * @param type The node type.
 * @return True if the type is a quantifier, false otherwise.
 */
bool chomsky3_ast_node_is_quantifier(chomsky3_ast_node_type_t type);

/**
 * Check if a node type is an anchor.
 * 
 * @param type The node type.
 * @return True if the type is an anchor, false otherwise.
 */
bool chomsky3_ast_node_is_anchor(chomsky3_ast_node_type_t type);

/**
 * Check if a node type is a lookaround assertion.
 * 
 * @param type The node type.
 * @return True if the type is a lookaround, false otherwise.
 */
bool chomsky3_ast_node_is_lookaround(chomsky3_ast_node_type_t type);

/**
 * Traverse the AST in depth-first order.
 * 
 * @param node The starting node.
 * @param callback Callback function called for each node.
 * @param user_data User data passed to callback.
 * @return CHOMSKY3_OK on success, error code on failure.
 */
typedef chomsky3_error_t (*chomsky3_ast_visitor_t)(
    chomsky3_ast_node_t *node,
    void *user_data
);

chomsky3_error_t chomsky3_ast_traverse(
    chomsky3_ast_node_t *node,
    chomsky3_ast_visitor_t callback,
    void *user_data
);

/**
 * Print the AST to a file stream (for debugging).
 * 
 * @param ast The AST to print.
 * @param stream Output stream.
 * @param indent_level Initial indentation level.
 */
void chomsky3_ast_print(
    const chomsky3_ast_t *ast,
    FILE *stream,
    int indent_level
);

/**
 * Print a single AST node to a file stream.
 * 
 * @param node The node to print.
 * @param stream Output stream.
 * @param indent_level Indentation level.
 */
void chomsky3_ast_node_print(
    const chomsky3_ast_node_t *node,
    FILE *stream,
    int indent_level
);

/**
 * Validate an AST for correctness.
 * 
 * Checks for invalid structures, dangling references, etc.
 * 
 * @param ast The AST to validate.
 * @return CHOMSKY3_OK if valid, error code otherwise.
 */
chomsky3_error_t chomsky3_ast_validate(const chomsky3_ast_t *ast);

/**
 * Get statistics about an AST.
 * 
 * @param ast The AST to analyze.
 * @param num_nodes Output: total number of nodes.
 * @param max_depth Output: maximum tree depth.
 * @param num_groups Output: number of capturing groups.
 * @return CHOMSKY3_OK on success, error code on failure.
 */
chomsky3_error_t chomsky3_ast_get_stats(
    const chomsky3_ast_t *ast,
    size_t *num_nodes,
    size_t *max_depth,
    uint32_t *num_groups
);

/**
 * Register a named group in the AST.
 * 
 * @param ast The AST.
 * @param name Group name (will be copied).
 * @param group_id Group ID.
 * @return CHOMSKY3_OK on success, error code on failure.
 */
chomsky3_error_t chomsky3_ast_register_named_group(
    chomsky3_ast_t *ast,
    const char *name,
    uint32_t group_id
);

/**
 * Look up a named group in the AST.
 * 
 * @param ast The AST.
 * @param name Group name.
 * @param group_id Output: group ID (if found).
 * @return CHOMSKY3_OK if found, error code otherwise.
 */
chomsky3_error_t chomsky3_ast_lookup_named_group(
    const chomsky3_ast_t *ast,
    const char *name,
    uint32_t *group_id
);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_AST_H */
