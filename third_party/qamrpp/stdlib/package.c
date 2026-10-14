#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../qlib/C/QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;
static qamrpp_value* nilv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }
static qamrpp_value* package_searchpath(qamrpp_context* ctx, qamrpp_value** a, size_t n) {
    if (n < 2) return g_api->value_nil(ctx);
    size_t name_len = 0, path_len = 0; const char* name = g_api->value_as_string(a[0], &name_len); const char* path = g_api->value_as_string(a[1], &path_len); if (!name || !path) return g_api->value_nil(ctx);
    char* module = (char*)malloc(name_len + 1); memcpy(module, name, name_len + 1); for (size_t i = 0; i < name_len; ++i) if (module[i] == '.') module[i] = '/';
    size_t start = 0; while (start <= path_len) { size_t end = start; while (end < path_len && path[end] != ';') ++end; size_t cap = (end - start) + name_len + 2; char* candidate = (char*)malloc(cap); size_t out = 0; for (size_t i = start; i < end; ++i) { if (path[i] == '?') { memcpy(candidate + out, module, name_len); out += name_len; } else candidate[out++] = path[i]; } candidate[out] = 0; FILE* file = fopen(candidate, "rb"); if (file) { fclose(file); qamrpp_value* result = g_api->value_string(ctx, candidate, out); free(candidate); free(module); return result; } free(candidate); if (end == path_len) break; start = end + 1; }
    free(module); return g_api->value_nil(ctx);
}

static qamrpp_native_binding kBindings[] = {
    {"package_searchpath", package_searchpath}, {"package_loadlib", nilv}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    g_api = host_api;
    const char* qpath = getenv("QAMRPP_PATH");
    g_api->set_global(ctx, "package_config", g_api->value_string(ctx, "/\n;\n?\n!\n-", 9));
    g_api->set_global(ctx, "package_cpath", g_api->value_string(ctx, qpath ? qpath : "", qpath ? strlen(qpath) : 0));
    g_api->set_global(ctx, "package_path", g_api->value_string(ctx, qpath ? qpath : "", qpath ? strlen(qpath) : 0));
    g_api->set_global(ctx, "package_loaded", g_api->value_nil(ctx));
    g_api->set_global(ctx, "package_preload", g_api->value_nil(ctx));
    qamrpp_value* table = g_api->value_table(ctx);
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "config", 6), g_api->get_global(ctx, "package_config"));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "cpath", 5), g_api->get_global(ctx, "package_cpath"));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "path", 4), g_api->get_global(ctx, "package_path"));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "searchpath", 10), g_api->get_global(ctx, "package_searchpath"));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "loadlib", 7), g_api->get_global(ctx, "package_loadlib"));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "loaded", 6), g_api->value_table(ctx));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "preload", 7), g_api->value_table(ctx));
    g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "searchers", 9), g_api->value_table(ctx));
    g_api->set_global(ctx, "package", table);
    return 0;
}

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "package", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
