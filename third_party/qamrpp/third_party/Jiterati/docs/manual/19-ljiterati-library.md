# `ljiterati` Lua Library

`ljiterati` is registered by the Lua binding layer in `Lua/` and declared in `include/Jiterati-Macro.hpp`.

## Runtime Objects
```cpp
class LuaRuntime;
class MacroRegistry;
class Macro;
```

`LuaRuntime` owns or references a `sol::state`, opens libraries, binds APIs, loads files, and runs strings.

## Registration API
```cpp
void register_ljiterati(sol::state& lua, MacroRegistry& registry);
void register_macro_api(sol::state& lua, MacroRegistry& registry);
```

Convenience helpers:
```cpp
Macro make_macro(std::string name);
void load_macro_script(LuaRuntime& runtime, const std::string& path);
void execute_macro_script(LuaRuntime& runtime, const std::string& code);
```

## Macro Registry
`MacroRegistry` supports:
- `has(name)`;
- `find(name)`;
- `emplace(name)`;
- `add(macro)`;
- `remove(name)`;
- `clear()`;
- `size()`.

## Lua Binding Boundary
Lua scripts should interact through registered `ljiterati` tables, not C++ internals. C++ owns registry lifetime and decides when `init`, `main`, and `cleanup` run.

## Error Handling
- File loading and script execution should surface Sol2/Lua errors to the caller.
- Registry lookup failure must be explicit.
- Macro callbacks should not leave partially-mutated IR without diagnostics.
