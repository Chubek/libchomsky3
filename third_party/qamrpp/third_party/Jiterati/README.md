# Jiterati

Jiterati is a C++17 compiler-construction and JIT experimentation toolkit:

- typed in-memory IR with C++ builders;
- terse and TAC textual IR, parsing, formatting, and validation;
- analysis and transformation passes;
- AMD64, AArch64, RV64, and WebAssembly backends;
- plugin and `.jpkg` package infrastructure;
- optional Lua/sol2 macro bindings;
- a command-line interface for IR, compilation, packages, and diagnostics.

The implementation is intentionally layered:

```text
C++ API or textual IR
        ↓
semantic IR → validation → passes
        ↓
backend lowering → instruction selection → allocation → emission
```

## Quick start

```sh
cmake -S . -B build \
  -DJITERATI_BUILD_TESTS=ON \
  -DJITERATI_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build
```

The CLI target is `Jiterati-CLI`:

```sh
build/Jiterati-CLI version
build/Jiterati-CLI validate examples/IR/01_add_i64.ir
build/Jiterati-CLI parse examples/IR/01_add_i64.ir
build/Jiterati-CLI format examples/IR/01_add_i64.ir --out /tmp/add.ir
build/Jiterati-CLI compile \
  --target amd64 --emit asm \
  examples/IR/01_add_i64.ir --out /tmp/add.s
```

## C++ example

```cpp
#include <Jiterati.hpp>
#include <iostream>

int main() {
    jiterati::Module module("demo");
    auto* fn = module.create_function<int(int, int)>("add");
    auto* entry = fn->create_block("entry");
    entry->ret(entry->add(entry->arg(0), entry->arg(1)));
    std::cout << module.to_string();
}
```

See `examples/API/` and `examples/IR/` for complete programs.

## Build options

| Option | Default | Effect |
|---|---:|---|
| `JITERATI_BUILD_TESTS` | `OFF` | Build and register tests with CTest. |
| `JITERATI_BUILD_EXAMPLES` | `OFF` | Build C++ API examples. |
| `JITERATI_BUILD_LUA` | `ON` | Build Lua/sol2 bindings. |
| `JITERATI_BUILD_DOCS` | `ON` | Configure the Doxygen documentation target. |

## Documentation

- [INSTALL.md](INSTALL.md): prerequisites, configuration, installation, and packaging.
- [GUIDE.md](GUIDE.md): workflows for IR, passes, backends, plugins, packages, and Lua.
- [`docs/manual/`](docs/manual/): detailed subsystem documentation.
- [`specs/`](specs/): IR, backend, pass, and plugin specifications.

The repository contains both implemented APIs and forward-looking specifications. Public headers and compiled sources define current behavior; specifications define intended contracts where implementation is incomplete.

## License

Jiterati is released under the MIT license. Meanwhile, Jiterati is built with the assisstance of Agentic AI, therefore, it is entirely public domain.
