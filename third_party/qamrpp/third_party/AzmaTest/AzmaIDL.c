#include "AzmaIDL.h"

#include <ctype.h>
#include <inttypes.h>

/*
 * AzmaIDL.c
 *
 * Current implementation level:
 *   - document allocation / teardown
 *   - diagnostics storage
 *   - internal AST representation
 *   - public query helpers
 *   - debug dumping
 *   - minimal parser scaffold with a tiny fallback parser
 *
 * The parser currently recognizes a conservative subset:
 *   - top-level declarations introduced by '%' or bare identifiers
 *   - simple name/value declarations using ':' or '='
 *   - scalar values: null, booleans, integers, strings, names
 *   - lists: [a, b, c]
 *   - records: { key: value, ... }
 *
 * It is intentionally structured so a dparser-backed frontend can replace
 * azma_idl_parse() internals later without changing the public API.
 */

/* ========================================================================= */
/* Internal data structures                                                  */
/* ========================================================================= */

typedef struct AzmaIDLNodeImpl AzmaIDLNodeImpl;
typedef struct AzmaIDLDeclImpl AzmaIDLDeclImpl;
typedef struct AzmaIDLValueImpl AzmaIDLValueImpl;
typedef struct AzmaIDLSectionImpl AzmaIDLSectionImpl;

typedef struct AzmaIDLVecPtr {
    void **items;
    size_t count;
    size_t capacity;
} AzmaIDLVecPtr;

typedef struct AzmaIDLFieldImpl {
    AzmaIDLStringView name;
    AzmaIDLValueImpl *value;
} AzmaIDLFieldImpl;

typedef struct AzmaIDLArgumentImpl {
    AzmaIDLStringView name;
    AzmaIDLValueImpl *value;
    int has_name;
} AzmaIDLArgumentImpl;

typedef struct AzmaIDLDiagBuilder {
    AzmaIDLDiagnostic *items;
    size_t count;
    size_t capacity;
} AzmaIDLDiagBuilder;

struct AzmaIDLNode {
    int _opaque;
};

struct AzmaIDLDecl {
    int _opaque;
};

struct AzmaIDLValue {
    int _opaque;
};

struct AzmaIDLSection {
    int _opaque;
};

struct AzmaIDLNodeImpl {
    AzmaIDLNodeKind kind;
    AzmaSourceRange range;
    AzmaIDLNodeImpl *parent;
    AzmaIDLVecPtr children;
};

struct AzmaIDLValueImpl {
    AzmaIDLNodeImpl base;
    AzmaIDLValueKind value_kind;
    union {
        int bool_value;
        int64_t int_value;
        struct {
            AzmaIDLStringView text;
            AzmaIDLStringKind kind;
        } string_value;
        AzmaIDLRangeValue range_value;
        AzmaIDLNamedValue name_value;
        struct {
            AzmaIDLStringView callee;
            AzmaIDLArgumentImpl *args;
            size_t arg_count;
        } call_value;
        struct {
            AzmaIDLValueImpl **items;
            size_t count;
        } list_value;
        struct {
            AzmaIDLFieldImpl *fields;
            size_t count;
        } record_value;
    } as;
};

struct AzmaIDLDeclImpl {
    AzmaIDLNodeImpl base;
    AzmaIDLDeclKind decl_kind;
    AzmaIDLStringView name;
    AzmaIDLValueImpl *value;
    AzmaIDLDeclImpl **nested;
    size_t nested_count;
};

struct AzmaIDLSectionImpl {
    AzmaIDLDeclImpl decl;
    AzmaIDLSectionKind section_kind;
};

struct AzmaIDLDocument {
    AzmaAllocator allocator;
    AzmaIDLSource source;
    AzmaIDLNodeImpl *root;
    AzmaIDLDeclImpl **decls;
    size_t decl_count;
    AzmaIDLDiagnosticList diagnostics;
};

/* ========================================================================= */
/* Internal helpers                                                          */
/* ========================================================================= */

typedef struct AzmaIDLParser {
    AzmaIDLDocument *doc;
    const uint8_t *cur;
    const uint8_t *end;
    const uint8_t *line_start;
    size_t line;
    size_t column;
    uint32_t flags;
} AzmaIDLParser;

static AzmaAllocator azma_idl__allocator_or_default(const AzmaIDLParseOptions *options) {
    if (options) {
        return options->allocator;
    }
    return azma_allocator_default();
}

