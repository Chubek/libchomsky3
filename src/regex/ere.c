/**
 * libchomsky3 - Extended Regular Expression (ERE) Parser and AST
 *
 * Implements a recursive-descent parser for POSIX ERE patterns with
 * common Perl-style extensions (non-capturing groups, lookaheads,
 * backreferences, \b \B \d \D \w \W \s \S escapes, lazy quantifiers).
 *
 * Grammar (highest to lowest precedence):
 *
 *   pattern      := alternation
 *   alternation  := concat ('|' concat)*
 *   concat       := repeat+
 *   repeat       := atom quantifier? ('?' for lazy)
 *   quantifier   := '*' | '+' | '?' | '{m}' | '{m,}' | '{m,n}'
 *   atom         := literal | '.' | '^' | '$' | class | group | escape
 *   group        := '(' pattern ')' | '(?:' pattern ')'
 *                 | '(?=' pattern ')' | '(?!' pattern ')'
 *   class        := '[' '^'? class-items ']'
 */

#include "chomsky3/regex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Node constructors
 * ======================================================================== */

static char *dup_range(const char *pattern, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, pattern, length);
    copy[length] = '\0';
    return copy;
}

static chomsky3_regex_node_t *node_new(chomsky3_node_type_t type) {
    chomsky3_regex_node_t *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = type;
        node->greedy = true;
    }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_literal(uint32_t codepoint) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_LITERAL);
    if (node) node->data.literal = codepoint;
    return node;
}

chomsky3_regex_node_t *chomsky3_node_char_class(chomsky3_cclass_flags_t flags) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_CHAR_CLASS);
    if (node) node->data.char_class.flags = flags;
    return node;
}

bool chomsky3_char_class_add_range(chomsky3_regex_node_t *node, uint32_t start, uint32_t end) {
    if (!node || node->type != CHOMSKY3_NODE_CHAR_CLASS) return false;
    size_t count = node->data.char_class.num_ranges;
    void *ranges = realloc(node->data.char_class.ranges, (count + 1) * sizeof(*node->data.char_class.ranges));
    if (!ranges) return false;
    node->data.char_class.ranges = ranges;
    node->data.char_class.ranges[count].start = start;
    node->data.char_class.ranges[count].end = end;
    node->data.char_class.num_ranges = count + 1;
    return true;
}

chomsky3_regex_node_t *chomsky3_node_concat(chomsky3_regex_node_t *left, chomsky3_regex_node_t *right) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_CONCAT);
    if (node) { node->left = left; node->right = right; }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_alternation(chomsky3_regex_node_t *left, chomsky3_regex_node_t *right) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_ALTERNATION);
    if (node) { node->left = left; node->right = right; }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_repeat(chomsky3_regex_node_t *child, uint32_t min, uint32_t max, bool greedy) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_REPEAT);
    if (node) {
        node->left = child;
        node->data.repeat.min = min;
        node->data.repeat.max = max;
        node->greedy = greedy;
    }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_group(chomsky3_regex_node_t *child, uint32_t group_id) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_GROUP);
    if (node) { node->left = child; node->data.group_id = group_id; }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_backref(uint32_t backref_id) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_BACKREF);
    if (node) node->data.backref_id = backref_id;
    return node;
}

chomsky3_regex_node_t *chomsky3_node_anchor(chomsky3_node_type_t type) {
    if (type != CHOMSKY3_NODE_ANCHOR_START && type != CHOMSKY3_NODE_ANCHOR_END) {
        return NULL;
    }
    return node_new(type);
}

chomsky3_regex_node_t *chomsky3_node_dot(void) { return node_new(CHOMSKY3_NODE_DOT); }

void chomsky3_node_free(chomsky3_regex_node_t *node) {
    if (!node) return;
    chomsky3_node_free(node->left);
    chomsky3_node_free(node->right);
    if (node->type == CHOMSKY3_NODE_CHAR_CLASS) {
        free(node->data.char_class.ranges);
        free(node->data.char_class.bitmap);
    }
    free(node);
}

