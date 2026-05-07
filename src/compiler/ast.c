/**
 * libchomsky3 - Abstract Syntax Tree (AST) Implementation
 * 
 * Implements the AST structure used to represent parsed regular expression
 * patterns before compilation to IR.
 */

#include "chomsky3/ast.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Helper macros */
#define INITIAL_CAPACITY 8

static char *chomsky3_ast_strdup(const char *str) {
    size_t length = strlen(str);
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, str, length + 1);
    }
    return copy;
}

#define strdup chomsky3_ast_strdup

/* Forward declarations for internal helpers */
static void free_node_data(chomsky3_ast_node_t *node);
static chomsky3_ast_node_t *clone_node_recursive(const chomsky3_ast_node_t *node);
static chomsky3_error_t traverse_recursive(
    chomsky3_ast_node_t *node,
    chomsky3_ast_visitor_t callback,
    void *user_data
);
static void print_node_recursive(
    const chomsky3_ast_node_t *node,
    FILE *stream,
    int indent_level
);
static void print_indent(FILE *stream, int level);
static chomsky3_error_t validate_node(
    const chomsky3_ast_node_t *node,
    const chomsky3_ast_t *ast
);
static void get_node_stats(
    const chomsky3_ast_node_t *node,
    size_t *num_nodes,
    size_t *max_depth,
    size_t current_depth
);

/**
 * Create a new AST structure.
 */
chomsky3_ast_t *chomsky3_ast_create(const char *pattern, uint32_t flags) {
    if (!pattern) {
        return NULL;
    }

    chomsky3_ast_t *ast = calloc(1, sizeof(chomsky3_ast_t));
    if (!ast) {
        return NULL;
    }

    size_t pattern_len = strlen(pattern);
    char *pattern_copy = malloc(pattern_len + 1);
    if (!pattern_copy) {
        free(ast);
        return NULL;
    }
    memcpy(pattern_copy, pattern, pattern_len + 1);

    ast->pattern = pattern_copy;
    ast->pattern_length = pattern_len;
    ast->flags = flags;
    ast->num_groups = 0;
    ast->root = NULL;

    return ast;
}

/**
 * Free an AST structure and all its nodes.
 */
void chomsky3_ast_free(chomsky3_ast_t *ast) {
    if (!ast) {
        return;
    }

    if (ast->root) {
        chomsky3_ast_node_free(ast->root);
    }

    if (ast->pattern) {
        free((void *)ast->pattern);
    }

    /* Free named groups */
    for (size_t i = 0; i < ast->named_groups.count; i++) {
        free(ast->named_groups.names[i]);
    }
    free(ast->named_groups.names);
    free(ast->named_groups.group_ids);

    free(ast);
}

/**
 * Create a new AST node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create(chomsky3_ast_node_type_t type) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    if (!node) {
        return NULL;
    }

    node->type = type;
    node->parent = NULL;
    node->user_data = NULL;
    node->start_offset = 0;
    node->end_offset = 0;

    return node;
}

/**
 * Free an AST node and all its children recursively.
 */
void chomsky3_ast_node_free(chomsky3_ast_node_t *node) {
    if (!node) {
        return;
    }

    free_node_data(node);
    free(node);
}

/**
 * Free node-specific data.
 */
