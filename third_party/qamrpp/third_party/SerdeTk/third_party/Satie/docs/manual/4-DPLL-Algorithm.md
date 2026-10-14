# DPLL Solver

## Algorithmic pipeline

`DPLLSolver` applies classical DPLL with three simplification layers:

1. unit propagation;
2. pure literal elimination;
3. branching with recursive backtracking.

Conflict checks occur before and after simplification to prune infeasible branches early.

## Unit propagation

For each unsatisfied clause:

- if no unassigned literal remains => conflict;
- if exactly one unassigned literal remains => force assignment.

Propagation iterates to fixed point.

Effect:

- reduces branching factor;
- surfaces contradictions before decision branching.

## Pure literal elimination

For unassigned variables in unresolved clauses:

- if variable appears only positive, set `true`;
- if only negative, set `false`.

Effect:

- monotonic simplification;
- does not reduce completeness.

## Branching heuristic

Current heuristic selects unassigned variable with maximum occurrence frequency in unresolved clauses.

Tradeoff:

- low overhead;
- weaker than activity-based VSIDS-class heuristics used in CDCL.

## Backtracking model

- branch true first;
- on failure, backtrack and branch false;
- if both fail, return conflict to caller.

Backtracking is chronological.

## Statistics semantics

- `recursive_calls`: total recursive search invocations.
- `decisions`: explicit branch-variable choices.
- `propagations`: unit assignments inferred.
- `pure_literal_eliminations`: assignments from polarity purity.
- `backtracks`: failed branch returns.
- `conflicts`: detected contradictions.

## Usage recommendations

- suitable for medium-size structured formulas;
- use when deterministic, interpretable search behavior is preferred;
- fallback to CDCL for harder industrial instances.