chomsky3_regex_node_t *chomsky3_node_clone(const chomsky3_regex_node_t *node) {
    if (!node) return NULL;
    chomsky3_regex_node_t *copy = node_new(node->type);
    if (!copy) return NULL;
    copy->data = node->data;
    copy->greedy = node->greedy;
    copy->position = node->position;
    if (node->type == CHOMSKY3_NODE_CHAR_CLASS) {
        copy->data.char_class.ranges = NULL;
        copy->data.char_class.bitmap = NULL;
        if (node->data.char_class.num_ranges) {
            size_t bytes = node->data.char_class.num_ranges * sizeof(*node->data.char_class.ranges);
            copy->data.char_class.ranges = malloc(bytes);
            if (copy->data.char_class.ranges) {
                memcpy(copy->data.char_class.ranges, node->data.char_class.ranges, bytes);
            } else {
                copy->data.char_class.num_ranges = 0;
            }
        }
    }
    copy->left = chomsky3_node_clone(node->left);
    copy->right = chomsky3_node_clone(node->right);
    if ((node->left && !copy->left) || (node->right && !copy->right)) {
        chomsky3_node_free(copy);
        return NULL;
    }
    return copy;
}

void chomsky3_node_print(const chomsky3_regex_node_t *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
    switch (node->type) {
        case CHOMSKY3_NODE_LITERAL:
            printf("literal '%c' (U+%04X)\n",
                   (node->data.literal >= 0x20 && node->data.literal < 0x7F)
                       ? (int)node->data.literal : '?',
                   node->data.literal);
            break;
        case CHOMSKY3_NODE_CHAR_CLASS:
            printf("char-class%s (%zu ranges)\n",
                   (node->data.char_class.flags & CHOMSKY3_CCLASS_NEGATED) ? " negated" : "",
                   node->data.char_class.num_ranges);
            break;
        case CHOMSKY3_NODE_DOT:           printf("dot\n"); break;
        case CHOMSKY3_NODE_ANCHOR_START:  printf("anchor-start\n"); break;
        case CHOMSKY3_NODE_ANCHOR_END:    printf("anchor-end\n"); break;
        case CHOMSKY3_NODE_CONCAT:        printf("concat\n"); break;
        case CHOMSKY3_NODE_ALTERNATION:   printf("alternation\n"); break;
        case CHOMSKY3_NODE_STAR:          printf("star%s\n", node->greedy ? "" : " (lazy)"); break;
        case CHOMSKY3_NODE_PLUS:          printf("plus%s\n", node->greedy ? "" : " (lazy)"); break;
        case CHOMSKY3_NODE_QUESTION:      printf("question%s\n", node->greedy ? "" : " (lazy)"); break;
        case CHOMSKY3_NODE_REPEAT:
            printf("repeat {%u,%s%u}%s\n", node->data.repeat.min,
                   node->data.repeat.max == UINT32_MAX ? "" : "",
                   node->data.repeat.max, node->greedy ? "" : " (lazy)");
            break;
        case CHOMSKY3_NODE_GROUP:         printf("group %u\n", node->data.group_id); break;
        case CHOMSKY3_NODE_BACKREF:       printf("backref %u\n", node->data.backref_id); break;
        case CHOMSKY3_NODE_LOOKAHEAD:     printf("lookahead\n"); break;
        case CHOMSKY3_NODE_LOOKAHEAD_NEG: printf("negative-lookahead\n"); break;
        case CHOMSKY3_NODE_WORD_BOUNDARY: printf("word-boundary\n"); break;
        case CHOMSKY3_NODE_NWORD_BOUNDARY: printf("non-word-boundary\n"); break;
    }
    chomsky3_node_print(node->left, indent + 1);
    chomsky3_node_print(node->right, indent + 1);
}

size_t chomsky3_node_count(const chomsky3_regex_node_t *node) {
    return node ? 1 + chomsky3_node_count(node->left) + chomsky3_node_count(node->right) : 0;
}

/* ========================================================================
 * Parser
 * ======================================================================== */

