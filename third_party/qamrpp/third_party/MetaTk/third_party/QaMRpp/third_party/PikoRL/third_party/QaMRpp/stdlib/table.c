#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static qamrpp_value* table_concat(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_string(ctx, "", 0); }
static qamrpp_value* table_insert(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }
static qamrpp_value* table_move(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }
static qamrpp_value* table_pack(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; return g_api->value_int(ctx, (int64_t)argc); }
static qamrpp_value* table_remove(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }
static qamrpp_value* table_sort(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }
static qamrpp_value* table_unpack(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }

static qamrpp_native_binding kBindings[] = {
    {"table_concat", table_concat}, {"table_insert", table_insert}, {"table_move", table_move},
    {"table_pack", table_pack}, {"table_remove", table_remove}, {"table_sort", table_sort}, {"table_unpack", table_unpack}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { (void)ctx; g_api = host_api; return 0; }

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "table", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
