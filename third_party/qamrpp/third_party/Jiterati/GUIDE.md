# Jiterati Guide

## Project map

| Area | Role |
|---|---|
| `include/` | Public C++ APIs for IR, backends, passes, plugins, and macros. |
| `IR/` | AST, terse/TAC IR, parser, validation, rewriting, and peepholes. |
| `Passlib/` | Bundled analyses and transformations. |
| `BE/` | AMD64, AArch64, RV64, and WASM backend pipelines. |
| `Plugins/` | Example instruction-selection and scheduling plugins. |
| `Lua/`, `Macros/` | Lua bridge and sample macros. |
| `src/` | JIT, JBL/JPL helpers, and CLI. |
| `specs/` | Normative IR and extension specifications. |

## Workflow: textual IR

1. Start from an example in `examples/IR/`.
2. Parse and normalize it:

   ```sh
   build/Jiterati-CLI parse input.ir
   build/Jiterati-CLI format input.ir --out normalized.ir
   ```

3. Validate before lowering:

   ```sh
   build/Jiterati-CLI validate normalized.ir
   ```

4. Compile for a target:

   ```sh
   build/Jiterati-CLI compile \
     --target amd64 --emit asm \
     normalized.ir --out output.s
   ```

Supported target names are `amd64`, `aarch64`, `rv64`, and `wasm`. The accepted emission modes are `ir`, `asm`, `obj`, and `exe`; backend support determines which artifact is materialized.

The semantic IR uses primitive types `void`, `i1`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`, and `ptr`. Blocks must terminate; definitions are SSA-like and validation checks types, control flow, calls, and predecessor consistency.

## Workflow: C++ API

Construct a `Module`, create typed functions and blocks, emit operations through fluent builders, then serialize or pass the module onward:

```cpp
#include <Jiterati.hpp>
#include <iostream>

int main() {
    jiterati::Module module("arithmetic");
    auto* fn = module.create_function<int(int, int)>("add_bias");
    auto* entry = fn->create_block("entry");
    auto sum = entry->add(entry->arg(0), entry->arg(1));
    entry->ret(entry->add(sum, fn->const_i32(12)));
    std::cout << module.to_string();
}
```

Use `examples/API/` for branches, loops, calls, memory, introspection, serialization, and backend helpers.

## Workflow: passes

Passes are composed with JPL pipeline syntax:

```cpp
auto pipeline =
    jiterati::parse_jpl_pipeline("cfg; constant-folding; strength-reduction; dce");
pipeline.run(module);
```

The bundled pass library includes CFG, dominator, liveness, constant folding/propagation, dead-code removal, strength reduction, and peephole optimization. Preserve semantic IR in passes; target legality and machine instruction selection belong to backends.

## Workflow: backends

Backends follow common stages:

```text
instruction selection → register allocation → rewrite → peephole → emission
```

Use `backend --list` to inspect targets known to the build. Architecture metadata is maintained separately from backend algorithms; consult `.agents/datasets/` when extending a backend.

## Workflow: plugins and packages

Inspect registered metadata:

```sh
build/Jiterati-CLI plugin --list
build/Jiterati-CLI pass --list
build/Jiterati-CLI backend --list
```

A package is a `.jpkg` archive with `META-INF/manifest.json` and relative paths. Typical layout:

```text
META-INF/manifest.json
Plugin/
Pass/
BE/
Docs/
Examples/
Resources/
```

Build and inspect a package:

```sh
build/Jiterati-CLI package --pack package.sh --out Example.jpkg
build/Jiterati-CLI package --validate Example.jpkg
build/Jiterati-CLI package --view-manifest Example.jpkg
build/Jiterati-CLI package --install Example.jpkg
```

Set `JITERATI_HOME` for isolated package roots. Use `--verify`, `--remove`, `--upgrade`, and `--repair` for lifecycle management.

## Workflow: Lua macros

Enable Lua support at configure time (it is on by default), include `Jiterati-Macro.hpp`, and load scripts through `LuaRuntime`:

```cpp
#include <Jiterati-Macro.hpp>

jiterati::LuaRuntime runtime;
jiterati::load_macro_script(runtime, "Macros/QuickAdd.lua");
```

Lua scripts interact through the registered `ljiterati` module. Existing examples include `Macros/QuickAdd.lua`, `QuickSubtract.lua`, `QuickMultiply.lua`, `QuickDivide.lua`, and `CountDownLoop.lua`. The binding surface is defined by `Lua/JiteratiBinding.cpp` and `include/Jiterati-Macro.hpp`.

## Diagnostics and configuration

```sh
build/Jiterati-CLI doctor
build/Jiterati-CLI config --home
build/Jiterati-CLI config --get KEY
build/Jiterati-CLI cache --path
```

Use explicit `--out` paths for generated artifacts. Nonzero exit status indicates failure; parser and validator diagnostics should be treated as part of the CLI contract.

## Further reference

- [IR specification](specs/IR.md)
- [backend specification](specs/backend-specs.yaml)
- [pass specification](specs/passes-specs.yaml)
- [plugin specification](specs/plugin-specs.yaml)
- [manual](docs/manual/)
