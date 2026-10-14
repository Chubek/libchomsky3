# Native Solver

## Algorithm

`NaiveSolver` performs exhaustive depth-first assignment search over variables `1..n`.

Per node:

1. detect immediate formula conflict under partial assignment;
2. if complete assignment, test full satisfaction;
3. branch `true`, then `false`.

## Correctness properties

- complete: explores full boolean search space absent pruning by conflicts;
- sound: returns `SAT` only when full formula evaluates true;
- terminating: finite binary tree of depth `n`.

## Complexity

- worst-case time: \(O(2^n \cdot m \cdot k)\), with `m` clauses and average clause width `k`;
- space: \(O(n)\) assignment + recursion overhead.

## Statistics semantics

- `assignments_tested`: number of full assignments evaluated.
- `recursive_calls`: DFS node count.
- `conflicts`: partial assignments cut by clause contradiction.
- `satisfiable_leafs`: satisfying complete assignments reached.
- `unsatisfiable_leafs`: non-satisfying complete assignments reached.

## Model counting

`count_models()` enumerates all satisfying assignments.

Constraint:

- throws `std::overflow_error` when variable count exceeds 63-bit feasible counting range guard (`>=64`).

## Usage recommendations

- use only for small formulas;
- use to validate parser/encoding correctness against advanced engines;
- do not use as production default.