static void free_node_data(chomsky3_ast_node_t *node) {
    switch (node->type) {
        case CHOMSKY3_AST_STRING:
            free(node->data.string.codepoints);
            break;

        case CHOMSKY3_AST_CHAR_CLASS:
        case CHOMSKY3_AST_NEGATED_CHAR_CLASS:
            free(node->data.char_class.ranges);
            break;

        case CHOMSKY3_AST_ZERO_OR_MORE:
        case CHOMSKY3_AST_ONE_OR_MORE:
        case CHOMSKY3_AST_ZERO_OR_ONE:
        case CHOMSKY3_AST_REPEAT:
        case CHOMSKY3_AST_ZERO_OR_MORE_LAZY:
        case CHOMSKY3_AST_ONE_OR_MORE_LAZY:
        case CHOMSKY3_AST_ZERO_OR_ONE_LAZY:
        case CHOMSKY3_AST_REPEAT_LAZY:
        case CHOMSKY3_AST_ZERO_OR_MORE_POSSESSIVE:
        case CHOMSKY3_AST_ONE_OR_MORE_POSSESSIVE:
        case CHOMSKY3_AST_ZERO_OR_ONE_POSSESSIVE:
        case CHOMSKY3_AST_REPEAT_POSSESSIVE:
            if (node->data.quantifier.child) {
                chomsky3_ast_node_free(node->data.quantifier.child);
            }
            break;

        case CHOMSKY3_AST_GROUP:
        case CHOMSKY3_AST_NON_CAPTURING_GROUP:
        case CHOMSKY3_AST_NAMED_GROUP:
        case CHOMSKY3_AST_ATOMIC_GROUP:
            if (node->data.group.child) {
                chomsky3_ast_node_free(node->data.group.child);
            }
            free(node->data.group.info.name);
            break;

        case CHOMSKY3_AST_ALTERNATION:
            for (size_t i = 0; i < node->data.alternation.num_alternatives; i++) {
                chomsky3_ast_node_free(node->data.alternation.alternatives[i]);
            }
            free(node->data.alternation.alternatives);
            break;

        case CHOMSKY3_AST_CONCATENATION:
            for (size_t i = 0; i < node->data.concatenation.num_children; i++) {
                chomsky3_ast_node_free(node->data.concatenation.children[i]);
            }
            free(node->data.concatenation.children);
            break;

        case CHOMSKY3_AST_BACKREF:
        case CHOMSKY3_AST_NAMED_BACKREF:
            free(node->data.backref.group_name);
            break;

        case CHOMSKY3_AST_LOOKAHEAD:
        case CHOMSKY3_AST_NEGATIVE_LOOKAHEAD:
        case CHOMSKY3_AST_LOOKBEHIND:
        case CHOMSKY3_AST_NEGATIVE_LOOKBEHIND:
            if (node->data.lookaround.child) {
                chomsky3_ast_node_free(node->data.lookaround.child);
            }
            break;

        case CHOMSKY3_AST_CONDITIONAL:
            if (node->data.conditional.condition) {
                chomsky3_ast_node_free(node->data.conditional.condition);
            }
            if (node->data.conditional.true_branch) {
                chomsky3_ast_node_free(node->data.conditional.true_branch);
            }
            if (node->data.conditional.false_branch) {
                chomsky3_ast_node_free(node->data.conditional.false_branch);
            }
            break;

        case CHOMSKY3_AST_UNICODE_PROPERTY:
        case CHOMSKY3_AST_NEGATED_UNICODE_PROPERTY:
            free(node->data.unicode_property.property_name);
            free(node->data.unicode_property.property_value);
            break;

        case CHOMSKY3_AST_SUBROUTINE:
        case CHOMSKY3_AST_RECURSION:
            free(node->data.subroutine.group_name);
            break;

        default:
            /* No dynamic data to free */
            break;
    }
}

/**
 * Clone an AST node and all its children.
 */
chomsky3_ast_node_t *chomsky3_ast_node_clone(const chomsky3_ast_node_t *node) {
    if (!node) {
        return NULL;
    }

    return clone_node_recursive(node);
}

/**
 * Clone a node recursively.
 */
