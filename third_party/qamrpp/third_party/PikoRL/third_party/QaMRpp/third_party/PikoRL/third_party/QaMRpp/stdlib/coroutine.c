#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;
static qamrpp_value* nilv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }
static qamrpp_value* falsev(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_bool(ctx, 0); }
static qamrpp_value* status(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_string(ctx, "dead", 4); }

static qamrpp_native_binding kBindings[] = {
    {"coroutine_create", nilv}, {"coroutine_isyieldable", falsev}, {"coroutine_resume", falsev},
    {"coroutine_running", nilv}, {"coroutine_status", status}, {"coroutine_wrap", nilv}, {"coroutine_yield", nilv}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { (void)ctx; g_api = host_api; return 0; }

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "coroutine", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
