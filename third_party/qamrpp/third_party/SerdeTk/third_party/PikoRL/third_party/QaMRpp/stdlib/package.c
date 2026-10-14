#include <stdlib.h>
#include <string.h>

#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;
static qamrpp_value* nilv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }
static qamrpp_value* package_searchpath(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }

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
    g_api->set_global(ctx, "package_searchers", g_api->value_nil(ctx));
    return 0;
}

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "package", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
