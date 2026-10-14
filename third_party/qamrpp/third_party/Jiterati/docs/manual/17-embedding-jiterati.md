# Embedding Jiterati

Embedding uses the public C++ headers and the static `jiterati` library built by CMake.

## CMake Consumption
Installed package metadata includes `jiterati.pc` generation. Direct source-tree embedding can link the `jiterati` target from this project.

## Core Include Set
```cpp
#include <Jiterati.hpp>
#include <Jiterati-BE.hpp>
#include <Jiterati-Pass.hpp>
#include <Jiterati-Plugin.hpp>
```

Lua macro users additionally include:
```cpp
#include <Jiterati-Macro.hpp>
```

## Embedding Workflow
1. Construct `Module` and `Function` objects or parse textual IR.
2. Build blocks and instructions.
3. Run validation/passes.
4. Select backend or `ICompiler` factory.
5. Handle `CompiledFunction` or `CompilationArtifact` ownership.

## JIT Driver
```cpp
JIT jit = JIT::create_default();
auto compiled = jit.compile(function, backend);
```

`CompiledFunction` owns any backend-provided code memory and exposes a raw function pointer through `ptr()`.

## Error Policy
- Parser failures return `std::nullopt`/`nullptr` plus diagnostics or error strings.
- Artifact compilation reports textual diagnostics in `CompilationArtifact::diagnostics`.
- Backend/JIT APIs may throw standard exceptions for unrecoverable misuse.

## Threading
No global thread-safety guarantee is documented for mutable registries, modules, or Lua runtimes. Use external synchronization for shared state.
