{
#include "chomsky3/ast.h"
#include <stdlib.h>
#include <string.h>

/* Helper functions for AST construction */
static chomsky3_ast_node_t* make_literal(char c) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_LITERAL;
    node->data.literal.c = c;
    return node;
}

static chomsky3_ast_node_t* make_dot() {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_DOT;
    return node;
}

static chomsky3_ast_node_t* make_char_class(int negated) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_CHAR_CLASS;
    node->data.char_class.negated = negated;
    node->data.char_class.ranges = NULL;
    node->data.char_class.num_ranges = 0;
    return node;
}

static void add_char_range(chomsky3_ast_node_t *cc, char start, char end) {
    cc->data.char_class.num_ranges++;
    cc->data.char_class.ranges = realloc(cc->data.char_class.ranges,
        cc->data.char_class.num_ranges * sizeof(chomsky3_char_range_t));
    cc->data.char_class.ranges[cc->data.char_class.num_ranges - 1].start = start;
    cc->data.char_class.ranges[cc->data.char_class.num_ranges - 1].end = end;
}

static chomsky3_ast_node_t* make_quantifier(chomsky3_ast_node_type_t type,
                                             chomsky3_ast_node_t *child,
                                             int greedy) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = type;
    if (type == CHOMSKY3_AST_REPEAT) {
        node->data.repeat.child = child;
        node->data.repeat.greedy = greedy;
    } else {
        node->data.quantifier.child = child;
        node->data.quantifier.greedy = greedy;
    }
    return node;
}

static chomsky3_ast_node_t* make_repeat(chomsky3_ast_node_t *child,
                                        int min, int max, int greedy) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_REPEAT;
    node->data.repeat.child = child;
    node->data.repeat.min = min;
    node->data.repeat.max = max;
    node->data.repeat.greedy = greedy;
    return node;
}

static chomsky3_ast_node_t* make_concat(chomsky3_ast_node_t *left,
                                        chomsky3_ast_node_t *right) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_CONCAT;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

static chomsky3_ast_node_t* make_alternation(chomsky3_ast_node_t *left,
                                             chomsky3_ast_node_t *right) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_ALTERNATION;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

static chomsky3_ast_node_t* make_group(int capturing, int group_num,
                                       chomsky3_ast_node_t *child) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = capturing ? CHOMSKY3_AST_CAPTURING_GROUP : CHOMSKY3_AST_NON_CAPTURING_GROUP;
    node->data.group.child = child;
    node->data.group.group_num = group_num;
    return node;
}

static chomsky3_ast_node_t* make_anchor(chomsky3_ast_node_type_t type) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = type;
    return node;
}

static chomsky3_ast_node_t* make_lookahead(int positive, chomsky3_ast_node_t *child) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = positive ? CHOMSKY3_AST_POSITIVE_LOOKAHEAD : CHOMSKY3_AST_NEGATIVE_LOOKAHEAD;
    node->data.lookaround.child = child;
    return node;
}

static chomsky3_ast_node_t* make_lookbehind(int positive, chomsky3_ast_node_t *child) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = positive ? CHOMSKY3_AST_POSITIVE_LOOKBEHIND : CHOMSKY3_AST_NEGATIVE_LOOKBEHIND;
    node->data.lookaround.child = child;
    return node;
}

static chomsky3_ast_node_t* make_backreference(int group_num) {
    chomsky3_ast_node_t *node = calloc(1, sizeof(chomsky3_ast_node_t));
    node->type = CHOMSKY3_AST_BACKREFERENCE;
    node->data.backreference.group_num = group_num;
    return node;
}

static int current_group_num = 1;
}

regex: alternation { $$ = $0; };

alternation:
    concatenation { $$ = $0; }
  | alternation '|' concatenation { $$ = make_alternation($0, $2); }
  ;

concatenation:
    quantified { $$ = $0; }
  | concatenation quantified { $$ = make_concat($0, $1); }
  ;

quantified:
    atom { $$ = $0; }
  | atom '*' { $$ = make_quantifier(CHOMSKY3_AST_ZERO_OR_MORE, $0, 1); }
  | atom '*' '?' { $$ = make_quantifier(CHOMSKY3_AST_ZERO_OR_MORE, $0, 0); }
  | atom '+' { $$ = make_quantifier(CHOMSKY3_AST_ONE_OR_MORE, $0, 1); }
  | atom '+' '?' { $$ = make_quantifier(CHOMSKY3_AST_ONE_OR_MORE, $0, 0); }
  | atom '?' { $$ = make_quantifier(CHOMSKY3_AST_ZERO_OR_ONE, $0, 1); }
  | atom '?' '?' { $$ = make_quantifier(CHOMSKY3_AST_ZERO_OR_ONE, $0, 0); }
  | atom '{' number '}' { $$ = make_repeat($0, $2, $2, 1); }
  | atom '{' number '}' '?' { $$ = make_repeat($0, $2, $2, 0); }
  | atom '{' number ',' '}' { $$ = make_repeat($0, $2, -1, 1); }
  | atom '{' number ',' '}' '?' { $$ = make_repeat($0, $2, -1, 0); }
  | atom '{' number ',' number '}' { $$ = make_repeat($0, $2, $4, 1); }
  | atom '{' number ',' number '}' '?' { $$ = make_repeat($0, $2, $4, 0); }
  ;

