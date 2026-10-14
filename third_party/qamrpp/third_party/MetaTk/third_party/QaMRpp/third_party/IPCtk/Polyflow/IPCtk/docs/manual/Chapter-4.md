# Chapter-4: IPC-L Language Manual

## Core Constructs

- resource declarations (`socket`, `queue`, `shared`, `signal`, lock primitives).
- pipeline stages (`recv`, `decode`, `enqueue`, `notify`, `send`).
- named flow composition.

## Validation

- symbol declaration before use;
- stage I/O compatibility;
- synchronization correctness around mutable shared state.

## Serialization Semantics

- decode boundary defines typed payload contract.
- transport framing is explicit at ingress/egress stages.

## Common Failures

- unresolved identifiers;
- incompatible stage chaining;
- ambiguous target mappings without ITKD coverage.