static void *azma_idl__alloc(AzmaIDLDocument *doc, size_t size) {
    void *p;
    AZMA_ASSERT(doc != NULL);
    p = azma_alloc(&doc->allocator, size);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

static void *azma_idl__realloc(AzmaIDLDocument *doc, void *ptr, size_t size) {
    AZMA_ASSERT(doc != NULL);
    return azma_realloc(&doc->allocator, ptr, size);
}

static AzmaSourcePos azma_idl__make_pos(size_t line, size_t column, size_t offset) {
    AzmaSourcePos p;
    p.line = line;
    p.column = column;
    p.offset = offset;
    return p;
}

static AzmaSourceRange azma_idl__make_empty_range(size_t offset) {
    AzmaSourceRange r;
    r.begin = azma_idl__make_pos(1, 1, offset);
    r.end = r.begin;
    return r;
}

static AzmaSourceRange azma_idl__range_from_offsets(AzmaIDLParser *p, size_t begin, size_t end) {
    AzmaSourceRange r;
    if (!p || !p->doc || !p->doc->source.data) {
        r.begin = azma_idl__make_pos(1, begin + 1, begin);
        r.end = azma_idl__make_pos(1, end + 1, end);
        return r;
    }

    {
        const uint8_t *base = p->doc->source.data;
        const uint8_t *it;
        size_t line = 1;
        size_t column = 1;
        const uint8_t *begin_ptr = base + begin;
        const uint8_t *end_ptr = base + end;

        for (it = base; it < begin_ptr; ++it) {
            if (*it == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
        }
        r.begin = azma_idl__make_pos(line, column, begin);

        for (; it < end_ptr; ++it) {
            if (*it == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
        }
        r.end = azma_idl__make_pos(line, column, end);
    }
    return r;
}

static AzmaSourceRange azma_idl__current_empty_range(AzmaIDLParser *p) {
    size_t offset = (size_t)(p->cur - p->doc->source.data);
    return azma_idl__range_from_offsets(p, offset, offset);
}

static int azma_idl__vec_push(AzmaIDLDocument *doc, AzmaIDLVecPtr *vec, void *ptr) {
    void **new_items;
    size_t new_cap;

    if (vec->count == vec->capacity) {
        new_cap = vec->capacity ? (vec->capacity * 2) : 8;
        new_items = (void **)azma_idl__realloc(doc, vec->items, new_cap * sizeof(void *));
        if (!new_items) {
            return 0;
        }
        vec->items = new_items;
        vec->capacity = new_cap;
    }

    vec->items[vec->count++] = ptr;
    return 1;
}

static int azma_idl__diag_push(
    AzmaIDLDocument *doc,
    AzmaIDLDiagBuilder *builder,
    AzmaIDLDiagnosticSeverity severity,
    AzmaSourceRange where,
    const char *code,
    const char *message
) {
    AzmaIDLDiagnostic *new_items;
    size_t new_cap;
    AzmaIDLDiagnostic *d;
    size_t code_len = code ? strlen(code) : 0;
    size_t msg_len = message ? strlen(message) : 0;
    char *code_copy = NULL;
    char *msg_copy = NULL;

    if (builder->count == builder->capacity) {
        new_cap = builder->capacity ? (builder->capacity * 2) : 8;
        new_items = (AzmaIDLDiagnostic *)azma_idl__realloc(
            doc,
            builder->items,
            new_cap * sizeof(AzmaIDLDiagnostic)
        );
        if (!new_items) {
            return 0;
        }
        builder->items = new_items;
        builder->capacity = new_cap;
    }

    if (code_len) {
        code_copy = (char *)azma_idl__alloc(doc, code_len + 1);
        if (!code_copy) return 0;
        memcpy(code_copy, code, code_len);
        code_copy[code_len] = '\0';
    }

    if (msg_len) {
        msg_copy = (char *)azma_idl__alloc(doc, msg_len + 1);
        if (!msg_copy) return 0;
        memcpy(msg_copy, message, msg_len);
        msg_copy[msg_len] = '\0';
    }

    d = &builder->items[builder->count++];
    d->severity = severity;
    d->where = where;
    d->code.data = code_copy;
    d->code.size = code_len;
    d->message.data = msg_copy;
    d->message.size = msg_len;
    return 1;
}

static AzmaIDLNodeImpl *azma_idl__new_node(
    AzmaIDLDocument *doc,
    AzmaIDLNodeKind kind,
    AzmaSourceRange range
) {
    AzmaIDLNodeImpl *n = (AzmaIDLNodeImpl *)azma_idl__alloc(doc, sizeof(*n));
    if (!n) return NULL;
    n->kind = kind;
    n->range = range;
    return n;
}

static AzmaIDLValueImpl *azma_idl__new_value(
    AzmaIDLDocument *doc,
    AzmaIDLNodeKind node_kind,
    AzmaIDLValueKind value_kind,
    AzmaSourceRange range
) {
    AzmaIDLValueImpl *v = (AzmaIDLValueImpl *)azma_idl__alloc(doc, sizeof(*v));
    if (!v) return NULL;
    v->base.kind = node_kind;
    v->base.range = range;
    v->value_kind = value_kind;
    return v;
}

static AzmaIDLDeclImpl *azma_idl__new_decl(
    AzmaIDLDocument *doc,
    AzmaIDLNodeKind node_kind,
    AzmaIDLDeclKind decl_kind,
    AzmaSourceRange range
) {
    AzmaIDLDeclImpl *d = (AzmaIDLDeclImpl *)azma_idl__alloc(doc, sizeof(*d));
    if (!d) return NULL;
    d->base.kind = node_kind;
    d->base.range = range;
    d->decl_kind = decl_kind;
    return d;
}

static AzmaIDLSectionImpl *azma_idl__new_section(
    AzmaIDLDocument *doc,
    AzmaSourceRange range
) {
    AzmaIDLSectionImpl *s = (AzmaIDLSectionImpl *)azma_idl__alloc(doc, sizeof(*s));
    if (!s) return NULL;
    s->decl.base.kind = AZMA_IDL_NODE_SECTION_DECL;
    s->decl.base.range = range;
    s->decl.decl_kind = AZMA_IDL_DECL_SECTION;
    s->section_kind = AZMA_IDL_SECTION_GENERIC;
    return s;
}

static int azma_idl__node_add_child(
    AzmaIDLDocument *doc,
    AzmaIDLNodeImpl *parent,
    AzmaIDLNodeImpl *child
) {
    if (!parent || !child) return 0;
    child->parent = parent;
    return azma_idl__vec_push(doc, &parent->children, child);
}

static int azma_idl__document_add_decl(AzmaIDLDocument *doc, AzmaIDLDeclImpl *decl) {
    AzmaIDLDeclImpl **new_decls;
    if (doc->decl_count == 0 || (doc->decl_count & (doc->decl_count - 1)) == 0) {
        size_t alloc_count = doc->decl_count ? doc->decl_count * 2 : 8;
        new_decls = (AzmaIDLDeclImpl **)azma_idl__realloc(
            doc,
            doc->decls,
            alloc_count * sizeof(AzmaIDLDeclImpl *)
        );
        if (!new_decls) {
            return 0;
        }
        doc->decls = new_decls;
    }

    doc->decls[doc->decl_count++] = decl;
    return 1;
}

static int azma_idl__decl_add_nested(
    AzmaIDLDocument *doc,
    AzmaIDLDeclImpl *decl,
    AzmaIDLDeclImpl *child
) {
    AzmaIDLDeclImpl **new_items;
    size_t new_cap;

    if (!decl || !child) return 0;

    if ((decl->nested_count & (decl->nested_count - 1)) == 0) {
        new_cap = decl->nested_count ? (decl->nested_count * 2) : 4;
        new_items = (AzmaIDLDeclImpl **)azma_idl__realloc(
            doc,
            decl->nested,
            new_cap * sizeof(AzmaIDLDeclImpl *)
        );
        if (!new_items) return 0;
        decl->nested = new_items;
    }

    decl->nested[decl->nested_count++] = child;
    child->base.parent = &decl->base;
    return 1;
}

static int azma_idl__value_list_append(
    AzmaIDLDocument *doc,
    AzmaIDLValueImpl *list,
    AzmaIDLValueImpl *item
) {
    AzmaIDLValueImpl **new_items;
    size_t new_cap;
    size_t n;

    AZMA_ASSERT(list != NULL);
    AZMA_ASSERT(list->value_kind == AZMA_IDL_VALUE_LIST);

    n = list->as.list_value.count;
    if ((n & (n - 1)) == 0) {
        new_cap = n ? (n * 2) : 4;
        new_items = (AzmaIDLValueImpl **)azma_idl__realloc(
            doc,
            list->as.list_value.items,
            new_cap * sizeof(AzmaIDLValueImpl *)
        );
        if (!new_items) return 0;
        list->as.list_value.items = new_items;
    }

    list->as.list_value.items[n] = item;
    list->as.list_value.count++;
    item->base.parent = &list->base;
    return azma_idl__node_add_child(doc, &list->base, &item->base);
}

static int azma_idl__value_record_append(
    AzmaIDLDocument *doc,
    AzmaIDLValueImpl *record,
    AzmaIDLStringView name,
    AzmaIDLValueImpl *value
) {
    AzmaIDLFieldImpl *new_fields;
    size_t new_cap;
    size_t n;

    AZMA_ASSERT(record != NULL);
    AZMA_ASSERT(record->value_kind == AZMA_IDL_VALUE_RECORD);

    n = record->as.record_value.count;
    if ((n & (n - 1)) == 0) {
        new_cap = n ? (n * 2) : 4;
        new_fields = (AzmaIDLFieldImpl *)azma_idl__realloc(
            doc,
            record->as.record_value.fields,
            new_cap * sizeof(AzmaIDLFieldImpl)
        );
        if (!new_fields) return 0;
        record->as.record_value.fields = new_fields;
    }

    record->as.record_value.fields[n].name = name;
    record->as.record_value.fields[n].value = value;
    record->as.record_value.count++;
    value->base.parent = &record->base;
    return azma_idl__node_add_child(doc, &record->base, &value->base);
}

static char *azma_idl__dup_bytes(AzmaIDLDocument *doc, const uint8_t *data, size_t size) {
    char *out;
    if (!size) {
        out = (char *)azma_idl__alloc(doc, 1);
        if (out) out[0] = '\0';
        return out;
    }
    out = (char *)azma_idl__alloc(doc, size + 1);
    if (!out) return NULL;
    memcpy(out, data, size);
    out[size] = '\0';
    return out;
}

static AzmaIDLStringView azma_idl__copy_sv(
    AzmaIDLDocument *doc,
    const uint8_t *data,
    size_t size
) {
    AzmaIDLStringView sv;
    sv.data = azma_idl__dup_bytes(doc, data, size);
    sv.size = sv.data ? size : 0;
    return sv;
}

static void azma_idl__skip_ws(AzmaIDLParser *p) {
    for (;;) {
        while (p->cur < p->end && isspace((unsigned char)*p->cur)) {
            if (*p->cur == '\n') {
                p->line++;
                p->column = 1;
                p->line_start = p->cur + 1;
            } else {
                p->column++;
            }
            p->cur++;
        }

        if (p->cur + 1 < p->end && p->cur[0] == '/' && p->cur[1] == '/') {
            p->cur += 2;
            p->column += 2;
            while (p->cur < p->end && *p->cur != '\n') {
                p->cur++;
                p->column++;
            }
            continue;
        }

        if (p->cur + 1 < p->end && p->cur[0] == '/' && p->cur[1] == '*') {
            p->cur += 2;
            p->column += 2;
            while (p->cur + 1 < p->end && !(p->cur[0] == '*' && p->cur[1] == '/')) {
                if (*p->cur == '\n') {
                    p->line++;
                    p->column = 1;
                    p->line_start = p->cur + 1;
                } else {
                    p->column++;
                }
                p->cur++;
            }
            if (p->cur + 1 < p->end) {
                p->cur += 2;
                p->column += 2;
            }
            continue;
        }

        break;
    }
}

static int azma_idl__is_ident_start(int c) {
    return isalpha(c) || c == '_' || c == '%';
}

static int azma_idl__is_ident_continue(int c) {
    return isalnum(c) || c == '_' || c == '-' || c == '.';
}

static AzmaIDLStringView azma_idl__parse_ident(AzmaIDLParser *p) {
    const uint8_t *start = p->cur;
    AzmaIDLStringView sv = {0};

    if (p->cur >= p->end || !azma_idl__is_ident_start((unsigned char)*p->cur)) {
        return sv;
    }

    p->cur++;
    while (p->cur < p->end && azma_idl__is_ident_continue((unsigned char)*p->cur)) {
        p->cur++;
    }

    sv.data = (const char *)start;
    sv.size = (size_t)(p->cur - start);
    return sv;
}

static int azma_idl__peek(AzmaIDLParser *p, uint8_t ch) {
    azma_idl__skip_ws(p);
    return p->cur < p->end && *p->cur == ch;
}

static int azma_idl__eat(AzmaIDLParser *p, uint8_t ch) {
    azma_idl__skip_ws(p);
    if (p->cur < p->end && *p->cur == ch) {
        p->cur++;
        p->column++;
        return 1;
    }
    return 0;
}

static AzmaIDLValueImpl *azma_idl__parse_value(AzmaIDLParser *p, AzmaIDLDiagBuilder *diags);

static AzmaIDLValueImpl *azma_idl__parse_string(AzmaIDLParser *p) {
    const uint8_t *start;
    const uint8_t *content;
    AzmaIDLValueImpl *v;
    char *out;
    size_t len = 0;

    if (p->cur >= p->end || *p->cur != '"') {
        return NULL;
    }

    start = p->cur;
    p->cur++;
    content = p->cur;

    while (p->cur < p->end && *p->cur != '"') {
        if (*p->cur == '\\' && p->cur + 1 < p->end) {
            p->cur += 2;
        } else {
            p->cur++;
        }
    }

    len = (size_t)(p->cur - content);
    v = azma_idl__new_value(
        p->doc,
        AZMA_IDL_NODE_STRING_VALUE,
        AZMA_IDL_VALUE_STRING,
        azma_idl__range_from_offsets(p,
            (size_t)(start - p->doc->source.data),
            (size_t)(p->cur - p->doc->source.data))
    );
    if (!v) return NULL;

    out = azma_idl__dup_bytes(p->doc, content, len);
    if (!out) return NULL;

    v->as.string_value.text.data = out;
    v->as.string_value.text.size = len;
    v->as.string_value.kind = AZMA_IDL_STRING_PLAIN;

    if (p->cur < p->end && *p->cur == '"') {
        p->cur++;
    }

    return v;
}

static AzmaIDLValueImpl *azma_idl__parse_number_or_name(AzmaIDLParser *p) {
    const uint8_t *start = p->cur;

    if (p->cur < p->end && (*p->cur == '-' || isdigit((unsigned char)*p->cur))) {
        char *endptr = NULL;
        int64_t value;
        AzmaIDLValueImpl *v;

        if (*p->cur == '-') {
            p->cur++;
        }
        if (p->cur >= p->end || !isdigit((unsigned char)*p->cur)) {
            p->cur = start;
        } else {
            while (p->cur < p->end && isdigit((unsigned char)*p->cur)) {
                p->cur++;
            }

            {
                char *tmp = azma_idl__dup_bytes(p->doc, start, (size_t)(p->cur - start));
                if (!tmp) return NULL;
                value = (int64_t)strtoll(tmp, &endptr, 10);
                (void)endptr;

                v = azma_idl__new_value(
                    p->doc,
                    AZMA_IDL_NODE_INT_VALUE,
                    AZMA_IDL_VALUE_INT,
                    azma_idl__range_from_offsets(
                        p,
                        (size_t)(start - p->doc->source.data),
                        (size_t)(p->cur - p->doc->source.data))
                );
                if (!v) return NULL;
                v->as.int_value = value;
                return v;
            }
        }
    }

    if (azma_idl__is_ident_start(p->cur < p->end ? *p->cur : 0)) {
        AzmaIDLStringView tok = azma_idl__parse_ident(p);
        AzmaIDLValueImpl *v;

        if (tok.size == 4 && memcmp(tok.data, "null", 4) == 0) {
            return azma_idl__new_value(
                p->doc, AZMA_IDL_NODE_NULL_VALUE, AZMA_IDL_VALUE_NULL,
                azma_idl__range_from_offsets(
                    p,
                    (size_t)((const uint8_t *)tok.data - p->doc->source.data),
                    (size_t)((const uint8_t *)tok.data - p->doc->source.data + tok.size))
            );
        }

        if (tok.size == 4 && memcmp(tok.data, "true", 4) == 0) {
            v = azma_idl__new_value(
                p->doc, AZMA_IDL_NODE_BOOL_VALUE, AZMA_IDL_VALUE_BOOL,
                azma_idl__range_from_offsets(
                    p,
                    (size_t)((const uint8_t *)tok.data - p->doc->source.data),
                    (size_t)((const uint8_t *)tok.data - p->doc->source.data + tok.size))
            );
            if (!v) return NULL;
            v->as.bool_value = 1;
            return v;
        }

        if (tok.size == 5 && memcmp(tok.data, "false", 5) == 0) {
            v = azma_idl__new_value(
                p->doc, AZMA_IDL_NODE_BOOL_VALUE, AZMA_IDL_VALUE_BOOL,
                azma_idl__range_from_offsets(
                    p,
                    (size_t)((const uint8_t *)tok.data - p->doc->source.data),
                    (size_t)((const uint8_t *)tok.data - p->doc->source.data + tok.size))
            );
            if (!v) return NULL;
            v->as.bool_value = 0;
            return v;
        }

        v = azma_idl__new_value(
            p->doc,
            AZMA_IDL_NODE_NAME_VALUE,
            AZMA_IDL_VALUE_NAME,
            azma_idl__range_from_offsets(
                p,
                (size_t)((const uint8_t *)tok.data - p->doc->source.data),
                (size_t)((const uint8_t *)tok.data - p->doc->source.data + tok.size))
        );
        if (!v) return NULL;
        v->as.name_value.name = azma_idl__copy_sv(p->doc, (const uint8_t *)tok.data, tok.size);
        return v;
    }

    return NULL;
}

static AzmaIDLValueImpl *azma_idl__parse_list(AzmaIDLParser *p, AzmaIDLDiagBuilder *diags) {
    AzmaIDLValueImpl *list;
    const uint8_t *start;

    if (!azma_idl__eat(p, '[')) {
        return NULL;
    }

    start = p->cur - 1;
    list = azma_idl__new_value(
        p->doc,
        AZMA_IDL_NODE_LIST_VALUE,
        AZMA_IDL_VALUE_LIST,
        azma_idl__range_from_offsets(
            p,
            (size_t)(start - p->doc->source.data),
            (size_t)(start - p->doc->source.data + 1))
    );
    if (!list) return NULL;

    azma_idl__skip_ws(p);
    if (azma_idl__eat(p, ']')) {
        return list;
    }

    for (;;) {
        AzmaIDLValueImpl *item = azma_idl__parse_value(p, diags);
        if (!item) {
            azma_idl__diag_push(
                p->doc,
                diags,
                AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_VALUE",
                "expected list element"
            );
            return list;
        }

        if (!azma_idl__value_list_append(p->doc, list, item)) {
            return NULL;
        }

        azma_idl__skip_ws(p);
        if (azma_idl__eat(p, ']')) {
            break;
        }
        if (!azma_idl__eat(p, ',')) {
            azma_idl__diag_push(
                p->doc,
                diags,
                AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_COMMA",
                "expected ',' or ']' in list"
            );
            break;
        }
    }

    list->base.range.end.offset = (size_t)(p->cur - p->doc->source.data);
    return list;
}

static AzmaIDLValueImpl *azma_idl__parse_record(AzmaIDLParser *p, AzmaIDLDiagBuilder *diags) {
    AzmaIDLValueImpl *rec;
    const uint8_t *start;

    if (!azma_idl__eat(p, '{')) {
        return NULL;
    }

    start = p->cur - 1;
    rec = azma_idl__new_value(
        p->doc,
        AZMA_IDL_NODE_RECORD_VALUE,
        AZMA_IDL_VALUE_RECORD,
        azma_idl__range_from_offsets(
            p,
            (size_t)(start - p->doc->source.data),
            (size_t)(start - p->doc->source.data + 1))
    );
    if (!rec) return NULL;

    azma_idl__skip_ws(p);
    if (azma_idl__eat(p, '}')) {
        return rec;
    }

    for (;;) {
        AzmaIDLStringView name_src;
        AzmaIDLStringView name;
        AzmaIDLValueImpl *value;

        azma_idl__skip_ws(p);
        name_src = azma_idl__parse_ident(p);
        if (!name_src.data) {
            azma_idl__diag_push(
                p->doc, diags, AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_FIELD",
                "expected record field name"
            );
            return rec;
        }

        name = azma_idl__copy_sv(p->doc, (const uint8_t *)name_src.data, name_src.size);

        if (!azma_idl__eat(p, ':') && !azma_idl__eat(p, '=')) {
            azma_idl__diag_push(
                p->doc, diags, AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_COLON",
                "expected ':' or '=' after field name"
            );
            return rec;
        }

        value = azma_idl__parse_value(p, diags);
        if (!value) {
            azma_idl__diag_push(
                p->doc, diags, AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_VALUE",
                "expected field value"
            );
            return rec;
        }

        if (!azma_idl__value_record_append(p->doc, rec, name, value)) {
            return NULL;
        }

        azma_idl__skip_ws(p);
        if (azma_idl__eat(p, '}')) {
            break;
        }
        if (!azma_idl__eat(p, ',')) {
            azma_idl__diag_push(
                p->doc, diags, AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_COMMA",
                "expected ',' or '}' in record"
            );
            break;
        }
    }

    rec->base.range.end.offset = (size_t)(p->cur - p->doc->source.data);
    return rec;
}

static AzmaIDLValueImpl *azma_idl__parse_value(AzmaIDLParser *p, AzmaIDLDiagBuilder *diags) {
    azma_idl__skip_ws(p);

    if (p->cur >= p->end) {
        return NULL;
    }

    if (*p->cur == '"') {
        return azma_idl__parse_string(p);
    }
    if (*p->cur == '[') {
        return azma_idl__parse_list(p, diags);
    }
    if (*p->cur == '{') {
        return azma_idl__parse_record(p, diags);
    }

    return azma_idl__parse_number_or_name(p);
}

static AzmaIDLDeclKind azma_idl__decl_kind_from_name(AzmaIDLStringView name, AzmaIDLNodeKind *node_kind) {
    const char *s = name.data;
    size_t n = name.size;

    if (n && s[0] == '%') {
        s++;
        n--;
    }

    if (n == 8 && memcmp(s, "metadata", 8) == 0) {
        *node_kind = AZMA_IDL_NODE_METADATA_DECL;
        return AZMA_IDL_DECL_METADATA;
    }
    if (n == 6 && memcmp(s, "import", 6) == 0) {
        *node_kind = AZMA_IDL_NODE_IMPORT_DECL;
        return AZMA_IDL_DECL_IMPORT;
    }
    if (n == 6 && memcmp(s, "config", 6) == 0) {
        *node_kind = AZMA_IDL_NODE_CONFIG_DECL;
        return AZMA_IDL_DECL_CONFIG;
    }
    if (n == 3 && memcmp(s, "api", 3) == 0) {
        *node_kind = AZMA_IDL_NODE_API_DECL;
        return AZMA_IDL_DECL_API;
    }
    if (n == 6 && memcmp(s, "symbol", 6) == 0) {
        *node_kind = AZMA_IDL_NODE_SYMBOL_DECL;
        return AZMA_IDL_DECL_SYMBOL;
    }
    if (n == 7 && memcmp(s, "section", 7) == 0) {
        *node_kind = AZMA_IDL_NODE_SECTION_DECL;
        return AZMA_IDL_DECL_SECTION;
    }
    if (n == 4 && memcmp(s, "emit", 4) == 0) {
        *node_kind = AZMA_IDL_NODE_EMIT_DECL;
        return AZMA_IDL_DECL_EMIT;
    }
    if (n == 8 && memcmp(s, "requires", 8) == 0) {
        *node_kind = AZMA_IDL_NODE_REQUIRES_DECL;
        return AZMA_IDL_DECL_REQUIRES;
    }
    if (n == 3 && memcmp(s, "tag", 3) == 0) {
        *node_kind = AZMA_IDL_NODE_TAG_DECL;
        return AZMA_IDL_DECL_TAG;
    }

    *node_kind = AZMA_IDL_NODE_CONFIG_DECL;
    return AZMA_IDL_DECL_CONFIG;
}

static AzmaIDLDeclImpl *azma_idl__parse_decl(AzmaIDLParser *p, AzmaIDLDiagBuilder *diags) {
    AzmaIDLStringView keyword_src;
    AzmaIDLStringView name_copy;
    AzmaIDLNodeKind node_kind = AZMA_IDL_NODE_INVALID;
    AzmaIDLDeclKind decl_kind;
    AzmaIDLDeclImpl *decl;
    const uint8_t *start;

    azma_idl__skip_ws(p);
    if (p->cur >= p->end) {
        return NULL;
    }

    start = p->cur;
    keyword_src = azma_idl__parse_ident(p);
    if (!keyword_src.data) {
        azma_idl__diag_push(
            p->doc, diags, AZMA_IDL_DIAG_ERROR,
            azma_idl__current_empty_range(p),
            "E_DECL",
            "expected declaration keyword"
        );
        return NULL;
    }

    decl_kind = azma_idl__decl_kind_from_name(keyword_src, &node_kind);

    if (decl_kind == AZMA_IDL_DECL_SECTION) {
        AzmaIDLSectionImpl *section = azma_idl__new_section(
            p->doc,
            azma_idl__range_from_offsets(
                p,
                (size_t)(start - p->doc->source.data),
                (size_t)(p->cur - p->doc->source.data))
        );
        if (!section) return NULL;

        azma_idl__skip_ws(p);
        if (p->cur < p->end && azma_idl__is_ident_start((unsigned char)*p->cur)) {
            AzmaIDLStringView sec_name_src = azma_idl__parse_ident(p);
            section->decl.name = azma_idl__copy_sv(
                p->doc,
                (const uint8_t *)sec_name_src.data,
                sec_name_src.size
            );

            if (azma_idl_sv_eq_cstr(section->decl.name, "unit")) {
                section->section_kind = AZMA_IDL_SECTION_UNIT;
            } else if (azma_idl_sv_eq_cstr(section->decl.name, "fuzz")) {
                section->section_kind = AZMA_IDL_SECTION_FUZZ;
            } else {
                section->section_kind = AZMA_IDL_SECTION_GENERIC;
            }
        }

        if (azma_idl__eat(p, '{')) {
            for (;;) {
                AzmaIDLDeclImpl *child;
                azma_idl__skip_ws(p);
                if (azma_idl__eat(p, '}')) {
                    break;
                }
                child = azma_idl__parse_decl(p, diags);
                if (!child) break;
                if (!azma_idl__decl_add_nested(p->doc, &section->decl, child)) {
                    return &section->decl;
                }
                azma_idl__node_add_child(p->doc, &section->decl.base, &child->base);
                azma_idl__eat(p, ';');
            }
        } else {
            azma_idl__eat(p, ';');
        }

        return &section->decl;
    }

    decl = azma_idl__new_decl(
        p->doc,
        node_kind,
        decl_kind,
        azma_idl__range_from_offsets(
            p,
            (size_t)(start - p->doc->source.data),
            (size_t)(p->cur - p->doc->source.data))
    );
    if (!decl) return NULL;

    azma_idl__skip_ws(p);
    if (p->cur < p->end && azma_idl__is_ident_start((unsigned char)*p->cur)) {
        AzmaIDLStringView nsrc = azma_idl__parse_ident(p);
        name_copy = azma_idl__copy_sv(p->doc, (const uint8_t *)nsrc.data, nsrc.size);
        decl->name = name_copy;
    }

    azma_idl__skip_ws(p);

    if (azma_idl__eat(p, ':') || azma_idl__eat(p, '=')) {
        decl->value = azma_idl__parse_value(p, diags);
        if (decl->value) {
            azma_idl__node_add_child(p->doc, &decl->base, &decl->value->base);
        } else {
            azma_idl__diag_push(
                p->doc, diags, AZMA_IDL_DIAG_ERROR,
                azma_idl__current_empty_range(p),
                "E_VALUE",
                "expected declaration value"
            );
        }
    } else if (azma_idl__peek(p, '{')) {
        AzmaIDLValueImpl *rec = azma_idl__parse_record(p, diags);
        decl->value = rec;
        if (rec) {
            azma_idl__node_add_child(p->doc, &decl->base, &rec->base);
        }
    }

    azma_idl__eat(p, ';');
    decl->base.range.end.offset = (size_t)(p->cur - p->doc->source.data);
    return decl;
}

/* ========================================================================= */
/* Public parse API                                                          */
/* ========================================================================= */

AzmaStatus azma_idl_parse(
    const AzmaIDLSource *source,
    const AzmaIDLParseOptions *options,
    AzmaIDLDocument **out_document
) {
    AzmaIDLDocument *doc;
    AzmaIDLParser p;
    AzmaIDLDiagBuilder diags;
    AzmaStatus result = AZMA_STATUS_OK;

    if (out_document) {
        *out_document = NULL;
    }

    if (!source || !out_document) {
        return AZMA_STATUS_INVALID_ARGUMENT;
    }

    if (!source->data && source->size != 0) {
        return AZMA_STATUS_INVALID_ARGUMENT;
    }

    {
        AzmaAllocator alloc = azma_idl__allocator_or_default(options);
        doc = (AzmaIDLDocument *)azma_alloc(&alloc, sizeof(*doc));
    }
    if (!doc) {
        return AZMA_STATUS_OOM;
    }
    memset(doc, 0, sizeof(*doc));

    doc->allocator = azma_idl__allocator_or_default(options);
    doc->source = *source;

    doc->root = azma_idl__new_node(
        doc,
        AZMA_IDL_NODE_DOCUMENT,
        azma_idl__range_from_offsets(NULL, 0, source->size)
    );
    if (!doc->root) {
        azma_idl_document_destroy(doc);
        return AZMA_STATUS_OOM;
    }

    memset(&p, 0, sizeof(p));
    p.doc = doc;
    p.cur = source->data;
    p.end = source->data + source->size;
    p.line = 1;
    p.column = 1;
    p.flags = options ? options->flags : 0;

    memset(&diags, 0, sizeof(diags));

    while (1) {
        AzmaIDLDeclImpl *decl;

        azma_idl__skip_ws(&p);
        if (p.cur >= p.end) {
            break;
        }

        decl = azma_idl__parse_decl(&p, &diags);
        if (!decl) {
            result = AZMA_STATUS_PARSE_ERROR;
            if (!(p.flags & AZMA_IDL_PARSE_RECOVER)) {
                break;
            }

            while (p.cur < p.end && *p.cur != '\n' && *p.cur != ';') {
                p.cur++;
            }
            azma_idl__eat(&p, ';');
            continue;
        }

        if (!azma_idl__document_add_decl(doc, decl) ||
            !azma_idl__node_add_child(doc, doc->root, &decl->base)) {
            azma_idl_document_destroy(doc);
            return AZMA_STATUS_OOM;
        }
    }

    doc->diagnostics.items = diags.items;
    doc->diagnostics.count = diags.count;

    if (diags.count != 0 && result == AZMA_STATUS_OK) {
        result = AZMA_STATUS_PARSE_ERROR;
    }

    *out_document = doc;
    return result;
}

void azma_idl_document_destroy(AzmaIDLDocument *document) {
    if (!document) return;
    azma_free(&document->allocator, document);
}

/* ========================================================================= */
/* Public document queries                                                   */
/* ========================================================================= */

const AzmaIDLSource *azma_idl_document_source(const AzmaIDLDocument *document) {
    return document ? &document->source : NULL;
}

const AzmaIDLDiagnosticList *azma_idl_document_diagnostics(const AzmaIDLDocument *document) {
    return document ? &document->diagnostics : NULL;
}

const AzmaIDLNode *azma_idl_document_root(const AzmaIDLDocument *document) {
    return document ? (const AzmaIDLNode *)document->root : NULL;
}

size_t azma_idl_document_decl_count(const AzmaIDLDocument *document) {
    return document ? document->decl_count : 0;
}

const AzmaIDLDecl *azma_idl_document_decl_at(const AzmaIDLDocument *document, size_t index) {
    if (!document || index >= document->decl_count) {
        return NULL;
    }
    return (const AzmaIDLDecl *)document->decls[index];
}

/* ========================================================================= */
/* Generic node queries                                                      */
/* ========================================================================= */

AzmaIDLNodeKind azma_idl_node_kind(const AzmaIDLNode *node) {
    const AzmaIDLNodeImpl *n = (const AzmaIDLNodeImpl *)node;
    return n ? n->kind : AZMA_IDL_NODE_INVALID;
}

AzmaSourceRange azma_idl_node_range(const AzmaIDLNode *node) {
    const AzmaIDLNodeImpl *n = (const AzmaIDLNodeImpl *)node;
    return n ? n->range : azma_idl__make_empty_range(0);
}

const AzmaIDLNode *azma_idl_node_parent(const AzmaIDLNode *node) {
    const AzmaIDLNodeImpl *n = (const AzmaIDLNodeImpl *)node;
    return n ? (const AzmaIDLNode *)n->parent : NULL;
}

size_t azma_idl_node_child_count(const AzmaIDLNode *node) {
    const AzmaIDLNodeImpl *n = (const AzmaIDLNodeImpl *)node;
    return n ? n->children.count : 0;
}

const AzmaIDLNode *azma_idl_node_child_at(const AzmaIDLNode *node, size_t index) {
    const AzmaIDLNodeImpl *n = (const AzmaIDLNodeImpl *)node;
    if (!n || index >= n->children.count) {
        return NULL;
    }
    return (const AzmaIDLNode *)n->children.items[index];
}

/* ========================================================================= */
/* Declaration queries                                                       */
/* ========================================================================= */

const AzmaIDLNode *azma_idl_decl_as_node(const AzmaIDLDecl *decl) {
    return (const AzmaIDLNode *)decl;
}

AzmaIDLDeclKind azma_idl_decl_kind(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    return d ? d->decl_kind : AZMA_IDL_DECL_INVALID;
}

AzmaSourceRange azma_idl_decl_range(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    return d ? d->base.range : azma_idl__make_empty_range(0);
}

AzmaIDLStringView azma_idl_decl_name(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    AzmaIDLStringView sv = {0};
    return d ? d->name : sv;
}

int azma_idl_decl_has_value(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    return d && d->value != NULL;
}

const AzmaIDLValue *azma_idl_decl_value(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    return d ? (const AzmaIDLValue *)d->value : NULL;
}

size_t azma_idl_decl_nested_count(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    return d ? d->nested_count : 0;
}

const AzmaIDLDecl *azma_idl_decl_nested_at(const AzmaIDLDecl *decl, size_t index) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    if (!d || index >= d->nested_count) return NULL;
    return (const AzmaIDLDecl *)d->nested[index];
}

/* ========================================================================= */
/* Section queries                                                           */
/* ========================================================================= */

const AzmaIDLSection *azma_idl_decl_as_section(const AzmaIDLDecl *decl) {
    const AzmaIDLDeclImpl *d = (const AzmaIDLDeclImpl *)decl;
    if (!d || d->decl_kind != AZMA_IDL_DECL_SECTION) {
        return NULL;
    }
    return (const AzmaIDLSection *)decl;
}

const AzmaIDLNode *azma_idl_section_as_node(const AzmaIDLSection *section) {
    return (const AzmaIDLNode *)section;
}

AzmaIDLSectionKind azma_idl_section_kind(const AzmaIDLSection *section) {
    const AzmaIDLSectionImpl *s = (const AzmaIDLSectionImpl *)section;
    return s ? s->section_kind : AZMA_IDL_SECTION_INVALID;
}

AzmaIDLStringView azma_idl_section_name(const AzmaIDLSection *section) {
    const AzmaIDLSectionImpl *s = (const AzmaIDLSectionImpl *)section;
    AzmaIDLStringView sv = {0};
    return s ? s->decl.name : sv;
}

size_t azma_idl_section_decl_count(const AzmaIDLSection *section) {
    const AzmaIDLSectionImpl *s = (const AzmaIDLSectionImpl *)section;
    return s ? s->decl.nested_count : 0;
}

const AzmaIDLDecl *azma_idl_section_decl_at(const AzmaIDLSection *section, size_t index) {
    const AzmaIDLSectionImpl *s = (const AzmaIDLSectionImpl *)section;
    if (!s || index >= s->decl.nested_count) {
        return NULL;
    }
    return (const AzmaIDLDecl *)s->decl.nested[index];
}

/* ========================================================================= */
/* Value queries                                                             */
/* ========================================================================= */

const AzmaIDLNode *azma_idl_value_as_node(const AzmaIDLValue *value) {
    return (const AzmaIDLNode *)value;
}

AzmaIDLValueKind azma_idl_value_kind(const AzmaIDLValue *value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    return v ? v->value_kind : AZMA_IDL_VALUE_INVALID;
}

AzmaSourceRange azma_idl_value_range(const AzmaIDLValue *value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    return v ? v->base.range : azma_idl__make_empty_range(0);
}

int azma_idl_value_as_bool(const AzmaIDLValue *value, int *out_value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_BOOL || !out_value) {
        return 0;
    }
    *out_value = v->as.bool_value;
    return 1;
}

int azma_idl_value_as_int(const AzmaIDLValue *value, int64_t *out_value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_INT || !out_value) {
        return 0;
    }
    *out_value = v->as.int_value;
    return 1;
}

