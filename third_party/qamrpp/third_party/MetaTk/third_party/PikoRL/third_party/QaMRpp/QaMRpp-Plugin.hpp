#ifndef QAMRPP_PLUGIN_HPP
#define QAMRPP_PLUGIN_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "QaMRpp-Library.h"

namespace qamrpp {

class Context;
struct Node;

enum class HookPoint : uint8_t {
    BeforeRun = 0,
    AfterParse = 1,
    AfterEval = 2,
    OnError = 3
};

struct HookPayload {
    const std::string* source = nullptr;
    const Node* ast = nullptr;
    const char* error_message = nullptr;
};

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual const char* name() const = 0;
    virtual void install(Context& ctx) = 0;
};

} /* namespace qamrpp */

#ifdef __cplusplus
extern "C" {
#endif

#define QAMRPP_PLUGIN_API_VERSION 1u

typedef struct qamrpp_plugin_function {
    const char* name;
    qamrpp_native_fn function;
} qamrpp_plugin_function;

typedef struct qamrpp_plugin_descriptor {
    uint32_t api_version;
    const char* name;
    const qamrpp_plugin_function* functions;
    size_t function_count;
    int (*on_load)(qamrpp_context* ctx);
    void (*on_unload)(qamrpp_context* ctx);

    /* Optional extension fields (zero/null when unused). */
    size_t descriptor_size;
    uint32_t flags;
    const void* reserved0;
    const void* reserved1;
} qamrpp_plugin_descriptor;

static inline int qamrpp_validate_plugin_descriptor(const qamrpp_plugin_descriptor* descriptor) {
    size_t i;

    if (!descriptor) return 0;
    if (descriptor->api_version != QAMRPP_PLUGIN_API_VERSION) return 0;
    if (!descriptor->name || descriptor->name[0] == '\0') return 0;
    if (descriptor->function_count > 0 && !descriptor->functions) return 0;

    for (i = 0; i < descriptor->function_count; ++i) {
        if (!descriptor->functions[i].name || descriptor->functions[i].name[0] == '\0') return 0;
        if (!descriptor->functions[i].function) return 0;
    }

    return 1;
}

#define QAMRPP_PLUGIN_DESCRIPTOR_INIT(plugin_name, function_table, function_count_value, on_load_fn, on_unload_fn) \
    { \
        QAMRPP_PLUGIN_API_VERSION, \
        (plugin_name), \
        (function_table), \
        (function_count_value), \
        (on_load_fn), \
        (on_unload_fn), \
        sizeof(qamrpp_plugin_descriptor), \
        0u, \
        NULL, \
        NULL \
    }

#define QAMRPP_PLUGIN_EXPORT_DESCRIPTOR(descriptor_symbol) \
    QAMRPP_EXPORT const qamrpp_plugin_descriptor* qamrpp_get_plugin_descriptor(void) { \
        return &(descriptor_symbol); \
    }

#ifdef __cplusplus
}
#endif

#endif
