/**
 * libchomsky3 - C Code Generator
 *
 * Translates libchomsky3 bytecode into a self-contained C source file
 * implementing a backtracking matcher. The generated function has the
 * signature:
 *
 *   bool <name>(const char *input, size_t length, size_t *match_start,
 *               size_t *match_end);
 *
 * and returns true on the first match, writing the match extent through
 * the out-parameters (either may be NULL). The generated code depends
 * only on the C standard library.
 */

#include "chomsky3/bytecode.h"
#include "chomsky3/codegen_c.h"
#include "chomsky3/ir.h"
#include "chomsky3/pattern.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* Growable string buffer for source emission */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    bool oom;
} srcbuf_t;

static void sb_init(srcbuf_t *sb) {
    memset(sb, 0, sizeof(*sb));
}

static void sb_free(srcbuf_t *sb) {
    free(sb->data);
}

static void sb_grow(srcbuf_t *sb, size_t extra) {
    if (sb->oom) return;
    if (sb->len + extra + 1 > sb->cap) {
        size_t ncap = sb->cap ? sb->cap * 2 : 4096;
        while (ncap < sb->len + extra + 1) ncap *= 2;
        char *grown = realloc(sb->data, ncap);
        if (!grown) {
            sb->oom = true;
            return;
        }
        sb->data = grown;
        sb->cap = ncap;
    }
}

static void sb_puts(srcbuf_t *sb, const char *s) {
    size_t n = strlen(s);
    sb_grow(sb, n);
    if (sb->oom) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_append(srcbuf_t *sb, const char *s, size_t n) {
    if (sb->oom || !s || !n) return;
    sb_grow(sb, n);
    if (sb->oom) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_printf(srcbuf_t *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char small[512];
    int needed = vsnprintf(small, sizeof(small), fmt, ap);
    va_end(ap);
    if (needed < 0) {
        sb->oom = true;
        return;
    }
    if ((size_t)needed < sizeof(small)) {
        sb_puts(sb, small);
        return;
    }
    sb_grow(sb, (size_t)needed);
    if (sb->oom) return;
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);
    sb->len += (size_t)needed;
}

static char *dup_substr(const char *s, size_t n) {
    char *out = malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *dup_string(const char *s) {
    if (!s) return NULL;
    return dup_substr(s, strlen(s));
}

static bool read_text_file(const char *path, char **out_data, size_t *out_len) {
    FILE *file = NULL;
    char *buffer = NULL;
    long size;

    if (out_data) *out_data = NULL;
    if (out_len) *out_len = 0;
    if (!path || !out_data) {
        return false;
    }

    file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }

    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return false;
    }
    buffer[size] = '\0';
    if (out_len) *out_len = (size_t)size;
    *out_data = buffer;
    fclose(file);
    return true;
}

static size_t estimate_source_size(const char *text, size_t optimization_level) {
    if (!text) return 0;
    size_t base = strlen(text);
    if (optimization_level > 3) {
        optimization_level = 3;
    }
    if (optimization_level == 0) {
        return base;
    }
    return base + (base * (4 - (size_t)optimization_level)) / 10;
}

static void c_generator_free_internal(struct chomsky3_c_generator *generator) {
    if (!generator) return;
    free(generator->internal);
    free(generator);
}

/* Emit the constant data section as a C array (if any). */
static void emit_data_section(srcbuf_t *sb, const chomsky3_bytecode_t *bc) {
    if (!bc->data_section_size) {
        return;
    }
    sb_puts(sb, "static const unsigned char chomsky3_data[] = {\n    ");
    for (size_t i = 0; i < bc->data_section_size; i++) {
        sb_printf(sb, "%u,", (unsigned)bc->data[i]);
        if ((i & 15) == 15) sb_puts(sb, "\n    ");
    }
    sb_puts(sb, "\n};\n\n");
}

/* Emit the body of one instruction as C code at the current indent.
 * Failure paths jump to `fail` (the caller provides that label); control
 * continues to the next instruction on success unless noted. */