int azma_idl_value_as_string(
    const AzmaIDLValue *value,
    AzmaIDLStringView *out_value,
    AzmaIDLStringKind *out_kind
) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_STRING || !out_value || !out_kind) {
        return 0;
    }
    *out_value = v->as.string_value.text;
    *out_kind = v->as.string_value.kind;
    return 1;
}

int azma_idl_value_as_range(const AzmaIDLValue *value, AzmaIDLRangeValue *out_value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_RANGE || !out_value) {
        return 0;
    }
    *out_value = v->as.range_value;
    return 1;
}

int azma_idl_value_as_name(const AzmaIDLValue *value, AzmaIDLNamedValue *out_value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_NAME || !out_value) {
        return 0;
    }
    *out_value = v->as.name_value;
    return 1;
}

int azma_idl_value_as_call(const AzmaIDLValue *value, AzmaIDLCallInfo *out_value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_CALL || !out_value) {
        return 0;
    }
    out_value->callee = v->as.call_value.callee;
    out_value->argument_count = v->as.call_value.arg_count;
    return 1;
}

size_t azma_idl_value_item_count(const AzmaIDLValue *value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_LIST) {
        return 0;
    }
    return v->as.list_value.count;
}

const AzmaIDLValue *azma_idl_value_item_at(const AzmaIDLValue *value, size_t index) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_LIST || index >= v->as.list_value.count) {
        return NULL;
    }
    return (const AzmaIDLValue *)v->as.list_value.items[index];
}

