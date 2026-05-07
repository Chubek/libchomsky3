#include "chomsky3/ast.h"
#include "chomsky3/regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ERE-specific parsing: handles POSIX Extended Regular Expression syntax
// Differences from basic regex:
// - No backslash needed for |, +, ?, {}, ()
// - Backslash escapes these to make them literal
// - Character classes use [:class:] syntax

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
    char *error;
} ere_parser_t;

static void set_error(ere_parser_t *p, const char *msg) {
    if (p->error) free(p->error);
    p->error = strdup(msg);
}

static int peek(ere_parser_t *p) {
    if (p->pos >= p->len) return -1;
    return p->input[p->pos];
}

static int consume(ere_parser_t *p) {
    if (p->pos >= p->len) return -1;
    return p->input[p->pos++];
}

// Forward declarations
static chomsky3_ast_node_t* parse_alternation(ere_parser_t *p);
static chomsky3_ast_node_t* parse_concatenation(ere_parser_t *p);
static chomsky3_ast_node_t* parse_quantified(ere_parser_t *p);
static chomsky3_ast_node_t* parse_atom(ere_parser_t *p);

// Parse POSIX character class [:name:]
static chomsky3_char_class_type_t parse_posix_class(ere_parser_t *p) {
    if (peek(p) != '[') return CHOMSKY3_CC_NONE;
    
    size_t start = p->pos;
    consume(p); // '['
    
    if (peek(p) != ':') {
        p->pos = start;
        return CHOMSKY3_CC_NONE;
    }
    consume(p); // ':'
    
    char name[32];
    int i = 0;
    while (peek(p) != ':' && peek(p) != -1 && i < 31) {
        name[i++] = consume(p);
    }
    name[i] = '\0';
    
    if (peek(p) != ':') {
        p->pos = start;
        return CHOMSKY3_CC_NONE;
    }
    consume(p); // ':'
    
    if (peek(p) != ']') {
        p->pos = start;
        return CHOMSKY3_CC_NONE;
    }
    consume(p); // ']'
    
    // Map POSIX class names
    if (strcmp(name, "alnum") == 0) return CHOMSKY3_CC_ALNUM;
    if (strcmp(name, "alpha") == 0) return CHOMSKY3_CC_ALPHA;
    if (strcmp(name, "blank") == 0) return CHOMSKY3_CC_BLANK;
    if (strcmp(name, "cntrl") == 0) return CHOMSKY3_CC_CNTRL;
    if (strcmp(name, "digit") == 0) return CHOMSKY3_CC_DIGIT;
    if (strcmp(name, "graph") == 0) return CHOMSKY3_CC_GRAPH;
    if (strcmp(name, "lower") == 0) return CHOMSKY3_CC_LOWER;
    if (strcmp(name, "print") == 0) return CHOMSKY3_CC_PRINT;
    if (strcmp(name, "punct") == 0) return CHOMSKY3_CC_PUNCT;
    if (strcmp(name, "space") == 0) return CHOMSKY3_CC_SPACE;
    if (strcmp(name, "upper") == 0) return CHOMSKY3_CC_UPPER;
    if (strcmp(name, "xdigit") == 0) return CHOMSKY3_CC_XDIGIT;
    
    p->pos = start;
    return CHOMSKY3_CC_NONE;
}

