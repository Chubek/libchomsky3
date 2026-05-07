#include "chomsky3/ast.h"
#include "chomsky3/regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// PCRE parser: supports Perl-Compatible Regular Expression syntax
// Extensions beyond basic regex:
// - Named groups: (?<name>...) or (?'name'...)
// - Non-capturing groups: (?:...)
// - Lookaheads: (?=...) and (?!...)
// - Lookbehinds: (?<=...) and (?<!...)
// - Atomic groups: (?>...)
// - Possessive quantifiers: *+, ++, ?+, {m,n}+
// - Inline modifiers: (?i), (?-i), etc.
// - Unicode escapes: \x{...}, \u{...}
// - Additional escapes: \h, \H, \v, \V, \R, \N

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
    char *error;
} pcre_parser_t;

static void set_error(pcre_parser_t *p, const char *msg) {
    if (p->error) free(p->error);
    p->error = strdup(msg);
}

static int peek(pcre_parser_t *p) {
    if (p->pos >= p->len) return -1;
    return p->input[p->pos];
}

static int consume(pcre_parser_t *p) {
    if (p->pos >= p->len) return -1;
    return p->input[p->pos++];
}

static int peek_ahead(pcre_parser_t *p, size_t offset) {
    if (p->pos + offset >= p->len) return -1;
    return p->input[p->pos + offset];
}

// Forward declarations
static chomsky3_ast_node_t* parse_alternation(pcre_parser_t *p);
static chomsky3_ast_node_t* parse_concatenation(pcre_parser_t *p);
static chomsky3_ast_node_t* parse_quantified(pcre_parser_t *p);
static chomsky3_ast_node_t* parse_atom(pcre_parser_t *p);

// Parse hex escape \xHH or \x{HHHH}
static int parse_hex_escape(pcre_parser_t *p) {
    if (peek(p) == '{') {
        consume(p); // '{'
        int value = 0;
        int digits = 0;
        while (isxdigit(peek(p)) && digits < 8) {
            int c = consume(p);
            value = value * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
            digits++;
        }
        if (peek(p) != '}') {
            set_error(p, "Unclosed hex escape");
            return -1;
        }
        consume(p); // '}'
        return value;
    } else {
        // \xHH
        int value = 0;
        for (int i = 0; i < 2; i++) {
            if (!isxdigit(peek(p))) break;
            int c = consume(p);
            value = value * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
        }
        return value;
    }
}

// Parse Unicode escape \u{HHHH}
static int parse_unicode_escape(pcre_parser_t *p) {
    if (peek(p) == '{') {
        consume(p); // '{'
        int value = 0;
        int digits = 0;
        while (isxdigit(peek(p)) && digits < 6) {
            int c = consume(p);
            value = value * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
            digits++;
        }
        if (peek(p) != '}') {
            set_error(p, "Unclosed unicode escape");
            return -1;
        }
        consume(p); // '}'
        return value;
    }
    return -1;
}

