# Using Satie

## Public interface

Primary include:

```cpp
#include "Satie.hpp"
```

Core API:

- `satie::Solver`
- `satie::Engine`
- `satie::SolveOptions`
- `satie::SolverReport`
- `satie::parse_auto`
- `satie::parse_auto_file`
- `satie::parse(text, format)`
- `satie::parse_file(path, format)`
- `satie::solve` overloads
- `satie::solve_with_report`

## Engine selection

`Engine` values:

- `Engine::Native`: exhaustive search baseline.
- `Engine::DPLL`: unit propagation + pure literal + branching.
- `Engine::CDCL`: clause learning + non-chronological backjumping.

Default engine in unified API: `Engine::CDCL`.

## Input formats

Satie supports two input notations:

- CNF DSL: `(a | ~b) & (c | d)`
- DIMACS CNF:

```text
c sample
p cnf 3 2
1 -2 0
3 0
```

`parse_auto` dispatches by lexical shape:

- leading `p` line => DIMACS;
- otherwise => CNF DSL.

Deterministic parsing API:

- `parse(text, ParseFormat::CNF)`;
- `parse(text, ParseFormat::DIMACS)`;
- `parse(text, ParseFormat::Auto)`.

## Minimal solve flow

```cpp
using namespace satie;

CNF cnf = parse_auto("(a | b) & (~a | c)");
SolveResult result = solve(cnf, Engine::CDCL);

if (result.satisfiable()) {
  // result.assignment.get_var(1) ...
}
```

## Stateful solver flow

```cpp
using namespace satie;

Solver solver;
solver.load(parse_auto("(a | b) & (~a | c)"));

SolveResult sat = solver.solve(Engine::DPLL);
bool ok = solver.satisfiable(Engine::Native);
```

## Statistics flow

```cpp
using namespace satie;

Solver solver(parse_auto("(a | b) & (~a | c)"));
SolverReport report = solver.solve_with_report({.engine = Engine::CDCL});

if (report.statistics.cdcl) {
  auto st = *report.statistics.cdcl;
  // st.decisions, st.conflicts, st.learned_clauses, st.restarts ...
}
```

## File-based flow

```cpp
using namespace satie;

CNF cnf = parse_auto_file("problem.cnf");
SolverReport report = solve_with_report(cnf, {.engine = Engine::DPLL});
```

## Result contract

`SolveResult`:

- `status`: `SAT`, `UNSAT`, `UNKNOWN`.
- `assignment`: model when `SAT`; empty/unknown assignment when `UNSAT`.

Convenience predicates:

- `satisfiable()`
- `unsatisfiable()`

## CNF/DIMACS interop

Helpers from `Common.hpp` remain available through `Satie.hpp`:

- `cnf_to_dimacs(cnf)`
- `dimacs_to_cnf(dimacs)`
- `to_dimacs_string(cnf)`
- `parse_dimacs(stream)`

Solvers canonicalize through CNF↔DIMACS conversion path during `load`.

## DAG visualization

Graph construction and DOT emission:

- `ProblemDAG dag = cnf_to_dag(cnf);`
- `std::string dot = dag_to_dot(dag);`
- `std::string dot2 = cnf_to_dot(cnf);`

Render with Graphviz:

```bash
dot -Tpng problem.dot -o problem.png
```

## Error model

Parsing operations may throw:

- `satie::ParseError` with line/column diagnostics.
- `std::runtime_error` on file I/O failures.

DIMACS strict checks include:

- malformed `p cnf` header;
- clause count mismatch;
- out-of-range or undeclared-variable literals;
- unterminated clause not ending in `0`.

## Integration guidance

- Use `Engine::CDCL` for production default.
- Use `Engine::DPLL` for smaller formulas and deterministic teaching traces.
- Use `Engine::Native` only for baseline verification and model counting experiments.
- Normalize and export DIMACS for reproducible benchmarks.
- Persist `SolverReport` statistics for regression tracking.
- CDCL applies fixed conflict-interval restarts; monitor `statistics.cdcl->restarts`.
