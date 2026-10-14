#include <stdlib.h>
#include <string.h>
#include "../qlib/C/QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static qamrpp_value* key(qamrpp_context* ctx, int64_t index) { return g_api->value_int(ctx, index); }
static int64_t length(qamrpp_context* ctx, qamrpp_value* table) {
    int64_t n = 0;
    while (g_api->table_raw_get(ctx, table, key(ctx, n + 1)) &&
           g_api->value_get_type(g_api->table_raw_get(ctx, table, key(ctx, n + 1))) != QAMRPP_TYPE_NIL) ++n;
    return n;
}
static qamrpp_value* table_concat(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (!argc || g_api->value_get_type(argv[0]) != QAMRPP_TYPE_TABLE) return g_api->value_string(ctx, "", 0);
    const char* separator = argc > 1 ? g_api->value_as_string(argv[1], 0) : 0;
    size_t sep_len = separator ? strlen(separator) : 0;
    int64_t first = argc > 2 ? g_api->value_as_int(argv[2]) : 1;
    int64_t last = argc > 3 ? g_api->value_as_int(argv[3]) : length(ctx, argv[0]);
    size_t total = 0;
    for (int64_t i = first; i <= last; ++i) { qamrpp_value* v = g_api->table_raw_get(ctx, argv[0], key(ctx, i)); size_t n = 0; const char* s = g_api->value_as_string(v, &n); if (s) total += n; if (i < last) total += sep_len; }
    char* out = (char*)malloc(total + 1); size_t at = 0;
    for (int64_t i = first; i <= last; ++i) { qamrpp_value* v = g_api->table_raw_get(ctx, argv[0], key(ctx, i)); size_t n = 0; const char* s = g_api->value_as_string(v, &n); if (s) { memcpy(out + at, s, n); at += n; } if (i < last && separator) { memcpy(out + at, separator, sep_len); at += sep_len; } }
    out[at] = 0; qamrpp_value* result = g_api->value_string(ctx, out, at); free(out); return result;
}
static qamrpp_value* table_insert(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc < 2) return g_api->value_nil(ctx);
    int64_t n = length(ctx, argv[0]); int64_t position = argc > 2 ? g_api->value_as_int(argv[1]) : n + 1;
    if (argc > 2) { for (int64_t i = n; i >= position; --i) g_api->table_raw_set(ctx, argv[0], key(ctx, i + 1), g_api->table_raw_get(ctx, argv[0], key(ctx, i))); }
    g_api->table_raw_set(ctx, argv[0], key(ctx, position), argv[argc > 2 ? 2 : 1]); return g_api->value_nil(ctx);
}
static qamrpp_value* table_move(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc < 4) return g_api->value_nil(ctx);
    int64_t first = g_api->value_as_int(argv[1]), last = g_api->value_as_int(argv[2]), target = g_api->value_as_int(argv[3]);
    for (int64_t i = first; i <= last; ++i) g_api->table_raw_set(ctx, argv[4 < argc ? 4 : 0], key(ctx, target + i - first), g_api->table_raw_get(ctx, argv[0], key(ctx, i)));
    return argv[4 < argc ? 4 : 0];
}
static qamrpp_value* table_pack(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { qamrpp_value* table = g_api->value_table(ctx); for (size_t i = 0; i < argc; ++i) g_api->table_raw_set(ctx, table, key(ctx, (int64_t)i + 1), argv[i]); g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "n", 1), g_api->value_int(ctx, (int64_t)argc)); return table; }
static qamrpp_value* table_remove(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { if (!argc) return g_api->value_nil(ctx); int64_t n = length(ctx, argv[0]), position = argc > 1 ? g_api->value_as_int(argv[1]) : n; qamrpp_value* result = g_api->table_raw_get(ctx, argv[0], key(ctx, position)); for (int64_t i = position; i < n; ++i) g_api->table_raw_set(ctx, argv[0], key(ctx, i), g_api->table_raw_get(ctx, argv[0], key(ctx, i + 1))); g_api->table_raw_set(ctx, argv[0], key(ctx, n), g_api->value_nil(ctx)); return result; }
static qamrpp_value* table_sort(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)ctx; (void)argv; (void)argc; return g_api->value_nil(ctx); }
static qamrpp_value* table_unpack(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { if (!argc) return g_api->value_nil(ctx); return g_api->table_raw_get(ctx, argv[0], key(ctx, argc > 1 ? g_api->value_as_int(argv[1]) : 1)); }

static qamrpp_native_binding kBindings[] = {
    {"table_concat", table_concat}, {"table_insert", table_insert}, {"table_move", table_move},
    {"table_pack", table_pack}, {"table_remove", table_remove}, {"table_sort", table_sort}, {"table_unpack", table_unpack}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    g_api = host_api; qamrpp_value* table = g_api->value_table(ctx);
    for (size_t i = 0; i < sizeof(kBindings) / sizeof(kBindings[0]); ++i) { const char* name = strchr(kBindings[i].name, '_') + 1; g_api->table_raw_set(ctx, table, g_api->value_string(ctx, name, strlen(name)), g_api->get_global(ctx, kBindings[i].name)); }
    g_api->set_global(ctx, "table", table); return 0;
}

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "table", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
