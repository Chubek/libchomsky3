# Chapter-3: Core C++ API

## API Surface

- primary include: `IPCtk.hpp`.
- DSL helper include: `DSLUtils.hpp`.

## API Contracts

- header-only consumption model.
- semantic operations should remain transport-agnostic.
- error surfaces should preserve parse/validation context.

## Ownership and Lifetime

- resource wrappers must define deterministic teardown semantics.
- staged pipeline nodes should avoid hidden shared mutable state.

## Concurrency

- pipeline composition should isolate synchronization boundaries.
- shared-state mutation requires explicit lock stage pairing.

## Serialization

- encode/decode stages must be explicitly typed and ordered.
- backend generation must not alter semantic ordering.
