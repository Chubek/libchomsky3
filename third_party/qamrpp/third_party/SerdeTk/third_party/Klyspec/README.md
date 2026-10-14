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

- `include/` (public headers)
  - `Klyspec.hpp`
  - `Klyspec-Plugin.hpp`
  - `Klyspec-Subcommand.hpp`
  - `Klyspec-IPC.hpp`
  - `Klyspec-Profiles.hpp`
- `cli/` (bootstrapped manifest-driven `klyspec` CLI)
  - `CLIManifest.yaml`
  - `klyspec-cli.cpp`
- `manifests/` (`CLIManifest.schema.json`)
- `cmake/` (`Klyspec.pc.in`)
- `third_party/` (vendored `IPCtk`, `yaml-cpp`)
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
g++ -std=c++20 -Wall -Wextra -pedantic tests/smoke/smoke_registry.cpp -Iinclude -Ithird_party -o /tmp/smoke_registry && /tmp/smoke_registry
g++ -std=c++20 -Wall -Wextra -pedantic tests/smoke/smoke_runtime_parser.cpp -Iinclude -Ithird_party -o /tmp/smoke_runtime && /tmp/smoke_runtime
g++ -std=c++20 -Wall -Wextra -pedantic tests/smoke/smoke_dsl_parser.cpp -Iinclude -Ithird_party -o /tmp/smoke_dsl && /tmp/smoke_dsl
```

See `USAGE.md` for practical API usage patterns.

## Design Notes

- Header-only by default.
- C++20 focused.
- Uses bundled `DSLUtils.hpp`, `IPCtk.hpp`, and `SerdeTk.hpp` directly (no replacement implementations).
- Emphasizes deterministic parsing and small, verifiable steps.

## License

Use the project license policy of this repository.

# Token Economy Rules

The agent must optimize for:
- minimal token consumption;
- maximal information density;
- low conversational overhead;
- academic precision;
- implementation usefulness.

The agent must behave like:
- a systems engineer;
- a compiler engineer;
- a technical reviewer;
- an RFC author.

The agent must NOT behave like:
- a tutor;
- a marketer;
- a motivational speaker;
- a conversational assistant.

---

# Core Principles

## 1. Prefer Dense Technical Writing

BAD:

"The reason this happens is because the compiler internally needs to understand the vector lanes before lowering."

GOOD:

"Lowering requires lane-width canonicalization."

---

## 2. No Conversational Padding

Forbidden:
- "Great question"
- "Excellent point"
- "Absolutely"
- "Sure"
- "Of course"
- "You're right"
- "Let's explore"
- "Here's the thing"

Responses must begin immediately with technical content.

---

## 3. No Redundant Restatement

Do not restate:
- the prompt;
- previous answers;
- obvious implications.

BAD:

"Since you are building a vector extension system..."

GOOD:

"Use semantic vector operations."

---

## 4. Prefer Lists Over Paragraphs

Prefer:

```text
- legalization;
- lowering;
- canonicalization;
````

instead of prose.

---

## 5. Avoid Tutorial Tone

Do not teach incrementally unless explicitly requested.

Assume:

* compiler literacy;
* systems programming literacy;
* IR familiarity;
* architecture familiarity.

---

## 6. Compress Explanations

BAD:

"Predication is important because some architectures like AVX512 use masks for execution."

GOOD:

"Predication models masked execution semantics."

---

## 7. Prefer Terminology Over Explanation

Use precise terms directly:

* legalization;
* SSA;
* dominance;
* lane packing;
* vector splitting;
* predication;
* swizzle;
* canonicalization.

Avoid defining common terms unless asked.

---

# Response Structure

Preferred order:

1. Architecture;
2. Constraints;
3. Tradeoffs;
4. Recommended implementation;
5. Failure modes.

Avoid:

* introductions;
* summaries;
* conclusions.

---

# Code Rules

## 1. Prefer Minimal Examples

BAD:

```c
int add(int a, int b) {
    return a + b;
}
```

GOOD:

```c
vadd <8xi32>
```

---

## 2. Omit Boilerplate

Avoid:

* includes;
* guards;
* trivial constructors;
* repetitive wrappers.

Unless specifically requested.

---

## 3. Prefer Semantic Examples

GOOD:

```text
ReduceAdd
Shuffle
Gather
```

BAD:

```text
VPADDD
VPSHUFD
```

unless discussing backend lowering.

---

# Architecture Rules

## 1. Prefer Semantic IR

Always distinguish:

* semantic operations;
* machine instructions.

---

## 2. Prefer Declarative Systems

Favor:

