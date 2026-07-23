/**
 * libchomsky3 - Top-level Public API
 *
 * Implements the primary entry points declared in chomsky3/chomsky3.h:
 * contexts, compilation, execution, C source generation, and
 * serialization, tying together the regex parser, IR, bytecode backend,
 * VM interpreter, and JIT.
 */

#include "chomsky3/chomsky3.h"
#include "chomsky3/pattern.h"
#include "chomsky3/regex.h"
#include "chomsky3/ir.h"
#include "chomsky3/bytecode.h"
#include "chomsky3/compiler.h"
#include "chomsky3/codegen_c.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef CHOMSKY3_HAS_JIT
#include "chomsky3/jit.h"
#endif

/* Context: shared state for compilation/execution. Deliberately small;
 * exists so resource policies can be attached later without ABI churn. */
struct chomsky3_context {
    size_t max_steps;
    size_t max_backtrack;
    size_t max_stack_depth;
    int jit_disabled;               /* Runtime JIT kill-switch */
};

/* Pattern layout is in pattern.h; bytecode.code holds an owned
 * chomsky3_bytecode_t* for BYTECODE and JIT targets. */

chomsky3_context_t *chomsky3_context_new(void) {
    return calloc(1, sizeof(chomsky3_context_t));
}

void chomsky3_context_free(chomsky3_context_t *ctx) {
    free(ctx);
}

const char *chomsky3_version(void) {
    return "3.0.0";
}

bool chomsky3_jit_available(void) {
#ifdef CHOMSKY3_HAS_JIT
    return true;
#else
    return false;
#endif
}

/* Runtime JIT control: callers can force the interpreter even when the
 * library was built with JIT support. */
void chomsky3_context_set_jit_enabled(chomsky3_context_t *ctx, bool enabled) {
    if (ctx) {
        ctx->jit_disabled = enabled ? 0 : 1;
    }
}

static chomsky3_regex_t *parse_pattern(const char *pattern_str,
                                       chomsky3_error_t *error) {
    chomsky3_regex_t *regex = chomsky3_regex_new(pattern_str, strlen(pattern_str));
    if (!regex) {
        if (error) *error = CHOMSKY3_ERR_OUT_OF_MEMORY;
        return NULL;
    }
    if (regex->parse_error != CHOMSKY3_OK) {
        if (error) *error = regex->parse_error;
        chomsky3_regex_free(regex);
        return NULL;
    }
    return regex;
}