typedef struct {
    const char *pat;
    size_t len;
    size_t pos;
    uint32_t group_count;
    chomsky3_error_t error;
    size_t error_pos;
    int depth;                  /* Recursion depth guard */
} ere_parser_t;

#define ERE_MAX_DEPTH 200

static void parse_fail(ere_parser_t *p, chomsky3_error_t err, size_t pos) {
    if (p->error == CHOMSKY3_OK) {
        p->error = err;
        p->error_pos = pos;
    }
}

static int peek(ere_parser_t *p) {
    return p->pos < p->len ? (unsigned char)p->pat[p->pos] : -1;
}

static int peek_at(ere_parser_t *p, size_t offset) {
    return p->pos + offset < p->len ? (unsigned char)p->pat[p->pos + offset] : -1;
}

static int advance(ere_parser_t *p) {
    return p->pos < p->len ? (unsigned char)p->pat[p->pos++] : -1;
}

static bool parse_alternation(ere_parser_t *p, chomsky3_regex_node_t **out);

/* Escape class helper: builds a char class node for \d \w \s (and negations). */
static chomsky3_regex_node_t *make_escape_class(ere_parser_t *p, int esc, size_t pos) {
    chomsky3_cclass_flags_t flags = CHOMSKY3_CCLASS_NONE;
    bool negated = false;
    switch (esc) {
        case 'd': flags = CHOMSKY3_CCLASS_DIGIT; break;
        case 'D': flags = CHOMSKY3_CCLASS_DIGIT; negated = true; break;
        case 'w': flags = CHOMSKY3_CCLASS_WORD; break;
        case 'W': flags = CHOMSKY3_CCLASS_WORD; negated = true; break;
        case 's': flags = CHOMSKY3_CCLASS_SPACE; break;
        case 'S': flags = CHOMSKY3_CCLASS_SPACE; negated = true; break;
        default: return NULL;
    }
    chomsky3_regex_node_t *node = chomsky3_node_char_class(flags);
    if (!node) {
        parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, pos);
        return NULL;
    }
    if (negated) node->data.char_class.flags |= CHOMSKY3_CCLASS_NEGATED;
    return node;
}

/* Decode a single escape sequence occurring outside a character class.
 * Consumes the escape from the stream. Returns a new node or NULL on error. */
static chomsky3_regex_node_t *parse_escape_node(ere_parser_t *p, size_t esc_pos) {
    int c = advance(p); /* character after backslash */
    if (c < 0) {
        parse_fail(p, CHOMSKY3_ERROR_PARSE_UNEXPECTED_END, esc_pos);
        return NULL;
    }

    switch (c) {
        case 'd': case 'D': case 'w': case 'W': case 's': case 'S':
            return make_escape_class(p, c, esc_pos);
        case 'b': return node_new(CHOMSKY3_NODE_WORD_BOUNDARY);
        case 'B': return node_new(CHOMSKY3_NODE_NWORD_BOUNDARY);
        case 'A': return node_new(CHOMSKY3_NODE_ANCHOR_START);
        case 'z': return node_new(CHOMSKY3_NODE_ANCHOR_END);
        case 'n': return chomsky3_node_literal('\n');
        case 'r': return chomsky3_node_literal('\r');
        case 't': return chomsky3_node_literal('\t');
        case 'f': return chomsky3_node_literal('\f');
        case 'v': return chomsky3_node_literal('\v');
        case '0': return chomsky3_node_literal(0);
        case 'x': {
            /* \xHH */
            uint32_t value = 0;
            int digits = 0;
            while (digits < 2) {
                int h = peek(p);
                int d;
                if (h >= '0' && h <= '9') d = h - '0';
                else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                else break;
                value = value * 16u + (uint32_t)d;
                advance(p);
                digits++;
            }
            if (digits == 0) {
                parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_ESCAPE, esc_pos);
                return NULL;
            }
            return chomsky3_node_literal(value);
        }
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': {
            uint32_t id = (uint32_t)(c - '0');
            while (peek(p) >= '0' && peek(p) <= '9') {
                id = id * 10u + (uint32_t)(advance(p) - '0');
            }
            if (id == 0 || id > p->group_count) {
                parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_BACKREF, esc_pos);
                return NULL;
            }
            return chomsky3_node_backref(id);
        }
        default:
            /* Escaped metacharacters and ordinary escaped literals */
            return chomsky3_node_literal((uint32_t)c);
    }
}

