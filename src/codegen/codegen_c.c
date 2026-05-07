/**
 * libchomsky3 - C Code Generator
 * 
 * Generates optimized C code from bytecode or IR for ahead-of-time compilation.
 */

#include "chomsky3/bytecode.h"
#include "chomsky3/ir.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Code generation buffer */
typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
} code_buffer_t;

/* C code generation context */
typedef struct {
    chomsky3_context_t *ctx;
    
    /* Output buffer */
    code_buffer_t code;
    
    /* Function name */
    char *function_name;
    
    /* Label counter */
    size_t label_counter;
    
    /* Indentation level */
    int indent_level;
    
    /* Generation options */
    uint32_t flags;
    
    /* Capture group tracking */
    uint32_t num_captures;
    
    /* Error tracking */
    chomsky3_error_t error;
} codegen_c_ctx_t;

/* Code generation flags */
#define CHOMSKY3_CODEGEN_OPTIMIZE       (1 << 0)  /* Enable optimizations */
#define CHOMSKY3_CODEGEN_INLINE         (1 << 1)  /* Generate inline functions */
#define CHOMSKY3_CODEGEN_STATIC         (1 << 2)  /* Generate static functions */
#define CHOMSKY3_CODEGEN_COMMENTS       (1 << 3)  /* Include comments */
#define CHOMSKY3_CODEGEN_DEBUG          (1 << 4)  /* Include debug code */

/* Forward declarations */
static chomsky3_error_t init_codegen_c_ctx(
    codegen_c_ctx_t *ctx,
    chomsky3_context_t *parent_ctx,
    const char *function_name,
    uint32_t flags
);
static void free_codegen_c_ctx(codegen_c_ctx_t *ctx);
static chomsky3_error_t emit_code(
    codegen_c_ctx_t *ctx,
    const char *format,
    ...
);
static chomsky3_error_t emit_line(
    codegen_c_ctx_t *ctx,
    const char *format,
    ...
);
static chomsky3_error_t emit_indent(codegen_c_ctx_t *ctx);
static chomsky3_error_t generate_header(codegen_c_ctx_t *ctx);
static chomsky3_error_t generate_function_signature(codegen_c_ctx_t *ctx);
static chomsky3_error_t generate_function_body(
    codegen_c_ctx_t *ctx,
    const chomsky3_bytecode_t *bytecode
);
static chomsky3_error_t generate_instruction(
    codegen_c_ctx_t *ctx,
    const chomsky3_instruction_t *inst,
    size_t index
);
static chomsky3_error_t generate_ir_node(
    codegen_c_ctx_t *ctx,
    const chomsky3_ir_node_t *node
);
static chomsky3_error_t generate_footer(codegen_c_ctx_t *ctx);
static chomsky3_error_t grow_code_buffer(codegen_c_ctx_t *ctx, size_t required);
static const char *escape_string(const char *str, size_t len);
static const char *sanitize_function_name(const char *name);

/**
 * Generate C code from bytecode.
 */
