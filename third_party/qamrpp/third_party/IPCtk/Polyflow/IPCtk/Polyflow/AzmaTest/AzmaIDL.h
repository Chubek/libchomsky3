#ifndef AZMA_IDL_H
#define AZMA_IDL_H

/*
 * AzmaIDL.h
 * Public API for the AzmaIDL parser and front-end.
 *
 * This header defines:
 *   - source buffers and parse options
 *   - syntax tree node kinds
 *   - handles for documents, declarations, values, and lists
 *   - diagnostics and query helpers
 *   - parser and lifetime management entrypoints
 *
 * Design goals:
 *   - keep the public surface compact and stable
 *   - allow an internal dparser-based implementation
 *   - expose a tree that is easy to inspect from tests/tools
 *   - avoid forcing callers to know internal node layouts
 */

#include "Common.h"

AZMA_EXTERN_C_BEGIN

/* =========================
   Version
   ========================= */

#define AZMA_IDL_VERSION_MAJOR 0
#define AZMA_IDL_VERSION_MINOR 1
#define AZMA_IDL_VERSION_PATCH 0

/* =========================
   Forward declarations
   ========================= */

typedef struct AzmaIDLDocument AzmaIDLDocument;
typedef struct AzmaIDLNode AzmaIDLNode;
typedef struct AzmaIDLDecl AzmaIDLDecl;
typedef struct AzmaIDLValue AzmaIDLValue;
typedef struct AzmaIDLExpr AzmaIDLExpr;
typedef struct AzmaIDLSection AzmaIDLSection;
typedef struct AzmaIDLDiagnostic AzmaIDLDiagnostic;
typedef struct AzmaIDLDiagnosticList AzmaIDLDiagnosticList;

/* =========================
   Enums
   ========================= */

typedef enum AzmaIDLNodeKind {
    AZMA_IDL_NODE_INVALID = 0,
    AZMA_IDL_NODE_DOCUMENT,

    /* top-level declarations */
    AZMA_IDL_NODE_METADATA_DECL,
    AZMA_IDL_NODE_IMPORT_DECL,
    AZMA_IDL_NODE_CONFIG_DECL,
    AZMA_IDL_NODE_API_DECL,
    AZMA_IDL_NODE_SYMBOL_DECL,
    AZMA_IDL_NODE_SECTION_DECL,

    /* section-local / cross-cutting declarations */
    AZMA_IDL_NODE_EMIT_DECL,
    AZMA_IDL_NODE_REQUIRES_DECL,
    AZMA_IDL_NODE_TAG_DECL,

    /* values */
    AZMA_IDL_NODE_NULL_VALUE,
    AZMA_IDL_NODE_BOOL_VALUE,
    AZMA_IDL_NODE_INT_VALUE,
    AZMA_IDL_NODE_STRING_VALUE,
    AZMA_IDL_NODE_RANGE_VALUE,
    AZMA_IDL_NODE_LIST_VALUE,
    AZMA_IDL_NODE_RECORD_VALUE,
    AZMA_IDL_NODE_NAME_VALUE,
    AZMA_IDL_NODE_CALL_VALUE,

    /* helpers */
    AZMA_IDL_NODE_FIELD,
    AZMA_IDL_NODE_ARGUMENT,
    AZMA_IDL_NODE_SELECTOR,
    AZMA_IDL_NODE_PATH
} AzmaIDLNodeKind;

typedef enum AzmaIDLDeclKind {
    AZMA_IDL_DECL_INVALID = 0,
    AZMA_IDL_DECL_METADATA,
    AZMA_IDL_DECL_IMPORT,
    AZMA_IDL_DECL_CONFIG,
    AZMA_IDL_DECL_API,
    AZMA_IDL_DECL_SYMBOL,
    AZMA_IDL_DECL_SECTION,
    AZMA_IDL_DECL_EMIT,
    AZMA_IDL_DECL_REQUIRES,
    AZMA_IDL_DECL_TAG
} AzmaIDLDeclKind;

typedef enum AzmaIDLValueKind {
    AZMA_IDL_VALUE_INVALID = 0,
    AZMA_IDL_VALUE_NULL,
    AZMA_IDL_VALUE_BOOL,
    AZMA_IDL_VALUE_INT,
    AZMA_IDL_VALUE_STRING,
    AZMA_IDL_VALUE_RANGE,
    AZMA_IDL_VALUE_LIST,
    AZMA_IDL_VALUE_RECORD,
    AZMA_IDL_VALUE_NAME,
    AZMA_IDL_VALUE_CALL
} AzmaIDLValueKind;