size_t azma_idl_value_field_count(const AzmaIDLValue *value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_RECORD) {
        return 0;
    }
    return v->as.record_value.count;
}

int azma_idl_value_field_at(
    const AzmaIDLValue *value,
    size_t index,
    AzmaIDLFieldInfo *out_field
) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_RECORD || !out_field ||
        index >= v->as.record_value.count) {
        return 0;
    }

    out_field->name = v->as.record_value.fields[index].name;
    out_field->value = (const AzmaIDLValue *)v->as.record_value.fields[index].value;
    return 1;
}

int azma_idl_value_field_by_name(
    const AzmaIDLValue *value,
    AzmaIDLStringView name,
    AzmaIDLFieldInfo *out_field
) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    size_t i;

    if (!v || v->value_kind != AZMA_IDL_VALUE_RECORD || !out_field) {
        return 0;
    }

    for (i = 0; i < v->as.record_value.count; ++i) {
        if (azma_idl_sv_eq(v->as.record_value.fields[i].name, name)) {
            out_field->name = v->as.record_value.fields[i].name;
            out_field->value = (const AzmaIDLValue *)v->as.record_value.fields[i].value;
            return 1;
        }
    }

    return 0;
}

size_t azma_idl_value_argument_count(const AzmaIDLValue *value) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_CALL) {
        return 0;
    }
    return v->as.call_value.arg_count;
}

