#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static double argf(qamrpp_value** argv, size_t argc, size_t i) {
    if (i >= argc) return 0.0;
    return g_api->value_as_float(argv[i]);
}

static qamrpp_value* math_abs(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, fabs(argf(argv, argc, 0))); }
static qamrpp_value* math_acos(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, acos(argf(argv, argc, 0))); }
static qamrpp_value* math_asin(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, asin(argf(argv, argc, 0))); }
static qamrpp_value* math_atan(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, atan(argf(argv, argc, 0))); }
static qamrpp_value* math_ceil(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, ceil(argf(argv, argc, 0))); }
static qamrpp_value* math_cos(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, cos(argf(argv, argc, 0))); }
static qamrpp_value* math_deg(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, argf(argv, argc, 0) * 57.29577951308232); }
static qamrpp_value* math_exp(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, exp(argf(argv, argc, 0))); }
static qamrpp_value* math_floor(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, floor(argf(argv, argc, 0))); }
static qamrpp_value* math_fmod(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, fmod(argf(argv, argc, 0), argf(argv, argc, 1))); }
static qamrpp_value* math_log(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, log(argf(argv, argc, 0))); }
static qamrpp_value* math_max(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { double m = argc ? argf(argv, argc, 0) : 0.0; for (size_t i=1;i<argc;++i) if (argf(argv,argc,i)>m) m=argf(argv,argc,i); return g_api->value_float(ctx,m); }
static qamrpp_value* math_min(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { double m = argc ? argf(argv, argc, 0) : 0.0; for (size_t i=1;i<argc;++i) if (argf(argv,argc,i)<m) m=argf(argv,argc,i); return g_api->value_float(ctx,m); }
static qamrpp_value* math_modf(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { double ip = 0.0; (void)modf(argf(argv, argc, 0), &ip); return g_api->value_float(ctx, ip); }
static qamrpp_value* math_rad(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, argf(argv, argc, 0) * 0.017453292519943295); }
static qamrpp_value* math_random(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc == 0) return g_api->value_float(ctx, (double)rand() / (double)RAND_MAX);
    if (argc == 1) return g_api->value_int(ctx, (int64_t)(rand() % ((int)g_api->value_as_int(argv[0]) + 1)));
    int a = (int)g_api->value_as_int(argv[0]);
    int b = (int)g_api->value_as_int(argv[1]);
    if (b < a) { int t = a; a = b; b = t; }
    return g_api->value_int(ctx, a + (rand() % (b - a + 1)));
}
static qamrpp_value* math_randomseed(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { srand((unsigned int)argf(argv, argc, 0)); return g_api->value_nil(ctx); }
static qamrpp_value* math_sin(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, sin(argf(argv, argc, 0))); }
static qamrpp_value* math_sqrt(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, sqrt(argf(argv, argc, 0))); }
static qamrpp_value* math_tan(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_float(ctx, tan(argf(argv, argc, 0))); }
static qamrpp_value* math_tointeger(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_int(ctx, (int64_t)argf(argv, argc, 0)); }
static qamrpp_value* math_type(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { return g_api->value_string(ctx, (argc && g_api->value_get_type(argv[0]) == QAMRPP_TYPE_INT) ? "integer" : "float", (argc && g_api->value_get_type(argv[0]) == QAMRPP_TYPE_INT) ? 7 : 5); }
static qamrpp_value* math_ult(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { uint64_t a = (uint64_t)argf(argv, argc, 0); uint64_t b = (uint64_t)argf(argv, argc, 1); return g_api->value_bool(ctx, a < b); }

static qamrpp_native_binding kBindings[] = {
    {"math_abs", math_abs}, {"math_acos", math_acos}, {"math_asin", math_asin}, {"math_atan", math_atan},
    {"math_ceil", math_ceil}, {"math_cos", math_cos}, {"math_deg", math_deg}, {"math_exp", math_exp},
    {"math_floor", math_floor}, {"math_fmod", math_fmod}, {"math_log", math_log}, {"math_max", math_max},
    {"math_min", math_min}, {"math_modf", math_modf}, {"math_rad", math_rad}, {"math_random", math_random},
    {"math_randomseed", math_randomseed}, {"math_sin", math_sin}, {"math_sqrt", math_sqrt}, {"math_tan", math_tan},
    {"math_tointeger", math_tointeger}, {"math_type", math_type}, {"math_ult", math_ult}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    g_api = host_api;
    g_api->set_global(ctx, "math_pi", g_api->value_float(ctx, 3.141592653589793));
    g_api->set_global(ctx, "math_huge", g_api->value_float(ctx, HUGE_VAL));
    g_api->set_global(ctx, "math_maxinteger", g_api->value_int(ctx, INT64_MAX));
    g_api->set_global(ctx, "math_mininteger", g_api->value_int(ctx, INT64_MIN));
    srand((unsigned int)time(NULL));
    return 0;
}

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION, "math", kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0
};
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
