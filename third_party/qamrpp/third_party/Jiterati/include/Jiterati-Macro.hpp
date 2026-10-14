#pragma once

// -----------------------------------------------------------------------------
// Jiterati-Macro.hpp
//
// Lua macro interface for Jiterati.
// This header provides the Lua/sol2-facing scaffolding used to define and
// register macros that operate on the Jiterati IR.
// -----------------------------------------------------------------------------
//
// Expected dependencies:
//   - Jiterati.hpp
//   - sol2 (vendored in dependencies/)
//   - Lua headers/libraries
//
// Notes:
//   - It mirrors the header-only style used by Jiterati.hpp.
//   - Macros are intended to be authored in Lua and exposed through ljiterati.
// -----------------------------------------------------------------------------

#include "Jiterati.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// We have CMake take care of including `dependencies/sol2` into the include paths
#include <sol/sol.hpp>

namespace jiterati {

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------

class Macro;
class MacroRegistry;
class LuaRuntime;

// -----------------------------------------------------------------------------
// Macro
// -----------------------------------------------------------------------------

/**
 * @brief Represents a Lua-defined Jiterati macro.
 *
 * A macro is typically created from Lua via:
 *   ljiterati.macro.new("Name")
 *
 * and then populated with callbacks such as:
 *   main, init, cleanup
 */
class Macro {
public:
    using MainFn = std::function<void()>;
    using InitFn = std::function<void()>;
    using CleanupFn = std::function<void()>;

    Macro() = default;
    explicit Macro(std::string name);

    const std::string& name() const noexcept;
    void set_name(std::string name);

    bool valid() const noexcept;

    void set_init(InitFn fn);
    void set_main(MainFn fn);
    void set_cleanup(CleanupFn fn);

    void init();
    void main();
    void cleanup();

private:
    std::string name_;
    InitFn init_;
    MainFn main_;
    CleanupFn cleanup_;
};

// -----------------------------------------------------------------------------
// MacroRegistry
// -----------------------------------------------------------------------------

/**
 * @brief Stores and resolves macros by name.
 */
class MacroRegistry {
public:
    MacroRegistry() = default;

    bool has(std::string_view name) const;
    Macro* find(std::string_view name);
    const Macro* find(std::string_view name) const;

    Macro& emplace(std::string name);
    void add(Macro macro);
    void remove(std::string_view name);
    void clear();

    std::size_t size() const noexcept;

private:
    std::unordered_map<std::string, Macro> macros_;
};

// -----------------------------------------------------------------------------
// LuaRuntime
// -----------------------------------------------------------------------------

/**
 * @brief Owns the Lua state and exposes the ljiterati API.
 *
 * This is the bridge layer between sol2 and the Jiterati macro system.
 */
class LuaRuntime {
public:
    LuaRuntime();
    explicit LuaRuntime(sol::state& lua);

    sol::state& state();
    const sol::state& state() const;

    MacroRegistry& registry();
    const MacroRegistry& registry() const;

    void open_libraries();
    void bind_api();

    void load_file(const std::string& path);
    void run_string(const std::string& code);

private:
    sol::state* lua_ = nullptr;
    std::unique_ptr<sol::state> owned_lua_;
    MacroRegistry registry_;
};

// -----------------------------------------------------------------------------
// Lua binding helpers
// -----------------------------------------------------------------------------

/**
 * @brief Registers the ljiterati module into a Lua state.
 */
void register_ljiterati(sol::state& lua, MacroRegistry& registry);

/**
 * @brief Registers macro-related helpers and constructors.
 */
void register_macro_api(sol::state& lua, MacroRegistry& registry);

// -----------------------------------------------------------------------------
// Convenience API
// -----------------------------------------------------------------------------

/**
 * @brief Creates a macro object from C++.
 */
Macro make_macro(std::string name);

/**
 * @brief Loads and executes a Lua macro script.
 */
void load_macro_script(LuaRuntime& runtime, const std::string& path);

/**
 * @brief Executes a Lua string as a macro script.
 */
void execute_macro_script(LuaRuntime& runtime, const std::string& code);

} // namespace jiterati

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