int azma_idl_value_argument_at(
    const AzmaIDLValue *value,
    size_t index,
    AzmaIDLArgumentInfo *out_argument
) {
    const AzmaIDLValueImpl *v = (const AzmaIDLValueImpl *)value;
    if (!v || v->value_kind != AZMA_IDL_VALUE_CALL || !out_argument ||
        index >= v->as.call_value.arg_count) {
        return 0;
    }

    out_argument->name = v->as.call_value.args[index].name;
    out_argument->value = (const AzmaIDLValue *)v->as.call_value.args[index].value;
    out_argument->has_name = v->as.call_value.args[index].has_name;
    return 1;
}

/* ========================================================================= */
/* Text helpers                                                              */
/* ========================================================================= */

AzmaIDLStringView azma_idl_sv_from_cstr(const char *text) {
    AzmaIDLStringView sv;
    sv.data = text;
    sv.size = text ? strlen(text) : 0;
    return sv;
}

int azma_idl_sv_eq(AzmaIDLStringView a, AzmaIDLStringView b) {
    return a.size == b.size && (a.size == 0 || memcmp(a.data, b.data, a.size) == 0);
}

int azma_idl_sv_eq_cstr(AzmaIDLStringView a, const char *b) {
    size_t n = b ? strlen(b) : 0;
    return a.size == n && (n == 0 || memcmp(a.data, b, n) == 0);
}