static void emit_instruction(srcbuf_t *sb, const chomsky3_bytecode_t *bc,
                             const chomsky3_instruction_t *inst, uint32_t pc) {
    (void)bc;
    switch (inst->opcode) {
        case CHOMSKY3_OP_CHAR:
            sb_printf(sb,
                "if (sp >= end || (unsigned char)*sp != %uu) goto fail;\n"
                "sp++;\n", inst->operand1);
            break;

        case CHOMSKY3_OP_CHAR_RANGE:
            sb_printf(sb,
                "if (sp >= end || (unsigned char)*sp < %uu || (unsigned char)*sp > %uu) goto fail;\n"
                "sp++;\n", inst->operand1, inst->operand2);
            break;

        case CHOMSKY3_OP_CHAR_CLASS:
            sb_printf(sb,
                "if (sp >= end) goto fail;\n"
                "if (!chomsky3_class_match((unsigned char)*sp, chomsky3_data + %uu, %uu)) goto fail;\n"
                "sp++;\n", inst->operand1, inst->operand2);
            break;

        case CHOMSKY3_OP_ANY:
            if (inst->operand1) {
                sb_puts(sb,
                    "if (sp >= end || *sp == '\\n') goto fail;\n"
                    "sp++;\n");
            } else {
                sb_puts(sb,
                    "if (sp >= end) goto fail;\n"
                    "sp++;\n");
            }
            break;

        case CHOMSKY3_OP_ANY_NL:
            sb_puts(sb,
                "if (sp >= end) goto fail;\n"
                "sp++;\n");
            break;

        case CHOMSKY3_OP_STRING:
            sb_printf(sb,
                "if ((size_t)(end - sp) < %uu || memcmp(sp, chomsky3_data + %uu, %uu) != 0) goto fail;\n"
                "sp += %uu;\n", inst->operand2, inst->operand1,
                inst->operand2, inst->operand2);
            break;

        case CHOMSKY3_OP_STRING_ICASE:
            sb_printf(sb,
                "if ((size_t)(end - sp) < %uu) goto fail;\n"
                "for (size_t i = 0; i < %uu; i++) {\n"
                "    char c1 = sp[i], c2 = (char)chomsky3_data[%uu + i];\n"
                "    if (c1 >= 'A' && c1 <= 'Z') c1 = (char)(c1 + 32);\n"
                "    if (c2 >= 'A' && c2 <= 'Z') c2 = (char)(c2 + 32);\n"
                "    if (c1 != c2) goto fail;\n"
                "}\n"
                "sp += %uu;\n", inst->operand2, inst->operand2,
                inst->operand1, inst->operand2);
            break;

        case CHOMSKY3_OP_JUMP:
            sb_printf(sb, "goto L%u;\n", inst->operand1);
            break;

        case CHOMSKY3_OP_SPLIT:
            sb_printf(sb,
                "bt[bt_top].pc = %uu; bt[bt_top].pos = sp; bt_top++;\n"
                "goto L%u;\n", inst->operand2, inst->operand1);
            break;

        case CHOMSKY3_OP_MATCH:
            sb_puts(sb, "goto matched;\n");
            break;

        case CHOMSKY3_OP_FAIL:
            sb_puts(sb, "goto fail;\n");
            break;

        case CHOMSKY3_OP_ANCHOR_START:
            sb_puts(sb, "if (sp != input) goto fail;\n");
            break;

        case CHOMSKY3_OP_ANCHOR_END:
            sb_puts(sb, "if (sp != end) goto fail;\n");
            break;

        case CHOMSKY3_OP_ANCHOR_LINE_START:
            sb_puts(sb, "if (sp != input && sp[-1] != '\\n') goto fail;\n");
            break;

        case CHOMSKY3_OP_ANCHOR_LINE_END:
            sb_puts(sb, "if (sp != end && *sp != '\\n') goto fail;\n");
            break;

        case CHOMSKY3_OP_ANCHOR_WORD:
            sb_puts(sb,
                "{\n"
                "    bool bw = (sp > input) && chomsky3_is_word(sp[-1]);\n"
                "    bool aw = (sp < end) && chomsky3_is_word(*sp);\n"
                "    if (bw == aw) goto fail;\n"
                "}\n");
            break;

        case CHOMSKY3_OP_ANCHOR_NWORD:
            sb_puts(sb,
                "{\n"
                "    bool bw = (sp > input) && chomsky3_is_word(sp[-1]);\n"
                "    bool aw = (sp < end) && chomsky3_is_word(*sp);\n"
                "    if (bw != aw) goto fail;\n"
                "}\n");
            break;

        case CHOMSKY3_OP_SAVE_START:
            /* Capture slots beyond group 0 require dynamic state; group 0
             * start is tracked implicitly by the match out-params. */
            if (inst->operand1 == 0) {
                sb_puts(sb, "cap0_start = sp;\n");
            } else {
                sb_printf(sb, "caps[%uu] = sp;\n", inst->operand1 * 2);
            }
            break;

        case CHOMSKY3_OP_SAVE_END:
            if (inst->operand1 == 0) {
                sb_puts(sb, "cap0_end = sp;\n");
            } else {
                sb_printf(sb, "caps[%uu] = sp;\n", inst->operand1 * 2 + 1);
            }
            break;

        case CHOMSKY3_OP_BACKREF:
            sb_printf(sb,
                "{\n"
                "    const char *rs = caps[%uu];\n"
                "    const char *re = caps[%uu];\n"
                "    if (!rs || !re) goto fail;\n"
                "    size_t rl = (size_t)(re - rs);\n"
                "    if ((size_t)(end - sp) < rl || memcmp(sp, rs, rl) != 0) goto fail;\n"
                "    sp += rl;\n"
                "}\n", inst->operand1 * 2, inst->operand1 * 2 + 1);
            break;

        case CHOMSKY3_OP_LOOK_AHEAD:
        case CHOMSKY3_OP_LOOK_AHEAD_NEG:
            /* Lookahead bodies are emitted as separate static functions by
             * the caller; here we just invoke them. The mapping from
             * instruction index to helper name is fixed: la_fn_<pc>. */
            sb_printf(sb,
                "if (%sla_fn_%u(input, end, sp, caps, caps_count)) goto fail;\n"
                "goto L%u;\n",
                inst->opcode == CHOMSKY3_OP_LOOK_AHEAD_NEG ? "" : "!",
                pc, inst->operand2);
            break;

        case CHOMSKY3_OP_NOP:
        case CHOMSKY3_OP_DEBUG:
        case CHOMSKY3_OP_CHECKPOINT:
            break;

        default:
            sb_printf(sb,
                "/* unsupported opcode 0x%02x: treat as fail */\n"
                "goto fail;\n", inst->opcode);
            break;
    }
}

