# Podlets Developer Notes

## Runtime hook points

Podlet runtime integration is attached to existing package loading, not a parallel loader:

- `Context::load_library_named` first tries shared libraries.
- If not found, it tries Podlet archive discovery.

Podlet discovery roots reuse existing search roots:

- `QAMRPP_PATH` entries
- `~/.qamrpp`
- current directory (`.`)

Plus per-root Podlet subpath probing:

- `<root>/<name>.qpod`
- `<root>/podlet/<name>.qpod`

## Archive contract

Current `.qpod` contract:

- top-level object with keys:
  - `format` = `qamrpp.qpod/1`
  - `manifest` (object of string fields)
  - `files` (object of file payloads)

Required manifest fields:

- `name`
- `version`
- `entrypoint`

## Runtime export materialization

Current materialization is conservative metadata-first:

- export table includes `name`, `version`, `entrypoint`, `source`, and `manifest`.
- table is cached by `name@version`.
- failures do not write partial cache.

Errors are surfaced through existing runtime error state:

- `last_error_code`
- `last_error_message`

## Packaging core

Reusable packaging API:

- `podlet/PodletPackaging.hpp`

Responsibilities:

- manifest file detection/parsing (`Podpack.qmr` / `Podpack.lua`)
- manifest validation
- file collection
- SerdeTk-backed MessagePack archive emission

This API is CLI-agnostic by design; `podpack` is a thin wrapper over this layer.

## Compatibility guarantees

- Existing shared-library loading behavior is preserved.
- Podlet loading is additive fallback only.
- No rewrite/replacement of current `require` implementation.