chomsky3_error_t chomsky3_codegen_c_from_bytecode(
    chomsky3_context_t *ctx,
    const chomsky3_bytecode_t *bytecode,
    const char *function_name,
    uint32_t flags,
    char **output
) {
    if (!ctx || !bytecode || !function_name || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Initialize code generation context */
    codegen_c_ctx_t codegen;
    chomsky3_error_t err = init_codegen_c_ctx(&codegen, ctx, function_name, flags);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    codegen.num_captures = bytecode->header.num_captures;

    /* Generate header */
    err = generate_header(&codegen);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate function signature */
    err = generate_function_signature(&codegen);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate function body */
    err = generate_function_body(&codegen, bytecode);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate footer */
    err = generate_footer(&codegen);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Transfer ownership of generated code */
    *output = codegen.code.buffer;
    codegen.code.buffer = NULL;
    
    free_codegen_c_ctx(&codegen);
    return CHOMSKY3_OK;
}

/**
 * Generate C code from IR.
 */
chomsky3_error_t chomsky3_codegen_c_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    const char *function_name,
    uint32_t flags,
    char **output
) {
    if (!ctx || !ir || !function_name || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Initialize code generation context */
    codegen_c_ctx_t codegen;
    chomsky3_error_t err = init_codegen_c_ctx(&codegen, ctx, function_name, flags);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    /* Generate header */
    err = generate_header(&codegen);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate function signature */
    err = generate_function_signature(&codegen);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate function body from IR */
    err = emit_line(&codegen, "{");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }
    
    codegen.indent_level++;

    /* Declare variables */
    err = emit_line(&codegen, "const char *pos = input;");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    err = emit_line(&codegen, "const char *end = input + input_len;");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    if (codegen.num_captures > 0) {
        err = emit_line(&codegen, "const char *captures[%u * 2];", codegen.num_captures);
        if (err != CHOMSKY3_OK) {
            free_codegen_c_ctx(&codegen);
            return err;
        }
    }

    err = emit_line(&codegen, "");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate code for each IR node */
    for (size_t i = 0; i < ir->node_count; i++) {
        err = generate_ir_node(&codegen, &ir->nodes[i]);
        if (err != CHOMSKY3_OK) {
            free_codegen_c_ctx(&codegen);
            return err;
        }
    }

    /* Success return */
    err = emit_line(&codegen, "");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    err = emit_line(&codegen, "return 1;  /* Match */");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    codegen.indent_level--;
    err = emit_line(&codegen, "}");
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Generate footer */
    err = generate_footer(&codegen);
    if (err != CHOMSKY3_OK) {
        free_codegen_c_ctx(&codegen);
        return err;
    }

    /* Transfer ownership of generated code */
    *output = codegen.code.buffer;
    codegen.code.buffer = NULL;
    
    free_codegen_c_ctx(&codegen);
    return CHOMSKY3_OK;
}

/**
 * Initialize C code generation context.
 */
static chomsky3_error_t init_codegen_c_ctx(
    codegen_c_ctx_t *ctx,
    chomsky3_context_t *parent_ctx,
    const char *function_name,
    uint32_t flags
) {
    memset(ctx, 0, sizeof(codegen_c_ctx_t));
    ctx->ctx = parent_ctx;
    ctx->flags = flags;

    /* Allocate code buffer */
    ctx->code.capacity = 4096;
    ctx->code.buffer = malloc(ctx->code.capacity);
    if (!ctx->code.buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    ctx->code.buffer[0] = '\0';

    /* Copy and sanitize function name */
    ctx->function_name = strdup(sanitize_function_name(function_name));
    if (!ctx->function_name) {
        free(ctx->code.buffer);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    return CHOMSKY3_OK;
}

/**
 * Free C code generation context.
 */
static void free_codegen_c_ctx(codegen_c_ctx_t *ctx) {
    if (ctx->code.buffer) {
        free(ctx->code.buffer);
    }
    if (ctx->function_name) {
        free(ctx->function_name);
    }
}

/**
 * Emit code without newline.
 */
static chomsky3_error_t emit_code(
    codegen_c_ctx_t *ctx,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);

    /* Calculate required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return CHOMSKY3_ERR_INTERNAL;
    }

    /* Grow buffer if needed */
    if (ctx->code.size + needed + 1 > ctx->code.capacity) {
        chomsky3_error_t err = grow_code_buffer(ctx, needed + 1);
        if (err != CHOMSKY3_OK) {
            va_end(args);
            return err;
        }
    }

    /* Append to buffer */
    vsnprintf(ctx->code.buffer + ctx->code.size, needed + 1, format, args);
    ctx->code.size += needed;

    va_end(args);
    return CHOMSKY3_OK;
}

/**
 * Emit code with newline and indentation.
 */
static chomsky3_error_t emit_line(
    codegen_c_ctx_t *ctx,
    const char *format,
    ...
) {
    /* Emit indentation */
    chomsky3_error_t err = emit_indent(ctx);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    /* Emit formatted line */
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return CHOMSKY3_ERR_INTERNAL;
    }

    if (ctx->code.size + needed + 2 > ctx->code.capacity) {
        err = grow_code_buffer(ctx, needed + 2);
        if (err != CHOMSKY3_OK) {
            va_end(args);
            return err;
        }
    }

    vsnprintf(ctx->code.buffer + ctx->code.size, needed + 1, format, args);
    ctx->code.size += needed;
    ctx->code.buffer[ctx->code.size++] = '\n';
    ctx->code.buffer[ctx->code.size] = '\0';

    va_end(args);
    return CHOMSKY3_OK;
}

