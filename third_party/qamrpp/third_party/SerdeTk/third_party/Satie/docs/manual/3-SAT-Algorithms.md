# SAT Algorithms

## Engine portfolio

Satie provides three engines with a uniform `SolveResult` contract:

- Native (`NaiveSolver`)
- DPLL (`DPLLSolver`)
- CDCL (`CDCLSolver`)

## Comparative profile

| Engine | Core method | Strength | Limitation | Primary use |
|---|---|---|---|---|
| Native | exhaustive DFS over assignments | correctness baseline | exponential without pruning | sanity checks, small formulas |
| DPLL | backtracking + unit propagation + pure literals | moderate pruning with low implementation complexity | no learning, repeated conflicts | medium problems, deterministic traces |
| CDCL | DPLL + conflict analysis + learned clauses + backjumping | state-of-practice structure | higher implementation complexity | production default |

## Shared preprocessing and representation

All engines rely on shared `Common.hpp` invariants:

- normalized CNF storage;
- tri-valued assignment semantics;
- clause/formula conflict predicates;
- CNF↔DIMACS canonicalization in load path.

This yields comparable semantics across engines and eliminates parser-dependent divergence.

## Selection guidance

- `CDCL`: default for throughput and scale.
- `DPLL`: preferred when requiring simpler traceability.
- `Native`: only for educational baseline and model count exploration.

## Operational metrics

Use per-engine statistics for regression detection.

- Native: tested assignments, recursion depth behavior, conflicts.
- DPLL: decisions, propagations, pure-literal eliminations, backtracks.
- CDCL: decisions, propagations, conflicts, learned clauses, backjumps.

Metric drift rules:

- rising decisions with stable input size indicates weaker branching.
- rising conflicts without learned-clause growth suggests ineffective learning path.
- rising backtracks/backjumps often indicates encoding quality degradation.

## Solver lifecycle model

1. Parse (`parse_auto`, `parse_cnf`, or `parse_dimacs`)
2. Normalize and load (`Solver::load` or constructor)
3. Solve (`solve` or `solve_with_report`)
4. Consume assignment/statistics
5. Export DIMACS/DOT for reproducibility and diagnostics

This lifecycle should be stable in CI benchmark harnesses.
