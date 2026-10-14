# QaMRpp Developer Guide

A comprehensive guide to the QaMRpp runtime, extension ABI, plugin lifecycle, and command-line tooling.

---

## Chapter 1 — Architecture and Design Goals

QaMRpp is a lightweight interpreter stack centered around three extension layers:

1. **`QaMRpp.hpp`**: header-only runtime and embedding API (C++).
2. **`QaMRpp-Library.h` / `QaMRpp-Library.c`**: stable C ABI for loadable native libraries.
3. **`QaMRpp-Plugin.hpp`**: hook-based plugin model for runtime behavior customization.

### Core principles

- **Embeddability first**: host applications create a `qamrpp::Context` and execute source directly.
- **Pluggability**: native capabilities can be loaded at runtime as shared objects.
- **Low ceremony**: no generated glue is required for simple native functions.
- **Separation of concerns**:
  - Libraries export callable symbols into the script environment.
  - Plugins alter lifecycle behavior via hooks (parse/eval/error phases).

### Runtime layers in practice

- Parser and evaluator run inside `qamrpp::Context`.
- Global functions/values are stored in context global state.
- Dynamic loader supports explicit path loading and named lookup via search roots.
- Standard library modules are delivered as shared libraries and loaded on demand.

---

## Chapter 2 — `QaMRpp.hpp`: Context, Values, and Execution

`QaMRpp.hpp` defines the primary API surface for embedding and execution.

### Main types

- **`qamrpp::Context`**: runtime state holder; owns globals, hooks, linker, and loaded plugins/libraries.
- **`qamrpp::Value`**: tagged runtime value with Lua-like categories (`NIL`, `BOOL`, `INT`, `FLOAT`, `STRING`, `FUNCTION`, `USERDATA`).
- **`qamrpp::Node`**: AST node representation.
- **`qamrpp::Linker`**: utility to compose multi-unit programs from in-memory strings or files.

### Running source

```cpp
#include "QaMRpp.hpp"

qamrpp::Context ctx;
qamrpp::ValuePtr out = ctx.run("1 + 2");
```

Execution pipeline for `Context::run(...)`:

1. Emit `BeforeRun` hook.
2. Parse source into AST.
3. Emit `AfterParse` hook.
4. Evaluate AST.
5. Emit `AfterEval` hook.
6. On exception, emit `OnError` hook.

### Registering native functions

```cpp
ctx.register_native("add", [](qamrpp::Context&, std::vector<qamrpp::ValuePtr>& args) {
    int64_t a = args.size() > 0 ? args[0]->int_value : 0;
    int64_t b = args.size() > 1 ? args[1]->int_value : 0;
    return std::make_shared<qamrpp::Value>(a + b);
});
```

This path is ideal when you control host binary builds and do not need a standalone shared library ABI.

---

## Chapter 3 — `QaMRpp.hpp`: Dynamic Libraries and Standard Library Loading

QaMRpp can load shared objects in two ways:

- **Path-based**: `ctx.load_library("./mylib.so")`
- **Named**: `ctx.load_library_named("string")`

### Search roots for named loads

The named loader resolves platform-specific filenames (`libqamrpp_<name>.so` on Linux) across roots such as:

- directories from `QAMRPP_PATH` (colon-separated)
- `~/.qamrpp`
- current directory

This allows simple deployment of stdlib and third-party modules.

### Standard library loading

`Context::load_standard_library(...)` loads selected modules from the dynamic library path using `StdLib` flags.

```cpp
ctx.load_standard_library(qamrpp::StdLib::CORE | qamrpp::StdLib::MATH);
```

### Linker usage

`qamrpp::Linker` supports staged script composition:

```cpp
qamrpp::Linker linker;
linker.add_source("init.lua", "x = 7");
linker.add_source("main.lua", "x + 5");
auto result = linker.link(ctx);
```

---

## Chapter 4 — `QaMRpp-Library.h`: C ABI Contract

The C ABI exists so library authors can build plugins/modules without depending on internal C++ symbols.

### ABI highlights

- API versioned (`QAMRPP_LIBRARY_API_VERSION`).
- Descriptor-driven function export.
- Host API callbacks provided by runtime.
- Backward-compatible helper wrappers in `QaMRpp-Library.c`.

### Key structs

