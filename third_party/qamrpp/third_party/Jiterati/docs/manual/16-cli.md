# CLI

`jiterati-cli` is implemented in `src/Jiterati-CLI.cpp`.

## Top-Level Commands
```text
compile
parse
validate
format
package
plugin
backend
pass
config
cache
doctor
version
help
```

Every command supports command-specific help through `help <command>` or `--help` where implemented.

## IR Commands
```sh
jiterati-cli parse input.ir
jiterati-cli validate input.ir
jiterati-cli format input.ir --out output.ir
jiterati-cli compile --target amd64 input.ir --emit asm --out output.s
```

Compilation options:
- `--target`: backend target.
- `--emit`: output format (`ir`, `asm`, `obj`, `exe` depending on backend support).
- `--out`: output path.

## Introspection Commands
```sh
jiterati-cli backend --list
jiterati-cli plugin --list
jiterati-cli pass --list
jiterati-cli version
jiterati-cli doctor
```

## Configuration Commands
```sh
jiterati-cli config --home
jiterati-cli config --get KEY
jiterati-cli config --set KEY=VALUE
jiterati-cli cache --path
jiterati-cli cache --clear
```

## Package Commands
See `docs/manual/15-package-system.md`.

## Diagnostics
CLI errors should be concise, stable, and nonzero-exit. Source diagnostics should include location when parser/validator data is available.

## Scripting Contract
- Prefer explicit `--out` for generated files.
- Treat stdout as user-facing text unless a command documents machine-readable output.
- Treat nonzero exit as failure.
