#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;
static qamrpp_value* nilv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }
static qamrpp_value* traceback(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_string(ctx, "", 0); }

static qamrpp_native_binding kBindings[] = {
    {"debug_debug", nilv}, {"debug_gethook", nilv}, {"debug_getinfo", nilv}, {"debug_getlocal", nilv},
    {"debug_getmetatable", nilv}, {"debug_getregistry", nilv}, {"debug_getupvalue", nilv}, {"debug_getuservalue", nilv},
    {"debug_sethook", nilv}, {"debug_setlocal", nilv}, {"debug_setmetatable", nilv}, {"debug_setupvalue", nilv},
    {"debug_setuservalue", nilv}, {"debug_traceback", traceback}, {"debug_upvalueid", nilv}, {"debug_upvaluejoin", nilv}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { (void)ctx; g_api = host_api; return 0; }

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "debug", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