/* Parse the body of a character class [...]. On entry, p->pos is just
 * past '['. Consumes up to and including the closing ']'. */
static chomsky3_regex_node_t *parse_char_class(ere_parser_t *p, size_t start_pos) {
    chomsky3_regex_node_t *node = chomsky3_node_char_class(CHOMSKY3_CCLASS_NONE);
    if (!node) {
        parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, start_pos);
        return NULL;
    }

    if (peek(p) == '^') {
        advance(p);
        node->data.char_class.flags |= CHOMSKY3_CCLASS_NEGATED;
    }

    bool first = true;
    bool have_prev = false;
    uint32_t prev = 0;

    while (p->error == CHOMSKY3_OK) {
        int c = peek(p);
        if (c < 0) {
            parse_fail(p, CHOMSKY3_ERROR_PARSE_UNMATCHED_BRACKET, start_pos);
            break;
        }
        if (c == ']' && !first) {
            advance(p);
            return node;
        }
        first = false;

        uint32_t lo;
        if (c == '\\') {
            advance(p);
            int esc = advance(p);
            if (esc < 0) {
                parse_fail(p, CHOMSKY3_ERROR_PARSE_UNEXPECTED_END, start_pos);
                break;
            }
            switch (esc) {
                case 'd': case 'D': case 'w': case 'W': case 's': case 'S': {
                    /* Fold the named class into this class via flags. */
                    chomsky3_cclass_flags_t extra = CHOMSKY3_CCLASS_NONE;
                    if (esc == 'd' || esc == 'D') extra = CHOMSKY3_CCLASS_DIGIT;
                    else if (esc == 'w' || esc == 'W') extra = CHOMSKY3_CCLASS_WORD;
                    else extra = CHOMSKY3_CCLASS_SPACE;
                    node->data.char_class.flags |= extra;
                    have_prev = false; /* named classes cannot start a range */
                    continue;
                }
                case 'n': lo = '\n'; break;
                case 'r': lo = '\r'; break;
                case 't': lo = '\t'; break;
                case 'f': lo = '\f'; break;
                case 'v': lo = '\v'; break;
                case 'x': {
                    uint32_t value = 0;
                    int digits = 0;
                    while (digits < 2) {
                        int h = peek(p);
                        int d;
                        if (h >= '0' && h <= '9') d = h - '0';
                        else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                        else break;
                        value = value * 16u + (uint32_t)d;
                        advance(p);
                        digits++;
                    }
                    if (digits == 0) {
                        parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_ESCAPE, start_pos);
                        goto fail;
                    }
                    lo = value;
                    break;
                }
                default: lo = (uint32_t)esc; break;
            }
        } else {
            lo = (uint32_t)advance(p);
        }

        /* Possible range: lo-hi */
        if (peek(p) == '-' && peek_at(p, 1) != ']' && peek_at(p, 1) >= 0) {
            advance(p); /* consume '-' */
            uint32_t hi;
            int hc = peek(p);
            if (hc == '\\') {
                advance(p);
                int esc = advance(p);
                if (esc < 0) {
                    parse_fail(p, CHOMSKY3_ERROR_PARSE_UNEXPECTED_END, start_pos);
                    goto fail;
                }
                hi = (uint32_t)esc;
            } else {
                hi = (uint32_t)advance(p);
            }
            if (hi < lo) {
                parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_RANGE, p->pos);
                goto fail;
            }
            if (!chomsky3_char_class_add_range(node, lo, hi)) {
                parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, start_pos);
                goto fail;
            }
            have_prev = false;
            (void)prev;
        } else {
            if (!chomsky3_char_class_add_range(node, lo, lo)) {
                parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, start_pos);
                goto fail;
            }
            prev = lo;
            have_prev = true;
            (void)have_prev;
        }
    }

