# ITKD Manual

## Introduction

ITKD defines target mapping and emission templates for IPC-L semantic graphs.

## Design Goals

- deterministic generation;
- target-aware runtime mapping;
- reusable destination templates;
- strict semantic preservation.

## File Structure

- target metadata;
- mapping clauses;
- template blocks;
- helper runtime bindings.

## Type and Syntax Model

- declaration-oriented mapping statements;
- placeholder substitutions for emitted symbols/types;
- ordered emission sections.

## Backend Targets

- C: `dest/C.itkd`.
- Python: `dest/Python.itkd`.
- Ruby: `dest/Ruby.itkd`.

## Template/Destination Behavior

- semantic nodes map to target code skeletons.
- stage ordering is retained.
- transport/resource lifecycle code emitted per template rules.

## Generated Artifacts

- deterministic symbol naming;
- explicit setup/teardown sections;
- predictable error-propagation boundaries.

## Best Practices

- keep mapping coverage complete for all IPC-L constructs;
- avoid backend-specific semantics in IPC-L layer;
- version template changes with compatibility notes.

## Troubleshooting

- unresolved template placeholder;
- incomplete mapping tables;
- target runtime dependency mismatch.

## Reference Tables

| Target | Destination File | Typical Output |
|---|---|---|
| C | `dest/C.itkd` | source/header stubs |
| Python | `dest/Python.itkd` | module wrapper code |
| Ruby | `dest/Ruby.itkd` | extension wrapper code |
