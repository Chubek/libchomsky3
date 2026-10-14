#include "QaMRpp-Library.h"

#if !defined(__STDC_NO_ATOMICS__) && (__STDC_VERSION__ >= 201112L)
#include <stdatomic.h>
#define QAMRPP_HAVE_ATOMICS 1
#else
#define QAMRPP_HAVE_ATOMICS 0
#endif

#if QAMRPP_HAVE_ATOMICS
static _Atomic(const qamrpp_host_api*) g_qamrpp_host_api = NULL;
#else
static const qamrpp_host_api* g_qamrpp_host_api = NULL;
#endif

static const qamrpp_host_api* qamrpp_api_load(void) {
#if QAMRPP_HAVE_ATOMICS
    return atomic_load_explicit(&g_qamrpp_host_api, memory_order_acquire);
#else
    return g_qamrpp_host_api;
#endif
}

static void qamrpp_api_store(const qamrpp_host_api* host_api) {
#if QAMRPP_HAVE_ATOMICS
    atomic_store_explicit(&g_qamrpp_host_api, host_api, memory_order_release);
#else
    g_qamrpp_host_api = host_api;
#endif
}

static int qamrpp_api_has(const qamrpp_host_api* api) {
    return api != NULL && api->api_version >= QAMRPP_LIBRARY_API_VERSION;
}

QAMRPP_EXPORT uint32_t qamrpp_library_api_version(void) {
    return QAMRPP_LIBRARY_API_VERSION;
}

QAMRPP_EXPORT void qamrpp_library_set_host_api(const qamrpp_host_api* host_api) {
    qamrpp_api_store(host_api);
}

QAMRPP_EXPORT const qamrpp_host_api* qamrpp_library_host_api(void) {
    return qamrpp_api_load();
}

QAMRPP_EXPORT int qamrpp_library_has_host_api(void) {
    return qamrpp_api_has(qamrpp_api_load());
}

QAMRPP_EXPORT int qamrpp_library_host_api_compatible(const qamrpp_host_api* host_api) {
    return qamrpp_api_has(host_api);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_nil(qamrpp_context* ctx) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_nil) return NULL;
    return api->value_nil(ctx);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_bool(qamrpp_context* ctx, int value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_bool) return NULL;
    return api->value_bool(ctx, value);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_int(qamrpp_context* ctx, int64_t value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_int) return NULL;
    return api->value_int(ctx, value);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_float(qamrpp_context* ctx, double value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_float) return NULL;
    return api->value_float(ctx, value);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_string(qamrpp_context* ctx, const char* value, size_t len) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_string) return NULL;
    return api->value_string(ctx, value, len);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_userdata(qamrpp_context* ctx, void* data, void (*destructor)(void*)) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_userdata) return NULL;
    return api->value_userdata(ctx, data, destructor);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_make_table(qamrpp_context* ctx) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_table) return NULL;
    return api->value_table(ctx);
}

QAMRPP_EXPORT qamrpp_value_type qamrpp_value_type_of(const qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_get_type || !value) return QAMRPP_TYPE_NIL;
    return api->value_get_type(value);
}

QAMRPP_EXPORT int qamrpp_value_to_bool(const qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_as_bool || !value) return 0;
    return api->value_as_bool(value);
}

QAMRPP_EXPORT int64_t qamrpp_value_to_int(const qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_as_int || !value) return 0;
    return api->value_as_int(value);
}

QAMRPP_EXPORT double qamrpp_value_to_float(const qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_as_float || !value) return 0.0;
    return api->value_as_float(value);
}

QAMRPP_EXPORT const char* qamrpp_value_to_string(const qamrpp_value* value, size_t* out_len) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (out_len) *out_len = 0;
    if (!qamrpp_api_has(api) || !api->value_as_string || !value) return NULL;
    return api->value_as_string(value, out_len);
}

