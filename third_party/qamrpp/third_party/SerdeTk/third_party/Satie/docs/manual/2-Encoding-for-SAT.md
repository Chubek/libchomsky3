# Encoding for SAT

## Scope

This section specifies how to encode constraints into SAT input accepted by Satie.

Accepted frontends:

- CNF DSL parser: symbolic identifiers (`a`, `node_12`, `flag`).
- DIMACS parser: integer-indexed literals with `p cnf` header.

## Variable mapping

CNF DSL identifiers are interned into positive integer variables.

- first distinct identifier => variable `1`;
- next distinct identifier => `2`, etc.

DIMACS directly provides variable indices.

Encoding rule:

- maintain stable symbol-to-index mapping for reproducibility;
- avoid semantic overloading of variable names.

## Primitive constraints

### Unit constraint

`x = true`:

- CNF: `(x)`
- DIMACS: `k 0`

`x = false`:

- CNF: `(~x)`
- DIMACS: `-k 0`

### Implication

`a -> b`:

- CNF: `(~a | b)`
- DIMACS: `-a b 0`

### Equivalence

`a <-> b`:

- CNF: `(~a | b) & (~b | a)`

### At-least-one

`x1 v x2 v ... v xn`:

- one clause containing all variables.

### At-most-one (pairwise)

For set \(S = \{x_1...x_n\}\):

\[
\bigwedge_{i<j}(\neg x_i \lor \neg x_j)
\]

Complexity: \(O(n^2)\) clauses.

### Exactly-one

- at-least-one + at-most-one.

## Structural encoding patterns

### Resource exclusivity

If task `t` must select one resource from `R`:

- variables `t_r` for each `r in R`;
- encode exactly-one over `{t_r}`.

### Dependency closure

If selecting `a` requires all `b_i`:

- encode `a -> b_i` for each dependency.

### Mutual exclusion across groups

If `g1` excludes `g2`:

- encode `~x v ~y` for each `x in g1`, `y in g2`.

## DIMACS emission in Satie

Use:

- `to_dimacs_string(cnf)` for benchmark export;
- `cnf_to_dimacs(cnf)` for structured conversion.

Export rule:

- include fixed problem line `p cnf <vars> <clauses>`;
- terminate every clause with `0`.

## Validation checklist

- no literal `0` inside in-memory clauses;
- declared DIMACS variable count >= max variable index;
- declared DIMACS clause count == parsed clauses;
- no accidental empty clause unless forcing UNSAT test;
- normalize before long-running benchmarks.