typedef enum AzmaIDLSectionKind {
    AZMA_IDL_SECTION_INVALID = 0,
    AZMA_IDL_SECTION_UNIT,
    AZMA_IDL_SECTION_FUZZ,
    AZMA_IDL_SECTION_GENERIC
} AzmaIDLSectionKind;

typedef enum AzmaIDLStringKind {
    AZMA_IDL_STRING_PLAIN = 0,
    AZMA_IDL_STRING_INTERPOLATED
} AzmaIDLStringKind;

typedef enum AzmaIDLParseFlag {
    AZMA_IDL_PARSE_NONE                = 0,
    AZMA_IDL_PARSE_COLLECT_DIAGNOSTICS = 1u << 0,
    AZMA_IDL_PARSE_RECOVER             = 1u << 1,
    AZMA_IDL_PARSE_ALLOW_EXPERIMENTAL  = 1u << 2
} AzmaIDLParseFlag;

/* =========================
   Small public structs
   ========================= */

typedef struct AzmaIDLSource {
    const char *path;
    const uint8_t *data;
    size_t size;
} AzmaIDLSource;

typedef struct AzmaIDLParseOptions {
    uint32_t flags;
    AzmaAllocator allocator;
    void *user;
} AzmaIDLParseOptions;

typedef struct AzmaIDLStringView {
    const char *data;
    size_t size;
} AzmaIDLStringView;

typedef struct AzmaIDLRangeValue {
    int64_t start;
    int64_t end;
    int inclusive_end;
} AzmaIDLRangeValue;

typedef struct AzmaIDLNamedValue {
    AzmaIDLStringView name;
} AzmaIDLNamedValue;

typedef struct AzmaIDLCallInfo {
    AzmaIDLStringView callee;
    size_t argument_count;
} AzmaIDLCallInfo;

typedef struct AzmaIDLFieldInfo {
    AzmaIDLStringView name;
    const AzmaIDLValue *value;
} AzmaIDLFieldInfo;

typedef struct AzmaIDLArgumentInfo {
    AzmaIDLStringView name;
    const AzmaIDLValue *value;
    int has_name;
} AzmaIDLArgumentInfo;

/* =========================
   Diagnostics
   ========================= */

typedef enum AzmaIDLDiagnosticSeverity {
    AZMA_IDL_DIAG_NOTE = 0,
    AZMA_IDL_DIAG_WARNING,
    AZMA_IDL_DIAG_ERROR
} AzmaIDLDiagnosticSeverity;

struct AzmaIDLDiagnostic {
    AzmaIDLDiagnosticSeverity severity;
    AzmaSourceRange where;
    AzmaIDLStringView message;
    AzmaIDLStringView code;
};

struct AzmaIDLDiagnosticList {
    const AzmaIDLDiagnostic *items;
    size_t count;
};

/* =========================
   Opaque handle notes
   ========================= */

/*
 * AzmaIDLNode / AzmaIDLDecl / AzmaIDLValue / AzmaIDLSection are intentionally
 * opaque in the public API.
 *
 * Query functions below let callers:
 *   - inspect kind/name/range
 *   - iterate children
 *   - navigate top-level declarations
 *   - inspect values in a typed way
 */

/* =========================
   Parse / document lifetime
   ========================= */

AZMA_NODISCARD AzmaStatus azma_idl_parse(
    const AzmaIDLSource *source,
    const AzmaIDLParseOptions *options,
    AzmaIDLDocument **out_document
);

void azma_idl_document_destroy(AzmaIDLDocument *document);

/* =========================
   Document queries
   ========================= */

AZMA_NODISCARD const AzmaIDLSource *azma_idl_document_source(
    const AzmaIDLDocument *document
);

AZMA_NODISCARD const AzmaIDLDiagnosticList *azma_idl_document_diagnostics(
    const AzmaIDLDocument *document
);

AZMA_NODISCARD const AzmaIDLNode *azma_idl_document_root(
    const AzmaIDLDocument *document
);

AZMA_NODISCARD size_t azma_idl_document_decl_count(
    const AzmaIDLDocument *document
);

AZMA_NODISCARD const AzmaIDLDecl *azma_idl_document_decl_at(
    const AzmaIDLDocument *document,
    size_t index
);

/* =========================
   Generic node queries
   ========================= */

