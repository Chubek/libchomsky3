# Introduction

Jiterati is a C++17 compiler-construction toolkit centered on a small typed IR, pass infrastructure, target backends, plugins, packages, and optional Lua macros. The repository currently contains both implemented runtime components and forward-looking specifications; this manual separates those layers explicitly.

## Audience
- Compiler engineers embedding Jiterati into tools.
- Backend authors implementing target lowering.
- Pass authors adding analyses or transformations.
- Package/plugin authors distributing extensions.

## Repository Map
- `include/`: public C++ headers.
- `IR/`: textual/TAC/Terse IR model, parser, validation, rewriting, peepholes.
- `BE/`: AMD64, AArch64, RV64, and WASM backend implementations plus backend metadata.
- `Passlib/`: bundled pass implementations.
- `Plugins/`: instruction-selection and scheduling plugin examples.
- `Lua/`: `ljiterati` Lua binding layer.
- `src/`: JIT driver, JBL/JPL helpers, and CLI implementation.
- `specs/`: IR, backend, pass, and plugin specifications.
- `examples/`: C++ API and textual IR examples.
- `tests/`: regression tests and package fixtures.

## Stability Classes
- **Implemented API**: public declarations in `include/` and functions compiled by `CMakeLists.txt`.
- **Implemented CLI**: behavior in `src/Jiterati-CLI.cpp`.
- **Specification**: normative intent in `specs/`; may exceed current implementation.
- **Dataset authority**: architecture facts in `.agents/datasets/*.lua`; backends should converge to these datasets.

## Build Model
```sh
cmake -S . -B build -DJITERATI_BUILD_TESTS=ON -DJITERATI_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build
```

Optional build switches:
- `JITERATI_BUILD_TESTS`: builds tests.
- `JITERATI_BUILD_EXAMPLES`: builds examples.
- `JITERATI_BUILD_LUA`: enables Lua/Sol2 bindings.
- `JITERATI_BUILD_DOCS`: enables Doxygen/manual generation.

## Primary Workflow
1. Construct or parse IR.
2. Validate invariants.
3. Run analyses/transforms.
4. Lower through a backend or emit serialized IR.
5. Package extensions when distribution is required.

## Manual Conventions
- API names are authoritative only when present in source headers.
- CLI examples describe current `jiterati-cli` behavior unless marked as specification.
- Backend architecture facts defer to `.agents/datasets/`.
