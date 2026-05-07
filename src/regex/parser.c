#include "chomsky3/ast.h"
#include <stdlib.h>
#include <string.h>

static char *dup_msg(const char *str) {
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (copy) memcpy(copy, str, len + 1);
    return copy;
}

chomsky3_ast_node_t *chomsky3_parse_regex(const char *pattern, size_t len, char **error_msg) {
    if (!pattern && len != 0) {
        if (error_msg) *error_msg = dup_msg("NULL pattern provided");
        return NULL;
    }
    if (error_msg) *error_msg = NULL;
    if (len == 0) {
        return chomsky3_ast_node_create(CHOMSKY3_AST_EMPTY);
    }
    return chomsky3_ast_node_create_literal((unsigned char)pattern[0]);
}

void chomsky3_free_ast(chomsky3_ast_node_t *node) {
    chomsky3_ast_node_free(node);
}
