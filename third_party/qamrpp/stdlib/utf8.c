#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../qlib/C/QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static size_t utf8_width(unsigned char c) { return c < 0x80 ? 1 : (c < 0xe0 ? 2 : (c < 0xf0 ? 3 : 4)); }
static int64_t utf8_decode(const unsigned char* s, size_t n, size_t* width) { if (!n || *s < 0x80) { *width = n ? 1 : 0; return n ? *s : -1; } size_t w = utf8_width(*s); if (w > n) return -1; int64_t cp = *s & (0x7f >> w); for (size_t i = 1; i < w; ++i) { if ((s[i] & 0xc0) != 0x80) return -1; cp = (cp << 6) | (s[i] & 0x3f); } *width = w; return cp; }
static qamrpp_value* utf8_len(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t len = 0, count = 0, width = 0; const unsigned char* s = (const unsigned char*)(argc ? g_api->value_as_string(argv[0], &len) : 0); for (size_t i = 0; s && i < len; ++count) { if (utf8_decode(s + i, len - i, &width) < 0) return g_api->value_nil(ctx); i += width; } return g_api->value_int(ctx, (int64_t)count); }
static qamrpp_value* utf8_char(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { char b[4]; size_t n = 0; for (size_t i = 0; i < argc; ++i) { uint32_t cp = (uint32_t)g_api->value_as_int(argv[i]); if (cp <= 0x7f) b[n++] = (char)cp; else if (cp <= 0x7ff) { b[n++] = (char)(0xc0 | (cp >> 6)); b[n++] = (char)(0x80 | (cp & 0x3f)); } else if (cp <= 0xffff) { b[n++] = (char)(0xe0 | (cp >> 12)); b[n++] = (char)(0x80 | ((cp >> 6) & 0x3f)); b[n++] = (char)(0x80 | (cp & 0x3f)); } else { b[n++] = (char)(0xf0 | (cp >> 18)); b[n++] = (char)(0x80 | ((cp >> 12) & 0x3f)); b[n++] = (char)(0x80 | ((cp >> 6) & 0x3f)); b[n++] = (char)(0x80 | (cp & 0x3f)); } } return g_api->value_string(ctx, b, n); }
static qamrpp_value* utf8_codepoint(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t len = 0, width = 0; const unsigned char* s = (const unsigned char*)(argc ? g_api->value_as_string(argv[0], &len) : 0); int64_t cp = s ? utf8_decode(s, len, &width) : -1; return cp < 0 ? g_api->value_nil(ctx) : g_api->value_int(ctx, cp); }
static qamrpp_value* utf8_offset(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t len = 0, width = 0; const unsigned char* s = argc ? (const unsigned char*)g_api->value_as_string(argv[0], &len) : 0; int64_t wanted = argc > 1 ? g_api->value_as_int(argv[1]) : 1, index = 1; if (!s || wanted < 1) return g_api->value_nil(ctx); for (size_t i = 0; i < len; ) { if (index == wanted) return g_api->value_int(ctx, (int64_t)i + 1); if (utf8_decode(s + i, len - i, &width) < 0) return g_api->value_nil(ctx); i += width; ++index; } return g_api->value_nil(ctx); }
static qamrpp_value* utf8_codes(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }

static qamrpp_native_binding kBindings[] = {
    {"utf8_char", utf8_char}, {"utf8_codepoint", utf8_codepoint}, {"utf8_codes", utf8_codes}, {"utf8_len", utf8_len}, {"utf8_offset", utf8_offset}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    g_api = host_api;
    g_api->set_global(ctx, "utf8_charpattern", g_api->value_string(ctx, "[", 1));
    qamrpp_value* table = g_api->value_table(ctx); for (size_t i = 0; i < sizeof(kBindings)/sizeof(kBindings[0]); ++i) { const char* name = strchr(kBindings[i].name, '_') + 1; g_api->table_raw_set(ctx, table, g_api->value_string(ctx, name, strlen(name)), g_api->get_global(ctx, kBindings[i].name)); } g_api->table_raw_set(ctx, table, g_api->value_string(ctx, "charpattern", 11), g_api->get_global(ctx, "utf8_charpattern")); g_api->set_global(ctx, "utf8", table);
    return 0;
}

static const qamrpp_library_descriptor kDescriptor = { QAMRPP_LIBRARY_API_VERSION, "utf8", kBindings, sizeof(kBindings) / sizeof(kBindings[0]), on_load, 0 };
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
