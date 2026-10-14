# Satie: Small, Header-Only, Multi-Algorithm SAT Solver

**This library has been created with the help of GPT 3.5 Codex, if you have moral qualms about using AI-generated libraries, this is your time to close this tab, and use another minimal library like PicoSAT or MiniSAT**.

Satie is a small-footprint, minimal, header-only library that implements a naive (exhaustive) SAT solver, DPLL, and CDCL algorithms. CNF and DIMACS parsing are provided in `Common.hpp`, and all solver headers include it.

A manual has been provided in `docs/manual/`, covering theory, encoding, algorithms, and the public API.

SAT is NP-complete; don't expect a tiny library to be end-all. See *The Handbook of Satisfiability* for background.

~ Chubak

---

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest
```

Requirements: a C++20 compiler. The library, CLI, REPL, tests, and examples
depend only on the standard library.

## Components

| Path | Description |
|------|-------------|
| `include/Common.hpp` | Core types: `Var`, `Lit`, `Clause`, `CNF`, `Assignment`, `SolveResult`, DIMACS + CNF DSL parsers. |
| `include/DSLtk.hpp` | Header-only combinatory parser / AST toolkit (`dsl::` namespace). |
| `include/SatieDPLL.hpp` | DPLL engine: unit propagation, pure literals, backtracking. |
| `include/SatieCDCL.hpp` | CDCL engine: conflict analysis, clause learning, backjumping. |
| `include/SatieNative.hpp` | Exhaustive baseline + model counter. |
| `include/Satie.hpp` | Unified facade: `Solver`, `Engine`, parse entry points. |
| `cli/satie-cli.cpp` | Non-interactive batch driver. |
| `cli/satie-repl.cpp` | Interactive REPL (standard-library only, syntax highlighting). |
| `cli/Satie.syn` | DSL token → ANSI color definition consumed by the REPL. |
| `tests/` | 30 Catch2-compatible tests (Common, DSLtk, solvers). |
| `docs/` | Doxygen config + 8-chapter manual. |
| `distrib/` | Shell completions, Vim/Neovim, Sublime, LSP scaffold. |
| `examples/` | API, DSL, and NativeDSL examples. |

## Usage

```cpp
#include "Satie.hpp"
satie::CNF cnf({{1, 2}, {-1, 3}});
std::cout << satie::Solver(cnf).satisfiable(); // 1 (SAT)
```

CLI:

```sh
satie-cli --engine cdcl --model formula.cnf
```

REPL:

```sh
satie-repl
satie> (a | b) & (~a | c)
satie> :solve
satie> :model
```

## Build options

| Option | Default | Effect |
|--------|---------|--------|
| `INSTALL_CLI` | `ON` | Build and install `satie-cli` / `satie-repl`. |
| `BUILD_TESTING` | `ON` | Build the Catch2 test suite. |
| `BUILD_EXAMPLES` | `ON` | Build example programs. |
| `GENERATE_DOCS` | `OFF` | Run Doxygen to generate API docs. |
| `INSTALL_FISH/ZSH/BASH/VIM/LSP/SUBLIME` | `OFF` | Install distribution assets. |

## Roadmap (Satie Protocol)

- [x] **Phase 1** — `DSLtk.hpp` combinatory parser toolkit.
- [x] **Phase 2** — CLI + REPL (`cli/satie-repl.cpp`), syntax highlighting (`Satie.syn`).
- [x] **Phase 3** — Documentation: `docs/manual/` (8 chapters), `FrontPage.md`, `Doxyfile.in`.
- [x] **Phase 4** — Build system: namespaced `satie::satie`, gated subdirectories.
- [x] **Phase 5** — Unit tests: 30 cases (Common / DSLtk / solvers), CTest-registered.
- [x] **Phase 6** — Distribution: Fish, Zsh, Bash, Vim, Sublime, LSP scaffold.
- [x] **Phase 7** — Examples: API, DSL (N-Queens, pigeonhole), NativeDSL.

## License

MIT. Technically, since this is AI-generated code, only God owns it.