- **`qamrpp_host_api`**: callbacks for value creation/introspection, error handling, and global access.
- **`qamrpp_library_descriptor`**: metadata and exported native binding table.
- **`qamrpp_native_binding`**: `{name, function}` entries exposed to scripts.

### Required export shape

Libraries expose a descriptor function and can optionally consume runtime-provided host API at load.

Typical runtime interactions:

- `qamrpp_library_set_host_api(const qamrpp_host_api*)`
- `qamrpp_get_library_descriptor()`

The runtime checks API version before registering functions.

### ABI rules (production profile)

- **Version gate**: descriptor `api_version` must equal `QAMRPP_LIBRARY_API_VERSION`.
- **Descriptor validity**:
  - `name` must be non-empty.
  - if `function_count > 0`, then `functions != NULL`.
  - each binding must have non-empty `name` and non-null `function`.
- **Function contract**:
  - native callback signature is fixed: `(qamrpp_context*, qamrpp_value**, size_t) -> qamrpp_value*`.
  - callbacks must tolerate `argc == 0`.
- **Ownership/lifetime**:
  - values returned from host constructors are host-managed values.
  - `qamrpp_value*` passed in `argv` are borrowed and must not be freed by the module.
- **Threading**:
  - module entrypoints should be treated as context-affine unless the embedding application guarantees external synchronization.
  - `qamrpp_library_set_host_api` is safe to call repeatedly; wrappers resolve the current pointer at call time.
- **Error model**:
  - return `nil`/sentinel values on failure where appropriate.
  - set a concrete error through `qamrpp_set_error`/`host_api->set_error` for diagnosability.

### Descriptor validation helpers

`QaMRpp-Library.h` now exposes:

- `qamrpp_validate_library_descriptor(...)`
- `qamrpp_library_has_host_api()`
- `qamrpp_library_host_api_compatible(...)`

Use these in module startup paths to fail early and consistently.

---

## Chapter 5 — `QaMRpp-Library.c`: Helpers and Host API Wiring

`QaMRpp-Library.c` provides helper implementations shared by library authors:

- global host API pointer storage
- accessor/setter helpers
- value construction wrappers (`nil`, `bool`, `int`, `float`, `string`, userdata)
- value inspection wrappers (`qamrpp_value_type_of`, `qamrpp_value_to_*`)
- context/global helpers (`qamrpp_context_get_userdata`, `qamrpp_get_global_value`, ...)
- error and descriptor utilities (`qamrpp_error_code_to_string`, `qamrpp_validate_library_descriptor`)

### Why this file matters

- Keeps individual library code small and ABI-aligned.
- Centralizes host API dispatch logic and consistency checks.
- Avoids copy/paste ABI drift across independent shared libraries.
- Uses atomic host API pointer access when C11 atomics are available.

### Library author workflow

1. Include `QaMRpp-Library.h` in your module.
2. Link against compiled `QaMRpp-Library.c` helper object/library.
3. Export descriptor and binding table.
4. Deploy resulting `.so` into `QAMRPP_PATH` location or `~/.qamrpp`.

---

## Chapter 6 — Authoring Native Libraries End-to-End

This chapter maps the practical flow for adding new native capabilities.

### Step 1: Define native functions

A native function receives runtime context and argv-style value array via ABI types.

### Step 2: Fill binding table

Declare each function and export under script-visible names.

### Step 3: Export descriptor

Set:

- library name
- API version
- binding pointer + count
- optional `on_load` / `on_unload`

### Step 4: Build as shared object

- Linux: `-shared -fPIC`
- place result in `~/.qamrpp` or any directory inside `QAMRPP_PATH`

### Step 5: Load from host

```cpp
ctx.load_library_named("mylib");
```

### Error and robustness notes

- Validate argument count/types defensively.
- Emit errors via host API error hooks when possible.
- Keep exported function names stable; treat them as contract.
- Run `qamrpp_validate_library_descriptor` before exporting/returning the descriptor in tests.

---

## Chapter 7 — `QaMRpp-Plugin.hpp`: Hook System and Lifecycle

Plugins in QaMRpp are **header-based C++ extensions** that alter runtime behavior.

### Hook points

`HookPoint` currently provides:

- `BeforeRun`
- `AfterParse`
- `AfterEval`
- `OnError`

