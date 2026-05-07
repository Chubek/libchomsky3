#include "chomsky3/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lint warning structure
typedef struct lint_warning {
    int line;
    int column;
    char *message;
    struct lint_warning *next;
} lint_warning_t;

static lint_warning_t *warnings_head = NULL;

static void add_warning(int line, int col, const char *msg) {
    lint_warning_t *w = malloc(sizeof(lint_warning_t));
    w->line = line;
    w->column = col;
    w->message = strdup(msg);
    w->next = warnings_head;
    warnings_head = w;
}

static void free_warnings(void) {
    lint_warning_t *current = warnings_head;
    while (current) {
        lint_warning_t *next = current->next;
        free(current->message);
        free(current);
        current = next;
    }
    warnings_head = NULL;
}

// Recursive AST linting functions
static void lint_node(chomsky3_ast_node_t *node);

static void lint_alternation(chomsky3_ast_node_t *node) {
    if (!node) return;
    
    // Check for empty alternatives
    if (node->type == CHOMSKY3_AST_ALTERNATION) {
        if (!node->data.alternation.left || !node->data.alternation.right) {
            add_warning(0, 0, "Empty alternative in alternation");
        }
        lint_node(node->data.alternation.left);
        lint_node(node->data.alternation.right);
    }
}

static void lint_quantifier(chomsky3_ast_node_t *node) {
    if (!node || node->type != CHOMSKY3_AST_QUANTIFIER) return;
    
    chomsky3_ast_node_t *child = node->data.quantifier.child;
    
    // Check for nested quantifiers
    if (child && child->type == CHOMSKY3_AST_QUANTIFIER) {
        add_warning(0, 0, "Nested quantifiers are redundant or invalid");
    }
    
    // Check for quantifier on empty group
    if (child && child->type == CHOMSKY3_AST_GROUP) {
        if (!child->data.group.child) {
            add_warning(0, 0, "Quantifier applied to empty group");
        }
    }
    
    lint_node(child);
}

static void lint_group(chomsky3_ast_node_t *node) {
    if (!node || node->type != CHOMSKY3_AST_GROUP) return;
    
    // Check for empty groups
    if (!node->data.group.child) {
        if (node->data.group.capturing) {
            add_warning(0, 0, "Empty capturing group");
        } else {
            add_warning(0, 0, "Empty non-capturing group");
        }
    }
    
    lint_node(node->data.group.child);
}

static void lint_char_class(chomsky3_ast_node_t *node) {
    if (!node || node->type != CHOMSKY3_AST_CHAR_CLASS) return;
    
    // Check for empty character class
    if (!node->data.char_class.ranges || node->data.char_class.range_count == 0) {
        add_warning(0, 0, "Empty character class");
    }
    
    // Check for redundant ranges (e.g., [a-a])
    for (size_t i = 0; i < node->data.char_class.range_count; i++) {
        if (node->data.char_class.ranges[i].start == 
            node->data.char_class.ranges[i].end) {
            add_warning(0, 0, "Single-character range in character class (use literal instead)");
        }
    }
}

static void lint_escape(chomsky3_ast_node_t *node) {
    if (!node || node->type != CHOMSKY3_AST_ESCAPE) return;
    
    // Check for unnecessary escapes
    const char *unnecessary = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    if (node->data.escape.type == CHOMSKY3_ESCAPE_LITERAL) {
        if (strchr(unnecessary, node->data.escape.value)) {
            add_warning(0, 0, "Unnecessary escape sequence");
        }
    }
}

static void lint_concatenation(chomsky3_ast_node_t *node) {
    if (!node || node->type != CHOMSKY3_AST_CONCATENATION) return;
    
    lint_node(node->data.concatenation.left);
    lint_node(node->data.concatenation.right);
}

static void lint_node(chomsky3_ast_node_t *node) {
    if (!node) return;
    
    switch (node->type) {
        case CHOMSKY3_AST_ALTERNATION:
            lint_alternation(node);
            break;
        case CHOMSKY3_AST_CONCATENATION:
            lint_concatenation(node);
            break;
        case CHOMSKY3_AST_QUANTIFIER:
            lint_quantifier(node);
            break;
        case CHOMSKY3_AST_GROUP:
            lint_group(node);
            break;
        case CHOMSKY3_AST_CHAR_CLASS:
            lint_char_class(node);
            break;
        case CHOMSKY3_AST_ESCAPE:
            lint_escape(node);
            break;
        case CHOMSKY3_AST_LITERAL:
        case CHOMSKY3_AST_DOT:
        case CHOMSKY3_AST_ANCHOR:
        case CHOMSKY3_AST_BACKREFERENCE:
            // No specific linting needed for these
            break;
    }
}

// Public API
int chomsky3_lint_regex(chomsky3_ast_node_t *ast, char **error_messages, int *error_count) {
    if (!ast) return -1;
    
    // Clear previous warnings
    free_warnings();
    
    // Perform linting
    lint_node(ast);
    
    // Count warnings
    int count = 0;
    lint_warning_t *current = warnings_head;
    while (current) {
        count++;
        current = current->next;
    }
    
    if (error_count) {
        *error_count = count;
    }
    
    // Build error messages array if requested
    if (error_messages && count > 0) {
        *error_messages = malloc(count * sizeof(char*));
        current = warnings_head;
        for (int i = 0; i < count; i++) {
            (*error_messages)[i] = strdup(current->message);
            current = current->next;
        }
    }
    
    return count > 0 ? 1 : 0;
}

void chomsky3_free_lint_warnings(char **messages, int count) {
    if (!messages) return;
    
    for (int i = 0; i < count; i++) {
        free(messages[i]);
    }
    free(messages);
}
