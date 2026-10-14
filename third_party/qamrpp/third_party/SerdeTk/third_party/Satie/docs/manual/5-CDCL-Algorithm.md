# CDCL Solver

## Core model

`CDCLSolver` extends DPLL with:

- implication trail with decision levels;
- conflict analysis by clause resolution;
- learned clause insertion;
- non-chronological backjumping.

## State representation

Primary internal state:

- clause database (`original + learned`);
- per-variable assignment value;
- per-variable decision level;
- per-variable reason clause index;
- trail entries `(literal, level, antecedent)`;
- variable activity scores.

## Propagation

`propagate_full()` scans clauses repeatedly:

- conflicting unsatisfied clause => conflict index;
- unit unresolved clause => implied assignment with antecedent.

Propagation stops at fixpoint or first conflict.

## Conflict analysis

On conflict:

1. initialize learned clause from conflicting clause;
2. resolve against antecedent clauses of current-level literals;
3. reduce current-level literal count toward one-UIP-like boundary;
4. compute backjump level as max non-current level in learned clause;
5. bump activity of variables in learned clause.

## Learning and backjump

- learned clause appended to clause database;
- backjump unassigns trail entries above target level;
- search resumes from reduced level with stronger clause set.

Effect:

- avoids rediscovering equivalent conflicts;
- compresses search depth via non-chronological rewind.

## Branching

Current branching selects highest activity unassigned variable.

- conflict-driven activity growth biases toward recently conflicting variables.
- tie behavior is deterministic by scan order.

## Statistics semantics

- `decisions`: decision assignments.
- `propagations`: implied assignments.
- `conflicts`: encountered conflicts.
- `learned_clauses`: total inserted learned clauses.
- `restarts`: reserved metric (currently not triggered by restart policy).
- `backjumps`: non-chronological backtracks.

## Operational recommendations

- use as default production engine;
- persist learned-clause and conflict metrics for benchmark regression;
- pair with high-quality CNF encoding to maximize pruning efficiency.