### Payload

`HookPayload` may include:

- source pointer
- AST pointer
- error message pointer

### Plugin contract

A plugin derives from `qamrpp::Plugin` and implements:

- `const char* name() const`
- `void install(Context& ctx)`

Inside `install`, plugins register hooks and/or expose helper natives via `register_native(...)`.

### C ABI compatibility

`QaMRpp-Plugin.hpp` also contains C-side descriptor definitions for legacy/loadable plugin symbols. This enables coexistence with descriptor-based runtime loading.

### Plugin ABI hardening additions

The plugin descriptor now includes optional forward-compatibility fields:

- `descriptor_size`
- `flags`
- `reserved0`
- `reserved1`

Current runtime behavior remains compatible with existing plugin descriptors; new fields can be left zeroed.

### Recommended plugin export pattern

- Initialize descriptors using `QAMRPP_PLUGIN_DESCRIPTOR_INIT(...)`.
- Export with `QAMRPP_PLUGIN_EXPORT_DESCRIPTOR(...)`.
- Validate with `qamrpp_validate_plugin_descriptor(...)` in plugin-side tests.

---

## Chapter 8 — Built-in and Extended Plugin Patterns

This chapter summarizes plugin patterns in the tree:

### `plugins/QaMRpp-Readline.hpp`

- Provides REPL line-input abstraction and history support.
- Used by CLI fallback mode when no input scripts are supplied.

### `plugins/QaMRpp-PrintAST.hpp`

- Hooks `AfterParse`.
- Logs AST node-level data for diagnostics.

### `plugins/QaMRpp-Dump.hpp`

- Hooks parse stage and emits GraphViz DOT representation.
- Controlled via dump-target setup (`--dump` in CLI flow).

### `plugins/QaMRpp-Serialize2JSON.hpp`

- Captures parse/error stage and serializes state (including AST tree) to JSON.
- Exposed to CLI via `--serialize` workflow.

### Compile plugins

- `plugins/QaMRpp-Compile2C.hpp`
- `plugins/QaMRpp-Compile2Bytecode.hpp`

These provide compilation-oriented extension points and native callable interfaces for downstream tooling.

---

## Chapter 9 — CLI Tools: `qamrpp-cli`, `qamrpp-assembler`, `qamrpp-linker`

### `cli/qamrpp-cli.cpp`

Primary interactive/script runner.

Supported modes and flags include:

- no args: REPL mode (Readline-backed), with `exit`/`quit`.
- `--script` / `-s`: run scripts matching glob patterns.
- `--require`: preload Lua files before main script runs.
- `--load`: load native library by path or named lookup.
- `--dump <file>`: enable DOT AST dump output.
- `--serialize`: print JSON serialization for current program stage.
- `--qbf <bundle>`: load bundle file entries into linker pipeline.

### `cli/qamrpp-assembler.cpp`

- Takes script globs and packages them into QBF.
- Useful for deployable script bundles.

### `cli/qamrpp-linker.cpp`

- Merges multiple QBF units into a single bundle.
- Useful for build pipelines combining modules from separate teams/components.

### `plugins/QaMRpp-QBF.hpp`

Shared data model and read/write helpers used by assembler/linker/CLI.

---

## Chapter 10 — Build, Install, and Documentation Tooling

### CMake integration

Top-level CMake includes stdlib shared module builds and optional CLI builds.

- `BUILD_CLI=ON` by default.
- CLI binaries built in active build directory.
- `make install` installs CLI binaries into `bin`.

### Stdlib installation

Stdlib modules are built as shared libraries and installed into `~/.qamrpp`, making named runtime loads straightforward.

### Documentation build system

A docs-local `Makefile` supports generation to HTML and PDF via Pandoc.

Typical use:

```bash
cd docs
make build-html
make build-pdf
```

---

## Chapter 11 — Case Study: Implementing a Native Library Module

### Goal

Expose a `sha256(text)` function to scripts through the C library ABI.

### Design

- Input: one string.
- Output: hex digest string.
- Failure: returns nil + sets error when input is invalid.

### Implementation outline

1. Include `QaMRpp-Library.h`.
2. Create native callback that validates argc and converts input.
3. Use host API string constructor for return value.
4. Register in `qamrpp_native_binding` table.
5. Export descriptor.

### Integration