/* Find lookahead instructions so their bodies can be emitted as helper
 * functions. Returns number found. */
static size_t collect_lookaheads(const chomsky3_bytecode_t *bc,
                                 uint32_t *pcs, size_t max) {
    size_t n = 0;
    for (uint32_t i = 0; i < bc->header.num_instructions && n < max; i++) {
        chomsky3_opcode_t op = bc->instructions[i].opcode;
        if (op == CHOMSKY3_OP_LOOK_AHEAD || op == CHOMSKY3_OP_LOOK_AHEAD_NEG) {
            pcs[n++] = i;
        }
    }
    return n;
}

static const char *C_PREAMBLE =
    "#include <stddef.h>\n"
    "#include <stdbool.h>\n"
    "#include <string.h>\n"
    "\n"
    "typedef struct { unsigned pc; const char *pos; } chomsky3_bt_t;\n"
    "\n"
    "static bool chomsky3_is_word(char c) {\n"
    "    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||\n"
    "           (c >= '0' && c <= '9') || c == '_';\n"
    "}\n"
    "\n"
    "static bool chomsky3_class_match(unsigned char ch, const unsigned char *d, unsigned sz) {\n"
    "    if (sz < 4) return false;\n"
    "    unsigned nr = (unsigned)d[0] | ((unsigned)d[1] << 8);\n"
    "    unsigned fl = (unsigned)d[2] | ((unsigned)d[3] << 8);\n"
    "    bool m = false;\n"
    "    for (unsigned i = 0; i < nr; i++) {\n"
    "        if (4 + i * 2 + 1 >= sz) break;\n"
    "        if (ch >= d[4 + i * 2] && ch <= d[4 + i * 2 + 1]) { m = true; break; }\n"
    "    }\n"
    "    if (!m && (fl & 2)) m = ch >= '0' && ch <= '9';\n"
    "    if (!m && (fl & 4)) m = chomsky3_is_word((char)ch);\n"
    "    if (!m && (fl & 8)) m = ch == ' ' || ch == '\\t' || ch == '\\n' ||\n"
    "                            ch == '\\r' || ch == '\\f' || ch == '\\v';\n"
    "    if (fl & 1) m = !m;\n"
    "    return m;\n"
    "}\n"
    "\n";

