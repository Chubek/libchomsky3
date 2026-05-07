#include "chomsky3/ast.h"
#include <stdlib.h>

int chomsky3_lint_regex(chomsky3_ast_node_t *ast, char **error_messages, int *error_count) {
    (void)ast;
    (void)error_messages;
    if (error_count) {
        *error_count = 0;
    }
    return 0;
}

void chomsky3_free_lint_warnings(char **messages, int count) {
    if (!messages) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free(messages[i]);
    }
    free(messages);
}