- Build as `libqamrpp_crypto.so`.
- Install to `~/.qamrpp`.
- Load with `ctx.load_library_named("crypto")`.

### Lessons

- ABI version checks prevent silent misloads.
- Keeping host API operations in helper wrappers simplifies cross-module consistency.

---

## Chapter 12 — Case Studies: Plugins and Embedding Applications

### Case Study A: AST Visualization Plugin for CI

- Install `DumpPlugin` in host application.
- Configure output path per test run.
- On parser regressions, compare DOT graphs over time.
- Benefit: deterministic, artifact-friendly syntax diagnostics.

### Case Study B: Runtime Trace/Telemetry Plugin

- Implement plugin hooking `BeforeRun`, `AfterEval`, and `OnError`.
- Collect timing and failure metadata.
- Emit JSON lines for observability pipeline.
- Benefit: low-intrusion production introspection.

### Case Study C: Embedding QaMRpp in a Service Application

A backend service can embed QaMRpp as follows:

1. Construct one context per request/session or pool contexts by tenant.
2. Register host-native helpers (`log`, `http_get`, business primitives).
3. Load stdlib subset and approved modules.
4. Execute scripts through linker for multi-file policies.
5. Enable `Serialize2JSON` for debugging mode only.

#### Hardening checklist

- Sandboxed function surface (whitelist natives only).
- Strict resource/time limits around execution.
- Defensive argument checking in all native bindings.
- Version pinning for shared libraries and plugins.

#### Outcome

This model provides a compact embeddable scripting layer while preserving host ownership over safety and lifecycle.

---

## Appendix A — Recommended Directory Layout

```text
.
├── QaMRpp.hpp
├── QaMRpp-Library.h
├── QaMRpp-Library.c
├── QaMRpp-Plugin.hpp
├── plugins/
│   ├── QaMRpp-Readline.hpp
│   ├── QaMRpp-PrintAST.hpp
│   ├── QaMRpp-Dump.hpp
│   ├── QaMRpp-Serialize2JSON.hpp
│   ├── QaMRpp-Compile2C.hpp
│   ├── QaMRpp-Compile2Bytecode.hpp
│   └── QaMRpp-QBF.hpp
├── cli/
│   ├── qamrpp-cli.cpp
│   ├── qamrpp-assembler.cpp
│   └── qamrpp-linker.cpp
├── stdlib/
└── docs/
```

## Appendix B — Quick Command Reference

```bash
# Build project
cmake -S . -B build
cmake --build build -j

# Install artifacts
cmake --install build

# CLI examples
./build/cli/qamrpp-cli --script "examples/*.lua"
./build/cli/qamrpp-cli --script "examples/*.lua" --dump ast.dot --serialize
./build/cli/qamrpp-assembler out.qbf "examples/*.lua"
./build/cli/qamrpp-linker merged.qbf out1.qbf out2.qbf
```

## Appendix C — Migration Notes (ABI Cleanup)

This release keeps old module code working, but introduces cleaner production APIs.

### `QaMRpp-Library.h` / `QaMRpp-Library.c`

- **Still supported** (legacy wrappers):
  - `qamrpp_make_nil`, `qamrpp_make_string`, `qamrpp_table_get_value`, ...
- **New recommended wrappers**:
  - `qamrpp_value_type_of`, `qamrpp_value_to_*`
  - `qamrpp_set_error`
  - `qamrpp_context_get_userdata` / `qamrpp_context_set_userdata`
  - `qamrpp_get_global_value` / `qamrpp_set_global_value`
  - `qamrpp_validate_library_descriptor`

Minimal migration:

1. Keep your existing descriptor export (`QAMRPP_LIBRARY_EXPORT_DESCRIPTOR`).
2. Add descriptor validation in tests/CI.
3. Replace direct host API pointer access with wrapper helpers incrementally.
4. Use explicit error codes/messages on every failure branch.

### `QaMRpp-Plugin.hpp`

- Existing plugins implementing `qamrpp::Plugin` need no code changes.
- For C descriptor-based plugins, prefer:
  - `QAMRPP_PLUGIN_DESCRIPTOR_INIT(...)`
  - `QAMRPP_PLUGIN_EXPORT_DESCRIPTOR(...)`
- Optional descriptor extension fields can remain zero for now.
