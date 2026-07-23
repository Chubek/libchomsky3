/**
 * libchomsky3 - C Code Generation Interface
 * 
 * Header file providing the C source code generation interface for
 * transforming IR into standalone C code.
 */

#ifndef CHOMSKY3_CODEGEN_C_H
#define CHOMSKY3_CODEGEN_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "chomsky3.h"
#include "compiler.h"
#include "codegen.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct chomsky3_c_generator chomsky3_c_generator_t;
typedef struct chomsky3_c_output chomsky3_c_output_t;
typedef struct chomsky3_bytecode chomsky3_bytecode_t;

/* Direct bytecode-to-C translation (simple one-shot interface) */

/**
 * Generate C source from compiled bytecode.
 *
 * @param ctx Context
 * @param bytecode Bytecode to translate
 * @param function_name Name for the generated match function
 * @param flags Compilation flags (chomsky3_flags_t)
 * @param output Output C source string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_c_from_bytecode(
    chomsky3_context_t *ctx,
    const chomsky3_bytecode_t *bytecode,
    const char *function_name,
    uint32_t flags,
    char **output
);

/**
 * Generate C source from IR.
 *
 * @param ctx Context
 * @param ir Intermediate representation
 * @param function_name Name for the generated match function
 * @param flags Compilation flags (chomsky3_flags_t)
 * @param output Output C source string (must be freed by caller)
 * @return Error code
 */
chomsky3_error_t chomsky3_codegen_c_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    const char *function_name,
    uint32_t flags,
    char **output
);

/* C code generation style */
typedef enum {
    CHOMSKY3_C_STYLE_COMPACT = 0,   /* Compact, minimal whitespace */
    CHOMSKY3_C_STYLE_READABLE = 1,  /* Readable with proper indentation */
    CHOMSKY3_C_STYLE_DEBUG = 2      /* Debug-friendly with extra comments */
} chomsky3_c_style_t;

/* C standard version */
typedef enum {
    CHOMSKY3_C_STD_C89 = 0,         /* ANSI C (C89/C90) */
    CHOMSKY3_C_STD_C99 = 1,         /* C99 */
    CHOMSKY3_C_STD_C11 = 2,         /* C11 */
    CHOMSKY3_C_STD_C17 = 3,         /* C17 */
    CHOMSKY3_C_STD_C23 = 4          /* C23 */
} chomsky3_c_std_t;

/* C output format */
typedef enum {
    CHOMSKY3_C_FORMAT_SINGLE_FILE = 0,  /* Single .c file */
    CHOMSKY3_C_FORMAT_HEADER_SOURCE = 1, /* Separate .h and .c */
    CHOMSKY3_C_FORMAT_INLINE_HEADER = 2  /* Header-only with inline functions */
} chomsky3_c_format_t;

/* C optimization hints */
typedef enum {
    CHOMSKY3_C_OPT_NONE = 0,        /* No optimization hints */
    CHOMSKY3_C_OPT_INLINE = 1 << 0, /* Use inline hints */
    CHOMSKY3_C_OPT_CONST = 1 << 1,  /* Use const qualifiers */
    CHOMSKY3_C_OPT_RESTRICT = 1 << 2, /* Use restrict keyword */
    CHOMSKY3_C_OPT_LIKELY = 1 << 3, /* Use likely/unlikely macros */
    CHOMSKY3_C_OPT_UNROLL = 1 << 4, /* Unroll loops */
    CHOMSKY3_C_OPT_ALL = 0xFF       /* All optimizations */
} chomsky3_c_opt_hints_t;

/* C code generation options */
typedef struct {
    /* Basic options */
    const char *function_name;      /* Generated function name */
    const char *prefix;             /* Prefix for generated symbols */
    chomsky3_c_style_t style;       /* Code style */
    chomsky3_c_std_t std_version;   /* C standard version */
    chomsky3_c_format_t format;     /* Output format */
    
    /* Function attributes */
    bool static_function;           /* Make function static */
    bool inline_function;           /* Make function inline */
    const char *visibility;         /* Visibility attribute (e.g., "hidden") */
    
    /* Code generation */
    bool emit_comments;             /* Emit explanatory comments */
    bool emit_line_directives;      /* Emit #line directives */
    bool emit_assertions;           /* Emit runtime assertions */
    bool emit_bounds_checks;        /* Emit bounds checking code */
    uint32_t optimization_hints;    /* Optimization hints (chomsky3_c_opt_hints_t) */
    
    /* Header options */
    const char *header_guard;       /* Header guard name (NULL = auto) */
    const char *header_includes;    /* Additional includes for header */
    bool extern_c;                  /* Wrap in extern "C" */
    
    /* Documentation */
    bool emit_doxygen;              /* Emit Doxygen comments */
    const char *function_doc;       /* Function documentation */
    
    /* Advanced */
    size_t indent_size;             /* Indentation size (spaces) */
    bool use_tabs;                  /* Use tabs instead of spaces */
    size_t max_line_length;         /* Maximum line length (0 = no limit) */
    bool emit_metadata;             /* Emit pattern metadata as comments */
} chomsky3_c_options_t;

/* C output structure */
struct chomsky3_c_output {
    char *source;                   /* Generated C source code */
    size_t source_len;              /* Source code length */
    char *header;                   /* Generated header (if separate) */
    size_t header_len;              /* Header length */
    
    /* Metadata */
    size_t num_functions;           /* Number of generated functions */
    size_t num_static_data;         /* Number of static data items */
    size_t estimated_stack_usage;   /* Estimated stack usage in bytes */
};

/* C generator context */
struct chomsky3_c_generator {
    chomsky3_context_t *ctx;        /* Parent context */
    chomsky3_c_options_t options;   /* C generation options */
    
