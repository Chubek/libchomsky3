#ifndef QAMRPP_LIBRARY_H
#define QAMRPP_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * C ABI version of the host/library boundary.
 * Bump only for breaking ABI changes.
 */
#define QAMRPP_LIBRARY_API_VERSION 1u

#if defined(_WIN32)
#define QAMRPP_EXPORT __declspec(dllexport)
#else
#define QAMRPP_EXPORT __attribute__((visibility("default")))
#endif

#define QAMRPP_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct qamrpp_context qamrpp_context;
typedef struct qamrpp_value qamrpp_value;

typedef enum qamrpp_value_type {
    QAMRPP_TYPE_NIL = 0,
    QAMRPP_TYPE_BOOL = 1,
    QAMRPP_TYPE_INT = 2,
    QAMRPP_TYPE_FLOAT = 3,
    QAMRPP_TYPE_STRING = 4,
    QAMRPP_TYPE_FUNCTION = 5,
    QAMRPP_TYPE_USERDATA = 6,
    QAMRPP_TYPE_TABLE = 7
} qamrpp_value_type;

typedef enum qamrpp_error_code {
    QAMRPP_OK = 0,
    QAMRPP_ERR_GENERIC = 1,
    QAMRPP_ERR_BAD_ARGUMENT = 2,
    QAMRPP_ERR_NOT_FOUND = 3,
    QAMRPP_ERR_IO = 4,
    QAMRPP_ERR_API_MISMATCH = 5,
    QAMRPP_ERR_UNAVAILABLE = 6
} qamrpp_error_code;

typedef qamrpp_value* (*qamrpp_native_fn)(
    qamrpp_context* ctx,
    qamrpp_value** argv,
    size_t argc
);

typedef struct qamrpp_native_binding {
    const char* name;
    qamrpp_native_fn function;
} qamrpp_native_binding;

typedef struct qamrpp_host_api {
    uint32_t api_version;

    qamrpp_value* (*value_nil)(qamrpp_context* ctx);
    qamrpp_value* (*value_bool)(qamrpp_context* ctx, int value);
    qamrpp_value* (*value_int)(qamrpp_context* ctx, int64_t value);
    qamrpp_value* (*value_float)(qamrpp_context* ctx, double value);
    qamrpp_value* (*value_string)(qamrpp_context* ctx, const char* value, size_t len);
    qamrpp_value* (*value_userdata)(qamrpp_context* ctx, void* data, void (*destructor)(void*));
    qamrpp_value* (*value_table)(qamrpp_context* ctx);

    qamrpp_value_type (*value_get_type)(const qamrpp_value* value);
    int (*value_as_bool)(const qamrpp_value* value);
    int64_t (*value_as_int)(const qamrpp_value* value);
    double (*value_as_float)(const qamrpp_value* value);
    const char* (*value_as_string)(const qamrpp_value* value, size_t* out_len);
    void* (*value_as_userdata)(const qamrpp_value* value);

    qamrpp_value* (*table_raw_get)(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key);
    int (*table_raw_set)(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value);
    qamrpp_value* (*table_get)(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key);
    int (*table_set)(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value);

    qamrpp_value* (*value_get_metatable)(qamrpp_context* ctx, qamrpp_value* value);
    int (*value_set_metatable)(qamrpp_context* ctx, qamrpp_value* value, qamrpp_value* metatable);

    void (*set_error)(qamrpp_context* ctx, qamrpp_error_code code, const char* message);
    void* (*context_get_userdata)(qamrpp_context* ctx);
    void (*context_set_userdata)(qamrpp_context* ctx, void* data);
    qamrpp_value* (*get_global)(qamrpp_context* ctx, const char* name);
    void (*set_global)(qamrpp_context* ctx, const char* name, qamrpp_value* value);
} qamrpp_host_api;