/**
 * Emit indentation.
 */
static chomsky3_error_t emit_indent(codegen_c_ctx_t *ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        chomsky3_error_t err = emit_code(ctx, "    ");
        if (err != CHOMSKY3_OK) {
            return err;
        }
    }
    return CHOMSKY3_OK;
}

/**
 * Generate file header.
 */
static chomsky3_error_t generate_header(codegen_c_ctx_t *ctx) {
    chomsky3_error_t err;

    if (ctx->flags & CHOMSKY3_CODEGEN_COMMENTS) {
        err = emit_line(ctx, "/**");
        if (err != CHOMSKY3_OK) return err;
        
        err = emit_line(ctx, " * Generated by libchomsky3 C code generator");
        if (err != CHOMSKY3_OK) return err;
        
        err = emit_line(ctx, " * DO NOT EDIT - This file is auto-generated");
        if (err != CHOMSKY3_OK) return err;
        
        err = emit_line(ctx, " */");
        if (err != CHOMSKY3_OK) return err;
        
        err = emit_line(ctx, "");
        if (err != CHOMSKY3_OK) return err;
    }

    /* Include headers */
    err = emit_line(ctx, "#include <stddef.h>");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "#include <stdint.h>");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "#include <string.h>");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "");
    if (err != CHOMSKY3_OK) return err;

    return CHOMSKY3_OK;
}

/**
 * Generate function signature.
 */
static chomsky3_error_t generate_function_signature(codegen_c_ctx_t *ctx) {
    chomsky3_error_t err;

    /* Function attributes */
    if (ctx->flags & CHOMSKY3_CODEGEN_STATIC) {
        err = emit_code(ctx, "static ");
        if (err != CHOMSKY3_OK) return err;
    }

    if (ctx->flags & CHOMSKY3_CODEGEN_INLINE) {
        err = emit_code(ctx, "inline ");
        if (err != CHOMSKY3_OK) return err;
    }

    /* Function signature */
    err = emit_line(ctx, "int %s(", ctx->function_name);
    if (err != CHOMSKY3_OK) return err;

    ctx->indent_level++;
    
    err = emit_line(ctx, "const char *input,");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "size_t input_len,");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "size_t *match_start,");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "size_t *match_end");
    if (err != CHOMSKY3_OK) return err;

    ctx->indent_level--;
    
    err = emit_code(ctx, ")");
    if (err != CHOMSKY3_OK) return err;

    return CHOMSKY3_OK;
}

/**
 * Generate function body from bytecode.
 */
static chomsky3_error_t generate_function_body(
    codegen_c_ctx_t *ctx,
    const chomsky3_bytecode_t *bytecode
) {
    chomsky3_error_t err;

    err = emit_line(ctx, "");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "{");
    if (err != CHOMSKY3_OK) return err;
    
    ctx->indent_level++;

    /* Declare variables */
    err = emit_line(ctx, "const char *pos = input;");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "const char *end = input + input_len;");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "const char *mark = NULL;");
    if (err != CHOMSKY3_OK) return err;

    if (ctx->num_captures > 0) {
        err = emit_line(ctx, "const char *captures[%u * 2];", ctx->num_captures);
        if (err != CHOMSKY3_OK) return err;
        
        err = emit_line(ctx, "memset(captures, 0, sizeof(captures));");
        if (err != CHOMSKY3_OK) return err;
    }

    err = emit_line(ctx, "");
    if (err != CHOMSKY3_OK) return err;

    /* Generate code for each instruction */
    for (size_t i = 0; i < bytecode->header.num_instructions; i++) {
        err = generate_instruction(ctx, &bytecode->instructions[i], i);
        if (err != CHOMSKY3_OK) return err;
    }

    /* Failure label */
    err = emit_line(ctx, "");
    if (err != CHOMSKY3_OK) return err;
    
    err = emit_line(ctx, "fail:");
    if (err != CHOMSKY3_OK) return err;
    
    ctx->indent_level++;
    err = emit_line(ctx, "return 0;  /* No match */");
    if (err != CHOMSKY3_OK) return err;
    ctx->indent_level--;

    ctx->indent_level--;
    err = emit_line(ctx, "}");
    if (err != CHOMSKY3_OK) return err;

    return CHOMSKY3_OK;
}

