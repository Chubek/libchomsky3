/**
 * libchomsky3 - C Code Generator
 *
 * Minimal source generator scaffolding.
 */

#include "chomsky3/bytecode.h"
#include "chomsky3/ir.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *dup_string(const char *str) {
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

static chomsky3_error_t make_stub_source(
    const char *function_name,
    uint32_t flags,
    char **output
) {
    (void)flags;
    const char *name = function_name ? function_name : "chomsky3_generated_match";
    int needed = snprintf(
        NULL,
        0,
        "#include <stddef.h>\n#include <stdbool.h>\n\n"
        "bool %s(const char *input, size_t length) {\n"
        "    (void)input;\n"
        "    return length == 0;\n"
        "}\n",
        name
    );
    if (needed < 0) {
        return CHOMSKY3_ERR_INTERNAL;
    }

    *output = malloc((size_t)needed + 1);
    if (!*output) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    snprintf(
        *output,
        (size_t)needed + 1,
        "#include <stddef.h>\n#include <stdbool.h>\n\n"
        "bool %s(const char *input, size_t length) {\n"
        "    (void)input;\n"
        "    return length == 0;\n"
        "}\n",
        name
    );
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_codegen_c_from_bytecode(
    chomsky3_context_t *ctx,
    const chomsky3_bytecode_t *bytecode,
    const char *function_name,
    uint32_t flags,
    char **output
) {
    (void)ctx;
    if (!bytecode || !function_name || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    return make_stub_source(function_name, flags, output);
}

chomsky3_error_t chomsky3_codegen_c_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    const char *function_name,
    uint32_t flags,
    char **output
) {
    (void)ctx;
    if (!ir || !function_name || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    return make_stub_source(function_name, flags, output);
}

const char *chomsky3_codegen_c_escape_for_test(const char *str) {
    return str ? str : "";
}

char *chomsky3_codegen_c_dup_for_test(const char *str) {
    return str ? dup_string(str) : NULL;
}
