# Podlets for QaMRpp (Stage 1/2 Contract)

This document defines the initial Podlet runtime contract and `.qpod` shape, implemented as a compatibility-preserving extension on top of existing package loading.

## Goals

- Preserve existing `load_library` / `load_library_named` behavior.
- Add Podlet discovery as fallback, not replacement.
- Keep runtime loading independent from CLI/packaging orchestration.

## Manifest (minimal v1)

Podlet manifest is a string map with required keys:

- `name`: package identifier (e.g. `hello_podlet`)
- `version`: semantic or project version string (e.g. `0.1.0`)
- `entrypoint`: file key that must exist in archive `files` map (e.g. `Podlet.cpp`)

Optional keys:

- `podlet_api`: Podlet contract version (`1`)
- `format_version`: archive schema revision (`1`)
- `description`: free-form text

## `.qpod` archive shape (v1)

A `.qpod` file is MessagePack-encoded map with:

- `format` (string): must be `qamrpp.qpod/1`
- `manifest` (map<string,string>): manifest object
- `files` (map<string,string|bin>): packaged file payloads

Unknown top-level keys are ignored for forward compatibility.

## Loader I/O contract

Input:

- Podlet candidate path (`*.qpod`) discovered from search roots.
- Optional requested package name from named load call.

Output on success:

- Returns `true` from loader path.
- Materializes export table in runtime value space.
- Publishes exports under global `manifest.name`.

Output on failure:

- Returns `false`.
- Uses existing runtime error channel (`qamrpp_set_error` / `Context::last_error_*`) for malformed archives or invalid manifests.

## Runtime export materialization (initial)

Stage 2 exports are conservative and metadata-first:

- top-level table fields: `name`, `version`, `entrypoint`, `source`
- `manifest` subtable: all manifest key/value pairs

`source` is the raw entrypoint file payload from archive `files`.

This keeps behavior explicit while allowing future stages to compile/execute Podlet payloads into richer function/table exports.

## Discovery and install roots (runtime side)

For `load_library_named("<name>")`, Podlet fallback probes:

- `<root>/<name>.qpod`
- `<root>/podlet/<name>.qpod`

where `<root>` comes from existing QaMRpp search roots (`QAMRPP_PATH`, `~/.qamrpp`, `.`).

This makes `~/.qamrpp/podlet` naturally supported without replacing current roots.