/**
 * Generate code for a single instruction.
 */
static chomsky3_error_t generate_instruction(
    codegen_c_ctx_t *ctx,
    const chomsky3_instruction_t *inst,
    size_t index
) {
    chomsky3_error_t err;

    /* Label for this instruction */
    if (ctx->flags & CHOMSKY3_CODEGEN_COMMENTS) {
        err = emit_line(ctx, "/* Instruction %zu: %s */", index,
                       chomsky3_opcode_name(inst->opcode));
        if (err != CHOMSKY3_OK) return err;
    }

    err = emit_line(ctx, "L%zu:", index);
    if (err != CHOMSKY3_OK) return err;

    ctx->indent_level++;

    switch (inst->opcode) {
        case CHOMSKY3_OP_CHAR:
            err = emit_line(ctx, "if (pos >= end || *pos != %u) goto fail;",
                           inst->operand1);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos++;");
            break;

        case CHOMSKY3_OP_CHAR_RANGE:
            err = emit_line(ctx, "if (pos >= end || *pos < %u || *pos > %u) goto fail;",
                           inst->operand1, inst->operand2);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos++;");
            break;

        case CHOMSKY3_OP_ANY:
            err = emit_line(ctx, "if (pos >= end || *pos == '\\n') goto fail;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos++;");
            break;

        case CHOMSKY3_OP_ANY_NL:
            err = emit_line(ctx, "if (pos >= end) goto fail;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos++;");
            break;

        case CHOMSKY3_OP_STRING: {
            err = emit_line(ctx, "if (pos + %u > end) goto fail;",
                           inst->operand2);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "if (memcmp(pos, data + %u, %u) != 0) goto fail;",
                           inst->operand1, inst->operand2);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos += %u;", inst->operand2);
            break;
        }

        case CHOMSKY3_OP_ANCHOR_START:
            err = emit_line(ctx, "if (pos != input) goto fail;");
            break;

        case CHOMSKY3_OP_ANCHOR_END:
            err = emit_line(ctx, "if (pos != end) goto fail;");
            break;

        case CHOMSKY3_OP_ANCHOR_LINE_START:
            err = emit_line(ctx, "if (pos != input && pos[-1] != '\\n') goto fail;");
            break;

        case CHOMSKY3_OP_ANCHOR_LINE_END:
            err = emit_line(ctx, "if (pos != end && *pos != '\\n') goto fail;");
            break;

        case CHOMSKY3_OP_ANCHOR_WORD:
            err = emit_line(ctx, "{");
            if (err != CHOMSKY3_OK) return err;
            ctx->indent_level++;
            err = emit_line(ctx, "int before = (pos > input) && isalnum(pos[-1]);");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "int after = (pos < end) && isalnum(*pos);");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "if (before == after) goto fail;");
            if (err != CHOMSKY3_OK) return err;
            ctx->indent_level--;
            err = emit_line(ctx, "}");
            break;

        case CHOMSKY3_OP_SAVE_START:
            err = emit_line(ctx, "captures[%u * 2] = pos;", inst->operand1);
            break;

        case CHOMSKY3_OP_SAVE_END:
            err = emit_line(ctx, "captures[%u * 2 + 1] = pos;", inst->operand1);
            break;

        case CHOMSKY3_OP_BACKREF:
            err = emit_line(ctx, "{");
            if (err != CHOMSKY3_OK) return err;
            ctx->indent_level++;
            err = emit_line(ctx, "const char *ref_start = captures[%u * 2];",
                           inst->operand1);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "const char *ref_end = captures[%u * 2 + 1];",
                           inst->operand1);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "size_t ref_len = ref_end - ref_start;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "if (!ref_start || pos + ref_len > end) goto fail;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "if (memcmp(pos, ref_start, ref_len) != 0) goto fail;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos += ref_len;");
            if (err != CHOMSKY3_OK) return err;
            ctx->indent_level--;
            err = emit_line(ctx, "}");
            break;

        case CHOMSKY3_OP_JUMP:
            err = emit_line(ctx, "goto L%u;", inst->operand1);
            break;

        case CHOMSKY3_OP_SPLIT:
            err = emit_line(ctx, "mark = pos;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "/* Try first alternative at L%u */", inst->operand1);
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "/* On failure, try L%u */", inst->operand2);
            break;

        case CHOMSKY3_OP_MATCH:
            err = emit_line(ctx, "if (match_start) *match_start = input - input;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "if (match_end) *match_end = pos - input;");
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "return 1;  /* Match */");
            break;

        case CHOMSKY3_OP_FAIL:
            err = emit_line(ctx, "goto fail;");
            break;

        case CHOMSKY3_OP_NOP:
            err = emit_line(ctx, "/* NOP */");
            break;

        default:
            if (ctx->flags & CHOMSKY3_CODEGEN_COMMENTS) {
                err = emit_line(ctx, "/* Unimplemented opcode: %u */", inst->opcode);
                if (err != CHOMSKY3_OK) return err;
            }
            err = emit_line(ctx, "goto fail;");
            break;
    }

    if (err != CHOMSKY3_OK) return err;

    ctx->indent_level--;
    err = emit_line(ctx, "");
    if (err != CHOMSKY3_OK) return err;

    return CHOMSKY3_OK;
}

