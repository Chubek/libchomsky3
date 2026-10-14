# IPC-L Manual

## Introduction

IPC-L defines protocol graphs over IPC primitives using explicit, composable stages.

## Design Goals

- declarative flow specification;
- explicit synchronization;
- deterministic validation;
- backend portability via ITKD.

## File Structure

- resource declarations;
- flow definitions;
- stage chains;
- endpoint/dispatch wiring.

## Lexical Conventions

- UTF-8 source;
- identifier-oriented declarations;
- stage separators for dataflow composition.

## Type System

- typed decode/encode boundaries;
- payload schema compatibility across stage transitions;
- backend-mappable primitive and aggregate forms.

## Declaration Forms

- sockets, queues, shared-memory regions, signals, mutex/semaphore primitives.
- named pipelines with explicit ingress and egress stages.

## Serialization Behavior

- ingress framing and decode are explicit stages.
- egress encode and send are explicit stages.
- semantic order is preserved in generation output.

## Error Model

- lexical/parse failures with source location;
- symbol-resolution failures;
- stage type incompatibility;
- backend mapping failures.

## Validation Rules

- declaration-before-use;
- chain compatibility;
- lock/unlock pairing for shared-state mutation;
- no unresolved terminal transport stages.

## Best Practices

- normalize reusable stage patterns;
- isolate transport from business transforms;
- reduce mutable shared-state usage via queue fanout.

## Troubleshooting

- parser failure: tokenization or declaration form mismatch;
- unresolved symbol: missing or shadowed declaration;
- generation failure: missing ITKD mapping coverage.

## Reference Tables

| Category | Examples |
|---|---|
| Resource | socket, queue, shared, signal, mutex |
| Ingress stages | recv, decode, validate |
| Transform stages | map, filter, route, enqueue |
| Egress stages | encode, send, notify |