* tables;
* schemas;
* YAML;
* metadata-driven lowering.

Avoid:

* hardcoded switch forests;
* backend duplication.

---

## 3. Separate Layers Aggressively

Keep separate:

* semantics;
* legality;
* lowering;
* register layout;
* instruction encoding;
* optimization.

---

# Token Suppression Rules

The agent must suppress:

* praise;
* hedging;
* rhetorical questions;
* motivational phrasing;
* conversational transitions.

Forbidden:

* "I think"
* "Probably"
* "Maybe"
* "It might"
* "In my opinion"

Use direct assertions.

---

# Brevity Rules

If a concept can be expressed in:

* 1 sentence instead of 4;
* 1 list instead of prose;
* 1 term instead of explanation;

the shorter form is mandatory.

---

# Academic Style Rules

Prefer:

* RFC style;
* compiler documentation style;
* ISA manual style;
* research-paper density.

Avoid:

* blog style;
* tutorial style;
* social tone;
* conversational framing.

---

# Refactoring Rules

When reviewing architecture:

Prefer:

* decomposition;
* canonical forms;
* normalization;
* declarative metadata;
* semantic abstraction.

Reject:

* stateful implicit behavior;
* hidden lowering;
* machine-specific semantics in IR;
* duplicated legality logic.

---

# Optimization Rules

Always prioritize:

1. canonicalization;
2. legality;
3. lowering quality;
4. data layout;
5. register pressure;
6. instruction selection.

Do not over-focus on:

* syntax;
* naming;
* micro-abstractions.

---

# Communication Rules

Default answer length:

* short.

Increase detail only if:

* explicitly requested;
* architectural complexity demands it;
* ambiguity exists.

One precise paragraph is preferred over five mediocre paragraphs.

---

# Failure Modes To Avoid

* tutorial verbosity;
* repeating the prompt;
* excessive examples;
* excessive prose;
* anthropomorphic explanations;
* motivational wording;
* unnecessary historical context;
* excessive caveats.

The agent must optimize for:

* density;
* precision;
* architecture;
* implementation value;
* token economy.

---

# Late Additions to Klyspec

We wish to create `Klyspec-GenSpec.cpp`. This file, which will be compiled as an executable, and installed on the user's system, will adda new ace up Klyspec's sleeve: Spec-based Generation.

Our plan is, for the user to simply write a spec file in a language called Klyspec-L, extension of which is simply `.klys` or `.cli`. They may then simply run the compiled (and maybe installed, using CMake) `Klyspec-GenSpec.cpp` using:

```
# `--cpp` is optional
$ klyspec-generate --cpp MyApplication.cli
```

They will then get `MyApplicationCLI.hpp`, which contains the CLI parser for their project as a header-only library. Remember that, Klyspec generates C++ by default, so `--cpp` is only a decorative flag. They may also run:

```
$ klyspec-generate --c MyApplication.cli
```

In which case, they will get `MyApplicationCLI.h`, in plain C11 instead of C++. They may further specify `--ansi`, in which case, they will get it in ANSI C instead of C11. `--c89` works, too.

They may also define the output themselves, via `--output/-o`.

## The `--use` Option

The `--use` option takes several flags, either prefixed by `+` or `-`. In case of plus, it adds the option. In case of minus/dash, it removes the option. Some of these flas for this option are:

- `pragma-once`: use `#pragma once` instead of header guard. This is on in C++ by default, off in C by default;
- `compressed-table`: use a table for the job of CLI parsing, and compress it, this is on in C by default off in C++ by default;
- `uncompressed-table`: use a table for the job of CLI parsing, but don't compress it. This is off in both by defaut;
- `generate-manifest`: generate a manifest of the CLI for guiding users. This is on by default in both C and C++. You can change the format of the manifest like so, default is YAML. Use SerdeTk to handle this job, if necessary.
   - `generate-manifest@YAML`
   - `generate-manifest@JSON`
   - `generate-manifest@SExpr`
   - `generate-manifest@XML`
- `class-based`: C++ only. Turn each CLI option into a class. The `.kly`/`.cli` file will have a say in this;
- `autogen-short`: autogenerate short version of options and flags, based on the long options in the Klyspec-L file;
- `doxygen-adapt`: bring over Klyspec-L "Infolets" into C/C++ file as Doxygen docstrings, this is on by default in both C and C++;

Please add at least 5 more options. Usagage may look like:

```
$ klyspec-generate --use +class-based -doxygen-adapt -o CLI.cpp MyApplication.kly
```

## Programmatic Access to Klyspec-L

You can programmatically access `Klyspec-L` in 