char *azma_idl_sv_to_cstr(AzmaAllocator *allocator, AzmaIDLStringView value) {
    AzmaAllocator a = allocator ? *allocator : azma_allocator_default();
    char *out = (char *)azma_alloc(&a, value.size + 1);
    if (!out) return NULL;
    if (value.size) {
        memcpy(out, value.data, value.size);
    }
    out[value.size] = '\0';
    return out;
}

/* ========================================================================= */
/* Names                                                                     */
/* ========================================================================= */

const char *azma_idl_node_kind_name(AzmaIDLNodeKind kind) {
    switch (kind) {
        case AZMA_IDL_NODE_INVALID: return "invalid";
        case AZMA_IDL_NODE_DOCUMENT: return "document";
        case AZMA_IDL_NODE_METADATA_DECL: return "metadata_decl";
        case AZMA_IDL_NODE_IMPORT_DECL: return "import_decl";
        case AZMA_IDL_NODE_CONFIG_DECL: return "config_decl";
        case AZMA_IDL_NODE_API_DECL: return "api_decl";
        case AZMA_IDL_NODE_SYMBOL_DECL: return "symbol_decl";
        case AZMA_IDL_NODE_SECTION_DECL: return "section_decl";
        case AZMA_IDL_NODE_EMIT_DECL: return "emit_decl";
        case AZMA_IDL_NODE_REQUIRES_DECL: return "requires_decl";
        case AZMA_IDL_NODE_TAG_DECL: return "tag_decl";
        case AZMA_IDL_NODE_NULL_VALUE: return "null_value";
        case AZMA_IDL_NODE_BOOL_VALUE: return "bool_value";
        case AZMA_IDL_NODE_INT_VALUE: return "int_value";
        case AZMA_IDL_NODE_STRING_VALUE: return "string_value";
        case AZMA_IDL_NODE_RANGE_VALUE: return "range_value";
        case AZMA_IDL_NODE_LIST_VALUE: return "list_value";
        case AZMA_IDL_NODE_RECORD_VALUE: return "record_value";
        case AZMA_IDL_NODE_NAME_VALUE: return "name_value";
        case AZMA_IDL_NODE_CALL_VALUE: return "call_value";
        case AZMA_IDL_NODE_FIELD: return "field";
        case AZMA_IDL_NODE_ARGUMENT: return "argument";
        case AZMA_IDL_NODE_SELECTOR: return "selector";
        case AZMA_IDL_NODE_PATH: return "path";
        default: return "unknown";
    }
}

