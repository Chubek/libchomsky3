# SAT Theory

## Problem definition

Boolean satisfiability (SAT) asks whether there exists an assignment \(\sigma: V \rightarrow \{0,1\}\) such that formula \(F\) evaluates to true.

- Decision output: `SAT` or `UNSAT`.
- Witness output (for `SAT`): model assignment.
- Certificate output (for `UNSAT`): not produced by current Satie interfaces.

## CNF normal form

Satie operates on Conjunctive Normal Form (CNF):

\[
F = \bigwedge_{i=1}^{m} C_i, \quad C_i = \bigvee_{j} l_{ij}, \quad l_{ij} \in \{x_k, \neg x_k\}
\]

Semantics:

- Clause is satisfied if any literal is true.
- CNF is satisfied if all clauses are satisfied.
- Empty clause is immediate contradiction.
- Empty formula is trivially satisfiable.

## Complexity profile

- SAT is NP-complete.
- Modern practical SAT solving is heuristic-driven.
- Structural regularity dominates runtime more than raw clause count.

Engineering implications:

- Parser and normalization quality affect downstream solver behavior.
- Branching heuristic quality controls exponential search width.
- Conflict learning quality controls repeated-search suppression.

## Core operational invariants

Satie represents literals as signed 32-bit integers.

- Positive `k` represents variable `x_k`.
- Negative `-k` represents `~x_k`.
- `0` is clause terminator in DIMACS and not a valid literal inside clause storage.

Assignment domain:

- `TRUE`, `FALSE`, `UNKNOWN`.
- Unknown state enables partial assignment propagation.

## Satisfiability and conflict predicates

Given partial assignment \(A\):

- clause-satisfied: some literal evaluates `TRUE`;
- clause-conflicting: all literals evaluate `FALSE`;
- formula-conflicting: any clause conflicting;
- formula-satisfied: all clauses satisfied and no empty-clause contradiction.

These predicates are shared across Native, DPLL, and CDCL in `Common.hpp`.

## Canonical dataflow in Satie

Internal solver loading path normalizes through CNF↔DIMACS transforms:

1. Input parse (CNF DSL or DIMACS);
2. CNF normalization (literal dedup/order, metadata updates);
3. Conversion compatibility path (`cnf_to_dimacs` / `dimacs_to_cnf`);
4. Engine-specific solving.

Result: homogeneous internal structure for all engines and deterministic baseline behavior.
