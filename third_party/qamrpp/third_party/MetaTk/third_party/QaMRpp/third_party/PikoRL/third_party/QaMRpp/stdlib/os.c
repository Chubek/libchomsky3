#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static qamrpp_value* os_clock(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_float(ctx, (double)clock() / CLOCKS_PER_SEC); }
static qamrpp_value* os_time(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_int(ctx, (int64_t)time(NULL)); }
static qamrpp_value* os_difftime(qamrpp_context* ctx, qamrpp_value** a, size_t n) { double t1 = n > 0 ? g_api->value_as_float(a[0]) : 0.0; double t2 = n > 1 ? g_api->value_as_float(a[1]) : 0.0; return g_api->value_float(ctx, difftime((time_t)t1, (time_t)t2)); }
static qamrpp_value* os_date(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; time_t now = time(NULL); struct tm* tmv = localtime(&now); char buf[64]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tmv); return g_api->value_string(ctx, buf, strlen(buf)); }
static qamrpp_value* os_execute(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_int(ctx, 0); }
static qamrpp_value* os_exitv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_nil(ctx); }
static qamrpp_value* os_getenv(qamrpp_context* ctx, qamrpp_value** a, size_t n) { if (!n) return g_api->value_nil(ctx); size_t len=0; const char* k=g_api->value_as_string(a[0], &len); if(!k) return g_api->value_nil(ctx); const char* v=getenv(k); if(!v) return g_api->value_nil(ctx); return g_api->value_string(ctx,v,strlen(v)); }
static qamrpp_value* os_removev(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_bool(ctx, 0); }
static qamrpp_value* os_renamev(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_bool(ctx, 0); }
static qamrpp_value* os_setlocale(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_string(ctx, "C", 1); }
static qamrpp_value* os_tmpname(qamrpp_context* ctx, qamrpp_value** a, size_t n) { (void)a; (void)n; return g_api->value_string(ctx, "/tmp/qamrpp.tmp", 15); }

static qamrpp_native_binding kBindings[] = {
    {"os_clock", os_clock}, {"os_date", os_date}, {"os_difftime", os_difftime}, {"os_execute", os_execute},
    {"os_exit", os_exitv}, {"os_getenv", os_getenv}, {"os_remove", os_removev}, {"os_rename", os_renamev},
    {"os_setlocale", os_setlocale}, {"os_time", os_time}, {"os_tmpname", os_tmpname}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { (void)ctx; g_api = host_api; return 0; }

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "os", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