const char *azma_idl_decl_kind_name(AzmaIDLDeclKind kind) {
    switch (kind) {
        case AZMA_IDL_DECL_INVALID: return "invalid";
        case AZMA_IDL_DECL_METADATA: return "metadata";
        case AZMA_IDL_DECL_IMPORT: return "import";
        case AZMA_IDL_DECL_CONFIG: return "config";
        case AZMA_IDL_DECL_API: return "api";
        case AZMA_IDL_DECL_SYMBOL: return "symbol";
        case AZMA_IDL_DECL_SECTION: return "section";
        case AZMA_IDL_DECL_EMIT: return "emit";
        case AZMA_IDL_DECL_REQUIRES: return "requires";
        case AZMA_IDL_DECL_TAG: return "tag";
        default: return "unknown";
    }
}

const char *azma_idl_value_kind_name(AzmaIDLValueKind kind) {
    switch (kind) {
        case AZMA_IDL_VALUE_INVALID: return "invalid";
        case AZMA_IDL_VALUE_NULL: return "null";
        case AZMA_IDL_VALUE_BOOL: return "bool";
        case AZMA_IDL_VALUE_INT: return "int";
        case AZMA_IDL_VALUE_STRING: return "string";
        case AZMA_IDL_VALUE_RANGE: return "range";
        case AZMA_IDL_VALUE_LIST: return "list";
        case AZMA_IDL_VALUE_RECORD: return "record";
        case AZMA_IDL_VALUE_NAME: return "name";
        case AZMA_IDL_VALUE_CALL: return "call";
        default: return "unknown";
    }
}