static chomsky3_ast_node_t *clone_node_recursive(const chomsky3_ast_node_t *node) {
    chomsky3_ast_node_t *clone = chomsky3_ast_node_create(node->type);
    if (!clone) {
        return NULL;
    }

    clone->start_offset = node->start_offset;
    clone->end_offset = node->end_offset;

    switch (node->type) {
        case CHOMSKY3_AST_LITERAL:
            clone->data.literal = node->data.literal;
            break;

        case CHOMSKY3_AST_STRING:
            clone->data.string.length = node->data.string.length;
            clone->data.string.codepoints = malloc(
                node->data.string.length * sizeof(uint32_t)
            );
            if (!clone->data.string.codepoints) {
                free(clone);
                return NULL;
            }
            memcpy(
                clone->data.string.codepoints,
                node->data.string.codepoints,
                node->data.string.length * sizeof(uint32_t)
            );
            break;

        case CHOMSKY3_AST_CHAR_CLASS:
        case CHOMSKY3_AST_NEGATED_CHAR_CLASS:
            clone->data.char_class.num_ranges = node->data.char_class.num_ranges;
            clone->data.char_class.capacity = node->data.char_class.capacity;
            clone->data.char_class.negated = node->data.char_class.negated;
            if (node->data.char_class.num_ranges > 0) {
                clone->data.char_class.ranges = malloc(
                    node->data.char_class.capacity * sizeof(chomsky3_char_range_t)
                );
                if (!clone->data.char_class.ranges) {
                    free(clone);
                    return NULL;
                }
                memcpy(
                    clone->data.char_class.ranges,
                    node->data.char_class.ranges,
                    node->data.char_class.num_ranges * sizeof(chomsky3_char_range_t)
                );
            }
            break;

        case CHOMSKY3_AST_ZERO_OR_MORE:
        case CHOMSKY3_AST_ONE_OR_MORE:
        case CHOMSKY3_AST_ZERO_OR_ONE:
        case CHOMSKY3_AST_REPEAT:
        case CHOMSKY3_AST_ZERO_OR_MORE_LAZY:
        case CHOMSKY3_AST_ONE_OR_MORE_LAZY:
        case CHOMSKY3_AST_ZERO_OR_ONE_LAZY:
        case CHOMSKY3_AST_REPEAT_LAZY:
        case CHOMSKY3_AST_ZERO_OR_MORE_POSSESSIVE:
        case CHOMSKY3_AST_ONE_OR_MORE_POSSESSIVE:
        case CHOMSKY3_AST_ZERO_OR_ONE_POSSESSIVE:
        case CHOMSKY3_AST_REPEAT_POSSESSIVE:
            clone->data.quantifier.bounds = node->data.quantifier.bounds;
            clone->data.quantifier.greedy = node->data.quantifier.greedy;
            clone->data.quantifier.possessive = node->data.quantifier.possessive;
            clone->data.quantifier.child = clone_node_recursive(node->data.quantifier.child);
            if (!clone->data.quantifier.child) {
                free(clone);
                return NULL;
            }
            clone->data.quantifier.child->parent = clone;
            break;

        case CHOMSKY3_AST_GROUP:
        case CHOMSKY3_AST_NON_CAPTURING_GROUP:
        case CHOMSKY3_AST_NAMED_GROUP:
        case CHOMSKY3_AST_ATOMIC_GROUP:
            clone->data.group.info = node->data.group.info;
            if (node->data.group.info.name) {
                clone->data.group.info.name = strdup(node->data.group.info.name);
                if (!clone->data.group.info.name) {
                    free(clone);
                    return NULL;
                }
            }
            clone->data.group.child = clone_node_recursive(node->data.group.child);
            if (!clone->data.group.child) {
                free(clone->data.group.info.name);
                free(clone);
                return NULL;
            }
            clone->data.group.child->parent = clone;
            break;

        case CHOMSKY3_AST_ALTERNATION:
            clone->data.alternation.num_alternatives = node->data.alternation.num_alternatives;
            clone->data.alternation.alternatives = malloc(
                node->data.alternation.num_alternatives * sizeof(chomsky3_ast_node_t *)
            );
            if (!clone->data.alternation.alternatives) {
                free(clone);
                return NULL;
            }
            for (size_t i = 0; i < node->data.alternation.num_alternatives; i++) {
                clone->data.alternation.alternatives[i] = 
                    clone_node_recursive(node->data.alternation.alternatives[i]);
                if (!clone->data.alternation.alternatives[i]) {
                    for (size_t j = 0; j < i; j++) {
                        chomsky3_ast_node_free(clone->data.alternation.alternatives[j]);
                    }
                    free(clone->data.alternation.alternatives);
                    free(clone);
                    return NULL;
                }
                clone->data.alternation.alternatives[i]->parent = clone;
            }
            break;

        case CHOMSKY3_AST_CONCATENATION:
            clone->data.concatenation.num_children = node->data.concatenation.num_children;
            clone->data.concatenation.children = malloc(
                node->data.concatenation.num_children * sizeof(chomsky3_ast_node_t *)
            );
            if (!clone->data.concatenation.children) {
                free(clone);
                return NULL;
            }
            for (size_t i = 0; i < node->data.concatenation.num_children; i++) {
                clone->data.concatenation.children[i] = 
                    clone_node_recursive(node->data.concatenation.children[i]);
                if (!clone->data.concatenation.children[i]) {
                    for (size_t j = 0; j < i; j++) {
                        chomsky3_ast_node_free(clone->data.concatenation.children[j]);
                    }
                    free(clone->data.concatenation.children);
                    free(clone);
                    return NULL;
                }
                clone->data.concatenation.children[i]->parent = clone;
            }
            break;

        case CHOMSKY3_AST_BACKREF:
        case CHOMSKY3_AST_NAMED_BACKREF:
            clone->data.backref.group_id = node->data.backref.group_id;
            if (node->data.backref.group_name) {
                clone->data.backref.group_name = strdup(node->data.backref.group_name);
                if (!clone->data.backref.group_name) {
                    free(clone);
                    return NULL;
                }
            }
            break;

        case CHOMSKY3_AST_LOOKAHEAD:
        case CHOMSKY3_AST_NEGATIVE_LOOKAHEAD:
        case CHOMSKY3_AST_LOOKBEHIND:
        case CHOMSKY3_AST_NEGATIVE_LOOKBEHIND:
            clone->data.lookaround.positive = node->data.lookaround.positive;
            clone->data.lookaround.ahead = node->data.lookaround.ahead;
            clone->data.lookaround.child = clone_node_recursive(node->data.lookaround.child);
            if (!clone->data.lookaround.child) {
                free(clone);
                return NULL;
            }
            clone->data.lookaround.child->parent = clone;
            break;

        case CHOMSKY3_AST_CONDITIONAL:
            clone->data.conditional.condition = 
                clone_node_recursive(node->data.conditional.condition);
            clone->data.conditional.true_branch = 
                clone_node_recursive(node->data.conditional.true_branch);
            clone->data.conditional.false_branch = 
                node->data.conditional.false_branch ? 
                clone_node_recursive(node->data.conditional.false_branch) : NULL;
            
            if (!clone->data.conditional.condition || !clone->data.conditional.true_branch) {
                if (clone->data.conditional.condition) {
                    chomsky3_ast_node_free(clone->data.conditional.condition);
                }
                if (clone->data.conditional.true_branch) {
                    chomsky3_ast_node_free(clone->data.conditional.true_branch);
                }
                if (clone->data.conditional.false_branch) {
                    chomsky3_ast_node_free(clone->data.conditional.false_branch);
                }
                free(clone);
                return NULL;
            }
            
            clone->data.conditional.condition->parent = clone;
            clone->data.conditional.true_branch->parent = clone;
            if (clone->data.conditional.false_branch) {
                clone->data.conditional.false_branch->parent = clone;
            }
            break;

        case CHOMSKY3_AST_UNICODE_PROPERTY:
        case CHOMSKY3_AST_NEGATED_UNICODE_PROPERTY:
            clone->data.unicode_property.negated = node->data.unicode_property.negated;
            if (node->data.unicode_property.property_name) {
                clone->data.unicode_property.property_name = 
                    strdup(node->data.unicode_property.property_name);
                if (!clone->data.unicode_property.property_name) {
                    free(clone);
                    return NULL;
                }
            }
            if (node->data.unicode_property.property_value) {
                clone->data.unicode_property.property_value = 
                    strdup(node->data.unicode_property.property_value);
                if (!clone->data.unicode_property.property_value) {
                    free(clone->data.unicode_property.property_name);
                    free(clone);
                    return NULL;
                }
            }
            break;

        case CHOMSKY3_AST_SUBROUTINE:
        case CHOMSKY3_AST_RECURSION:
            clone->data.subroutine.group_id = node->data.subroutine.group_id;
            if (node->data.subroutine.group_name) {
                clone->data.subroutine.group_name = strdup(node->data.subroutine.group_name);
                if (!clone->data.subroutine.group_name) {
                    free(clone);
                    return NULL;
                }
            }
            break;

        default:
            /* Simple types with no dynamic data */
            break;
    }

    return clone;
}