namespace jiterati {

// Macro -----------------------------------------------------------------------

inline Macro::Macro(std::string name)
    : name_(std::move(name)) {}

inline const std::string& Macro::name() const noexcept {
    return name_;
}

inline void Macro::set_name(std::string name) {
    name_ = std::move(name);
}

inline bool Macro::valid() const noexcept {
    return !name_.empty();
}

inline void Macro::set_init(InitFn fn) {
    init_ = std::move(fn);
}

inline void Macro::set_main(MainFn fn) {
    main_ = std::move(fn);
}

inline void Macro::set_cleanup(CleanupFn fn) {
    cleanup_ = std::move(fn);
}

inline void Macro::init() {
    if (init_) init_();
}

inline void Macro::main() {
    if (main_) main_();
}

inline void Macro::cleanup() {
    if (cleanup_) cleanup_();
}

// MacroRegistry ---------------------------------------------------------------

inline bool MacroRegistry::has(std::string_view name) const {
    return macros_.find(std::string(name)) != macros_.end();
}

inline Macro* MacroRegistry::find(std::string_view name) {
    auto it = macros_.find(std::string(name));
    return it == macros_.end() ? nullptr : &it->second;
}

inline const Macro* MacroRegistry::find(std::string_view name) const {
    auto it = macros_.find(std::string(name));
    return it == macros_.end() ? nullptr : &it->second;
}

inline Macro& MacroRegistry::emplace(std::string name) {
    auto [it, inserted] = macros_.try_emplace(std::move(name));
    (void)inserted;
    return it->second;
}

inline void MacroRegistry::add(Macro macro) {
    macros_.insert_or_assign(macro.name(), std::move(macro));
}

inline void MacroRegistry::remove(std::string_view name) {
    macros_.erase(std::string(name));
}

inline void MacroRegistry::clear() {
    macros_.clear();
}

inline std::size_t MacroRegistry::size() const noexcept {
    return macros_.size();
}

// LuaRuntime ------------------------------------------------------------------

inline LuaRuntime::LuaRuntime()
    : owned_lua_(std::make_unique<sol::state>()),
      lua_(owned_lua_.get()) {}

inline LuaRuntime::LuaRuntime(sol::state& lua)
    : lua_(&lua) {}

inline sol::state& LuaRuntime::state() {
    return *lua_;
}

inline const sol::state& LuaRuntime::state() const {
    return *lua_;
}

inline MacroRegistry& LuaRuntime::registry() {
    return registry_;
}

inline const MacroRegistry& LuaRuntime::registry() const {
    return registry_;
}

inline void LuaRuntime::open_libraries() {
    if (!lua_) return;
    lua_->open_libraries(sol::lib::base,
                         sol::lib::package,
                         sol::lib::string,
                         sol::lib::table,
                         sol::lib::math,
                         sol::lib::os,
                         sol::lib::debug);
}

inline void LuaRuntime::bind_api() {
    if (!lua_) return;
    register_ljiterati(*lua_, registry_);
}

inline void LuaRuntime::load_file(const std::string& path) {
    if (!lua_) return;
    lua_->script_file(path);
}

inline void LuaRuntime::run_string(const std::string& code) {
    if (!lua_) return;
    lua_->script(code);
}

// Helpers ---------------------------------------------------------------------

inline Macro make_macro(std::string name) {
    return Macro(std::move(name));
}

// Lua bindings ----------------------------------------------------------------

namespace detail {

inline std::string host_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "amd64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__riscv) && __riscv_xlen == 64
    return "rv64";
#elif defined(__wasm__)
    return "wasm";
#else
    return "unknown";
#endif
}

inline std::string host_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#elif defined(__wasm__)
    return "wasm";
#else
    return "unknown";
#endif
}