/* Emit a backtracking dispatch footer: pops the backtrack stack and
 * dispatches to the recorded label. `fail_label` is the label this code
 * is reached from. */
static void emit_backtrack_footer(srcbuf_t *sb, const chomsky3_bytecode_t *bc,
                                  uint32_t from, uint32_t to) {
    sb_puts(sb,
        "fail:\n"
        "    if (bt_top == 0) return false;\n"
        "    bt_top--;\n"
        "    sp = bt[bt_top].pos;\n"
        "    switch (bt[bt_top].pc) {\n");

    for (uint32_t i = from; i < to && i < bc->header.num_instructions; i++) {
        sb_printf(sb, "        case %uu: goto L%u;\n", i, i);
    }

    sb_puts(sb,
        "        default: return false;\n"
        "    }\n");
}

/* Emit a lookahead helper function covering [body, cont) of the program.
 * Self-contained: uses its own label namespace (the L%u labels never
 * collide because each helper is a separate function). */
static void emit_lookahead_fn(srcbuf_t *sb, const chomsky3_bytecode_t *bc,
                              uint32_t la_pc, uint32_t caps_count) {
    (void)caps_count;
    const chomsky3_instruction_t *la = &bc->instructions[la_pc];
    uint32_t body = la->operand1;
    uint32_t cont = la->operand2;

    sb_printf(sb,
        "static bool la_fn_%u(const char *input, const char *end, const char *sp,\n"
        "                     const char **caps, size_t caps_count) {\n"
        "    chomsky3_bt_t bt[128];\n"
        "    unsigned bt_top = 0;\n"
        "    (void)caps_count;\n",
        la_pc);

    for (uint32_t i = body; i < cont && i < bc->header.num_instructions; i++) {
        const chomsky3_instruction_t *inst = &bc->instructions[i];
        sb_printf(sb, "L%u:\n", i);
        switch (inst->opcode) {
            case CHOMSKY3_OP_MATCH:
                sb_puts(sb, "return true;\n");
                break;
            default:
                emit_instruction(sb, bc, inst, i);
                break;
        }
        /* Fall through to the next label */
    }

    emit_backtrack_footer(sb, bc, body, cont);
    sb_puts(sb, "}\n\n");
}