/**
 * Create a literal node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_literal(uint32_t codepoint) {
    chomsky3_ast_node_t *node = chomsky3_ast_node_create(CHOMSKY3_AST_LITERAL);
    if (!node) {
        return NULL;
    }

    node->data.literal = codepoint;
    return node;
}

/**
 * Create a string literal node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_string(
    const uint32_t *codepoints,
    size_t length
) {
    if (!codepoints || length == 0) {
        return NULL;
    }

    chomsky3_ast_node_t *node = chomsky3_ast_node_create(CHOMSKY3_AST_STRING);
    if (!node) {
        return NULL;
    }

    node->data.string.codepoints = malloc(length * sizeof(uint32_t));
    if (!node->data.string.codepoints) {
        free(node);
        return NULL;
    }

    memcpy(node->data.string.codepoints, codepoints, length * sizeof(uint32_t));
    node->data.string.length = length;

    return node;
}

/**
 * Create a character class node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_char_class(bool negated) {
    chomsky3_ast_node_t *node = chomsky3_ast_node_create(
        negated ? CHOMSKY3_AST_NEGATED_CHAR_CLASS : CHOMSKY3_AST_CHAR_CLASS
    );
    if (!node) {
        return NULL;
    }

    node->data.char_class.ranges = NULL;
    node->data.char_class.num_ranges = 0;
    node->data.char_class.capacity = 0;
    node->data.char_class.negated = negated;

    return node;
}

/**
 * Add a range to a character class node.
 */
