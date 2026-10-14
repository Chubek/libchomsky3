# Podlet Implementation Log

## 2026-06-12 — Stage 1/2 runtime discovery and first slice

### Discovered

- `require` currently resolves to a stub native in `stdlib/core.c` and is not an active package resolution pipeline yet.
- Package standard library loading is currently a thin dynamic-library load through `stdlib::load_package` and `Context::load_library_named`.
- Runtime already exposes the required value/table/metatable/error/context/global APIs needed for Podlet materialization.
- Existing named library search roots (`QAMRPP_PATH`, `~/.qamrpp`, `.`) can be reused for Podlet discovery.

### Changed

- Added Podlet contract and archive schema notes in `docs/Podlets.md`.

### Pending

- Add `.qpod` loader fallback in runtime after normal shared-library lookup.
- Add validation and error propagation for malformed archives/manifests.
- Add focused tests for valid load, invalid manifest, malformed archive, and repeat load behavior.

## 2026-06-12 — Stage 3 packaging core (SerdeTk-backed)

### Added

- `podlet/PodletPackaging.hpp` as reusable packaging API:
  - manifest discovery (`Podpack.qmr` or `Podpack.lua`)
  - minimal manifest parsing and validation (`name`, `version`, `entrypoint`)
  - package tree collection (including required `Podlet.cpp`)
  - deterministic file ordering
  - `.qpod` emission via SerdeTk MessagePack

### Added tests

- `tests/podlet_packaging_tests.cpp`:
  - valid package tree builds `.qpod`
  - missing manifest rejection
  - missing entrypoint rejection
- Existing runtime tests kept passing with packaged output shape.

### Notes

- Current SerdeTk MessagePack backend stores non-binary documents through the STKJ carrier, so package file payloads are emitted as strings for compatibility with current backend behavior.
- Packaging API is intentionally CLI-agnostic and ready for thin `podpack` command wiring.

## 2026-06-12 — Stage 4 CLI (`podpack`) wiring

### Added

- New CLI tool: `cli/podpack.cpp`
  - `podpack [source_dir] [--output file.qpod]` to build archives
  - `podpack --init [dir]` to scaffold `Podlet.cpp` and `Podpack.qmr`
  - `podpack --install <file.qpod> [--root install_root]` to install archives
- Default install root: `~/.qamrpp/podlet` (fallback `.qamrpp/podlet` if `HOME` unset)
- CMake wiring for `podpack` executable + install rule.

### Added tests

- `tests/podpack_cli_tests.cpp` covering:
  - scaffold generation (`--init`)
  - archive build output (`--output`)
  - install copy behavior (`--install --root`)

### Verification

- Built and executed:
  - `podlet_runtime_tests`
  - `podlet_packaging_tests`
  - `podpack_cli_tests`
  - `podpack` target build

## 2026-06-12 — Stage 5 integration/tests/docs

### Added end-to-end test

- `tests/podlet_e2e_tests.cpp`:
  - build `.qpod` from Podlet project via packaging API
  - install archive into `~/.qamrpp/podlet`-equivalent test root
  - load with `Context::load_library_named("hello_podlet")`
  - assert exported metadata/source visibility
  - assert missing Podlet misses cleanly without forcing runtime error state

### Added example project

- `examples/podlets/hello_podlet/Podlet.cpp`
- `examples/podlets/hello_podlet/Podpack.qmr`
- `examples/podlets/hello_podlet/README.md`

### Added docs

- `docs/Podlets-User.md` (author/build/install/load workflow)
- `docs/Podlets-Developer.md` (runtime hooks, archive contract, cache/error behavior)

### Verification

- Built and ran:
  - `podlet_packaging_tests`
  - `podlet_runtime_tests`
  - `podpack_cli_tests`
  - `podlet_e2e_tests`

## 2026-06-12 — shared-library precedence regression guard

### Added test

- `tests/podlet_precedence_tests.cpp`:
  - creates a fake `core.qpod`
  - sets `QAMRPP_PATH` so both shared libs and Podlet candidate are discoverable
  - asserts `load_library_named("core")` resolves to shared library behavior (`print` exists)
  - asserts no Podlet global export named `core` is materialized in this case

### Outcome

- Confirms shared-library loading keeps precedence and Podlet loading remains fallback-only.
