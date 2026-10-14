#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;
static qamrpp_value* nilv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }

static qamrpp_native_binding kBindings[] = {
    {"io_close", nilv}, {"io_flush", nilv}, {"io_input", nilv}, {"io_lines", nilv}, {"io_open", nilv},
    {"io_output", nilv}, {"io_popen", nilv}, {"io_read", nilv}, {"io_tmpfile", nilv}, {"io_type", nilv}, {"io_write", nilv}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { (void)ctx; g_api = host_api; return 0; }

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "io", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
