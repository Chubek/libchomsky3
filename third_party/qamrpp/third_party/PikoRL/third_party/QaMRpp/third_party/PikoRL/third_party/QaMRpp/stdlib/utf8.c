#include <stdint.h>
#include <stdlib.h>

#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static qamrpp_value* utf8_len(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t len = 0; if (argc) (void)g_api->value_as_string(argv[0], &len); return g_api->value_int(ctx, (int64_t)len); }
static qamrpp_value* utf8_char(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { char b[8]; if (!argc) return g_api->value_string(ctx, "", 0); b[0]=(char)g_api->value_as_int(argv[0]); return g_api->value_string(ctx,b,1); }
static qamrpp_value* utf8_codepoint(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t len = 0; const char* s = argc ? g_api->value_as_string(argv[0], &len) : 0; if (!s || !len) return g_api->value_nil(ctx); return g_api->value_int(ctx, (unsigned char)s[0]); }
static qamrpp_value* utf8_offset(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_int(ctx, 1); }
static qamrpp_value* utf8_codes(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }

static qamrpp_native_binding kBindings[] = {
    {"utf8_char", utf8_char}, {"utf8_codepoint", utf8_codepoint}, {"utf8_codes", utf8_codes}, {"utf8_len", utf8_len}, {"utf8_offset", utf8_offset}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    g_api = host_api;
    g_api->set_global(ctx, "utf8_charpattern", g_api->value_string(ctx, "[", 1));
    return 0;
}

static const qamrpp_library_descriptor kDescriptor = { QAMRPP_LIBRARY_API_VERSION, "utf8", kBindings, sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0 };
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
