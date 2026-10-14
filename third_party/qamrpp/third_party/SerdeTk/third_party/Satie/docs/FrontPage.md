# Satie — Front Page

**Small, header-only, multi-algorithm SAT solver.**

Satie implements three engines — exhaustive Native, DPLL, and CDCL — behind a
single facade. The library is header-only (plus one compiled TU for the facade)
and depends only on the C++20 standard library.

## Roadmap

| Phase | Component | Status |
|------:|-----------|--------|
| 1     | `DSLtk.hpp` combinatory parser toolkit | complete |
| 2     | CLI + REPL (`cli/satie-repl.cpp`) | complete |
| 3     | Documentation (`docs/manual/`, Doxygen) | complete |
| 4     | Build system (namespaced CMake) | complete |
| 5     | Unit tests (Catch2, 30 cases) | complete |
| 6     | Distribution assets (`distrib/`) | complete |
| 7     | Examples (`examples/`) | complete |

## Library structure

- @ref include/Common.hpp — core types: `Var`, `Lit`, `Clause`, `CNF`, `Assignment`, `SolveResult`, DIMACS parser, CNF DSL parser.
- @ref DSLtk.hpp — header-only combinatory parser/AST toolkit (`dsl::` namespace).
- @ref include/SatieDPLL.hpp — DPLL engine.
- @ref include/SatieCDCL.hpp — CDCL engine with clause learning.
- @ref include/SatieNative.hpp — exhaustive baseline engine + model counter.
- @ref include/Satie.hpp — unified facade: `Solver`, `Engine`, parsing entry points.

## Manual

1. @ref manual/1-SAT-Theory.md — SAT semantics, CNF, complexity.
2. @ref manual/2-Encoding-for-SAT.md — constraint encoding, DIMACS emission.
3. @ref manual/3-SAT-Algorithms.md — engine portfolio comparison.
4. @ref manual/4-Naive-Solutions.md — exhaustive search.
5. @ref manual/4-DPLL-Algorithm.md — unit propagation, pure literals, backtracking.
6. @ref manual/5-CDCL-Algorithm.md — conflict analysis, learning, backjumping.
7. @ref manual/6-Using-Satie.md — public interface, reports, DAG/DOT.
8. @ref manual/7-Possible-Errors.md — error taxonomy.

See @ref manual/README.md for the index and recommended reading order.

## Quick start

```cpp
#include "Satie.hpp"
satie::CNF cnf({{1, 2}, {-1, 3}});
std::cout << satie::Solver(cnf).satisfiable(); // 1 (SAT)
```