atom:
    literal { $$ = $0; }
  | '.' { $$ = make_dot(); }
  | char_class { $$ = $0; }
  | group { $$ = $0; }
  | anchor { $$ = $0; }
  | escape { $$ = $0; }
  | backreference { $$ = $0; }
  ;

literal: "[a-zA-Z0-9_]" { $$ = make_literal($n0.start_loc.s[0]); };

number: "[0-9]+" { $$.value = atoi($n0.start_loc.s); };

char_class:
    '[' char_class_items ']' { $$ = $1; }
  | '[' '^' char_class_items ']' { $2->data.char_class.negated = 1; $$ = $2; }
  ;

char_class_items:
    { $$ = make_char_class(0); }
  | char_class_items char_class_item { add_char_range($0, $1.start, $1.end); $$ = $0; }
  ;

char_class_item:
    char_class_char { $$.start = $0; $$.end = $0; }
  | char_class_char '-' char_class_char { $$.start = $0; $$.end = $2; }
  ;

char_class_char: "[^\\]\\-]" { $$ = $n0.start_loc.s[0]; };

group:
    '(' alternation ')' 
        { int gnum = current_group_num++; $$ = make_group(1, gnum, $1); }
  | '(' '?' ':' alternation ')' 
        { $$ = make_group(0, 0, $3); }
  | '(' '?' '=' alternation ')' 
        { $$ = make_lookahead(1, $3); }
  | '(' '?' '!' alternation ')' 
        { $$ = make_lookahead(0, $3); }
  | '(' '?' '<' '=' alternation ')' 
        { $$ = make_lookbehind(1, $4); }
  | '(' '?' '<' '!' alternation ')' 
        { $$ = make_lookbehind(0, $4); }
  ;

anchor:
    '^' { $$ = make_anchor(CHOMSKY3_AST_ANCHOR_START); }
  | '$' { $$ = make_anchor(CHOMSKY3_AST_ANCHOR_END); }
  ;

escape:
    "\\\\d" { $$ = make_char_class(0); /* \d = [0-9] */ }
  | "\\\\D" { $$ = make_char_class(1); /* \D = [^0-9] */ }
  | "\\\\w" { $$ = make_char_class(0); /* \w = [a-zA-Z0-9_] */ }
  | "\\\\W" { $$ = make_char_class(1); /* \W = [^a-zA-Z0-9_] */ }
  | "\\\\s" { $$ = make_char_class(0); /* \s = whitespace */ }
  | "\\\\S" { $$ = make_char_class(1); /* \S = non-whitespace */ }
  | "\\\\b" { $$ = make_anchor(CHOMSKY3_AST_ANCHOR_WORD_BOUNDARY); }
  | "\\\\B" { $$ = make_anchor(CHOMSKY3_AST_ANCHOR_NON_WORD_BOUNDARY); }
  | "\\\\n" { $$ = make_literal('\n'); }
  | "\\\\r" { $$ = make_literal('\r'); }
  | "\\\\t" { $$ = make_literal('\t'); }
  | "\\\\\\\\" { $$ = make_literal('\\'); }
  | "\\\\." { $$ = make_literal('.'); }
  | "\\\\*" { $$ = make_literal('*'); }
  | "\\\\+" { $$ = make_literal('+'); }
  | "\\\\?" { $$ = make_literal('?'); }
  | "\\\\|" { $$ = make_literal('|'); }
  | "\\\\(" { $$ = make_literal('('); }
  | "\\\\)" { $$ = make_literal(')'); }
  | "\\\\[" { $$ = make_literal('['); }
  | "\\\\]" { $$ = make_literal(']'); }
  | "\\\\{" { $$ = make_literal('{'); }
  | "\\\\}" { $$ = make_literal('}'); }
  | "\\\\^" { $$ = make_literal('^'); }
  | "\\\\$" { $$ = make_literal('$'); }
  ;

backreference: "\\\\[1-9][0-9]*" 
    { $$ = make_backreference(atoi($n0.start_loc.s + 1)); };