fail:
    chomsky3_node_free(node);
    return NULL;
}

/* Parse a group starting at '('. p->pos is just past '('. */
static chomsky3_regex_node_t *parse_group(ere_parser_t *p, size_t start_pos) {
    chomsky3_node_type_t wrap = CHOMSKY3_NODE_GROUP;
    uint32_t group_id = 0;

    if (peek(p) == '?') {
        advance(p);
        int kind = advance(p);
        switch (kind) {
            case ':':
                wrap = CHOMSKY3_NODE_CONCAT; /* non-capturing: child passes through */
                break;
            case '=':
                wrap = CHOMSKY3_NODE_LOOKAHEAD;
                break;
            case '!':
                wrap = CHOMSKY3_NODE_LOOKAHEAD_NEG;
                break;
            default:
                parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_GROUP, start_pos);
                return NULL;
        }
    } else {
        group_id = ++p->group_count;
        if (group_id > 255) {
            parse_fail(p, CHOMSKY3_ERROR_LIMIT_CAPTURE_GROUPS, start_pos);
            return NULL;
        }
    }

    chomsky3_regex_node_t *child = NULL;
    if (!parse_alternation(p, &child)) {
        return NULL;
    }

    if (peek(p) != ')') {
        chomsky3_node_free(child);
        parse_fail(p, CHOMSKY3_ERROR_PARSE_UNMATCHED_PAREN, start_pos);
        return NULL;
    }
    advance(p); /* consume ')' */

    if (!child) {
        parse_fail(p, CHOMSKY3_ERROR_PARSE_EMPTY_GROUP, start_pos);
        return NULL;
    }

    if (wrap == CHOMSKY3_NODE_GROUP) {
        chomsky3_regex_node_t *group = chomsky3_node_group(child, group_id);
        if (!group) {
            chomsky3_node_free(child);
            parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, start_pos);
            return NULL;
        }
        return group;
    }

    /* Lookahead wrapper (or pass-through for non-capturing groups) */
    if (wrap == CHOMSKY3_NODE_CONCAT) {
        return child;
    }

    chomsky3_regex_node_t *look = node_new(wrap);
    if (!look) {
        chomsky3_node_free(child);
        parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, start_pos);
        return NULL;
    }
    look->left = child;
    return look;
}

/* Parse an atom: literal, dot, anchor, class, group, or escape. */
static chomsky3_regex_node_t *parse_atom(ere_parser_t *p) {
    size_t start_pos = p->pos;
    int c = peek(p);

    switch (c) {
        case -1:
        case '|':
        case ')':
            return NULL; /* caller handles */
        case '(':
            advance(p);
            return parse_group(p, start_pos);
        case '[':
            advance(p);
            return parse_char_class(p, start_pos);
        case '.':
            advance(p);
            return chomsky3_node_dot();
        case '^':
            advance(p);
            return node_new(CHOMSKY3_NODE_ANCHOR_START);
        case '$':
            advance(p);
            return node_new(CHOMSKY3_NODE_ANCHOR_END);
        case '\\':
            advance(p);
            return parse_escape_node(p, start_pos);
        case '*': case '+': case '?':
            parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_QUANTIFIER, start_pos);
            return NULL;
        case '{':
            /* A '{' that does not begin a valid quantifier is a literal. */
            advance(p);
            return chomsky3_node_literal('{');
        default:
            advance(p);
            return chomsky3_node_literal((uint32_t)c);
    }
}

/* Parse a quantifier applied to `atom`. Consumes nothing if no quantifier.
 * Returns atom itself or a new repeat node wrapping it. */
