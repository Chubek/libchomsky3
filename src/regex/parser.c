#include "chomsky3/ast.h"
#include <dparse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generated parser tables from parser.g */
extern D_ParserTables parser_tables_gram;

chomsky3_ast_node_t* chomsky3_parse_regex(const char *pattern, size_t len, char **error_msg) {
    if (!pattern) {
        if (error_msg) {
            *error_msg = strdup("NULL pattern provided");
        }
        return NULL;
    }

    /* Create parser instance */
    D_Parser *parser = new_D_Parser(&parser_tables_gram, 0);
    if (!parser) {
        if (error_msg) {
            *error_msg = strdup("Failed to create parser");
        }
        return NULL;
    }

    /* Parse the input */
    if (!dparse(parser, pattern, len)) {
        if (error_msg) {
            *error_msg = strdup("Parse failed");
        }
        free_D_Parser(parser);
        return NULL;
    }

    /* Check for syntax errors */
    if (parser->syntax_errors) {
        if (error_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Syntax error: %d error(s) found", 
                     parser->syntax_errors);
            *error_msg = strdup(buf);
        }
        free_D_Parser(parser);
        return NULL;
    }

    /* Get the result AST from the parse tree root */
    chomsky3_ast_node_t *result = NULL;
    if (parser->user.result) {
        result = (chomsky3_ast_node_t*)parser->user.result;
    }

    /* Clean up parser */
    free_D_Parser(parser);

    if (!result && error_msg) {
        *error_msg = strdup("Parse succeeded but no AST was produced");
    }

    return result;
}

void chomsky3_free_ast(chomsky3_ast_node_t *node) {
    if (!node) return;

    switch (node->type) {
        case CHOMSKY3_AST_LITERAL:
        case CHOMSKY3_AST_DOT:
        case CHOMSKY3_AST_ANCHOR_START:
        case CHOMSKY3_AST_ANCHOR_END:
        case CHOMSKY3_AST_ANCHOR_WORD_BOUNDARY:
        case CHOMSKY3_AST_ANCHOR_NON_WORD_BOUNDARY:
            /* No children to free */
            break;

        case CHOMSKY3_AST_CHAR_CLASS:
            if (node->data.char_class.ranges) {
                free(node->data.char_class.ranges);
            }
            break;

        case CHOMSKY3_AST_ZERO_OR_MORE:
        case CHOMSKY3_AST_ONE_OR_MORE:
        case CHOMSKY3_AST_ZERO_OR_ONE:
            chomsky3_free_ast(node->data.quantifier.child);
            break;

        case CHOMSKY3_AST_REPEAT:
            chomsky3_free_ast(node->data.repeat.child);
            break;

        case CHOMSKY3_AST_CONCAT:
        case CHOMSKY3_AST_ALTERNATION:
            chomsky3_free_ast(node->data.binary.left);
            chomsky3_free_ast(node->data.binary.right);
            break;

        case CHOMSKY3_AST_CAPTURING_GROUP:
        case CHOMSKY3_AST_NON_CAPTURING_GROUP:
            chomsky3_free_ast(node->data.group.child);
            break;

        case CHOMSKY3_AST_POSITIVE_LOOKAHEAD:
        case CHOMSKY3_AST_NEGATIVE_LOOKAHEAD:
        case CHOMSKY3_AST_POSITIVE_LOOKBEHIND:
        case CHOMSKY3_AST_NEGATIVE_LOOKBEHIND:
            chomsky3_free_ast(node->data.lookaround.child);
            break;

        case CHOMSKY3_AST_BACKREFERENCE:
            /* No children to free */
            break;
    }

    free(node);
}