// Parse character class [...]
static chomsky3_ast_node_t* parse_char_class(ere_parser_t *p) {
    if (peek(p) != '[') return NULL;
    consume(p); // '['
    
    int negated = 0;
    if (peek(p) == '^') {
        negated = 1;
        consume(p);
    }
    
    chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_CHAR_CLASS;
    node->data.char_class.negated = negated;
    node->data.char_class.ranges = NULL;
    node->data.char_class.range_count = 0;
    
    size_t capacity = 8;
    node->data.char_class.ranges = malloc(capacity * sizeof(chomsky3_char_range_t));
    
    while (peek(p) != ']' && peek(p) != -1) {
        // Check for POSIX class
        chomsky3_char_class_type_t posix = parse_posix_class(p);
        if (posix != CHOMSKY3_CC_NONE) {
            // Add POSIX class as special range
            if (node->data.char_class.range_count >= capacity) {
                capacity *= 2;
                node->data.char_class.ranges = realloc(node->data.char_class.ranges, 
                                                       capacity * sizeof(chomsky3_char_range_t));
            }
            node->data.char_class.ranges[node->data.char_class.range_count].start = posix;
            node->data.char_class.ranges[node->data.char_class.range_count].end = posix;
            node->data.char_class.range_count++;
            continue;
        }
        
        int start = consume(p);
        
        if (peek(p) == '-' && p->pos + 1 < p->len && p->input[p->pos + 1] != ']') {
            consume(p); // '-'
            int end = consume(p);
            
            if (node->data.char_class.range_count >= capacity) {
                capacity *= 2;
                node->data.char_class.ranges = realloc(node->data.char_class.ranges,
                                                       capacity * sizeof(chomsky3_char_range_t));
            }
            node->data.char_class.ranges[node->data.char_class.range_count].start = start;
            node->data.char_class.ranges[node->data.char_class.range_count].end = end;
            node->data.char_class.range_count++;
        } else {
            if (node->data.char_class.range_count >= capacity) {
                capacity *= 2;
                node->data.char_class.ranges = realloc(node->data.char_class.ranges,
                                                       capacity * sizeof(chomsky3_char_range_t));
            }
            node->data.char_class.ranges[node->data.char_class.range_count].start = start;
            node->data.char_class.ranges[node->data.char_class.range_count].end = start;
            node->data.char_class.range_count++;
        }
    }
    
    if (peek(p) != ']') {
        set_error(p, "Unclosed character class");
        free(node->data.char_class.ranges);
        free(node);
        return NULL;
    }
    consume(p); // ']'
    
    return node;
}

// Parse atom: literal, ., [...], (...), escape
static chomsky3_ast_node_t* parse_atom(ere_parser_t *p) {
    int c = peek(p);
    
    if (c == -1) return NULL;
    
    // Dot
    if (c == '.') {
        consume(p);
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_DOT;
        return node;
    }
    
    // Character class
    if (c == '[') {
        return parse_char_class(p);
    }
    
    // Group
    if (c == '(') {
        consume(p);
        chomsky3_ast_node_t *inner = parse_alternation(p);
        if (!inner) return NULL;
        
        if (peek(p) != ')') {
            set_error(p, "Unclosed group");
            chomsky3_free_ast(inner);
            return NULL;
        }
        consume(p);
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_GROUP;
        node->data.group.capturing = 1;
        node->data.group.child = inner;
        return node;
    }
    
    // Escape sequence
    if (c == '\\') {
        consume(p);
        int next = consume(p);
        if (next == -1) {
            set_error(p, "Trailing backslash");
            return NULL;
        }
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_ESCAPE;
        
        // In ERE, backslash makes metacharacters literal
        if (strchr("|+?{}()[].*^$\\", next)) {
            node->data.escape.type = CHOMSKY3_ESCAPE_LITERAL;
            node->data.escape.value = next;
        } else {
            // Standard escape sequences
            switch (next) {
                case 'n': node->data.escape.type = CHOMSKY3_ESCAPE_NEWLINE; break;
                case 't': node->data.escape.type = CHOMSKY3_ESCAPE_TAB; break;
                case 'r': node->data.escape.type = CHOMSKY3_ESCAPE_CARRIAGE_RETURN; break;
                case 'd': node->data.escape.type = CHOMSKY3_ESCAPE_DIGIT; break;
                case 'D': node->data.escape.type = CHOMSKY3_ESCAPE_NON_DIGIT; break;
                case 's': node->data.escape.type = CHOMSKY3_ESCAPE_WHITESPACE; break;
                case 'S': node->data.escape.type = CHOMSKY3_ESCAPE_NON_WHITESPACE; break;
                case 'w': node->data.escape.type = CHOMSKY3_ESCAPE_WORD; break;
                case 'W': node->data.escape.type = CHOMSKY3_ESCAPE_NON_WORD; break;
                default:
                    node->data.escape.type = CHOMSKY3_ESCAPE_LITERAL;
                    node->data.escape.value = next;
            }
        }
        return node;
    }
    
    // Anchors
    if (c == '^') {
        consume(p);
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_ANCHOR;
        node->data.anchor.type = CHOMSKY3_ANCHOR_START;
        return node;
    }
    
    if (c == '$') {
        consume(p);
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_ANCHOR;
        node->data.anchor.type = CHOMSKY3_ANCHOR_END;
        return node;
    }
    
    // Literal (not a metacharacter in ERE context)
    if (!strchr("|+?*{}()[].", c)) {
        consume(p);
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_LITERAL;
        node->data.literal.value = c;
        return node;
    }
    
    return NULL;
}