chomsky3_error_t chomsky3_ast_char_class_add_range(
    chomsky3_ast_node_t *node,
    uint32_t start,
    uint32_t end
) {
    if (!node || (node->type != CHOMSKY3_AST_CHAR_CLASS && 
                  node->type != CHOMSKY3_AST_NEGATED_CHAR_CLASS)) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    if (start > end) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Grow array if needed */
    if (node->data.char_class.num_ranges >= node->data.char_class.capacity) {
        size_t new_capacity = node->data.char_class.capacity == 0 ? 
            INITIAL_CAPACITY : node->data.char_class.capacity * 2;
        
        chomsky3_char_range_t *new_ranges = realloc(
            node->data.char_class.ranges,
            new_capacity * sizeof(chomsky3_char_range_t)
        );
        if (!new_ranges) {
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }

        node->data.char_class.ranges = new_ranges;
        node->data.char_class.capacity = new_capacity;
    }

    /* Add range */
    node->data.char_class.ranges[node->data.char_class.num_ranges].start = start;
    node->data.char_class.ranges[node->data.char_class.num_ranges].end = end;
    node->data.char_class.num_ranges++;

    return CHOMSKY3_OK;
}

/**
 * Create a quantifier node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_quantifier(
    chomsky3_ast_node_t *child,
    uint32_t min,
    uint32_t max,
    bool greedy,
    bool possessive
) {
    if (!child) {
        return NULL;
    }

    chomsky3_ast_node_type_t type;
    if (possessive) {
        if (min == 0 && max == 1) {
            type = CHOMSKY3_AST_ZERO_OR_ONE_POSSESSIVE;
        } else if (min == 0 && max == UINT32_MAX) {
            type = CHOMSKY3_AST_ZERO_OR_MORE_POSSESSIVE;
        } else if (min == 1 && max == UINT32_MAX) {
            type = CHOMSKY3_AST_ONE_OR_MORE_POSSESSIVE;
        } else {
            type = CHOMSKY3_AST_REPEAT_POSSESSIVE;
        }
    } else if (!greedy) {
        if (min == 0 && max == 1) {
            type = CHOMSKY3_AST_ZERO_OR_ONE_LAZY;
        } else if (min == 0 && max == UINT32_MAX) {
            type = CHOMSKY3_AST_ZERO_OR_MORE_LAZY;
        } else if (min == 1 && max == UINT32_MAX) {
            type = CHOMSKY3_AST_ONE_OR_MORE_LAZY;
        } else {
            type = CHOMSKY3_AST_REPEAT_LAZY;
        }
    } else {
        if (min == 0 && max == 1) {
            type = CHOMSKY3_AST_ZERO_OR_ONE;
        } else if (min == 0 && max == UINT32_MAX) {
            type = CHOMSKY3_AST_ZERO_OR_MORE;
        } else if (min == 1 && max == UINT32_MAX) {
            type = CHOMSKY3_AST_ONE_OR_MORE;
        } else {
            type = CHOMSKY3_AST_REPEAT;
        }
    }

    chomsky3_ast_node_t *node = chomsky3_ast_node_create(type);
    if (!node) {
        return NULL;
    }

    node->data.quantifier.child = child;
    node->data.quantifier.bounds.min = min;
    node->data.quantifier.bounds.max = max;
    node->data.quantifier.greedy = greedy;
    node->data.quantifier.possessive = possessive;

    child->parent = node;

    return node;
}

/**
 * Create a group node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_group(
    chomsky3_ast_node_t *child,
    uint32_t group_id,
    const char *name,
    bool capturing
) {
    if (!child) {
        return NULL;
    }

    chomsky3_ast_node_type_t type;
    if (name) {
        type = CHOMSKY3_AST_NAMED_GROUP;
    } else if (capturing) {
        type = CHOMSKY3_AST_GROUP;
    } else {
        type = CHOMSKY3_AST_NON_CAPTURING_GROUP;
    }

    chomsky3_ast_node_t *node = chomsky3_ast_node_create(type);
    if (!node) {
        return NULL;
    }

    node->data.group.child = child;
    node->data.group.info.group_id = group_id;
    node->data.group.info.capturing = capturing;
    node->data.group.info.name = name ? strdup(name) : NULL;

    if (name && !node->data.group.info.name) {
        free(node);
        return NULL;
    }

    child->parent = node;

    return node;
}

/**
 * Create an alternation node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_alternation(
    chomsky3_ast_node_t **alternatives,
    size_t num_alternatives
) {
    if (!alternatives || num_alternatives == 0) {
        return NULL;
    }

    chomsky3_ast_node_t *node = chomsky3_ast_node_create(CHOMSKY3_AST_ALTERNATION);
    if (!node) {
        return NULL;
    }

    node->data.alternation.alternatives = malloc(
        num_alternatives * sizeof(chomsky3_ast_node_t *)
    );
    if (!node->data.alternation.alternatives) {
        free(node);
        return NULL;
    }

    memcpy(
        node->data.alternation.alternatives,
        alternatives,
        num_alternatives * sizeof(chomsky3_ast_node_t *)
    );
    node->data.alternation.num_alternatives = num_alternatives;

    for (size_t i = 0; i < num_alternatives; i++) {
        alternatives[i]->parent = node;
    }

    return node;
}

/**
 * Create a concatenation node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_concatenation(
    chomsky3_ast_node_t **children,
    size_t num_children
) {
    if (!children || num_children == 0) {
        return NULL;
    }

    chomsky3_ast_node_t *node = chomsky3_ast_node_create(CHOMSKY3_AST_CONCATENATION);
    if (!node) {
        return NULL;
    }

    node->data.concatenation.children = malloc(
        num_children * sizeof(chomsky3_ast_node_t *)
    );
    if (!node->data.concatenation.children) {
        free(node);
        return NULL;
    }

    memcpy(
        node->data.concatenation.children,
        children,
        num_children * sizeof(chomsky3_ast_node_t *)
    );
    node->data.concatenation.num_children = num_children;

    for (size_t i = 0; i < num_children; i++) {
        children[i]->parent = node;
    }

    return node;
}

/**
 * Create a backreference node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_backref(
    uint32_t group_id,
    const char *group_name
) {
    chomsky3_ast_node_t *node = chomsky3_ast_node_create(
        group_name ? CHOMSKY3_AST_NAMED_BACKREF : CHOMSKY3_AST_BACKREF
    );
    if (!node) {
        return NULL;
    }

    node->data.backref.group_id = group_id;
    node->data.backref.group_name = group_name ? strdup(group_name) : NULL;
    if (group_name && !node->data.backref.group_name) {
        chomsky3_ast_node_free(node);
        return NULL;
    }
    return node;
}

/**
 * Create a lookaround assertion node.
 */