/**
 * Generate code for an IR node.
 */
static chomsky3_error_t generate_ir_node(
    codegen_c_ctx_t *ctx,
    const chomsky3_ir_node_t *node
) {
    chomsky3_error_t err;

    switch (node->type) {
        case CHOMSKY3_IR_LITERAL: {
            if (node->data.literal.length == 1) {
                err = emit_line(ctx, "if (pos >= end || *pos != '%c') return 0;",
                               node->data.literal.text[0]);
                if (err != CHOMSKY3_OK) return err;
                err = emit_line(ctx, "pos++;");
            } else {
                const char *escaped = escape_string(
                    node->data.literal.text,
                    node->data.literal.length
                );
                err = emit_line(ctx, "if (pos + %zu > end) return 0;",
                               node->data.literal.length);
                if (err != CHOMSKY3_OK) return err;
                err = emit_line(ctx, "if (memcmp(pos, \"%s\", %zu) != 0) return 0;",
                               escaped, node->data.literal.length);
                if (err != CHOMSKY3_OK) return err;
                err = emit_line(ctx, "pos += %zu;", node->data.literal.length);
            }
            break;
        }

        case CHOMSKY3_IR_ANY_CHAR: {
            if (node->data.any_char.include_newline) {
                err = emit_line(ctx, "if (pos >= end) return 0;");
            } else {
                err = emit_line(ctx, "if (pos >= end || *pos == '\\n') return 0;");
            }
            if (err != CHOMSKY3_OK) return err;
            err = emit_line(ctx, "pos++;");
            break;
        }

        case CHOMSKY3_IR_ANCHOR: {
            switch (node->data.anchor.type) {
                case CHOMSKY3_ANCHOR_START:
                    err = emit_line(ctx, "if (pos != input) return 0;");
                    break;
                case CHOMSKY3_ANCHOR_END:
                    err = emit_line(ctx, "if (pos != end) return 0;");
                    break;
                case CHOMSKY3_ANCHOR_LINE_START:
                    err = emit_line(ctx, "if (pos != input && pos[-1] != '\\n') return 0;");
                    break;
                case CHOMSKY3_ANCHOR_LINE_END:
                    err = emit_line(ctx, "if (pos != end && *pos != '\\n') return 0;");
                    break;
                default:
                    err = CHOMSKY3_OK;
                    break;
            }
            break;
        }

        case CHOMSKY3_IR_CONCATENATION: {
            for (size_t i = 0; i < node->child_count; i++) {
                err = generate_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) return err;
            }
            break;
        }

        case CHOMSKY3_IR_ALTERNATION: {
            size_t label = ctx->label_counter++;
            
            for (size_t i = 0; i < node->child_count; i++) {
                err = emit_line(ctx, "{");
                if (err != CHOMSKY3_OK) return err;
                ctx->indent_level++;
                
                err = emit_line(ctx, "const char *save_pos = pos;");
                if (err != CHOMSKY3_OK) return err;
                
                err = generate_ir_node(ctx, node->children[i]);
                if (err != CHOMSKY3_OK) return err;
                
                err = emit_line(ctx, "goto alt_success_%zu;", label);
                if (err != CHOMSKY3_OK) return err;
                
                ctx->indent_level--;
                err = emit_line(ctx, "}");
                if (err != CHOMSKY3_OK) return err;