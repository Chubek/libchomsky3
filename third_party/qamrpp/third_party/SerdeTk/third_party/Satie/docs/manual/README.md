# Satie Manual Index

## Chapter map

1. `1-SAT-Theory.md`
   - SAT semantics, CNF formalism, complexity profile, invariants.

2. `2-Encoding-for-SAT.md`
   - Constraint-to-CNF encoding patterns and DIMACS emission rules.

3. `3-SAT-Algorithms.md`
   - Portfolio-level comparison of Native, DPLL, CDCL engines.

4. `4-Naive-Solutions.md`
   - Exhaustive-search engine behavior, complexity, and use cases.

5. `4-DPLL-Algorithm.md`
   - Unit propagation, pure-literal elimination, branching, backtracking.

6. `5-CDCL-Algorithm.md`
   - Conflict analysis, learning, backjumping, activity-driven branching.

7. `6-Using-Satie.md`
   - Public `Satie.hpp` interface, solve flows, reports, DAG/DOT usage.

8. `7-Possible-Errors.md`
   - Parser, I/O, runtime error taxonomy and defensive integration guidance.

## Recommended reading order

- Theory: 1 -> 2
- Solver internals: 3 -> 4 -> 5 -> 6
- Integration/runtime safety: 7 -> 8