// Parse character class [...]
static chomsky3_ast_node_t* parse_char_class(pcre_parser_t *p) {
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
        int start;
        
        // Handle escape sequences in character class
        if (peek(p) == '\\') {
            consume(p);
            int c = consume(p);
            switch (c) {
                case 'n': start = '\n'; break;
                case 't': start = '\t'; break;
                case 'r': start = '\r'; break;
                case 'x': start = parse_hex_escape(p); break;
                case 'u': start = parse_unicode_escape(p); break;
                default: start = c;
            }
        } else {
            start = consume(p);
        }
        
        if (peek(p) == '-' && peek_ahead(p, 1) != ']') {
            consume(p); // '-'
            int end;
            
            if (peek(p) == '\\') {
                consume(p);
                int c = consume(p);
                switch (c) {
                    case 'n': end = '\n'; break;
                    case 't': end = '\t'; break;
                    case 'r': end = '\r'; break;
                    case 'x': end = parse_hex_escape(p); break;
                    case 'u': end = parse_unicode_escape(p); break;
                    default: end = c;
                }
            } else {
                end = consume(p);
            }
            
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

// Parse group extensions: (?...) constructs
static chomsky3_ast_node_t* parse_group(pcre_parser_t *p) {
    if (peek(p) != '(') return NULL;
    consume(p); // '('
    
    chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_GROUP;
    node->data.group.capturing = 1;
    node->data.group.name = NULL;
    
    // Check for group extensions
    if (peek(p) == '?') {
        consume(p); // '?'
        int c = peek(p);
        
        // Non-capturing group: (?:...)
        if (c == ':') {
            consume(p);
            node->data.group.capturing = 0;
        }
        // Named group: (?<name>...) or (?'name'...)
        else if (c == '<' || c == '\'') {
            char delim = (c == '<') ? '>' : '\'';
            consume(p);
            
            char name[256];
            int i = 0;
            while (peek(p) != delim && peek(p) != -1 && i < 255) {
                name[i++] = consume(p);
            }
            name[i] = '\0';
            
            if (peek(p) != delim) {
                set_error(p, "Unclosed named group");
                free(node);
                return NULL;
            }
            consume(p);
            
            node->data.group.name = strdup(name);
        }
        // Lookahead: (?=...) positive, (?!...) negative
        else if (c == '=' || c == '!') {
            consume(p);
            node->type = CHOMSKY3_AST_LOOKAHEAD;
            node->data.lookahead.positive = (c == '=');
        }
        // Lookbehind: (?<=...) positive, (?<!...) negative
        else if (c == '<') {
            consume(p);
            if (peek(p) == '=' || peek(p) == '!') {
                int type = consume(p);
                node->type = CHOMSKY3_AST_LOOKBEHIND;
                node->data.lookbehind.positive = (type == '=');
            } else {
                set_error(p, "Invalid lookbehind syntax");
                free(node);
                return NULL;
            }
        }
        // Atomic group: (?>...)
        else if (c == '>') {
            consume(p);
            node->type = CHOMSKY3_AST_ATOMIC_GROUP;
        }
        // Inline modifiers: (?i), (?-i), etc.
        else if (strchr("imsxADSUXJ", c) || c == '-') {
            // Parse modifiers (simplified - just consume for now)
            while (strchr("imsxADSUXJ-", peek(p))) {
                consume(p);
            }
            if (peek(p) == ')') {
                consume(p);
                // Return a modifier node (could extend AST for this)
                node->type = CHOMSKY3_AST_GROUP;
                node->data.group.capturing = 0;
                node->data.group.child = NULL;
                return node;
            }
            if (peek(p) == ':') {
                consume(p);
                node->data.group.capturing = 0;
            }
        }
    }
    
    chomsky3_ast_node_t *inner = parse_alternation(p);
    if (!inner) {
        free(node->data.group.name);
        free(node);
        return NULL;
    }
    
    if (peek(p) != ')') {
        set_error(p, "Unclosed group");
        chomsky3_free_ast(inner);
        free(node->data.group.name);
        free(node);
        return NULL;
    }
    consume(p); // ')'
    
    if (node->type == CHOMSKY3_AST_GROUP) {
        node->data.group.child = inner;
    } else if (node->type == CHOMSKY3_AST_LOOKAHEAD) {
        node->data.lookahead.child = inner;
    } else if (node->type == CHOMSKY3_AST_LOOKBEHIND) {
        node->data.lookbehind.child = inner;
    } else if (node->type == CHOMSKY3_AST_ATOMIC_GROUP) {
        node->data.atomic_group.child = inner;
    }
    
    return node;
}

// Parse escape sequences
static chomsky3_ast_node_t* parse_escape(pcre_parser_t *p) {
    if (peek(p) != '\\') return NULL;
    consume(p); // '\'
    
    int c = consume(p);
    if (c == -1) {
        set_error(p, "Trailing backslash");
        return NULL;
    }
    
    chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_ESCAPE;
    
    switch (c) {
        case 'n': node->data.escape.type = CHOMSKY3_ESCAPE_NEWLINE; break;
        case 't': node->data.escape.type = CHOMSKY3_ESCAPE_TAB; break;
        case 'r': node->data.escape.type = CHOMSKY3_ESCAPE_CARRIAGE_RETURN; break;
        case 'd': node->data.escape.type = CHOMSKY3_ESCAPE_DIGIT; break;
        case 'D': node->data.escape.type = CHOMSKY3_ESCAPE_NON_DIGIT; break;
        case 's': node->data.escape.type = CHOMSKY3_ESCAPE_WHITESPACE; break;
        case 'S': node->data.escape.type = CHOMSKY3_ESCAPE_NON_WHITESPACE; break;
        case 'w': node->data.escape.type = CHOMSKY3_ESCAPE_WORD; break;
        case 'W': node->data.escape.type = CHOMSKY3_ESCAPE_NON_WORD; break;
        case 'b': node->data.escape.type = CHOMSKY3_ESCAPE_WORD_BOUNDARY; break;
        case 'B': node->data.escape.type = CHOMSKY3_ESCAPE_NON_WORD_BOUNDARY; break;
        // PCRE extensions
        case 'h': node->data.escape.type = CHOMSKY3_ESCAPE_HORIZONTAL_WHITESPACE; break;
        case 'H': node->data.escape.type = CHOMSKY3_ESCAPE_NON_HORIZONTAL_WHITESPACE; break;
        case 'v': node->data.escape.type = CHOMSKY3_ESCAPE_VERTICAL_WHITESPACE; break;
        case 'V': node->data.escape.type = CHOMSKY3_ESCAPE_NON_VERTICAL_WHITESPACE; break;
        case 'R': node->data.escape.type = CHOMSKY3_ESCAPE_LINEBREAK; break;
        case 'N': node->data.escape.type = CHOMSKY3_ESCAPE_NOT_NEWLINE; break;
        case 'x':
            node->data.escape.type = CHOMSKY3_ESCAPE_HEX;
            node->data.escape.value = parse_hex_escape(p);
            break;
        case 'u':
            node->data.escape.type = CHOMSKY3_ESCAPE_UNICODE;
            node->data.escape.value = parse_unicode_escape(p);
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            // Backreference
            node->type = CHOMSKY3_AST_BACKREFERENCE;
            node->data.backreference.number = c - '0';
            // Could parse multi-digit backreferences
            while (isdigit(peek(p))) {
                node->data.backreference.number = node->data.backreference.number * 10 + (consume(p) - '0');
            }
            break;
        default:
            // Literal escape
            node->data.escape.type = CHOMSKY3_ESCAPE_LITERAL;
            node->data.escape.value = c;
    }
    
    return node;
}

// Parse atom
static chomsky3_ast_node_t* parse_atom(pcre_parser_t *p) {
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
        return parse_group(p);
    }
    
    // Escape
    if (c == '\\') {
        return parse_escape(p);
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
    
    // Literal
    if (!strchr("|*+?{}()[].", c)) {
        consume(p);
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_LITERAL;
        node->data.literal.value = c;
        return node;
    }
    
    return NULL;
}

// Parse quantified expression
static chomsky3_ast_node_t* parse_quantified(pcre_parser_t *p) {
    chomsky3_ast_node_t *atom = parse_atom(p);
    if (!atom) return NULL;
    
    int c = peek(p);
    
    if (c == '*' || c == '+' || c == '?') {
        consume(p);
        
        chomsky3_ast_node_t *node = malloc(sizeof(chomsky3_ast_node_t));
        node->type = CHOMSKY3_AST_QUANTIFIER;
        node->data.quantifier.child = atom;
        node->data.quantifier.greedy = 1;
        node->data.quantifier.possessive = 0;
        
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
        
        // Check for lazy or possessive modifier
        if (peek(p) == '?') {
            consume(p);
            node->data.quantifier.greedy = 0;
        } else if (peek(p) == '+') {
            consume(p);
            node->data.quantifier.possessive = 1;
        }
        
        return node;
    }
    
    if (c == '{') {
        consume(p);
        
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
        node->data.quantifier.possessive = 0;
        
        if (peek(p) == '?') {
            consume(p);
            node->data.quantifier.greedy = 0;
        } else if (peek(p) == '+') {
            consume(p);
            node->data.quantifier.possessive = 1;
        }
        
        return node;
    }
    
    return atom;
}

// Parse concatenation
static chomsky3_ast_node_t* parse_concatenation(pcre_parser_t *p) {
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
static chomsky3_ast_node_t* parse_alternation(pcre_parser_t *p) {
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
chomsky3_ast_node_t* chomsky3_parse_pcre(const char *pattern, char **error_msg) {
    if (!pattern) {
        if (error_msg) *error_msg = strdup("NULL pattern");
        return NULL;
    }
    
    pcre_parser_t parser = {
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