AZMA_NODISCARD AzmaIDLNodeKind azma_idl_node_kind(const AzmaIDLNode *node);
AZMA_NODISCARD AzmaSourceRange azma_idl_node_range(const AzmaIDLNode *node);

AZMA_NODISCARD const AzmaIDLNode *azma_idl_node_parent(
    const AzmaIDLNode *node
);

AZMA_NODISCARD size_t azma_idl_node_child_count(
    const AzmaIDLNode *node
);

AZMA_NODISCARD const AzmaIDLNode *azma_idl_node_child_at(
    const AzmaIDLNode *node,
    size_t index
);

/* =========================
   Declaration queries
   ========================= */

AZMA_NODISCARD const AzmaIDLNode *azma_idl_decl_as_node(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD AzmaIDLDeclKind azma_idl_decl_kind(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD AzmaSourceRange azma_idl_decl_range(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD AzmaIDLStringView azma_idl_decl_name(
    const AzmaIDLDecl *decl
);

/*
 * Declaration body/value rules:
 *   - metadata/import/config/symbol/tag commonly carry one value
 *   - api/section may carry a body of nested declarations
 *   - requires may carry selector/value material
 *   - emit may carry a target name and optional record/list payload
 *
 * These queries intentionally stay generic.
 */

AZMA_NODISCARD int azma_idl_decl_has_value(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD const AzmaIDLValue *azma_idl_decl_value(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD size_t azma_idl_decl_nested_count(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD const AzmaIDLDecl *azma_idl_decl_nested_at(
    const AzmaIDLDecl *decl,
    size_t index
);

/* =========================
   Section queries
   ========================= */

AZMA_NODISCARD const AzmaIDLSection *azma_idl_decl_as_section(
    const AzmaIDLDecl *decl
);

AZMA_NODISCARD const AzmaIDLNode *azma_idl_section_as_node(
    const AzmaIDLSection *section
);

AZMA_NODISCARD AzmaIDLSectionKind azma_idl_section_kind(
    const AzmaIDLSection *section
);

AZMA_NODISCARD AzmaIDLStringView azma_idl_section_name(
    const AzmaIDLSection *section
);

AZMA_NODISCARD size_t azma_idl_section_decl_count(
    const AzmaIDLSection *section
);

AZMA_NODISCARD const AzmaIDLDecl *azma_idl_section_decl_at(
    const AzmaIDLSection *section,
    size_t index
);

/* =========================
   Value queries
   ========================= */

AZMA_NODISCARD const AzmaIDLNode *azma_idl_value_as_node(
    const AzmaIDLValue *value
);

AZMA_NODISCARD AzmaIDLValueKind azma_idl_value_kind(
    const AzmaIDLValue *value
);

AZMA_NODISCARD AzmaSourceRange azma_idl_value_range(
    const AzmaIDLValue *value
);

AZMA_NODISCARD int azma_idl_value_as_bool(
    const AzmaIDLValue *value,
    int *out_value
);

AZMA_NODISCARD int azma_idl_value_as_int(
    const AzmaIDLValue *value,
    int64_t *out_value
);

AZMA_NODISCARD int azma_idl_value_as_string(
    const AzmaIDLValue *value,
    AzmaIDLStringView *out_value,
    AzmaIDLStringKind *out_kind
);

AZMA_NODISCARD int azma_idl_value_as_range(
    const AzmaIDLValue *value,
    AzmaIDLRangeValue *out_value
);

AZMA_NODISCARD int azma_idl_value_as_name(
    const AzmaIDLValue *value,
    AzmaIDLNamedValue *out_value
);

AZMA_NODISCARD int azma_idl_value_as_call(
    const AzmaIDLValue *value,
    AzmaIDLCallInfo *out_value
);

AZMA_NODISCARD size_t azma_idl_value_item_count(
    const AzmaIDLValue *value
);

AZMA_NODISCARD const AzmaIDLValue *azma_idl_value_item_at(
    const AzmaIDLValue *value,
    size_t index
);

AZMA_NODISCARD size_t azma_idl_value_field_count(
    const AzmaIDLValue *value
);

AZMA_NODISCARD int azma_idl_value_field_at(
    const AzmaIDLValue *value,
    size_t index,
    AzmaIDLFieldInfo *out_field
);

AZMA_NODISCARD int azma_idl_value_field_by_name(
    const AzmaIDLValue *value,
    AzmaIDLStringView name,
    AzmaIDLFieldInfo *out_field
);

AZMA_NODISCARD size_t azma_idl_value_argument_count(
    const AzmaIDLValue *value
);

AZMA_NODISCARD int azma_idl_value_argument_at(
    const AzmaIDLValue *value,
    size_t index,
    AzmaIDLArgumentInfo *out_argument
);

/* =========================
   Text helpers
   ========================= */

AZMA_NODISCARD AzmaIDLStringView azma_idl_sv_from_cstr(const char *text);

AZMA_NODISCARD int azma_idl_sv_eq(
    AzmaIDLStringView a,
    AzmaIDLStringView b
);

AZMA_NODISCARD int azma_idl_sv_eq_cstr(
    AzmaIDLStringView a,
    const char *b
);

/*
 * Returns a newly allocated, NUL-terminated copy of the string view using the
 * provided allocator. Caller owns the result and must free it with azma_free().
 */
AZMA_NODISCARD char *azma_idl_sv_to_cstr(
    AzmaAllocator *allocator,
    AzmaIDLStringView value
);

/* =========================
   Debug / formatting helpers
   ========================= */

AZMA_NODISCARD const char *azma_idl_node_kind_name(AzmaIDLNodeKind kind);
AZMA_NODISCARD const char *azma_idl_decl_kind_name(AzmaIDLDeclKind kind);
AZMA_NODISCARD const char *azma_idl_value_kind_name(AzmaIDLValueKind kind);
AZMA_NODISCARD const char *azma_idl_section_kind_name(AzmaIDLSectionKind kind);
AZMA_NODISCARD const char *azma_idl_diag_severity_name(AzmaIDLDiagnosticSeverity severity);

/*
 * Debug dump helpers write a human-readable representation to the given FILE*.
 * They are intended for tests and development tooling, not stable serialization.
 */
void azma_idl_dump_document(FILE *out, const AzmaIDLDocument *document);
void azma_idl_dump_node(FILE *out, const AzmaIDLNode *node, int indent);

/* =========================
   Convenience iteration macros
   ========================= */

#define AZMA_IDL_FOR_EACH_DOCUMENT_DECL(doc_, index_name_, decl_name_)                     \
    for (size_t index_name_ = 0, azma__n_##index_name_ = azma_idl_document_decl_count(doc_); \
         index_name_ < azma__n_##index_name_ &&                                            \
         (((decl_name_) = azma_idl_document_decl_at((doc_), (index_name_))) != NULL || 1); \
         ++index_name_)

#define AZMA_IDL_FOR_EACH_DECL_CHILD(decl_, index_name_, child_name_)                      \
    for (size_t index_name_ = 0, azma__n_##index_name_ = azma_idl_decl_nested_count(decl_); \
         index_name_ < azma__n_##index_name_ &&                                            \
         (((child_name_) = azma_idl_decl_nested_at((decl_), (index_name_))) != NULL || 1); \
         ++index_name_)

#define AZMA_IDL_FOR_EACH_SECTION_DECL(section_, index_name_, decl_name_)                  \
    for (size_t index_name_ = 0, azma__n_##index_name_ = azma_idl_section_decl_count(section_); \
         index_name_ < azma__n_##index_name_ &&                                            \
         (((decl_name_) = azma_idl_section_decl_at((section_), (index_name_))) != NULL || 1); \
         ++index_name_)

#define AZMA_IDL_FOR_EACH_VALUE_ITEM(value_, index_name_, item_name_)                      \
    for (size_t index_name_ = 0, azma__n_##index_name_ = azma_idl_value_item_count(value_); \
         index_name_ < azma__n_##index_name_ &&                                            \
         (((item_name_) = azma_idl_value_item_at((value_), (index_name_))) != NULL || 1); \
         ++index_name_)

/* =========================
   Implementation notes
   ========================= */

/*
 * Expected parser behavior:
 *   - On success:
 *       returns AZMA_STATUS_OK
 *       *out_document is non-NULL
 *   - On parse/validation failure with diagnostics enabled:
 *       returns AZMA_STATUS_PARSE_ERROR or AZMA_STATUS_INVALID_DATA
 *       *out_document may still be non-NULL if recovery was enabled
 *   - On OOM:
 *       returns AZMA_STATUS_OOM
 *       *out_document is NULL
 *
 * Recommended caller pattern:
 *
 *   AzmaIDLDocument *doc = NULL;
 *   AzmaStatus st = azma_idl_parse(&src, &opt, &doc);
 *   if (doc) {
 *       ...
 *       azma_idl_document_destroy(doc);
 *   }
 */

AZMA_EXTERN_C_END

#endif /* AZMA_IDL_H */