inline void invoke_lua_callback(sol::table macro, char const* name) {
    sol::object callback = macro[name];
    if (callback == sol::nil) return;
    if (!callback.is<sol::protected_function>()) {
        throw std::runtime_error(std::string("ljiterati.macro callback is not callable: ") + name);
    }
    sol::protected_function fn = callback.as<sol::protected_function>();
    sol::protected_function_result result = fn();
    if (!result.valid()) {
        sol::error error = result;
        throw std::runtime_error(error.what());
    }
}

inline void emit_noop(std::string const&) {}

} // namespace detail

inline void register_ljiterati(sol::state& lua, MacroRegistry& registry) {
    sol::table lj = lua.create_named_table("ljiterati");
    register_macro_api(lua, registry);

    sol::table insn = lua.create_table();
    auto emit = [](std::string mnemonic) { detail::emit_noop(mnemonic); return mnemonic; };
    insn.set_function("nop", [emit] { return emit("nop"); });
    insn.set_function("load", [emit](sol::object) { return emit("load"); });
    insn.set_function("const", [emit](sol::object) { return emit("const"); });
    insn.set_function("const_i32", [emit](std::int32_t) { return emit("const_i32"); });
    insn.set_function("add", [emit] { return emit("add"); });
    insn.set_function("sub", [emit] { return emit("sub"); });
    insn.set_function("mul", [emit] { return emit("mul"); });
    insn.set_function("div", [emit] { return emit("div"); });
    insn.set_function("dec", [emit](sol::object) { return emit("dec"); });
    insn.set_function("cmp_eq", [emit] { return emit("cmp_eq"); });
    insn.set_function("jump", [emit](sol::object) { return emit("jump"); });
    insn.set_function("jump_if", [emit](sol::object) { return emit("jump_if"); });
    insn.set_function("label", [emit](sol::object) { return emit("label"); });
    insn.set_function("ret", [emit] { return emit("ret"); });
    insn.set_function("ret_void", [emit] { return emit("ret_void"); });
    lj["insn"] = insn;

    sol::table label = lua.create_table();
    label.set_function("new", [](std::string name) { return name; });
    lj["label"] = label;

    sol::table type = lua.create_table();
    type.set_function("i32", [] { return std::string("i32"); });
    type.set_function("i64", [] { return std::string("i64"); });
    type.set_function("f32", [] { return std::string("f32"); });
    type.set_function("f64", [] { return std::string("f64"); });
    type.set_function("ptr", [] { return std::string("ptr"); });
    lj["type"] = type;

    sol::table runtime = lua.create_table();
    runtime.set_function("version", [] { return std::string("Jiterati Lua Bridge 0.1.0"); });
    runtime.set_function("arch", [] { return detail::host_arch(); });
    runtime.set_function("os", [] { return detail::host_os(); });
    lj["runtime"] = runtime;
}

inline void register_macro_api(sol::state& lua, MacroRegistry& registry) {
    sol::table lj = lua["ljiterati"].get_or_create<sol::table>();
    sol::table macro = lua.create_table();

    macro.set_function("new", [&lua, &registry](std::string name) {
        Macro& registered = registry.emplace(name);
        registered.set_name(name);
        sol::table table = lua.create_table();
        table["name"] = name;
        table["init"] = sol::nil;
        table["main"] = sol::nil;
        table["cleanup"] = sol::nil;
        table["run"] = [table]() mutable {
            detail::invoke_lua_callback(table, "init");
            detail::invoke_lua_callback(table, "main");
            detail::invoke_lua_callback(table, "cleanup");
        };
        return table;
    });
    macro.set_function("count", [&registry] { return registry.size(); });
    macro.set_function("exists", [&registry](std::string name) { return registry.has(name); });
    macro.set_function("run", [](sol::table table) {
        detail::invoke_lua_callback(table, "init");
        detail::invoke_lua_callback(table, "main");
        detail::invoke_lua_callback(table, "cleanup");
    });

    lj["macro"] = macro;
}

inline void load_macro_script(LuaRuntime& runtime, const std::string& path) {
    runtime.load_file(path);
}

inline void execute_macro_script(LuaRuntime& runtime, const std::string& code) {
    runtime.run_string(code);
}

} // namespace jiterati

