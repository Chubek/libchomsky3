# PicoRL: Small, Capable, Header-Only REPL Toolkit

`PicoRL.hpp` is a small yet capable library for construction of REPLs. It embeds QaMRpp, my implementation of Lua, and this Lua comes with `lpicorl` library, whch we use to create robust buildinb blocks for a language's REPL, and then build up from these building bkocks a fully compliant shell for our languages,

Like an oroboros, QaMRpp itself uses PicoRL as its shall!!

## Features of PicoRL

- Pormpt keys (`lpicorl.prompt`)
- Syntax highlighting (`lpicorl.syntax`)
- Directive commands (`lpicorl.directive`)
- Shared library plugins (`lpicorl.plugin`)
- Navigable pages (`lpicorl.page`)
- Menues (`lpicorl.menue`)
- Web connectivity (`lpicorl.web`)
- Sandboxing (`licorl.sandboxing`)
- IDL fro the guest language (`lpicorl.idl`)
- Color Sscheems an themes (`lpicorl.them`, `lpicorl.colors`)
- Automation/Recordingg (`lpicorl.macros`)

## Bundle CLI (`pikorl-cli`)

PikoRL provides a small bundle scaffolder executable:

```bash
pikorl-cli [--stub path/to/stub.sh] <bundle-dir>
```

Example:

```bash
pikorl-cli foo
```

Creates:

- `colors.lua`
- `history.lua`
- `macros.lua`
- `MANIFEST.json`
- `syntax.lua`
- `theme.lua`
- `web.lua`
- `package.sh`
- `entrypoint.sh`
- `code/`
- `static/`

### Runnable archive packaging

Inside the bundle directory:

```bash
./package.sh [output-name]
```

This creates an executable archive by concatenating:

1. A shebang stub (`.pikorl_stub.sh`), and
2. A tar payload containing bundle files.

At runtime, the stub extracts the payload to a temporary directory and runs:

- `entrypoint.sh`, or
- `entrypoint.py`, or
- the first file matching `entrypoint*`.

Use `--stub` during scaffold creation to provide a custom runtime stub.