static chomsky3_regex_node_t *parse_quantifier(ere_parser_t *p, chomsky3_regex_node_t *atom) {
    if (!atom) return NULL;

    uint32_t min, max;
    int c = peek(p);

    switch (c) {
        case '*': advance(p); min = 0; max = UINT32_MAX; break;
        case '+': advance(p); min = 1; max = UINT32_MAX; break;
        case '?': advance(p); min = 0; max = 1; break;
        case '{': {
            /* Look ahead for a valid {m}, {m,} or {m,n} quantifier. */
            size_t save = p->pos;
            advance(p); /* '{' */
            if (peek(p) < '0' || peek(p) > '9') {
                p->pos = save; /* not a quantifier after all */
                return atom;
            }
            uint32_t lo = 0;
            while (peek(p) >= '0' && peek(p) <= '9') {
                if (lo > (UINT32_MAX - 9u) / 10u) {
                    parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_QUANTIFIER, save);
                    chomsky3_node_free(atom);
                    return NULL;
                }
                lo = lo * 10u + (uint32_t)(advance(p) - '0');
            }
            uint32_t hi = lo;
            if (peek(p) == ',') {
                advance(p);
                if (peek(p) >= '0' && peek(p) <= '9') {
                    hi = 0;
                    while (peek(p) >= '0' && peek(p) <= '9') {
                        if (hi > (UINT32_MAX - 9u) / 10u) {
                            parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_QUANTIFIER, save);
                            chomsky3_node_free(atom);
                            return NULL;
                        }
                        hi = hi * 10u + (uint32_t)(advance(p) - '0');
                    }
                } else {
                    hi = UINT32_MAX;
                }
            }
            if (peek(p) != '}') {
                p->pos = save; /* not a valid quantifier: treat '{' as literal */
                return atom;
            }
            advance(p); /* '}' */
            if (hi != UINT32_MAX && hi < lo) {
                parse_fail(p, CHOMSKY3_ERROR_PARSE_INVALID_QUANTIFIER, save);
                chomsky3_node_free(atom);
                return NULL;
            }
            min = lo;
            max = hi;
            break;
        }
        default:
            return atom; /* no quantifier */
    }

    /* Reject quantifier applied to assertions/anchors */
    switch (atom->type) {
        case CHOMSKY3_NODE_ANCHOR_START:
        case CHOMSKY3_NODE_ANCHOR_END:
        case CHOMSKY3_NODE_WORD_BOUNDARY:
        case CHOMSKY3_NODE_NWORD_BOUNDARY:
        case CHOMSKY3_NODE_LOOKAHEAD:
        case CHOMSKY3_NODE_LOOKAHEAD_NEG:
            parse_fail(p, CHOMSKY3_ERROR_PARSE_NESTED_QUANTIFIER, p->pos);
            chomsky3_node_free(atom);
            return NULL;
        default:
            break;
    }

    bool greedy = true;
    if (peek(p) == '?') {
        advance(p);
        greedy = false;
    }

    chomsky3_node_type_t shortcut = CHOMSKY3_NODE_REPEAT;
    if (min == 0 && max == UINT32_MAX) shortcut = CHOMSKY3_NODE_STAR;
    else if (min == 1 && max == UINT32_MAX) shortcut = CHOMSKY3_NODE_PLUS;
    else if (min == 0 && max == 1) shortcut = CHOMSKY3_NODE_QUESTION;

    chomsky3_regex_node_t *node;
    if (shortcut != CHOMSKY3_NODE_REPEAT) {
        node = node_new(shortcut);
        if (node) {
            node->left = atom;
            node->greedy = greedy;
        }
    } else {
        node = chomsky3_node_repeat(atom, min, max, greedy);
    }
    if (!node) {
        chomsky3_node_free(atom);
        parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, p->pos);
        return NULL;
    }
    return node;
}

/* concat := repeat+ ; returns false on error. */
static bool parse_concat(ere_parser_t *p, chomsky3_regex_node_t **out) {
    *out = NULL;
    while (p->error == CHOMSKY3_OK) {
        int c = peek(p);
        if (c < 0 || c == '|' || c == ')') {
            break;
        }
        chomsky3_regex_node_t *atom = parse_atom(p);
        if (p->error != CHOMSKY3_OK) {
            chomsky3_node_free(atom);
            chomsky3_node_free(*out);
            *out = NULL;
            return false;
        }
        if (!atom) {
            break;
        }
        chomsky3_regex_node_t *rep = parse_quantifier(p, atom);
        if (p->error != CHOMSKY3_OK || !rep) {
            chomsky3_node_free(*out);
            *out = NULL;
            return false;
        }
        if (!*out) {
            *out = rep;
        } else {
            chomsky3_regex_node_t *cat = chomsky3_node_concat(*out, rep);
            if (!cat) {
                chomsky3_node_free(rep);
                chomsky3_node_free(*out);
                *out = NULL;
                parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, p->pos);
                return false;
            }
            *out = cat;
        }
    }
    return true;
}