// Parse quantified expression
static chomsky3_ast_node_t* parse_quantified(ere_parser_t *p) {
    chomsky3_ast_node_t *atom = parse_atom(p);
    if (!atom) return NULL;
    
    int c = peek(p);
    
    if (c == '*' || c == '+' || c == '?') {
        consume(p);
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_QUANTIFIER;
        node->data.quantifier.child = atom;
        node->data.quantifier.greedy = 1;
        
        if (c == '*') {
            node->data.quantifier.min = 0;
            node->data.quantifier.max = -1;
        } else if (c == '+') {
            node->data.quantifier.min = 1;
            node->data.quantifier.max = -1;
        } else { // '?'
            node->data.quantifier.min = 0;
            node->data.quantifier.max = 1;
        }
        
        // Check for lazy modifier
        if (peek(p) == '?') {
            consume(p);
            node->data.quantifier.greedy = 0;
        }
        
        return node;
    }
    
    if (c == '{') {
        consume(p);
        
        // Parse {m,n}
        int min = 0, max = -1;
        while (isdigit(peek(p))) {
            min = min * 10 + (consume(p) - '0');
        }
        
        if (peek(p) == ',') {
            consume(p);
            if (isdigit(peek(p))) {
                max = 0;
                while (isdigit(peek(p))) {
                    max = max * 10 + (consume(p) - '0');
                }
            }
        } else {
            max = min;
        }
        
        if (peek(p) != '}') {
            set_error(p, "Unclosed quantifier");
            chomsky3_free_ast(atom);
            return NULL;
        }
        consume(p);
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_QUANTIFIER;
        node->data.quantifier.child = atom;
        node->data.quantifier.min = min;
        node->data.quantifier.max = max;
        node->data.quantifier.greedy = 1;
        
        if (peek(p) == '?') {
            consume(p);
            node->data.quantifier.greedy = 0;
        }
        
        return node;
    }
    
    return atom;
}

// Parse concatenation
static chomsky3_ast_node_t* parse_concatenation(ere_parser_t *p) {
    chomsky3_ast_node_t *left = parse_quantified(p);
    if (!left) return NULL;
    
    while (1) {
        int c = peek(p);
        if (c == -1 || c == '|' || c == ')') break;
        
        chomsky3_ast_node_t *right = parse_quantified(p);
        if (!right) break;
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_CONCATENATION;
        node->data.concatenation.left = left;
        node->data.concatenation.right = right;
        left = node;
    }
    
    return left;
}

// Parse alternation
static chomsky3_ast_node_t* parse_alternation(ere_parser_t *p) {
    chomsky3_ast_node_t *left = parse_concatenation(p);
    if (!left) return NULL;
    
    while (peek(p) == '|') {
        consume(p);
        chomsky3_ast_node_t *right = parse_concatenation(p);
        if (!right) {
            chomsky3_free_ast(left);
            return NULL;
        }
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_ALTERNATION;
        node->data.alternation.left = left;
        node->data.alternation.right = right;
        left = node;
    }
    
    return left;
}

// Public API
chomsky3_ast_node_t* chomsky3_parse_ere(const char *pattern, char **error_msg) {
    if (!pattern) {
        if (error_msg) *error_msg = strdup("NULL pattern");
        return NULL;
    }
    
    ere_parser_t parser = {
        .input = pattern,
        .pos = 0,
        .len = strlen(pattern),
        .error = NULL
    };
    
    chomsky3_ast_node_t *ast = parse_alternation(&parser);
    
    if (!ast || parser.pos < parser.len) {
        if (error_msg) {
            if (parser.error) {
                *error_msg = parser.error;
            } else {
                *error_msg = strdup("Parse error");
            }
        } else {
            free(parser.error);
        }
        if (ast) chomsky3_free_ast(ast);
        return NULL;
    }
    
    free(parser.error);
    return ast;
}