chomsky3_ast_node_t *chomsky3_ast_node_create_lookaround(
    chomsky3_ast_node_t *child,
    bool positive,
    bool ahead
) {
    chomsky3_ast_node_type_t type;
    if (ahead) {
        type = positive ? CHOMSKY3_AST_LOOKAHEAD : CHOMSKY3_AST_NEGATIVE_LOOKAHEAD;
    } else {
        type = positive ? CHOMSKY3_AST_LOOKBEHIND : CHOMSKY3_AST_NEGATIVE_LOOKBEHIND;
    }

    chomsky3_ast_node_t *node = chomsky3_ast_node_create(type);
    if (!node) {
        return NULL;
    }
    node->data.lookaround.child = child;
    node->data.lookaround.positive = positive;
    node->data.lookaround.ahead = ahead;
    if (child) {
        child->parent = node;
    }
    return node;
}

chomsky3_ast_node_t *chomsky3_ast_node_create_conditional(
    chomsky3_ast_node_t *condition,
    chomsky3_ast_node_t *true_branch,
    chomsky3_ast_node_t *false_branch
) {
    chomsky3_ast_node_t *node = chomsky3_ast_node_create(CHOMSKY3_AST_CONDITIONAL);
    if (!node) {
        return NULL;
    }
    node->data.conditional.condition = condition;
    node->data.conditional.true_branch = true_branch;
    node->data.conditional.false_branch = false_branch;
    if (condition) condition->parent = node;
    if (true_branch) true_branch->parent = node;
    if (false_branch) false_branch->parent = node;
    return node;
}

chomsky3_ast_node_t *chomsky3_ast_node_create_unicode_property(
    const char *property_name,
    const char *property_value,
    bool negated
) {
    if (!property_name) {
        return NULL;
    }
    chomsky3_ast_node_t *node = chomsky3_ast_node_create(
        negated ? CHOMSKY3_AST_NEGATED_UNICODE_PROPERTY : CHOMSKY3_AST_UNICODE_PROPERTY
    );
    if (!node) {
        return NULL;
    }
    node->data.unicode_property.property_name = strdup(property_name);
    node->data.unicode_property.property_value = property_value ? strdup(property_value) : NULL;
    node->data.unicode_property.negated = negated;
    if (!node->data.unicode_property.property_name ||
        (property_value && !node->data.unicode_property.property_value)) {
        chomsky3_ast_node_free(node);
        return NULL;
    }
    return node;
}

