# Macros

Macros are Lua-defined or C++-created units with optional lifecycle callbacks.

## C++ Model
```cpp
class Macro {
public:
    const std::string& name() const noexcept;
    bool valid() const noexcept;
    void set_init(InitFn);
    void set_main(MainFn);
    void set_cleanup(CleanupFn);
    void init();
    void main();
    void cleanup();
};
```

A macro is valid when its name is nonempty. Callback absence is legal; invoking an absent callback is a no-op.

## Lua Shape
Typical Lua-side construction:
```lua
M = ljiterati.macro.new("Name")
M.init = function() end
M.main = function() end
M.cleanup = function() end
```

Exact available Lua helpers are defined by `Lua/JiteratiBinding.cpp`.

## Lifecycle
Recommended host sequence:
1. Load script into `LuaRuntime`.
2. Resolve macro by name from `MacroRegistry`.
3. Run `init` once.
4. Run `main` for generation/transformation.
5. Run `cleanup` once.

## Design Rules
- Macro output must pass normal IR validation.
- Macro names must be stable package/API identifiers.
- Host code owns error reporting and rollback policy.
- Macros must not encode backend-only semantics in generic IR.

## Packaging
Place Lua macro resources under `Resources/`, `Scripts/`, or a package-defined macro directory and list them in `META-INF/manifest.json`.
