#ifndef QAMRPP_CPP_LIBRARY_HPP
#define QAMRPP_CPP_LIBRARY_HPP

#include "../C/QaMRpp-Library.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qamrpp::library {

class Context;

class Value {
public:
    Value() = default;
    Value(qamrpp_context* context, qamrpp_value* value) : context_(context), value_(value) {}

    qamrpp_value* get() const noexcept { return value_; }
    explicit operator bool() const noexcept { return value_ != nullptr; }
    qamrpp_value_type type() const noexcept { return qamrpp_value_type_of(value_); }
    bool truthy() const noexcept { return qamrpp_value_to_bool(value_) != 0; }
    int64_t integer() const noexcept { return qamrpp_value_to_int(value_); }
    double number() const noexcept { return qamrpp_value_to_float(value_); }
    std::string_view string() const noexcept {
        size_t length = 0;
        const char* data = qamrpp_value_to_string(value_, &length);
        return data ? std::string_view(data, length) : std::string_view();
    }

private:
    qamrpp_context* context_ = nullptr;
    qamrpp_value* value_ = nullptr;
};

class Context {
public:
    explicit Context(qamrpp_context* context) : context_(context) {}
    qamrpp_context* get() const noexcept { return context_; }

    Value nil() const { return make(qamrpp_make_nil(context_)); }
    Value boolean(bool value) const { return make(qamrpp_make_bool(context_, value ? 1 : 0)); }
    Value integer(int64_t value) const { return make(qamrpp_make_int(context_, value)); }
    Value number(double value) const { return make(qamrpp_make_float(context_, value)); }
    Value string(std::string_view value) const {
        return make(qamrpp_make_string(context_, value.data(), value.size()));
    }
    Value table() const { return make(qamrpp_make_table(context_)); }
    Value global(const char* name) const { return make(qamrpp_get_global_value(context_, name)); }
    void global(const char* name, Value value) const {
        qamrpp_set_global_value(context_, name, value.get());
    }

    Value raw_get(Value table, Value key) const {
        return make(qamrpp_table_raw_get_value(context_, table.get(), key.get()));
    }
    void raw_set(Value table, Value key, Value value) const {
        qamrpp_table_raw_set_value(context_, table.get(), key.get(), value.get());
    }
    Value get(Value table, Value key) const {
        return make(qamrpp_table_get_value(context_, table.get(), key.get()));
    }
    void set(Value table, Value key, Value value) const {
        qamrpp_table_set_value(context_, table.get(), key.get(), value.get());
    }
    void error(qamrpp_error_code code, std::string_view message) const {
        qamrpp_set_error(context_, code, std::string(message).c_str());
    }

private:
    Value make(qamrpp_value* value) const { return Value(context_, value); }
    qamrpp_context* context_;
};

using Native = qamrpp_native_fn;

struct Binding {
    const char* name;
    Native function;
};

class Module {
public:
    explicit Module(const char* name) : name_(name) {}

    Module& bind(const char* name, Native function) {
        bindings_.push_back({name, function});
        return *this;
    }
    const qamrpp_library_descriptor& descriptor() const {
        descriptor_.api_version = QAMRPP_LIBRARY_API_VERSION;
        descriptor_.name = name_;
        descriptor_.functions = bindings_.data();
        descriptor_.function_count = bindings_.size();
        descriptor_.on_load = on_load_;
        descriptor_.on_unload = on_unload_;
        return descriptor_;
    }
    Module& on_load(int (*function)(qamrpp_context*, const qamrpp_host_api*)) {
        on_load_ = function;
        return *this;
    }
    Module& on_unload(void (*function)(qamrpp_context*)) {
        on_unload_ = function;
        return *this;
    }

private:
    const char* name_;
    std::vector<qamrpp_native_binding> bindings_;
    mutable qamrpp_library_descriptor descriptor_{};
    int (*on_load_)(qamrpp_context*, const qamrpp_host_api*) = nullptr;
    void (*on_unload_)(qamrpp_context*) = nullptr;
};

template <size_t N>
constexpr size_t count(const Binding (&)[N]) noexcept { return N; }

}

#define QAMRPP_CPP_LIBRARY_EXPORT(descriptor_symbol) \
    QAMRPP_EXPORT const qamrpp_library_descriptor* qamrpp_get_library_descriptor(void) { \
        return &(descriptor_symbol); \
    }

#endif