const char *chomsky3_ast_node_type_name(chomsky3_ast_node_type_t type) {
    switch (type) {
        case CHOMSKY3_AST_LITERAL: return "literal";
        case CHOMSKY3_AST_STRING: return "string";
        case CHOMSKY3_AST_CHAR_CLASS: return "char_class";
        case CHOMSKY3_AST_NEGATED_CHAR_CLASS: return "negated_char_class";
        case CHOMSKY3_AST_DOT: return "dot";
        case CHOMSKY3_AST_GROUP: return "group";
        case CHOMSKY3_AST_NON_CAPTURING_GROUP: return "non_capturing_group";
        case CHOMSKY3_AST_NAMED_GROUP: return "named_group";
        case CHOMSKY3_AST_ALTERNATION: return "alternation";
        case CHOMSKY3_AST_CONCATENATION: return "concatenation";
        case CHOMSKY3_AST_BACKREF: return "backref";
        case CHOMSKY3_AST_NAMED_BACKREF: return "named_backref";
        case CHOMSKY3_AST_LOOKAHEAD: return "lookahead";
        case CHOMSKY3_AST_NEGATIVE_LOOKAHEAD: return "negative_lookahead";
        case CHOMSKY3_AST_LOOKBEHIND: return "lookbehind";
        case CHOMSKY3_AST_NEGATIVE_LOOKBEHIND: return "negative_lookbehind";
        case CHOMSKY3_AST_CONDITIONAL: return "conditional";
        case CHOMSKY3_AST_UNICODE_PROPERTY: return "unicode_property";
        case CHOMSKY3_AST_NEGATED_UNICODE_PROPERTY: return "negated_unicode_property";
        case CHOMSKY3_AST_EMPTY: return "empty";
        case CHOMSKY3_AST_FAIL: return "fail";
        default: return "unknown";
    }
}

bool chomsky3_ast_node_is_quantifier(chomsky3_ast_node_type_t type) {
    return type == CHOMSKY3_AST_ZERO_OR_MORE ||
           type == CHOMSKY3_AST_ONE_OR_MORE ||
           type == CHOMSKY3_AST_ZERO_OR_ONE ||
           type == CHOMSKY3_AST_REPEAT ||
           type == CHOMSKY3_AST_ZERO_OR_MORE_LAZY ||
           type == CHOMSKY3_AST_ONE_OR_MORE_LAZY ||
           type == CHOMSKY3_AST_ZERO_OR_ONE_LAZY ||
           type == CHOMSKY3_AST_REPEAT_LAZY ||
           type == CHOMSKY3_AST_ZERO_OR_MORE_POSSESSIVE ||
           type == CHOMSKY3_AST_ONE_OR_MORE_POSSESSIVE ||
           type == CHOMSKY3_AST_ZERO_OR_ONE_POSSESSIVE ||
           type == CHOMSKY3_AST_REPEAT_POSSESSIVE;
}

bool chomsky3_ast_node_is_anchor(chomsky3_ast_node_type_t type) {
    return type == CHOMSKY3_AST_START_ANCHOR ||
           type == CHOMSKY3_AST_END_ANCHOR ||
           type == CHOMSKY3_AST_WORD_BOUNDARY ||
           type == CHOMSKY3_AST_NON_WORD_BOUNDARY ||
           type == CHOMSKY3_AST_START_TEXT ||
           type == CHOMSKY3_AST_END_TEXT ||
           type == CHOMSKY3_AST_END_TEXT_NEWLINE;
}

bool chomsky3_ast_node_is_lookaround(chomsky3_ast_node_type_t type) {
    return type == CHOMSKY3_AST_LOOKAHEAD ||
           type == CHOMSKY3_AST_NEGATIVE_LOOKAHEAD ||
           type == CHOMSKY3_AST_LOOKBEHIND ||
           type == CHOMSKY3_AST_NEGATIVE_LOOKBEHIND;
}

chomsky3_error_t chomsky3_ast_traverse(
    chomsky3_ast_node_t *node,
    chomsky3_ast_visitor_t callback,
    void *user_data
) {
    if (!node || !callback) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    return traverse_recursive(node, callback, user_data);
}

