# Architecture

Jiterati separates semantic IR, optimization, lowering, target metadata, emission, and extension loading.

## Layers
| Layer | Location | Responsibility |
|---|---|---|
| Core API | `include/Jiterati.hpp` | Module/function/block/value construction, fluent builders, JIT entry point. |
| IR library | `IR/` | Terse AST, TAC IR, parser, validation, lowering, rewrites, peepholes. |
| Pass API | `include/Jiterati-Pass.hpp` | Pass base classes, pass manager, JPL parser. |
| Backends | `BE/` | Target selection, allocation, rewriting, peepholes, emission. |
| Backend metadata | `BE/Metadata.*` | Dataset-derived target descriptors. |
| Plugins | `include/Jiterati-Plugin.hpp`, `Plugins/` | Extension registration and example passes/plugins. |
| Lua macros | `include/Jiterati-Macro.hpp`, `Lua/` | Sol2-backed macro registry and `ljiterati` bindings. |
| CLI/package | `src/Jiterati-CLI.cpp` | Parse/validate/format/compile/package/admin commands. |

## Data Flow
```text
source IR / C++ API
  -> semantic IR
  -> validation
  -> analysis / transform passes
  -> backend-specific lowering
  -> assembly/object/runtime artifact
```

## Separation Rules
- Semantic IR must not encode machine instructions.
- Legality belongs between semantic IR and backend lowering.
- Register layout and instruction encoding belong in backend layers.
- Architecture constants belong in `.agents/datasets/` or generated metadata.
- Diagnostics are interface contracts, not incidental strings.

## Current Backend Pipeline
Backends are organized with common stage names:
- `ISel`: instruction selection.
- `RegAlloc`: register allocation.
- `Rewrite`: target-specific rewriting.
- `Peephole`: local cleanup.
- `Emit`: textual or binary emission.
- `IRCompiler`: compile from Terse/TAC-facing API to artifacts.

## Extension Boundaries
- Passes mutate `Module` or `Function` through public IR APIs.
- Plugins register extension metadata; full dynamic loading is specified separately.
- Packages distribute files plus manifest metadata.
- Lua macros operate through `LuaRuntime`, `MacroRegistry`, and registered bindings.