static chomsky3_error_t generate_c_source(const chomsky3_bytecode_t *bc,
                                          const char *function_name,
                                          char **output) {
    if (!chomsky3_bytecode_validate(bc)) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    srcbuf_t sb;
    sb_init(&sb);

    uint32_t la_pcs[64];
    size_t num_la = collect_lookaheads(bc, la_pcs, 64);
    uint32_t caps_count = bc->header.num_captures * 2;

    sb_puts(&sb, "/* Generated by libchomsky3 */\n");
    if (bc->pattern_source) {
        sb_printf(&sb, "/* Pattern: %s */\n", bc->pattern_source);
    }
    sb_puts(&sb, "\n");
    sb_puts(&sb, C_PREAMBLE);
    emit_data_section(&sb, bc);

    /* Lookahead helpers must precede the main function */
    for (size_t i = 0; i < num_la; i++) {
        emit_lookahead_fn(&sb, bc, la_pcs[i], caps_count);
    }

    sb_printf(&sb,
        "bool %s(const char *input, size_t length, size_t *match_start, size_t *match_end) {\n"
        "    const char *end = input + length;\n"
        "    const char *sp = input;\n"
        "    const char *cap0_start = NULL;\n"
        "    const char *cap0_end = NULL;\n",
        function_name);
    if (caps_count > 2) {
        sb_printf(&sb,
            "    const char *capstore[%uu];\n"
            "    const char **caps = capstore;\n"
            "    size_t caps_count = %uu;\n"
            "    for (size_t i = 0; i < %uu; i++) caps[i] = NULL;\n",
            caps_count, caps_count, caps_count);
    } else {
        sb_puts(&sb,
            "    const char *capstore[2];\n"
            "    const char **caps = capstore;\n"
            "    size_t caps_count = 2;\n"
            "    caps[0] = NULL; caps[1] = NULL;\n");
    }
    sb_printf(&sb,
        "    chomsky3_bt_t bt[%uu];\n"
        "    unsigned bt_top = 0;\n"
        "\n",
        bc->header.num_instructions > 0 ? bc->header.num_instructions * 2 : 64);

    for (uint32_t i = 0; i < bc->header.num_instructions; i++) {
        sb_printf(&sb, "L%u:\n", i);
        emit_instruction(&sb, bc, &bc->instructions[i], i);
        /* Fall through to the next label */
    }

    sb_puts(&sb,
        "matched:\n"
        "    if (match_start) *match_start = (size_t)(cap0_start ? cap0_start - input : 0);\n"
        "    if (match_end) *match_end = (size_t)(cap0_end ? cap0_end - input : (sp - input));\n"
        "    return true;\n"
        "\n");

    emit_backtrack_footer(&sb, bc, 0, bc->header.num_instructions);
    sb_puts(&sb, "}\n");

    if (sb.oom) {
        sb_free(&sb);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    *output = sb.data;
    return CHOMSKY3_OK;
}

chomsky3_c_generator_t *chomsky3_c_generator_new(
    chomsky3_context_t *ctx,
    const chomsky3_c_options_t *options
) {
    chomsky3_c_generator_t *generator = calloc(1, sizeof(*generator));
    if (!generator) return NULL;
    generator->ctx = ctx;
    if (options) {
        generator->options = *options;
    } else {
        chomsky3_c_options_default(&generator->options);
    }
    return generator;
}

void chomsky3_c_generator_free(chomsky3_c_generator_t *generator) {
    if (!generator) return;
    c_generator_free_internal(generator);
}

void chomsky3_c_options_default(chomsky3_c_options_t *options) {
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->function_name = "chomsky3_match";
    options->prefix = "chomsky3_";
    options->style = CHOMSKY3_C_STYLE_READABLE;
    options->std_version = CHOMSKY3_C_STD_C11;
    options->format = CHOMSKY3_C_FORMAT_SINGLE_FILE;
    options->emit_comments = true;
    options->emit_line_directives = false;
    options->emit_assertions = false;
    options->emit_bounds_checks = true;
    options->optimization_hints = CHOMSKY3_C_OPT_INLINE | CHOMSKY3_C_OPT_CONST;
    options->indent_size = 4;
    options->use_tabs = false;
    options->max_line_length = 120;
    options->emit_metadata = false;
}

static bool c_output_fill_from_source(
    chomsky3_c_output_t **output,
    const char *source,
    const char *header
) {
    if (!output) return false;
    *output = calloc(1, sizeof(**output));
    if (!*output) return false;
    (*output)->source = dup_string(source ? source : "");
    if (!(*output)->source) {
        free(*output);
        *output = NULL;
        return false;
    }
    (*output)->source_len = strlen((*output)->source);
    (*output)->header = dup_string(header);
    if (header && !(*output)->header) {
        free((*output)->source);
        free(*output);
        *output = NULL;
        return false;
    }
    (*output)->header_len = (*output)->header ? strlen((*output)->header) : 0;
    (*output)->num_functions = 1;
    (*output)->num_static_data = 1;
    (*output)->estimated_stack_usage = (source ? strlen(source) : 0) / 2 + 64;
    return true;
}

chomsky3_error_t chomsky3_c_generate(
    chomsky3_c_generator_t *generator,
    const chomsky3_ir_t *ir,
    chomsky3_c_output_t **output
) {
    chomsky3_bytecode_t *bc = NULL;
    chomsky3_error_t err;
    char *source = NULL;

    if (!generator || !ir || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    if (!chomsky3_c_options_validate(&generator->options)) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    err = chomsky3_bytecode_from_ir(generator->ctx, ir, &bc);
    if (err != CHOMSKY3_OK) return err;
    err = generate_c_source(
        bc,
        generator->options.function_name ? generator->options.function_name
                                        : "chomsky3_match",
        &source);
    chomsky3_bytecode_free(bc);
    if (err != CHOMSKY3_OK) {
        return err;
    }
    if (!c_output_fill_from_source(output, source, NULL)) {
        free(source);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    free(source);
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_c_generate_from_pattern(
    chomsky3_c_generator_t *generator,
    const chomsky3_pattern_t *pattern,
    chomsky3_c_output_t **output
) {
    chomsky3_bytecode_t *bc = NULL;
    chomsky3_error_t err;
    char *source = NULL;

    if (!generator || !pattern || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    if (!chomsky3_c_options_validate(&generator->options)) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    err = chomsky3_bytecode_from_pattern(pattern, &bc);
    if (err != CHOMSKY3_OK || !bc) return CHOMSKY3_ERR_COMPILATION_FAILED;
    err = generate_c_source(
        bc,
        generator->options.function_name ? generator->options.function_name
                                        : "chomsky3_match",
        &source);
    chomsky3_bytecode_free(bc);
    if (err != CHOMSKY3_OK) {
        return err;
    }
    if (!c_output_fill_from_source(output, source, NULL)) {
        free(source);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    free(source);
    return CHOMSKY3_OK;
}

void chomsky3_c_output_free(chomsky3_c_output_t *output) {
    if (!output) return;
    free(output->source);
    free(output->header);
    free(output);
}

chomsky3_error_t chomsky3_c_output_write(
    const chomsky3_c_output_t *output,
    const char *source_path,
    const char *header_path
) {
    FILE *file = NULL;
    if (!output || !source_path) return CHOMSKY3_ERR_INVALID_ARGUMENT;

    file = fopen(source_path, "w");
    if (!file) return CHOMSKY3_ERR_IO_ERROR;
    if (output->source && fputs(output->source, file) == EOF) {
        fclose(file);
        return CHOMSKY3_ERR_IO_ERROR;
    }
    if (fclose(file) != 0) return CHOMSKY3_ERR_IO_ERROR;

    if (header_path && output->header) {
        file = fopen(header_path, "w");
        if (!file) return CHOMSKY3_ERR_IO_ERROR;
        if (fputs(output->header, file) == EOF) {
            fclose(file);
            return CHOMSKY3_ERR_IO_ERROR;
        }
        if (fclose(file) != 0) return CHOMSKY3_ERR_IO_ERROR;
    }
    return CHOMSKY3_OK;
}

bool chomsky3_c_options_validate(const chomsky3_c_options_t *options) {
    if (!options) return false;
    if (!options->function_name || !options->function_name[0]) return false;
    if (options->std_version > CHOMSKY3_C_STD_C23) return false;
    if (options->format > CHOMSKY3_C_FORMAT_INLINE_HEADER) return false;
    if (options->style > CHOMSKY3_C_STYLE_DEBUG) return false;
    if (options->indent_size > 16) return false;
    if (options->optimization_hints & ~CHOMSKY3_C_OPT_ALL) return false;
    return true;
}

void chomsky3_c_options_set_std(
    chomsky3_c_options_t *options,
    chomsky3_c_std_t std_version
) {
    if (options) options->std_version = std_version;
}

void chomsky3_c_options_set_optimization_hints(
    chomsky3_c_options_t *options,
    uint32_t hints
) {
    if (!options) return;
    options->optimization_hints = hints;
}

void chomsky3_c_options_toggle_hint(
    chomsky3_c_options_t *options,
    chomsky3_c_opt_hints_t hint,
    bool enable
) {
    if (!options) return;
    if (hint == CHOMSKY3_C_OPT_NONE || hint == CHOMSKY3_C_OPT_ALL) return;
    if (enable) {
        options->optimization_hints |= (uint32_t)hint;
    } else {
        options->optimization_hints &= (uint32_t)~hint;
    }
}

char *chomsky3_c_make_identifier(const char *str, const char *prefix) {
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    size_t src_len = str ? strlen(str) : 0;
    char *identifier = calloc(1, prefix_len + src_len + 2);
    size_t out = 0;
    if (!identifier) return NULL;

    if (prefix_len > 0) {
        for (size_t i = 0; i < prefix_len; i++) {
            char ch = prefix[i];
            identifier[out++] = (isalnum((unsigned char)ch) || ch == '_') ? ch : '_';
        }
    }
    for (size_t i = 0; i < src_len; i++) {
        char ch = str[i];
        identifier[out++] = (isalnum((unsigned char)ch) || ch == '_') ? ch : '_';
    }

    if (!identifier[0] || (!isalpha((unsigned char)identifier[0]) && identifier[0] != '_')) {
        memmove(identifier + 1, identifier, out + 1);
        identifier[0] = '_';
        out++;
    }
    if (out == 0) {
        identifier[0] = '_';
        identifier[1] = '\0';
    } else {
        identifier[out] = '\0';
    }
    return identifier;
}

char *chomsky3_c_make_header_guard(const char *filename) {
    char *guard = NULL;
    char *ident = chomsky3_c_make_identifier(filename ? filename : "chomsky3", "CHOMSKY3");
    size_t len;

    if (!ident) return NULL;
    len = strlen(ident) + 2;
    guard = malloc(len + 1);
    if (!guard) {
        free(ident);
        return NULL;
    }
    strcpy(guard, ident);
    strcat(guard, "_H");
    free(ident);

    for (size_t i = 0; i < strlen(guard); i++) {
        guard[i] = (char)toupper((unsigned char)guard[i]);
    }
    return guard;
}

char *chomsky3_c_escape_string(const char *str, size_t len) {
    if (!str) return NULL;
    if (len == (size_t)-1) len = strlen(str);
    srcbuf_t sb;
    sb_init(&sb);

    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];
        switch (ch) {
            case '\\':
                sb_puts(&sb, "\\\\");
                break;
            case '"':
                sb_puts(&sb, "\\\"");
                break;
            case '\n':
                sb_puts(&sb, "\\n");
                break;
            case '\r':
                sb_puts(&sb, "\\r");
                break;
            case '\t':
                sb_puts(&sb, "\\t");
                break;
            default:
                if (ch < 0x20 || ch > 0x7e) {
                    sb_printf(&sb, "\\x%02x", ch);
                } else {
                    sb_append(&sb, (char *)&ch, 1);
                }
                break;
        }
        if (sb.oom) {
            sb_free(&sb);
            return NULL;
        }
    }
    return sb.data;
}

char *chomsky3_c_format_code(const char *code, chomsky3_c_style_t style) {
    if (!code) return NULL;
    if (style == CHOMSKY3_C_STYLE_COMPACT) {
        srcbuf_t sb;
        bool at_line_start = true;
        sb_init(&sb);
        for (size_t i = 0; code[i] != '\0'; i++) {
            char ch = code[i];
            if (code[i] == '\n') {
                sb_puts(&sb, "\n");
                at_line_start = true;
                continue;
            }
            if (at_line_start && (ch == ' ' || ch == '\t')) {
                continue;
            }
            at_line_start = false;
            sb_append(&sb, &ch, 1);
        }
        if (sb.oom) {
            sb_free(&sb);
            return NULL;
        }
        return sb.data;
    }
    if (style == CHOMSKY3_C_STYLE_DEBUG) {
        srcbuf_t sb;
        sb_init(&sb);
        sb_puts(&sb, "/* debug */\n");
        sb_puts(&sb, code);
        if (sb.oom) {
            sb_free(&sb);
            return NULL;
        }
        return sb.data;
    }
    return dup_string(code);
}

size_t chomsky3_c_estimate_code_size(
    const chomsky3_c_output_t *output,
    int optimization_level
) {
    if (!output || !output->source) return 0;
    if (optimization_level < 0) optimization_level = 0;
    return estimate_source_size(output->source, (size_t)optimization_level);
}

const char *chomsky3_c_std_name(chomsky3_c_std_t std_version) {
    switch (std_version) {
        case CHOMSKY3_C_STD_C89: return "C89";
        case CHOMSKY3_C_STD_C99: return "C99";
        case CHOMSKY3_C_STD_C11: return "C11";
        case CHOMSKY3_C_STD_C17: return "C17";
        case CHOMSKY3_C_STD_C23: return "C23";
        default: return "C";
    }
}

chomsky3_c_std_t chomsky3_c_get_min_std(const chomsky3_c_options_t *options) {
    (void)options;
    return CHOMSKY3_C_STD_C99;
}

static bool write_template_output(
    const char *template_text,
    const char *source,
    char **out
) {
    const char *marker = "{{SOURCE}}";
    const char *cursor = template_text;
    const char *found;
    srcbuf_t sb;
    sb_init(&sb);

    if (!template_text || !source || !out) return false;

    found = strstr(cursor, marker);
    if (!found) {
        sb_puts(&sb, template_text);
        sb_puts(&sb, "\n");
        sb_puts(&sb, source);
        if (sb.oom) {
            sb_free(&sb);
            return false;
        }
        *out = sb.data;
        return true;
    }
    while (found) {
        sb_append(&sb, cursor, (size_t)(found - cursor));
        sb_puts(&sb, source);
        cursor = found + strlen(marker);
        found = strstr(cursor, marker);
        if (sb.oom) {
            sb_free(&sb);
            return false;
        }
    }
    sb_puts(&sb, cursor);
    if (sb.oom) {
        sb_free(&sb);
        return false;
    }
    *out = sb.data;
    return true;
}

chomsky3_error_t chomsky3_c_generate_with_template(
    chomsky3_c_generator_t *generator,
    const chomsky3_ir_t *ir,
    const char *template_path,
    chomsky3_c_output_t **output
) {
    char *source = NULL;
    char *template_body = NULL;
    char *merged = NULL;
    chomsky3_error_t err;

    if (!generator || !ir || !output || !template_path) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    if (!read_text_file(template_path, &template_body, NULL)) {
        return CHOMSKY3_ERR_IO_ERROR;
    }

    err = chomsky3_codegen_c_from_ir(
        generator->ctx,
        ir,
        generator->options.function_name ? generator->options.function_name : "chomsky3_match",
        0,
        &source
    );
    if (err != CHOMSKY3_OK) {
        free(template_body);
        return err;
    }
    if (!write_template_output(template_body, source, &merged)) {
        free(source);
        free(template_body);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    free(source);
    free(template_body);

    if (!c_output_fill_from_source(output, merged, NULL)) {
        free(merged);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    free(merged);
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_c_generate_multi(
    chomsky3_c_generator_t *generator,
    const chomsky3_pattern_t **patterns,
    size_t num_patterns,
    chomsky3_c_output_t **output
) {
    srcbuf_t sb;
    sb_init(&sb);
    if (!generator || !patterns || !num_patterns || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    if (!chomsky3_c_options_validate(&generator->options)) {
        sb_free(&sb);
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < num_patterns; i++) {
        char name[64];
        char *source = NULL;
        const char *base = generator->options.function_name ? generator->options.function_name : "chomsky3_match";
        chomsky3_bytecode_t *bc = NULL;
        chomsky3_error_t err;

        if (!patterns[i]) {
            sb_free(&sb);
            return CHOMSKY3_ERR_INVALID_ARGUMENT;
        }
        err = chomsky3_bytecode_from_pattern(patterns[i], &bc);
        if (err != CHOMSKY3_OK || !bc) {
            sb_free(&sb);
            return CHOMSKY3_ERR_COMPILATION_FAILED;
        }
        if (num_patterns == 1) {
            snprintf(name, sizeof(name), "%s", base);
        } else {
            snprintf(name, sizeof(name), "%s_%zu", base, i);
        }
        err = generate_c_source(bc, name, &source);
        chomsky3_bytecode_free(bc);
        if (err != CHOMSKY3_OK || !source) {
            sb_free(&sb);
            return err != CHOMSKY3_OK ? err : CHOMSKY3_ERR_COMPILATION_FAILED;
        }
        if (i > 0) sb_puts(&sb, "\n\n");
        sb_append(&sb, source, strlen(source));
        free(source);
        if (sb.oom) {
            sb_free(&sb);
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
    }

    if (!c_output_fill_from_source(output, sb.data, NULL)) {
        sb_free(&sb);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    (*output)->num_functions = num_patterns;
    sb_free(&sb);
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_c_generate_profiled(
    chomsky3_c_generator_t *generator,
    const chomsky3_ir_t *ir,
    chomsky3_c_output_t **output
) {
    if (!generator || !ir || !output) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    if (!chomsky3_c_options_validate(&generator->options)) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    return chomsky3_c_generate(generator, ir, output);
}

chomsky3_error_t chomsky3_codegen_c_from_bytecode(
    chomsky3_context_t *ctx,
    const chomsky3_bytecode_t *bytecode,
    const char *function_name,
    uint32_t flags,
    char **output
) {
    (void)ctx;
    (void)flags;
    if (!bytecode || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }
    return generate_c_source(bytecode,
                             function_name ? function_name : "chomsky3_match",
                             output);
}

chomsky3_error_t chomsky3_codegen_c_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    const char *function_name,
    uint32_t flags,
    char **output
) {
    if (!ir || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* IR is a thin wrapper over the AST; lower it to bytecode first, then
     * translate. */
    chomsky3_bytecode_t *bc = NULL;
    chomsky3_error_t err = chomsky3_bytecode_from_ir(ctx, ir, &bc);
    if (err != CHOMSKY3_OK) {
        return err;
    }
    err = chomsky3_codegen_c_from_bytecode(ctx, bc, function_name, flags, output);
    chomsky3_bytecode_free(bc);
    return err;
}

char *chomsky3_codegen_c_dup_for_test(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

const char *chomsky3_codegen_c_escape_for_test(const char *str) {
    return str ? str : "";
}