void chomsky3_ast_print(
    const chomsky3_ast_t *ast,
    FILE *stream,
    int indent_level
) {
    if (ast && ast->root) {
        chomsky3_ast_node_print(ast->root, stream, indent_level);
    }
}

void chomsky3_ast_node_print(
    const chomsky3_ast_node_t *node,
    FILE *stream,
    int indent_level
) {
    if (!stream) {
        stream = stderr;
    }
    print_node_recursive(node, stream, indent_level);
}

chomsky3_error_t chomsky3_ast_validate(const chomsky3_ast_t *ast) {
    if (!ast) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    return ast->root ? validate_node(ast->root, ast) : CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_ast_get_stats(
    const chomsky3_ast_t *ast,
    size_t *num_nodes,
    size_t *max_depth,
    uint32_t *num_groups
) {
    if (!ast) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    if (num_nodes) *num_nodes = 0;
    if (max_depth) *max_depth = 0;
    if (ast->root && num_nodes && max_depth) {
        get_node_stats(ast->root, num_nodes, max_depth, 1);
    }
    if (num_groups) {
        *num_groups = ast->num_groups;
    }
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_ast_register_named_group(
    chomsky3_ast_t *ast,
    const char *name,
    uint32_t group_id
) {
    if (!ast || !name) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    size_t count = ast->named_groups.count;
    char **names = realloc(ast->named_groups.names, (count + 1) * sizeof(*names));
    if (!names) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    ast->named_groups.names = names;
    uint32_t *ids = realloc(ast->named_groups.group_ids, (count + 1) * sizeof(*ids));
    if (!ids) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    ast->named_groups.group_ids = ids;
    ast->named_groups.names[count] = strdup(name);
    if (!ast->named_groups.names[count]) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    ast->named_groups.group_ids[count] = group_id;
    ast->named_groups.count = count + 1;
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_ast_lookup_named_group(
    const chomsky3_ast_t *ast,
    const char *name,
    uint32_t *group_id
) {
    if (!ast || !name || !group_id) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < ast->named_groups.count; i++) {
        if (strcmp(ast->named_groups.names[i], name) == 0) {
            *group_id = ast->named_groups.group_ids[i];
            return CHOMSKY3_OK;
        }
    }
    return CHOMSKY3_ERR_INVALID_ARGUMENT;
}

static chomsky3_error_t traverse_recursive(
    chomsky3_ast_node_t *node,
    chomsky3_ast_visitor_t callback,
    void *user_data
) {
    if (!node) {
        return CHOMSKY3_OK;
    }
    chomsky3_error_t err = callback(node, user_data);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    switch (node->type) {
        case CHOMSKY3_AST_GROUP:
        case CHOMSKY3_AST_NON_CAPTURING_GROUP:
        case CHOMSKY3_AST_NAMED_GROUP:
        case CHOMSKY3_AST_ATOMIC_GROUP:
            return traverse_recursive(node->data.group.child, callback, user_data);
        case CHOMSKY3_AST_CONCATENATION:
            for (size_t i = 0; i < node->data.concatenation.num_children; i++) {
                err = traverse_recursive(node->data.concatenation.children[i], callback, user_data);
                if (err != CHOMSKY3_OK) return err;
            }
            break;
        case CHOMSKY3_AST_ALTERNATION:
            for (size_t i = 0; i < node->data.alternation.num_alternatives; i++) {
                err = traverse_recursive(node->data.alternation.alternatives[i], callback, user_data);
                if (err != CHOMSKY3_OK) return err;
            }
            break;
        default:
            break;
    }
    return CHOMSKY3_OK;
}

static void print_indent(FILE *stream, int level) {
    for (int i = 0; i < level; i++) {
        fputs("  ", stream);
    }
}

static void print_node_recursive(
    const chomsky3_ast_node_t *node,
    FILE *stream,
    int indent_level
) {
    if (!node) {
        return;
    }
    print_indent(stream, indent_level);
    fprintf(stream, "%s\n", chomsky3_ast_node_type_name(node->type));
}

static chomsky3_error_t validate_node(
    const chomsky3_ast_node_t *node,
    const chomsky3_ast_t *ast
) {
    (void)ast;
    return node ? CHOMSKY3_OK : CHOMSKY3_ERR_INVALID_ARGUMENT;
}

static void get_node_stats(
    const chomsky3_ast_node_t *node,
    size_t *num_nodes,
    size_t *max_depth,
    size_t current_depth
) {
    if (!node) {
        return;
    }
    (*num_nodes)++;
    if (current_depth > *max_depth) {
        *max_depth = current_depth;
    }
}