    /* Internal state */
    void *internal;                 /* Opaque internal state */
};

/**
 * Create a new C code generator instance.
 * 
 * @param ctx Parent context
 * @param options C generation options (NULL for defaults)
 * @return New C generator or NULL on failure
 */
chomsky3_c_generator_t *chomsky3_c_generator_new(
    chomsky3_context_t *ctx,
    const chomsky3_c_options_t *options
);

/**
 * Free a C code generator instance.
 * 
 * @param generator C generator to free
 */
void chomsky3_c_generator_free(chomsky3_c_generator_t *generator);

/**
 * Get default C generation options.
 * 
 * @param options Options structure to fill
 */
void chomsky3_c_options_default(chomsky3_c_options_t *options);

/**
 * Generate C code from IR.
 * 
 * @param generator C generator instance
 * @param ir Intermediate representation
 * @param output Output structure (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_c_generate(
    chomsky3_c_generator_t *generator,
    const chomsky3_ir_t *ir,
    chomsky3_c_output_t **output
);

/**
 * Generate C code from pattern.
 * 
 * @param generator C generator instance
 * @param pattern Compiled pattern
 * @param output Output structure (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_c_generate_from_pattern(
    chomsky3_c_generator_t *generator,
    const chomsky3_pattern_t *pattern,
    chomsky3_c_output_t **output
);

/**
 * Free C output structure.
 * 
 * @param output Output to free
 */
void chomsky3_c_output_free(chomsky3_c_output_t *output);

/**
 * Write C output to files.
 * 
 * @param output C output structure
 * @param source_path Path for source file
 * @param header_path Path for header file (NULL if not needed)
 * @return Error code
 */
chomsky3_error_t chomsky3_c_output_write(
    const chomsky3_c_output_t *output,
    const char *source_path,
    const char *header_path
);

/**
 * Validate C generation options.
 * 
 * @param options Options to validate
 * @return true if valid, false otherwise
 */
bool chomsky3_c_options_validate(const chomsky3_c_options_t *options);

/**
 * Set C standard version.
 * 
 * @param options Options structure
 * @param std_version C standard version
 */
void chomsky3_c_options_set_std(
    chomsky3_c_options_t *options,
    chomsky3_c_std_t std_version
);

/**
 * Set optimization hints.
 * 
 * @param options Options structure
 * @param hints Optimization hints bitmask
 */
void chomsky3_c_options_set_optimization_hints(
    chomsky3_c_options_t *options,
    uint32_t hints
);

/**
 * Enable/disable specific optimization hint.
 * 
 * @param options Options structure
 * @param hint Optimization hint
 * @param enable Enable or disable
 */
void chomsky3_c_options_toggle_hint(
    chomsky3_c_options_t *options,
    chomsky3_c_opt_hints_t hint,
    bool enable
);

/* Utility functions */

/**
 * Generate a valid C identifier from a string.
 * 
 * @param str Input string
 * @param prefix Optional prefix
 * @return Generated identifier (must be freed by caller)
 */
char *chomsky3_c_make_identifier(const char *str, const char *prefix);

/**
 * Generate a header guard name.
 * 
 * @param filename Header filename
 * @return Header guard name (must be freed by caller)
 */
char *chomsky3_c_make_header_guard(const char *filename);

/**
 * Escape a string for C string literal.
 * 
 * @param str Input string
 * @param len String length
 * @return Escaped string (must be freed by caller)
 */
char *chomsky3_c_escape_string(const char *str, size_t len);

/**
 * Format C code (pretty-print).
 * 
 * @param code Input C code
 * @param style Formatting style
 * @return Formatted code (must be freed by caller)
 */
char *chomsky3_c_format_code(const char *code, chomsky3_c_style_t style);

/**
 * Estimate compiled code size.
 * 
 * @param output C output structure
 * @param optimization_level Expected compiler optimization level (0-3)
 * @return Estimated size in bytes
 */
size_t chomsky3_c_estimate_code_size(
    const chomsky3_c_output_t *output,
    int optimization_level
);

/**
 * Get C standard version name.
 * 
 * @param std_version C standard version
 * @return Version name string
 */
const char *chomsky3_c_std_name(chomsky3_c_std_t std_version);

/**
 * Get minimum C standard required for features.
 * 
 * @param options C generation options
 * @return Minimum required C standard
 */
chomsky3_c_std_t chomsky3_c_get_min_std(const chomsky3_c_options_t *options);

/* Advanced generation */

/**
 * Generate C code with custom template.
 * 
 * @param generator C generator instance
 * @param ir Intermediate representation
 * @param template_path Path to custom template file
 * @param output Output structure (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_c_generate_with_template(
    chomsky3_c_generator_t *generator,
    const chomsky3_ir_t *ir,
    const char *template_path,
    chomsky3_c_output_t **output
);

/**
 * Generate multiple patterns into a single C file.
 * 
 * @param generator C generator instance
 * @param patterns Array of patterns
 * @param num_patterns Number of patterns
 * @param output Output structure (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_c_generate_multi(
    chomsky3_c_generator_t *generator,
    const chomsky3_pattern_t **patterns,
    size_t num_patterns,
    chomsky3_c_output_t **output
);

/**
 * Generate C code with profiling instrumentation.
 * 
 * @param generator C generator instance
 * @param ir Intermediate representation
 * @param output Output structure (on success)
 * @return Error code
 */
chomsky3_error_t chomsky3_c_generate_profiled(
    chomsky3_c_generator_t *generator,
    const chomsky3_ir_t *ir,
    chomsky3_c_output_t **output
);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_CODEGEN_C_H */