chomsky3_pattern_t *chomsky3_compile(
    chomsky3_context_t *ctx,
    const char *pattern,
    chomsky3_target_t target,
    chomsky3_flags_t flags,
    chomsky3_error_t *error
) {
    chomsky3_error_t err = CHOMSKY3_OK;
    chomsky3_context_t *owned_ctx = NULL;

    if (error) *error = CHOMSKY3_OK;
    if (!pattern) {
        if (error) *error = CHOMSKY3_ERR_INVALID_ARGUMENT;
        return NULL;
    }

    if (!ctx) {
        owned_ctx = chomsky3_context_new();
        if (!owned_ctx) {
            if (error) *error = CHOMSKY3_ERR_OUT_OF_MEMORY;
            return NULL;
        }
        ctx = owned_ctx;
    }

    /* JIT requested but unavailable (build-time off or runtime-disabled):
     * fall back to the bytecode interpreter so users get a working matcher
     * either way. */
    if (target == CHOMSKY3_TARGET_JIT &&
        (!chomsky3_jit_available() || ctx->jit_disabled)) {
        target = CHOMSKY3_TARGET_BYTECODE;
    }

    /* 1. Parse */
    chomsky3_regex_t *regex = parse_pattern(pattern, &err);
    if (!regex) {
        chomsky3_context_free(owned_ctx);
        if (error) *error = err;
        return NULL;
    }

    /* 2. Build IR */
    chomsky3_ir_builder_t *builder = chomsky3_ir_builder_create(ctx);
    chomsky3_ir_t *ir = NULL;
    if (builder) {
        err = chomsky3_ir_build_from_ast(builder, regex, &ir);
        chomsky3_ir_builder_destroy(builder);
    } else {
        err = CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    if (err != CHOMSKY3_OK || !ir) {
        chomsky3_regex_free(regex);
        chomsky3_context_free(owned_ctx);
        if (error) *error = err != CHOMSKY3_OK ? err : CHOMSKY3_ERR_INTERNAL;
        return NULL;
    }
    ir->flags = (uint32_t)flags;

    /* 3. Generate bytecode (the C source target also goes through bytecode
     *    first so chomsky3_generate_c has a canonical representation). */
    chomsky3_bytecode_t *bc = NULL;
    err = chomsky3_bytecode_from_ir(ctx, ir, &bc);
    chomsky3_ir_destroy(ir);
    if (err != CHOMSKY3_OK) {
        chomsky3_regex_free(regex);
        chomsky3_context_free(owned_ctx);
        if (error) *error = err;
        return NULL;
    }

    /* Optional peephole optimization pass */
    if (flags & CHOMSKY3_FLAG_OPTIMIZE) {
        chomsky3_bytecode_t *opt = NULL;
        if (chomsky3_bytecode_optimize(bc, 1, &opt) == CHOMSKY3_OK && opt) {
            chomsky3_bytecode_free(bc);
            bc = opt;
        }
    }

    /* 4. Assemble the pattern object */
    chomsky3_pattern_t *pat = calloc(1, sizeof(*pat));
    if (!pat) {
        chomsky3_bytecode_free(bc);
        chomsky3_regex_free(regex);
        chomsky3_context_free(owned_ctx);
        if (error) *error = CHOMSKY3_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    pat->ctx = ctx;
    pat->target = target;
    pat->flags = flags;
    pat->num_groups = regex->num_groups;

    switch (target) {
        case CHOMSKY3_TARGET_BYTECODE:
            pat->bytecode.code = bc;
            pat->bytecode.size = bc->total_size;
            pat->bytecode.num_registers = bc->header.num_captures * 2;
            break;

        case CHOMSKY3_TARGET_JIT: {
#ifdef CHOMSKY3_HAS_JIT
            chomsky3_jit_compiler_t *jc = chomsky3_jit_compiler_create(ctx, NULL);
            chomsky3_jit_code_t *jcode = NULL;
            chomsky3_error_t jerr = jc ? chomsky3_jit_compile(jc, bc, &jcode)
                                       : CHOMSKY3_ERR_OUT_OF_MEMORY;
            if (jc) chomsky3_jit_compiler_destroy(jc);
            if (jerr != CHOMSKY3_OK || !jcode) {
                /* Graceful degradation: keep the bytecode image and run it
                 * through the interpreter instead. */
                pat->target = CHOMSKY3_TARGET_BYTECODE;
                pat->bytecode.code = bc;
                pat->bytecode.size = bc->total_size;
                pat->bytecode.num_registers = bc->header.num_captures * 2;
            } else {
                pat->jit.code = jcode;
                pat->jit.size = jcode->code_size;
                /* Function-pointer-to-void* is not portable ISO C; go
                 * through a union instead (valid on all POSIX targets and
                 * silences -Wpedantic). */
                union { chomsky3_jit_func_t fn; void *obj; } ep;
                ep.fn = jcode->func;
                pat->jit.entry_point = ep.obj;
                /* Keep the bytecode too: used as fallback if JIT execution
                 * fails at runtime and for serialization. */
                pat->bytecode.code = bc;
                pat->bytecode.size = bc->total_size;
                pat->bytecode.num_registers = bc->header.num_captures * 2;
            }
#else
            pat->target = CHOMSKY3_TARGET_BYTECODE;
            pat->bytecode.code = bc;
            pat->bytecode.size = bc->total_size;
            pat->bytecode.num_registers = bc->header.num_captures * 2;
#endif
            break;
        }

        case CHOMSKY3_TARGET_C_SOURCE: {
            char *source = NULL;
            err = chomsky3_codegen_c_from_bytecode(ctx, bc, "chomsky3_match",
                                                   (uint32_t)flags, &source);
            if (err != CHOMSKY3_OK || !source) {
                chomsky3_bytecode_free(bc);
                free(pat);
                chomsky3_regex_free(regex);
                chomsky3_context_free(owned_ctx);
                if (error) *error = err != CHOMSKY3_OK ? err : CHOMSKY3_ERR_INTERNAL;
                return NULL;
            }
            pat->c_source.source = source;
            pat->c_source.length = strlen(source);
            /* Keep the bytecode image for execution */
            pat->bytecode.code = bc;
            pat->bytecode.size = bc->total_size;
            pat->bytecode.num_registers = bc->header.num_captures * 2;
            break;
        }

        default:
            chomsky3_bytecode_free(bc);
            free(pat);
            chomsky3_regex_free(regex);
            chomsky3_context_free(owned_ctx);
            if (error) *error = CHOMSKY3_ERR_INVALID_ARGUMENT;
            return NULL;
    }

    chomsky3_regex_free(regex);
    /* Note: an owned context is intentionally leaked into pat->ctx — the
     * pattern keeps it alive and chomsky3_pattern_free releases it. To
     * track that, owned contexts are marked via jit_disabled high bit. */
    if (owned_ctx) {
        owned_ctx->jit_disabled |= 0x40000000; /* "owned by pattern" marker */
    }

    if (error) *error = CHOMSKY3_OK;
    return pat;
}

void chomsky3_pattern_free(chomsky3_pattern_t *pattern) {
    if (!pattern) {
        return;
    }

    chomsky3_bytecode_t *bc = (chomsky3_bytecode_t *)pattern->bytecode.code;
    if (bc) {
        chomsky3_bytecode_free(bc);
    }

#ifdef CHOMSKY3_HAS_JIT
    if (pattern->target == CHOMSKY3_TARGET_JIT && pattern->jit.code) {
        chomsky3_jit_code_free((chomsky3_jit_code_t *)pattern->jit.code);
    }
#endif

    if (pattern->target == CHOMSKY3_TARGET_C_SOURCE && pattern->c_source.source) {
        free(pattern->c_source.source);
    }

    /* Release an internally-owned context */
    if (pattern->ctx && (pattern->ctx->jit_disabled & 0x40000000)) {
        chomsky3_context_free(pattern->ctx);
    }

    free(pattern);
}

bool chomsky3_exec(
    chomsky3_pattern_t *pattern,
    const char *input,
    size_t length,
    chomsky3_match_t *match
) {
    if (!pattern || !input) {
        return false;
    }

    chomsky3_match_t *vmatch = NULL;
    chomsky3_error_t err;

#ifdef CHOMSKY3_HAS_JIT
    if (pattern->target == CHOMSKY3_TARGET_JIT && pattern->jit.code &&
        pattern->ctx && !pattern->ctx->jit_disabled) {
        err = chomsky3_jit_execute((const chomsky3_jit_code_t *)pattern->jit.code,
                                   input, length, 0, &vmatch);
        if (err == CHOMSKY3_OK && vmatch) {
            goto done;
        }
        /* Fall through to the interpreter on any JIT failure */
    }
#endif

    {
        const chomsky3_bytecode_t *bc =
            (const chomsky3_bytecode_t *)pattern->bytecode.code;
        if (!bc) {
            return false;
        }

        chomsky3_vm_t *vm = chomsky3_vm_new(pattern->ctx, bc);
        if (!vm) {
            return false;
        }

        if (pattern->ctx &&
            (pattern->ctx->max_steps || pattern->ctx->max_backtrack ||
             pattern->ctx->max_stack_depth)) {
            chomsky3_vm_limits_t limits = {
                .max_steps = pattern->ctx->max_steps,
                .max_stack_depth = pattern->ctx->max_stack_depth,
                .max_backtrack = pattern->ctx->max_backtrack,
                .timeout_ns = 0
            };
            chomsky3_vm_set_limits(vm, &limits);
        }

        err = chomsky3_vm_execute(vm, input, length, &vmatch);
        chomsky3_vm_free(vm);
        if (err != CHOMSKY3_OK || !vmatch) {
            return false;
        }
    }

#ifdef CHOMSKY3_HAS_JIT
done:
#endif
    if (match) {
        *match = *vmatch;           /* transfers groups array */
        free(vmatch);
    } else {
        free(vmatch->groups);
        free(vmatch);
    }
    return true;
}

void chomsky3_match_free(chomsky3_match_t *match) {
    if (!match) {
        return;
    }
    free(match->groups);
    match->groups = NULL;
}

chomsky3_error_t chomsky3_generate_c(
    chomsky3_pattern_t *pattern,
    char *output,
    size_t output_size
) {
    if (!pattern || !output || output_size == 0) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* If the pattern was compiled with the C source target, reuse the
     * cached source; otherwise regenerate from the bytecode image. */
    const char *source = NULL;
    char *generated = NULL;

    if (pattern->target == CHOMSKY3_TARGET_C_SOURCE && pattern->c_source.source) {
        source = pattern->c_source.source;
    } else {
        const chomsky3_bytecode_t *bc =
            (const chomsky3_bytecode_t *)pattern->bytecode.code;
        if (!bc) {
            return CHOMSKY3_ERR_INVALID_ARGUMENT;
        }
        chomsky3_error_t err = chomsky3_codegen_c_from_bytecode(
            pattern->ctx, bc, "chomsky3_match", (uint32_t)pattern->flags,
            &generated);
        if (err != CHOMSKY3_OK) {
            return err;
        }
        source = generated;
    }

    size_t len = strlen(source);
    if (len + 1 > output_size) {
        free(generated);
        return CHOMSKY3_ERROR_LIMIT_MEMORY;
    }
    memcpy(output, source, len + 1);
    free(generated);
    return CHOMSKY3_OK;
}

/* Serialization: self-describing image = bytecode serialization with the
 * pattern's own header prepended implicitly by the bytecode header. */
chomsky3_error_t chomsky3_serialize(
    chomsky3_pattern_t *pattern,
    uint8_t *output,
    size_t output_size,
    size_t *bytes_written
) {
    if (!pattern || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    const chomsky3_bytecode_t *bc =
        (const chomsky3_bytecode_t *)pattern->bytecode.code;
    if (!bc) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    uint8_t *buffer = NULL;
    size_t size = 0;
    chomsky3_error_t err = chomsky3_bytecode_serialize(bc, &buffer, &size);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    if (size > output_size) {
        free(buffer);
        return CHOMSKY3_ERROR_LIMIT_MEMORY;
    }

    memcpy(output, buffer, size);
    free(buffer);
    if (bytes_written) {
        *bytes_written = size;
    }
    return CHOMSKY3_OK;
}

chomsky3_pattern_t *chomsky3_deserialize(
    chomsky3_context_t *ctx,
    const uint8_t *input,
    size_t input_size,
    chomsky3_error_t *error
) {
    if (error) *error = CHOMSKY3_OK;
    if (!input || input_size == 0) {
        if (error) *error = CHOMSKY3_ERR_INVALID_ARGUMENT;
        return NULL;
    }

    chomsky3_context_t *owned_ctx = NULL;
    if (!ctx) {
        owned_ctx = chomsky3_context_new();
        if (!owned_ctx) {
            if (error) *error = CHOMSKY3_ERR_OUT_OF_MEMORY;
            return NULL;
        }
        owned_ctx->jit_disabled |= 0x40000000;
        ctx = owned_ctx;
    }

    chomsky3_bytecode_t *bc = NULL;
    chomsky3_error_t err = chomsky3_bytecode_deserialize(ctx, input, input_size, &bc);
    if (err != CHOMSKY3_OK) {
        chomsky3_context_free(owned_ctx);
        if (error) *error = err;
        return NULL;
    }

    chomsky3_pattern_t *pat = calloc(1, sizeof(*pat));
    if (!pat) {
        chomsky3_bytecode_free(bc);
        chomsky3_context_free(owned_ctx);
        if (error) *error = CHOMSKY3_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    pat->ctx = ctx;
    pat->target = CHOMSKY3_TARGET_BYTECODE;
    pat->flags = (chomsky3_flags_t)bc->header.flags;
    pat->num_groups = bc->header.num_captures > 0 ? bc->header.num_captures - 1 : 0;
    pat->bytecode.code = bc;
    pat->bytecode.size = bc->total_size;
    pat->bytecode.num_registers = bc->header.num_captures * 2;

    return pat;
}
