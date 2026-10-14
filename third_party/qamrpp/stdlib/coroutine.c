#include <string.h>
#include "../qlib/C/QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;
static qamrpp_value* nilv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }
static qamrpp_value* falsev(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_bool(ctx, 0); }
static qamrpp_value* status(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_string(ctx, "dead", 4); }

static qamrpp_native_binding kBindings[] = {
    {"coroutine_create", nilv}, {"coroutine_isyieldable", falsev}, {"coroutine_resume", falsev},
    {"coroutine_running", nilv}, {"coroutine_status", status}, {"coroutine_wrap", nilv}, {"coroutine_yield", nilv}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { g_api = host_api; qamrpp_value* table = g_api->value_table(ctx); for (size_t i = 0; i < sizeof(kBindings)/sizeof(kBindings[0]); ++i) { const char* name = strchr(kBindings[i].name, '_') + 1; g_api->table_raw_set(ctx, table, g_api->value_string(ctx, name, strlen(name)), g_api->get_global(ctx, kBindings[i].name)); } g_api->set_global(ctx, "coroutine", table); return 0; }

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "coroutine", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
