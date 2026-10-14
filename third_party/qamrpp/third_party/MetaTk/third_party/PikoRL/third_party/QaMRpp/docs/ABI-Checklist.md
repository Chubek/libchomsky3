# QaMRpp ABI Checklist

A concise production checklist for third-party QaMRpp native libraries and descriptor-based plugins.

## 1) Descriptor correctness

- [ ] `api_version` exactly matches current ABI constant.
  - Library: `QAMRPP_LIBRARY_API_VERSION`
  - Plugin: `QAMRPP_PLUGIN_API_VERSION`
- [ ] `name` is non-null and non-empty.
- [ ] If `function_count > 0`, `functions` is non-null.
- [ ] Every binding has a non-empty `name` and non-null function pointer.
- [ ] Export function symbol is present:
  - Library: `qamrpp_get_library_descriptor`
  - Plugin (if descriptor-based): `qamrpp_get_plugin_descriptor`

## 2) Build and link hygiene

- [ ] Build shared object with PIC (`-fPIC`) and shared output (`-shared`).
- [ ] Link against `QaMRpp-Library.c` helpers (or equivalent ABI wrapper layer).
- [ ] Export only intended ABI symbols (use default-hidden visibility policy where possible).
- [ ] Avoid relying on host C++ internals from C ABI modules.

## 3) Host API usage

- [ ] Do not cache function pointers from host API into mutable global state unless synchronized.
- [ ] Treat `qamrpp_value*` arguments as borrowed; never free host-owned values.
- [ ] Prefer wrapper helpers in `QaMRpp-Library.h`/`.c` over direct callback usage.
- [ ] Gate optional operations by pointer checks when calling host API functions.

## 4) Argument validation and error handling

- [ ] Validate `argc` and argument types on every exported native function.
- [ ] On failure, set explicit errors via `qamrpp_set_error(...)`.
- [ ] Return deterministic failure values (`nil` or clear sentinel) for invalid calls.
- [ ] Avoid undefined behavior on null pointers or empty inputs.

## 5) Threading and lifecycle

- [ ] Assume context-affine execution unless the embedding host documents stronger guarantees.
- [ ] Keep `on_load`/`on_unload` idempotent and side-effect bounded.
- [ ] Avoid process-wide mutable state unless protected and justified.
- [ ] Ensure userdata destructors are safe and do not throw across C ABI boundaries.

## 6) Compatibility and migration

- [ ] Keep function names stable once published.
- [ ] Use descriptor validation in CI:
  - `qamrpp_validate_library_descriptor(...)`
  - `qamrpp_validate_plugin_descriptor(...)`
- [ ] Verify module behavior against at least one pinned runtime build.
- [ ] Add integration tests executed via `qamrpp-cli --load ... --script ...`.

## 7) Pre-release verification

- [ ] Module loads by full path (`--load ./libqamrpp_<name>.so`).
- [ ] Module loads by name from search path (`--load <name>` with `QAMRPP_PATH`/`~/.qamrpp`).
- [ ] Happy-path and error-path tests both pass.
- [ ] No unresolved dynamic symbols at runtime.

## Minimal CI command sketch

```bash
cmake -S . -B build
cmake --build build -j
./build/cli/qamrpp-cli --load <module> --script "tests/scripts/*.lua"
```