const char *azma_idl_section_kind_name(AzmaIDLSectionKind kind) {
    switch (kind) {
        case AZMA_IDL_SECTION_INVALID: return "invalid";
        case AZMA_IDL_SECTION_UNIT: return "unit";
        case AZMA_IDL_SECTION_FUZZ: return "fuzz";
        case AZMA_IDL_SECTION_GENERIC: return "generic";
        default: return "unknown";
    }
}

const char *azma_idl_diag_severity_name(AzmaIDLDiagnosticSeverity severity) {
    switch (severity) {
        case AZMA_IDL_DIAG_NOTE: return "note";
        case AZMA_IDL_DIAG_WARNING: return "warning";
        case AZMA_IDL_DIAG_ERROR: return "error";
        default: return "unknown";
    }
}

/* ========================================================================= */
/* Debug dump                                                                */
/* ========================================================================= */

static void azma_idl__print_sv(FILE *out, AzmaIDLStringView sv) {
    if (!out) return;
    if (!sv.data) {
        fputs("(null)", out);
        return;
    }
    fprintf(out, "%.*s", (int)sv.size, sv.data);
}

static void azma_idl__dump_indent(FILE *out, int indent) {
    int i;
    for (i = 0; i < indent; ++i) {
        fputc(' ', out);
    }
}

static void azma_idl__dump_value(FILE *out, const AzmaIDLValueImpl *v, int indent);

static void azma_idl__dump_decl(FILE *out, const AzmaIDLDeclImpl *d, int indent) {
    size_t i;
    azma_idl__dump_indent(out, indent);
    fprintf(out, "%s", azma_idl_decl_kind_name(d->decl_kind));
    if (d->name.data) {
        fputc(' ', out);
        azma_idl__print_sv(out, d->name);
    }
    fputc('\n', out);

    if (d->value) {
        azma_idl__dump_value(out, d->value, indent + 2);
    }

    for (i = 0; i < d->nested_count; ++i) {
        azma_idl__dump_decl(out, d->nested[i], indent + 2);
    }
}

static void azma_idl__dump_value(FILE *out, const AzmaIDLValueImpl *v, int indent) {
    size_t i;

    azma_idl__dump_indent(out, indent);
    fprintf(out, "value %s", azma_idl_value_kind_name(v->value_kind));

    switch (v->value_kind) {
        case AZMA_IDL_VALUE_NULL:
            fprintf(out, " null");
            break;
        case AZMA_IDL_VALUE_BOOL:
            fprintf(out, " %s", v->as.bool_value ? "true" : "false");
            break;
        case AZMA_IDL_VALUE_INT:
            fprintf(out, " %" PRId64, v->as.int_value);
            break;
        case AZMA_IDL_VALUE_STRING:
            fprintf(out, " \"");
            azma_idl__print_sv(out, v->as.string_value.text);
            fprintf(out, "\"");
            break;
        case AZMA_IDL_VALUE_NAME:
            fputc(' ', out);
            azma_idl__print_sv(out, v->as.name_value.name);
            break;
        default:
            break;
    }

    fputc('\n', out);

    if (v->value_kind == AZMA_IDL_VALUE_LIST) {
        for (i = 0; i < v->as.list_value.count; ++i) {
            azma_idl__dump_value(out, v->as.list_value.items[i], indent + 2);
        }
    } else if (v->value_kind == AZMA_IDL_VALUE_RECORD) {
        for (i = 0; i < v->as.record_value.count; ++i) {
            azma_idl__dump_indent(out, indent + 2);
            fprintf(out, "field ");
            azma_idl__print_sv(out, v->as.record_value.fields[i].name);
            fputc('\n', out);
            azma_idl__dump_value(out, v->as.record_value.fields[i].value, indent + 4);
        }
    }
}

void azma_idl_dump_document(FILE *out, const AzmaIDLDocument *document) {
    size_t i;
    if (!out || !document) return;

    fprintf(out, "AzmaIDLDocument\n");
    fprintf(out, "  path: %s\n", document->source.path ? document->source.path : "(null)");
    fprintf(out, "  size: %zu\n", document->source.size);
    fprintf(out, "  decls: %zu\n", document->decl_count);
    fprintf(out, "  diagnostics: %zu\n", document->diagnostics.count);

    for (i = 0; i < document->diagnostics.count; ++i) {
        const AzmaIDLDiagnostic *d = &document->diagnostics.items[i];
        fprintf(out, "  diag[%zu]: %s ", i, azma_idl_diag_severity_name(d->severity));
        if (d->code.data) {
            fprintf(out, "[%.*s] ", (int)d->code.size, d->code.data);
        }
        if (d->message.data) {
            fprintf(out, "%.*s", (int)d->message.size, d->message.data);
        }
        fputc('\n', out);
    }

    for (i = 0; i < document->decl_count; ++i) {
        azma_idl__dump_decl(out, document->decls[i], 2);
    }
}

void azma_idl_dump_node(FILE *out, const AzmaIDLNode *node, int indent) {
    const AzmaIDLNodeImpl *n = (const AzmaIDLNodeImpl *)node;
    size_t i;

    if (!out || !n) return;

    azma_idl__dump_indent(out, indent);
    fprintf(out, "%s\n", azma_idl_node_kind_name(n->kind));

    switch (n->kind) {
        case AZMA_IDL_NODE_METADATA_DECL:
        case AZMA_IDL_NODE_IMPORT_DECL:
        case AZMA_IDL_NODE_CONFIG_DECL:
        case AZMA_IDL_NODE_API_DECL:
        case AZMA_IDL_NODE_SYMBOL_DECL:
        case AZMA_IDL_NODE_SECTION_DECL:
        case AZMA_IDL_NODE_EMIT_DECL:
        case AZMA_IDL_NODE_REQUIRES_DECL:
        case AZMA_IDL_NODE_TAG_DECL:
            azma_idl__dump_decl(out, (const AzmaIDLDeclImpl *)n, indent);
            return;

        case AZMA_IDL_NODE_NULL_VALUE:
        case AZMA_IDL_NODE_BOOL_VALUE:
        case AZMA_IDL_NODE_INT_VALUE:
        case AZMA_IDL_NODE_STRING_VALUE:
        case AZMA_IDL_NODE_RANGE_VALUE:
        case AZMA_IDL_NODE_LIST_VALUE:
        case AZMA_IDL_NODE_RECORD_VALUE:
        case AZMA_IDL_NODE_NAME_VALUE:
        case AZMA_IDL_NODE_CALL_VALUE:
            azma_idl__dump_value(out, (const AzmaIDLValueImpl *)n, indent);
            return;

        default:
            break;
    }

    for (i = 0; i < n->children.count; ++i) {
        azma_idl_dump_node(out, (const AzmaIDLNode *)n->children.items[i], indent + 2);
    }
}