QAMRPP_EXPORT void* qamrpp_value_to_userdata(const qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_as_userdata || !value) return NULL;
    return api->value_as_userdata(value);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_table_raw_get_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->table_raw_get) return NULL;
    return api->table_raw_get(ctx, table, key);
}

QAMRPP_EXPORT int qamrpp_table_raw_set_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->table_raw_set) return -1;
    return api->table_raw_set(ctx, table, key, value);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_table_get_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->table_get) return NULL;
    return api->table_get(ctx, table, key);
}

QAMRPP_EXPORT int qamrpp_table_set_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->table_set) return -1;
    return api->table_set(ctx, table, key, value);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_get_metatable(qamrpp_context* ctx, qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_get_metatable) return NULL;
    return api->value_get_metatable(ctx, value);
}

QAMRPP_EXPORT int qamrpp_set_metatable(qamrpp_context* ctx, qamrpp_value* value, qamrpp_value* metatable) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->value_set_metatable) return -1;
    return api->value_set_metatable(ctx, value, metatable);
}

QAMRPP_EXPORT void qamrpp_set_error(qamrpp_context* ctx, qamrpp_error_code code, const char* message) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->set_error) return;
    api->set_error(ctx, code, message ? message : "");
}

QAMRPP_EXPORT void* qamrpp_context_get_userdata(qamrpp_context* ctx) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->context_get_userdata) return NULL;
    return api->context_get_userdata(ctx);
}

QAMRPP_EXPORT void qamrpp_context_set_userdata(qamrpp_context* ctx, void* data) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->context_set_userdata) return;
    api->context_set_userdata(ctx, data);
}

QAMRPP_EXPORT qamrpp_value* qamrpp_get_global_value(qamrpp_context* ctx, const char* name) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->get_global || !name) return NULL;
    return api->get_global(ctx, name);
}

QAMRPP_EXPORT void qamrpp_set_global_value(qamrpp_context* ctx, const char* name, qamrpp_value* value) {
    const qamrpp_host_api* api = qamrpp_api_load();
    if (!qamrpp_api_has(api) || !api->set_global || !name) return;
    api->set_global(ctx, name, value);
}

QAMRPP_EXPORT const char* qamrpp_error_code_to_string(qamrpp_error_code code) {
    switch (code) {
        case QAMRPP_OK: return "ok";
        case QAMRPP_ERR_GENERIC: return "generic error";
        case QAMRPP_ERR_BAD_ARGUMENT: return "bad argument";
        case QAMRPP_ERR_NOT_FOUND: return "not found";
        case QAMRPP_ERR_IO: return "io error";
        case QAMRPP_ERR_API_MISMATCH: return "api mismatch";
        case QAMRPP_ERR_UNAVAILABLE: return "unavailable";
        default: return "unknown error";
    }
}

QAMRPP_EXPORT int qamrpp_validate_library_descriptor(const qamrpp_library_descriptor* descriptor, const char** reason) {
    if (reason) *reason = NULL;

    if (!descriptor) {
        if (reason) *reason = "descriptor is null";
        return 0;
    }
    if (descriptor->api_version != QAMRPP_LIBRARY_API_VERSION) {
        if (reason) *reason = "api_version mismatch";
        return 0;
    }
    if (!descriptor->name || descriptor->name[0] == '\0') {
        if (reason) *reason = "name is missing";
        return 0;
    }
    if (descriptor->function_count > 0 && !descriptor->functions) {
        if (reason) *reason = "functions is null but function_count > 0";
        return 0;
    }

    if (descriptor->functions) {
        size_t i;
        for (i = 0; i < descriptor->function_count; ++i) {
            const qamrpp_native_binding* fn = &descriptor->functions[i];
            if (!fn->name || fn->name[0] == '\0') {
                if (reason) *reason = "binding name is missing";
                return 0;
            }
            if (!fn->function) {
                if (reason) *reason = "binding function is null";
                return 0;
            }
        }
    }

    return 1;
}
