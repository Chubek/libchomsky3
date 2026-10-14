# Klyspec

Klyspec is a header-only C++20 framework for building command-line interfaces with:

- a command/argument specification model,
- deterministic runtime argument parsing,
- a Klytmk textual DSL parser,
- plugin hooks,
- native subcommand dispatch,
- IPC integration via `IPCtk`,
- profile loading via `SerdeTk`.

It is designed for incremental, compile-first development and zero runtime dependencies beyond the STL (+ bundled headers in this repo).

## Status

This repository currently provides a working bootstrap implementation with smoke-tested core features:

- `Klyspec.hpp` (core model + runtime parser + Klytmk parser)
- `Klyspec-Plugin.hpp` (plugin contracts + plugin registry)
- `Klyspec-Subcommand.hpp` (native subcommand contract + dispatch registry)
- `Klyspec-IPC.hpp` (IPC service bridge using `IPCtk.hpp`)
- `Klyspec-Profiles.hpp` (profile loader using `SerdeTk.hpp`)

## Repository Layout

- `Klyspec.hpp`
- `Klyspec-Plugin.hpp`
- `Klyspec-Subcommand.hpp`
- `Klyspec-IPC.hpp`
- `Klyspec-Profiles.hpp`
- `examples/`
  - `plugins/`
  - `subcommands/`
  - `profiles/`
- `tests/`
  - `smoke/`
  - `unit/`
  - `integration/`
- `manual/` (12 chapter reStructuredText manual)

## Quick Start

```cpp
#include "Klyspec.hpp"

int main() {
  using namespace klyspec;

  Registry registry;
  registry.register_command(CommandSpec{.name = "build", .help = "Build artifacts"});

  registry.register_argument("build", ArgumentSpec{
      .id = "verbose",
      .kind = ArgumentKind::flag,
      .value_policy = ValuePolicy::none,
      .names = {"-v", "--verbose"},
      .help = "Enable verbose output"
  });

  registry.register_argument("build", ArgumentSpec{
      .id = "file",
      .kind = ArgumentKind::option,
      .value_policy = ValuePolicy::required,
      .names = {"-f", "--file"},
      .required = true
  });

  KlyCLIService cli(registry);
  ParseResult parsed = cli.parse("build", {"-v", "--file", "in.txt"});
  return parsed.ok ? 0 : 1;
}
```

Compile:

```bash
g++ -std=c++20 -Wall -Wextra -pedantic main.cpp -I. -o app
```

## Klytmk DSL Example

```klytmk
param "b/book="
{
    help-string
    {
        Book option
    };
};

command "build"
{
};

pre-evaluate
{
    exec = "/bin/bash";
    sanitize = "stdlib/shell.sh";
};
```

## Smoke Tests

Run current smoke tests individually (examples):

```bash
g++ -std=c++20 -Wall -Wextra -pedantic tests/smoke/smoke_registry.cpp -I. -o /tmp/smoke_registry && /tmp/smoke_registry
g++ -std=c++20 -Wall -Wextra -pedantic tests/smoke/smoke_runtime_parser.cpp -I. -o /tmp/smoke_runtime && /tmp/smoke_runtime
g++ -std=c++20 -Wall -Wextra -pedantic tests/smoke/smoke_dsl_parser.cpp -I. -o /tmp/smoke_dsl && /tmp/smoke_dsl
```

See `USAGE.md` for practical API usage patterns.

## Design Notes

- Header-only by default.
- C++20 focused.
- Uses bundled `DSLUtils.hpp`, `IPCtk.hpp`, and `SerdeTk.hpp` directly (no replacement implementations).
- Emphasizes deterministic parsing and small, verifiable steps.

## License

Use the project license policy of this repository.