typedef struct qamrpp_library_descriptor {
    uint32_t api_version;
    const char* name;
    const qamrpp_native_binding* functions;
    size_t function_count;
    int (*on_load)(qamrpp_context* ctx, const qamrpp_host_api* host_api);
    void (*on_unload)(qamrpp_context* ctx);
} qamrpp_library_descriptor;

QAMRPP_EXPORT uint32_t qamrpp_library_api_version(void);
QAMRPP_EXPORT void qamrpp_library_set_host_api(const qamrpp_host_api* host_api);
QAMRPP_EXPORT const qamrpp_host_api* qamrpp_library_host_api(void);
QAMRPP_EXPORT int qamrpp_library_has_host_api(void);
QAMRPP_EXPORT int qamrpp_library_host_api_compatible(const qamrpp_host_api* host_api);

QAMRPP_EXPORT qamrpp_value* qamrpp_make_nil(qamrpp_context* ctx);
QAMRPP_EXPORT qamrpp_value* qamrpp_make_bool(qamrpp_context* ctx, int value);
QAMRPP_EXPORT qamrpp_value* qamrpp_make_int(qamrpp_context* ctx, int64_t value);
QAMRPP_EXPORT qamrpp_value* qamrpp_make_float(qamrpp_context* ctx, double value);
QAMRPP_EXPORT qamrpp_value* qamrpp_make_string(qamrpp_context* ctx, const char* value, size_t len);
QAMRPP_EXPORT qamrpp_value* qamrpp_make_userdata(qamrpp_context* ctx, void* data, void (*destructor)(void*));
QAMRPP_EXPORT qamrpp_value* qamrpp_make_table(qamrpp_context* ctx);

QAMRPP_EXPORT qamrpp_value_type qamrpp_value_type_of(const qamrpp_value* value);
QAMRPP_EXPORT int qamrpp_value_to_bool(const qamrpp_value* value);
QAMRPP_EXPORT int64_t qamrpp_value_to_int(const qamrpp_value* value);
QAMRPP_EXPORT double qamrpp_value_to_float(const qamrpp_value* value);
QAMRPP_EXPORT const char* qamrpp_value_to_string(const qamrpp_value* value, size_t* out_len);
QAMRPP_EXPORT void* qamrpp_value_to_userdata(const qamrpp_value* value);

QAMRPP_EXPORT qamrpp_value* qamrpp_table_raw_get_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key);
QAMRPP_EXPORT int qamrpp_table_raw_set_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value);
QAMRPP_EXPORT qamrpp_value* qamrpp_table_get_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key);
QAMRPP_EXPORT int qamrpp_table_set_value(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value);
QAMRPP_EXPORT qamrpp_value* qamrpp_get_metatable(qamrpp_context* ctx, qamrpp_value* value);
QAMRPP_EXPORT int qamrpp_set_metatable(qamrpp_context* ctx, qamrpp_value* value, qamrpp_value* metatable);

QAMRPP_EXPORT void qamrpp_set_error(qamrpp_context* ctx, qamrpp_error_code code, const char* message);
QAMRPP_EXPORT void* qamrpp_context_get_userdata(qamrpp_context* ctx);
QAMRPP_EXPORT void qamrpp_context_set_userdata(qamrpp_context* ctx, void* data);
QAMRPP_EXPORT qamrpp_value* qamrpp_get_global_value(qamrpp_context* ctx, const char* name);
QAMRPP_EXPORT void qamrpp_set_global_value(qamrpp_context* ctx, const char* name, qamrpp_value* value);

QAMRPP_EXPORT const char* qamrpp_error_code_to_string(qamrpp_error_code code);
QAMRPP_EXPORT int qamrpp_validate_library_descriptor(const qamrpp_library_descriptor* descriptor, const char** reason);

#define QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(descriptor_symbol) \
    QAMRPP_EXPORT const qamrpp_library_descriptor* qamrpp_get_library_descriptor(void) { \
        return &(descriptor_symbol); \
    }

#ifdef __cplusplus
}
#endif

#endif
