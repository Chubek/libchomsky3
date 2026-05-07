/**
 * @file jit_sljit.c
 * @brief RegexJIT adapter for SLJIT's regex backend.
 *
 * This module provides a small compatibility layer that builds and stores
 * SLJIT regex machines through the bundled regexJIT API.
 */

#include "chomsky3/jit.h"
#include "chomsky3/util/memory.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <sljit/regex_src/regexJIT.h>

/* Internal compiled representation for the legacy regex-sljit backend API. */
typedef struct {
    struct regex_machine *machine;
} chomsky3_regex_jit_code_t;

int chomsky3_jit_sljit_compile(
    const void *bytecode,
    size_t bytecode_size,
    chomsky3_jit_config_t *config,
    void **out_code,
    size_t *out_size
) {
    (void)config;

    if (!bytecode || !out_code || !out_size || bytecode_size == 0) {
        return CHOMSKY3_ERROR_INVALID_ARGUMENT;
    }
    if (bytecode_size > (size_t)INT_MAX) {
        return CHOMSKY3_ERROR_LIMIT_PATTERN_SIZE;
    }

    int regex_error = REGEX_NO_ERROR;
    struct regex_machine *machine = regex_compile((const regex_char_t *)bytecode,
                                               (int)bytecode_size,
                                               REGEX_MATCH_BEGIN,
                                               &regex_error);
    if (!machine || regex_error != REGEX_NO_ERROR) {
        return CHOMSKY3_ERROR_COMPILE_CODEGEN_FAILED;
    }

    chomsky3_regex_jit_code_t *code = (chomsky3_regex_jit_code_t *)chomsky3_malloc(
        sizeof(*code));
    if (!code) {
        regex_free_machine(machine);
        return CHOMSKY3_ERROR_OUT_OF_MEMORY;
    }

    code->machine = machine;
    *out_code = code;
    *out_size = sizeof(*code);

    return CHOMSKY3_OK;
}

void chomsky3_jit_sljit_free(void *code) {
    if (!code) {
        return;
    }

    chomsky3_regex_jit_code_t *compiled = (chomsky3_regex_jit_code_t *)code;
    if (compiled->machine) {
        regex_free_machine(compiled->machine);
    }

    chomsky3_free(compiled);
}

const char *chomsky3_jit_sljit_version(void) {
    return regex_get_platform_name();
}

int chomsky3_jit_sljit_is_supported(void) {
    return regex_get_platform_name() != NULL;
}