/* alternation := concat ('|' concat)* */
static bool parse_alternation(ere_parser_t *p, chomsky3_regex_node_t **out) {
    if (++p->depth > ERE_MAX_DEPTH) {
        p->depth--;
        parse_fail(p, CHOMSKY3_ERROR_PARSE_RECURSION_LIMIT, p->pos);
        return false;
    }

    if (!parse_concat(p, out)) {
        p->depth--;
        return false;
    }

    while (peek(p) == '|' && p->error == CHOMSKY3_OK) {
        advance(p);
        chomsky3_regex_node_t *rhs = NULL;
        if (!parse_concat(p, &rhs)) {
            chomsky3_node_free(*out);
            *out = NULL;
            p->depth--;
            return false;
        }
        if (!rhs) {
            /* Empty alternative: treat as error per strict ERE */
            chomsky3_node_free(*out);
            *out = NULL;
            parse_fail(p, CHOMSKY3_ERROR_PARSE_SYNTAX, p->pos);
            p->depth--;
            return false;
        }
        chomsky3_regex_node_t *alt = chomsky3_node_alternation(*out, rhs);
        if (!alt) {
            chomsky3_node_free(rhs);
            chomsky3_node_free(*out);
            *out = NULL;
            parse_fail(p, CHOMSKY3_ERROR_OUT_OF_MEMORY, p->pos);
            p->depth--;
            return false;
        }
        *out = alt;
    }

    p->depth--;
    return p->error == CHOMSKY3_OK;
}

/* ========================================================================
 * Public entry point
 * ======================================================================== */

chomsky3_regex_t *chomsky3_regex_new(const char *pattern, size_t length) {
    if (!pattern) return NULL;

    chomsky3_regex_t *regex = calloc(1, sizeof(*regex));
    if (!regex) return NULL;

    regex->pattern = dup_range(pattern, length);
    if (!regex->pattern) {
        free(regex);
        return NULL;
    }
    regex->pattern_length = length;

    ere_parser_t p = {
        .pat = regex->pattern,
        .len = length,
        .pos = 0,
        .group_count = 0,
        .error = CHOMSKY3_OK,
        .error_pos = 0,
        .depth = 0
    };

    chomsky3_regex_node_t *root = NULL;
    if (!parse_alternation(&p, &root) || p.error != CHOMSKY3_OK) {
        chomsky3_node_free(root);
        regex->root = NULL;
        regex->parse_error = p.error ? p.error : CHOMSKY3_ERROR_PARSE_GENERIC;
        regex->parse_error_pos = p.error_pos;
        regex->num_groups = 0;
        return regex;
    }

    if (p.pos != p.len) {
        /* Trailing ')' or similar garbage */
        chomsky3_node_free(root);
        regex->root = NULL;
        regex->parse_error = CHOMSKY3_ERROR_PARSE_UNMATCHED_PAREN;
        regex->parse_error_pos = p.pos;
        return regex;
    }

    regex->root = root;
    regex->num_groups = p.group_count;
    return regex;
}

void chomsky3_regex_free(chomsky3_regex_t *regex) {
    if (!regex) return;
    chomsky3_node_free(regex->root);
    free(regex->pattern);
    free(regex);
}

bool chomsky3_regex_validate(const chomsky3_regex_t *regex) {
    if (!regex || !regex->pattern) {
        return false;
    }
    if (regex->parse_error != CHOMSKY3_OK) {
        return false;
    }
    return regex->root != NULL || regex->pattern_length == 0;
}
